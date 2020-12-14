#! /usr/bin/env bash

# Copyright (c) 2020 Private Internet Access, Inc.
#
# This file is part of the Private Internet Access Desktop Client.
#
# The Private Internet Access Desktop Client is free software: you can
# redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# The Private Internet Access Desktop Client is distributed in the hope that
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the Private Internet Access Desktop Client.  If not, see
# <https://www.gnu.org/licenses/>.

set -e

SCRIPTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BIND_MOUNTS=(/proc /sys /dev /dev/pts /home /tmp)

function show_usage() {
    echo "usage:"
    echo "$0 [--user user] [--group group] [--bind dir] [--cross-target arch]"
    echo "$0 --help"
    echo ""
    echo "Creates a Debian 9 (Stretch) chroot for building PIA Desktop on Linux."
    echo "(See README.md.)  Invoke as root.  Use setup.sh instead (as your normal"
    echo "user) to automatically permit your user to enter the chroot."
    echo ""
    echo "Parameters:"
    echo "  --user user: Permit 'user' to enter the chroot with schroot.  Can be"
    echo "    given more than once."
    echo "  --group group: Permit 'group' to enter the chroot with schroot.  Can"
    echo "    be given more than once."
    echo "  --bind dir: Also bind 'dir' into the chroot - such as --bind /opt if"
    echo "    Qt is installed to /opt instead of your home directory, or"
    echo "    --bind /media/user/workspace-disk if the repo is on external media."
    echo "    Can be given more than once.  Default binds are:"
    echo "      " "${BIND_MOUNTS[@]}"
    echo "    The script also verifies that pia_desktop is visible under at least"
    echo "    one bind mount and will prompt for an additional binding if needed."
    echo "  --cross-target arch: Install development dependencies for 'arch'"
    echo "    instead of the host architecture.  The resulting schroot config"
    echo "    will be named piabuild-stretch-<arch>"
    echo "  --help: Shows this help."
    echo ""
}

# Parse args before checking if the user is root; we can do --help without root
PERMIT_USERS=""
PERMIT_GROUPS=""
CROSS_TARGET=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        "--help")
            show_usage
            exit 0
            ;;
        "--user")
            PERMIT_USERS="$PERMIT_USERS${PERMIT_USERS:+,}$2"
            shift
            shift
            ;;
        "--group")
            PERMIT_GROUPS="$PERMIT_GROUPS${PERMIT_GROUPS:+,}$2"
            shift
            shift
            ;;
        "--bind")
            BIND_MOUNTS+=("$2")
            shift
            shift
            ;;
        "--cross-target")
            CROSS_TARGET="$2"
            shift
            shift
            ;;
        *)
            echo "Unknown option: $1" >&2
            show_usage
            exit 1
            ;;
    esac
done

if [ "$EUID" -ne 0 ]; then
    echo "Invoke as root to set up a PIA build chroot" >&2
    exit 1
fi

ARCH_SUFFIX="${CROSS_TARGET:+-$CROSS_TARGET}"
STRETCH_CHROOT="/opt/piabuild/stretch$ARCH_SUFFIX"

if [ -e "$STRETCH_CHROOT" ]; then
    echo "WARNING: Directory $STRETCH_CHROOT exists, existing contents will be deleted."
    read -rp "Continue? [y/N]: " DELETE_EXISTING
    if ! [[ $DELETE_EXISTING =~ ^[yY]$ ]]; then
        echo "Canceled."
        exit 0
    fi
fi

