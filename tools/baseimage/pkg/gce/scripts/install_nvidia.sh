#!/usr/bin/env bash

# Copyright (C) 2025 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -x
set -o errexit
export DEBIAN_FRONTEND=noninteractive

arch=$(uname -m)
nvidia_arch=${arch}
[ "${arch}" = "x86_64" ] && arch=amd64
[ "${arch}" = "aarch64" ] && arch=arm64

# NVIDIA driver needs dkms which requires /dev/fd
if [ ! -d /dev/fd ]; then
  ln -s /proc/self/fd /dev/fd
fi

# Query the installed concrete kernel package version on disk rather than the metapackage
kmodver=$(dpkg -l | grep '^ii' | awk '{print $2}' | \
          grep '^linux-image-[0-9]' | head -n1 | sed 's/linux-image-//')

apt-get install -y wget

# Dependencies for nvidia-installer
codename=$(. /etc/os-release && echo "${VERSION_CODENAME:-}")
backports_suite="${codename}-backports"

if [ -n "${codename}" ] && [ -f "/etc/apt/sources.list.d/${backports_suite}.list" ] && \
   dpkg -s "linux-image-${kmodver}" 2>/dev/null | grep -q "bpo\|${backports_suite}"; then
  echo "Installing linux-headers from ${backports_suite}..."
  apt-get install -y -t "${backports_suite}" \
    "linux-headers-${kmodver}" \
    dkms \
    libglvnd-dev \
    libc6-dev \
    pkg-config
else
  echo "Installing linux-headers from default suite..."
  apt-get install -y \
    "linux-headers-${kmodver}" \
    dkms \
    libglvnd-dev \
    libc6-dev \
    pkg-config
fi

nvidia_version=570.158.01

wget -q https://us.download.nvidia.com/tesla/${nvidia_version}/NVIDIA-Linux-${nvidia_arch}-${nvidia_version}.run
chmod a+x NVIDIA-Linux-${nvidia_arch}-${nvidia_version}.run
./NVIDIA-Linux-${nvidia_arch}-${nvidia_version}.run -x
arch_specific_flags=""
if [[ "${nvidia_arch}" = "x86_64" ]]; then
  arch_specific_flags="--no-install-compat32-libs"
fi
NVIDIA-Linux-${nvidia_arch}-${nvidia_version}/nvidia-installer \
  ${arch_specific_flags} \
  --silent \
  --no-backup \
  --no-wine-files \
  --install-libglvnd \
  --dkms \
  -k "${kmodver}"
