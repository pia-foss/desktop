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

function print_usage() {
    echo "usage:"
    echo "$0 [--] <command> <args>"
    echo "$0 --path"
    echo "$0 --help"
    echo ""
    echo "Finds the active Qt Creator build configuration of pia_desktop and runs"
    echo "<command> in that build.  Qt Creator should be running in order to"
    echo "detect the build configuration that is currently in use."
    echo ""
    echo "Simplifies running commands from the console while developing with Qt"
    echo "Creator.  For example, instead of:"
    echo "    ../build-pia_desktop-Desktop_Qt_5_12_3_clang_64bit5-Debug/Debug/install-root/Private\ Internet\ Access.app/Contents/MacOS/pia-cli -v"
    echo "use:"
    echo "    ./qtcbuild.sh pia-cli -v"
    echo ""
    echo "With --path, instead locates the build and prints the bin path, for"
    echo "example:"
    echo "    nano $(./qtcbuild.sh --path)/data/settings.json"
    echo "(note: the data dir location is platform-specific)"
    echo ""
}

if [ $# -lt 1 ]; then
    print_usage
    exit 1
fi

print_path_only=""
args_done=""

while [ "$#" -gt 0 ] && [ -z "$args_done" ]; do
    case "$1" in
        "--help")
            print_usage
            exit 0
            ;;
        "--path")
            print_path_only=1
            shift
            ;;
        "--")
            args_done=1
            shift
            ;;
        "--*")
            echo "Unknown option $1"
            exit 1
            ;;
        *)
            # Not an option, start of command (don't shift this arg)
            args_done=1
            ;;
    esac
done

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# Find the newest .bg.lock file in any build directory to determine which build
# configuration is active.
NEWEST_BG_LOCK=""

for bglock in "$ROOT"/../build-pia_desktop-*/*/*.bg.lock; do
    if [ ! -f "$bglock" ]; then
        break
    fi
    if [ -z "$NEWEST_BG_LOCK" ] || [ "$bglock" -nt "$NEWEST_BG_LOCK" ]; then
        NEWEST_BG_LOCK="$bglock"
    fi
done

if [ ! -f "$NEWEST_BG_LOCK" ]; then
    echo "Could not find Qt Creator's active build configuration."
    echo "Is pia_desktop.qbs open in Qt Creator, and has a build finished?"
    exit 1
fi

BUILD_ROOT=$(cd "$(dirname "$NEWEST_BG_LOCK")" && pwd)
BUILD_BIN_PATH=""

case $OSTYPE in
    darwin*)
        BUILD_BIN_PATH="install-root/*.app/Contents/MacOS/"
        BUILD_LIB_PATH="install-root/*.app/Contents/MacOS/"
        ;;
    msys*)
        BUILD_BIN_PATH="install-root/"
        BUILD_LIB_PATH="install-root/"
        ;;
    linux*)
        BUILD_BIN_PATH="install-root/bin/"
        BUILD_LIB_PATH="install-root/lib/"
        ;;
    *)
        echo "OS type not known: $OSTYPE"
        exit 1
        ;;
esac

# Add matching directories to PATH
for dir in $BUILD_ROOT/$BUILD_BIN_PATH; do
    if [ ! -d "$dir" ]; then
        echo "Unable to find binary directory matching $dir" >&2
        exit 1
    fi
    # With --path, instead just print paths
    if [ ! -z "$print_path_only" ]; then
        # Canonicalize the path
        (cd "$dir" && pwd)
    else
        # Add to PATH
        PATH=$dir:$PATH
    fi
done

# Run the command with the new PATH (if not just printing paths)
if [ -z "$print_path_only" ]; then
    for dir in $BUILD_ROOT/$BUILD_LIB_PATH; do
        case "$OSTYPE" in
            linux*)
                export LD_LIBRARY_PATH=$dir:$LD_LIBRARY_PATH
                ;;
            *)
                ;;
        esac
    done
    "$@"
fi
