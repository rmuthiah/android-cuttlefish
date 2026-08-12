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

set -o errexit -o nounset -o pipefail

if [[ $# -eq 0 ]] ; then
  echo "usage: $0 <linux-image-deb>"
  exit 1
fi
linux_image_deb=$1

arch=$(uname -m)
[ "${arch}" = "x86_64" ] && arch=amd64
[ "${arch}" = "aarch64" ] && arch=arm64

APT_GET="sudo chroot /mnt/image /usr/bin/env DEBIAN_FRONTEND=noninteractive /usr/bin/apt-get -y -o Dpkg::Options::=--force-confdef -o Dpkg::Options::=--force-confold"

sudo DEBIAN_FRONTEND=noninteractive apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get upgrade -y -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold"

version=$(sudo chroot /mnt/image/ /usr/bin/dpkg -l | grep '^ii' | \
  awk '{print $2}' | grep '^linux-image-[0-9]' | paste -sd' ' - || true)
echo "START VERSION: ${version}"

${APT_GET} update
${APT_GET} upgrade

version=$(sudo chroot /mnt/image/ /usr/bin/dpkg -l | grep '^ii' | \
  awk '{print $2}' | grep '^linux-image-[0-9]' | paste -sd' ' - || true)
echo "AFTER UPGRADE VERSION: ${version}"

${APT_GET} install ${linux_image_deb}

if ! sudo chroot /mnt/image /usr/bin/dpkg -s "${linux_image_deb}" >/dev/null 2>&1; then
  echo "CREATE IMAGE FAILED!!!"
  echo "Package ${linux_image_deb} is not installed"
  exit 1
fi

installed_kernel=$(sudo chroot /mnt/image/ /usr/bin/dpkg -l | grep '^ii' | \
  awk '{print $2}' | grep '^linux-image-[0-9]' | paste -sd' ' - || true)
echo "END VERSION: ${installed_kernel}"

# Remove old kernel packages, keeping only the target kernel and the
# linux-image-cloud-${arch} meta-package.
old_kernels=$(sudo chroot /mnt/image /usr/bin/dpkg -l | grep '^ii' | awk '{print $2}' | \
  grep '^linux-image-' | grep -v "^${linux_image_deb}$" | grep -v "^linux-image-cloud-${arch}$" || true)
if [ -n "${old_kernels}" ]; then
  echo "Removing old kernel packages: ${old_kernels}"
  ${APT_GET} purge ${old_kernels}
  # update-grub may fail in a chroot; the grub config will be rebuilt
  # when the image boots, so this is non-fatal.
  sudo chroot /mnt/image /bin/sh -c 'command -v update-grub >/dev/null && update-grub || true'
else
  echo "No old kernel packages to remove"
fi

# Skip unmounting:
#  Sometimes systemd starts, making it hard to unmount
#  In any case we'll unmount cleanly when the instance shuts down