# Check if the repo is visible in any of the bind mounts we have
REPO_BOUND=
for m in "${BIND_MOUNTS[@]}"; do
    if [[ "$SCRIPTDIR" == "$(readlink -f "$m")"/* ]]; then
        REPO_BOUND=1
    fi
done

if [ -z "$REPO_BOUND" ]; then
    # Parent of pia_desktop by default
    WORKSPACE_DIR="$(cd "$SCRIPTDIR/../../.." && pwd)"
    echo "The project directory is not under any bound mount, so an additional binding is"
    echo "needed to access it in the chroot."
    read -rp "Enter the path to bind (Ctrl+C to cancel): [$WORKSPACE_DIR]" ENTERED_WORKSPACE
    BIND_MOUNTS+=("${ENTERED_WORKSPACE:-$WORKSPACE_DIR}")
fi

echo "Clean existing chroot"
rm -rf "$STRETCH_CHROOT"

echo "Run debootstrap"
mkdir -p "$STRETCH_CHROOT"
debootstrap stretch "$STRETCH_CHROOT"

echo "Configure schroot"
# Create the fstab for the chroot - this is similar to the 'desktop'
# default fstab, but may contain an additional directory if the
# workspace wasn't under /home.
mkdir -p /etc/schroot/piabuild
{
    echo "# <file system> <mount point> <type>  <options> <dump> <pass>"
    for m in "${BIND_MOUNTS[@]}"; do
        echo "$m $m none rw,bind 0 0"
    done
} > "/etc/schroot/piabuild/stretch$ARCH_SUFFIX-fstab"

# Create an schroot config file
{
    echo "[piabuild-stretch$ARCH_SUFFIX]"
    echo "description=Debian 9 build environment for Private Internet Access"
    echo "type=directory"
    echo "directory=$STRETCH_CHROOT"
    echo "setup.fstab=piabuild/stretch$ARCH_SUFFIX-fstab"
    echo "users=$PERMIT_USERS"
    echo "groups=$PERMIT_GROUPS"
    echo "root-groups=root"
    echo "profile=desktop"
    echo "personality=linux"
    echo "preserve-environment=true"
} > "/etc/schroot/chroot.d/piabuild-stretch$ARCH_SUFFIX"

# Packages needed for host architecture
# For desktop:
HOST_PACKAGES_DTOP=(build-essential rake clang-7 git arch-test)
# For desktop-dep-build:
HOST_PACKAGES_DEP=(curl pv bison automake libtool python)
# Packages needed for target architecture(s)
# For desktop:
TARGET_PACKAGES_DTOP=(mesa-common-dev libnl-3-dev libnl-route-3-dev libnl-genl-3-dev zlib1g libglib2.0-0)
# For desktop-dep-build:
TARGET_PACKAGES_DEP=(libmnl-dev libclang-dev libssl-dev libxcb-xinerama0-dev libxcb-render0-dev libxcb-render-util0-dev libxcb-shape0-dev libxcb-randr0-dev libxcb-xfixes0-dev libxcb-sync-dev libxcb-shm0-dev libxcb-icccm4-dev libxcb-keysyms1-dev libxcb-image0-dev libxkbcommon-x11-dev libxi-dev libxrender-dev libxext-dev libx11-dev libx11-xcb-dev libxcb1-dev libfontconfig1-dev libfreetype6-dev libsm-dev libice-dev libglib2.0-dev libpq-dev libatspi2.0-dev libgl-dev libegl1-mesa-dev)

echo "Install host arch packages"
# Enter the chroot and install additional packages
schroot -c "piabuild-stretch$ARCH_SUFFIX" -- bash -c "apt-get update && apt-get install -y ${HOST_PACKAGES_DTOP[*]} ${HOST_PACKAGES_DEP[*]}"

echo "Install target arch packages"
# Install development deps for target architecture
ARCH_DEPS=("${ARCH_SUFFIX:+cross}build-essential$ARCH_SUFFIX")
for pkg in "${TARGET_PACKAGES_DTOP[@]}" "${TARGET_PACKAGES_DEP[@]}"; do
    ARCH_DEPS+=("$pkg${CROSS_TARGET:+:$CROSS_TARGET}")
done
schroot -c "piabuild-stretch$ARCH_SUFFIX" -- bash -c "${CROSS_TARGET:+dpkg --add-architecture $CROSS_TARGET && apt-get update &&} apt-get install -y ${ARCH_DEPS[*]}"
# Set up clang 7 in /usr/local/bin so we can build Qt with clang 7.
#
# The default gcc build works on arm64, but on armhf in stretch it seems to have
# ABI issues (gcc is observed trying to align std::initializer_list parameters
# to even-numbered registers in function calls, even though they have 4-byte
# alignment).
#
# There's no newer GCC in stretch, but clang 7 is new enough to build Qt and
# avoids the issue.  (clang 3.8 in the 'clang' package is too old.)  The 'clang'
# package just sets up symlinks like this for clang-3.8.
#
# This isn't needed for PIA Desktop itself, because clang.rb looks for the
# newest clang available.
echo "Set up clang 7 in /usr/local/bin"
schroot -c "piabuild-stretch$ARCH_SUFFIX" -- bash -c "cd /usr/local/bin; ln -s ../../lib/llvm-7/bin/clang; ln -s ../../lib/llvm-7/bin/clang++"

echo "Chroot configured successfully"
