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

sudo apt-get update
sudo apt-get upgrade -y

sudo chroot /mnt/image /usr/bin/apt-get update
sudo chroot /mnt/image /usr/bin/apt-get upgrade -y

# Run the installation inside the chroot
echo "Installing kernel from trixie backports"
sudo chroot /mnt/image /usr/bin/apt-get install -t trixie-backports -y ${linux_image_deb}

if ! sudo chroot /mnt/image /usr/bin/dpkg -s "${linux_image_deb}" >/dev/null 2>&1; then
  echo "CREATE IMAGE FAILED!!!"
  echo "Expected ${linux_image_deb} to be installed, but it is missing. Aborting purge."
  exit 1
fi
echo "Verification passed: ${linux_image_deb} installed successfully."

installed_kernels=$(sudo chroot /mnt/image /usr/bin/dpkg-query -W -f='${Package}\n' 'linux-image-[0-9]*' 2>/dev/null)

for kernel in $installed_kernels; do
  if [[ "$kernel" != "${linux_image_deb}" ]]; then
    echo "Deleting former kernel: $kernel"
    sudo chroot /mnt/image /usr/bin/apt-get --purge -y remove "$kernel"
  fi
done

echo "END: Kernel update complete. ${linux_image_deb} is the only kernel remaining."

# Skip unmounting:
#  Sometimes systemd starts, making it hard to unmount
#  In any case we'll unmount cleanly when the instance shuts down
