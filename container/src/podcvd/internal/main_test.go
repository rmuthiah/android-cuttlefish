// Copyright (C) 2026 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package internal

import (
	"context"
	"encoding/json"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

type fakeContainerManager struct {
	baseDir  string
	execFn   func(ctr string, cmd []string) error
	lastExec []string
}

func (f *fakeContainerManager) ImageExists(ctx context.Context, name string) (bool, error) {
	return true, nil
}

func (f *fakeContainerManager) PullImage(ctx context.Context, name string) error {
	return nil
}

func (f *fakeContainerManager) ListTags(ctx context.Context, imageRepo string) ([]string, error) {
	return nil, nil
}

func (f *fakeContainerManager) RemoveImages(ctx context.Context, images []string) error {
	return nil
}

func (f *fakeContainerManager) ContainerExists(ctx context.Context, name string) (bool, error) {
	return true, nil
}

func (f *fakeContainerManager) InspectContainer(ctx context.Context, name string) (*ContainerInfo, error) {
	return &ContainerInfo{
		Config: &ContainerConfig{
			Labels: map[string]string{
				labelAttemptID: "test-attempt",
				labelBaseDir:   f.baseDir,
			},
		},
	}, nil
}

func (f *fakeContainerManager) CreateAndStartContainer(ctx context.Context, extraFlags []string, name string) (string, error) {
	return "ctr-id", nil
}

func (f *fakeContainerManager) ListContainers(ctx context.Context, all bool) ([]ContainerListEntry, error) {
	return nil, nil
}

func (f *fakeContainerManager) CopyFromContainer(ctx context.Context, ctr string, srcPath string, dstPath string) error {
	return nil
}

func (f *fakeContainerManager) ExecOnContainer(ctx context.Context, ctr string, cmd []string, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
	f.lastExec = append([]string(nil), cmd...)
	if f.execFn != nil {
		return f.execFn(ctr, cmd)
	}
	return nil
}

func (f *fakeContainerManager) StopAndRemoveContainer(ctx context.Context, ctr string) error {
	return nil
}

func TestHasBoolFlagOnSubCommandArgs(t *testing.T) {
	cases := []struct {
		name string
		args []string
		want bool
	}{
		{
			name: "absent",
			args: []string{"snapshot_take", "--snapshot_path=/tmp/snap"},
			want: false,
		},
		{
			name: "standalone_double_dash_at_end",
			args: []string{"snapshot_take", "--snapshot_path=/tmp/snap", "--force"},
			want: true,
		},
		{
			name: "standalone_single_dash_before_other_flag",
			args: []string{"snapshot_take", "-force", "--snapshot_path=/tmp/snap"},
			want: true,
		},
		{
			name: "equals_true",
			args: []string{"snapshot_take", "--force=true", "--snapshot_path=/tmp/snap"},
			want: true,
		},
		{
			name: "equals_false",
			args: []string{"snapshot_take", "--force=false", "--snapshot_path=/tmp/snap"},
			want: false,
		},
		{
			name: "space_separated_bool",
			args: []string{"snapshot_take", "--force", "false", "--snapshot_path=/tmp/snap"},
			want: false,
		},
		{
			name: "negated_flag_overrides_previous",
			args: []string{"snapshot_take", "--force", "--noforce", "--snapshot_path=/tmp/snap"},
			want: false,
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			cvdArgs, err := ParseCvdArgs(tc.args)
			if err != nil {
				t.Fatalf("ParseCvdArgs(%v) failed: %v", tc.args, err)
			}
			if got := cvdArgs.HasBoolFlagOnSubCommandArgs("force"); got != tc.want {
				t.Errorf("HasBoolFlagOnSubCommandArgs(\"force\") = %v, want %v", got, tc.want)
			}
		})
	}
}

func TestHandleSnapshotTakeExecution(t *testing.T) {
	baseDir := t.TempDir()
	hostDestDir := filepath.Join(t.TempDir(), "out_snapshot")

	ccm := &fakeContainerManager{
		baseDir: baseDir,
		execFn: func(ctr string, cmd []string) error {
			var containerSnapPath string
			for _, arg := range cmd {
				if v, ok := strings.CutPrefix(arg, "-snapshot_path="); ok {
					containerSnapPath = v
				} else if v, ok := strings.CutPrefix(arg, "--snapshot_path="); ok {
					containerSnapPath = v
				}
			}
			if !strings.HasPrefix(containerSnapPath, "/podcvd_base/snapshots/") {
				t.Fatalf("unexpected container snapshot_path %q in cmd %v", containerSnapPath, cmd)
			}
			rel := strings.TrimPrefix(containerSnapPath, "/podcvd_base/")
			hostStageDir := filepath.Join(baseDir, rel)
			if err := os.MkdirAll(hostStageDir, 0755); err != nil {
				return err
			}
			meta := map[string]any{
				"snapshot_path": containerSnapPath,
				"cf_home":       "/podcvd_base/home",
			}
			b, err := json.Marshal(meta)
			if err != nil {
				return err
			}
			return os.WriteFile(filepath.Join(hostStageDir, "snapshot_meta.json"), b, 0644)
		},
	}

	cvdArgs, err := ParseCvdArgs([]string{"--group_name=cvd_1", "snapshot_take", "--snapshot_path=" + hostDestDir})
	if err != nil {
		t.Fatalf("ParseCvdArgs failed: %v", err)
	}
	if err := handleSnapshotTakeExecution(ccm, cvdArgs); err != nil {
		t.Fatalf("handleSnapshotTakeExecution failed: %v", err)
	}

	metaBytes, err := os.ReadFile(filepath.Join(hostDestDir, "snapshot_meta.json"))
	if err != nil {
		t.Fatalf("failed to read exported snapshot_meta.json: %v", err)
	}
	var meta map[string]any
	if err := json.Unmarshal(metaBytes, &meta); err != nil {
		t.Fatalf("failed to unmarshal snapshot_meta.json: %v", err)
	}
	if got := meta["snapshot_path"]; got != hostDestDir {
		t.Errorf("exported snapshot_meta.json snapshot_path = %v, want %q", got, hostDestDir)
	}

	// Second call without --force must fail because hostDestDir now exists.
	cvdArgsNoForce, err := ParseCvdArgs([]string{"--group_name=cvd_1", "snapshot_take", "--snapshot_path=" + hostDestDir})
	if err != nil {
		t.Fatalf("ParseCvdArgs failed: %v", err)
	}
	if err := handleSnapshotTakeExecution(ccm, cvdArgsNoForce); err == nil {
		t.Fatalf("expected error when snapshot_path already exists without --force")
	}

	// Third call with standalone --force must succeed and overwrite hostDestDir.
	cvdArgsForce, err := ParseCvdArgs([]string{"--group_name=cvd_1", "snapshot_take", "--force", "--snapshot_path=" + hostDestDir})
	if err != nil {
		t.Fatalf("ParseCvdArgs failed: %v", err)
	}
	if err := handleSnapshotTakeExecution(ccm, cvdArgsForce); err != nil {
		t.Fatalf("handleSnapshotTakeExecution with --force failed: %v", err)
	}
}

func TestStageSnapshotForRestore(t *testing.T) {
	baseDir := t.TempDir()
	hostSnapDir := filepath.Join(t.TempDir(), "src_snapshot")
	if err := os.MkdirAll(hostSnapDir, 0755); err != nil {
		t.Fatal(err)
	}
	initialMeta := map[string]any{
		"snapshot_path": hostSnapDir,
		"cf_home":       "/podcvd_base/home",
	}
	b, err := json.Marshal(initialMeta)
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(hostSnapDir, "snapshot_meta.json"), b, 0644); err != nil {
		t.Fatal(err)
	}

	// Seed a stale snapshot directory under baseDir/snapshots to verify cleanup.
	staleDir := filepath.Join(baseDir, "snapshots", "stale-id")
	if err := os.MkdirAll(staleDir, 0755); err != nil {
		t.Fatal(err)
	}

	ccm := &fakeContainerManager{baseDir: baseDir}
	for _, subcmd := range []string{"create", "start"} {
		t.Run(subcmd, func(t *testing.T) {
			cvdArgs, err := ParseCvdArgs([]string{"--group_name=cvd_1", subcmd, "--snapshot_path=" + hostSnapDir})
			if err != nil {
				t.Fatalf("ParseCvdArgs failed: %v", err)
			}
			if err := stageSnapshotForRestore(ccm, cvdArgs); err != nil {
				t.Fatalf("stageSnapshotForRestore(%s) failed: %v", subcmd, err)
			}

			if _, err := os.Stat(staleDir); !os.IsNotExist(err) {
				t.Errorf("expected stale staged snapshot %q to be removed, stat err = %v", staleDir, err)
			}

			stagedFlag, ok := cvdArgs.GetStringFlagValueOnSubCommandArgs("snapshot_path")
			if !ok || !strings.HasPrefix(stagedFlag, "/podcvd_base/snapshots/") {
				t.Fatalf("rewritten snapshot_path = %q (ok=%v), want /podcvd_base/snapshots/<uuid>", stagedFlag, ok)
			}

			entries, err := os.ReadDir(filepath.Join(baseDir, "snapshots"))
			if err != nil {
				t.Fatalf("ReadDir(snapshots) failed: %v", err)
			}
			if len(entries) != 1 {
				t.Fatalf("expected exactly 1 staged snapshot dir in %s/snapshots, got %d", baseDir, len(entries))
			}

			stagedHostPath := filepath.Join(baseDir, strings.TrimPrefix(stagedFlag, "/podcvd_base/"))
			metaBytes, err := os.ReadFile(filepath.Join(stagedHostPath, "snapshot_meta.json"))
			if err != nil {
				t.Fatalf("failed to read staged snapshot_meta.json: %v", err)
			}
			var meta map[string]any
			if err := json.Unmarshal(metaBytes, &meta); err != nil {
				t.Fatalf("failed to unmarshal staged snapshot_meta.json: %v", err)
			}
			if got := meta["snapshot_path"]; got != stagedFlag {
				t.Errorf("staged snapshot_meta.json snapshot_path = %v, want %q", got, stagedFlag)
			}
		})
	}
}
