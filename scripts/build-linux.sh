#!/bin/bash

# Copyright (c) 2023 Private Internet Access, Inc.
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

BRAND=${BRAND:-pia}

# ===========================
# Instructions to run locally
# ===========================
#
# First, Qt 5.15 or above must be installed on your system. You cannot use Qt
# from your package manager, get PIA's build from:
#   https://github.com/pia-foss/desktop-dep-build/releases
#
# If it's installed to /opt/Qt/ or $HOME/Qt/, the build process will find it
# automatically.  Otherwise, set QTROOT to the installation path, such as:
#    export QTROOT=.../Qt/5.12.8
#
# ===============
# Reproducibility
# ===============
#
# Linux build artifacts are reproducible given the same build environment and
# source directory location.
#
# The build script fetches server lists (daemon/res/*.json) if they do not
# exist, or after a new commit (see rake/product/desktop.rb).
#
# To override this, build with SERVER_DATA_DIR=<directory> to copy data files
# from the specified directory instead.
#
# The build uses the last Git commit timestamp by default.  To override this,
# you can set SOURCE_DATE_EPOCH=<timestamp> in the environment.

# PIA root directory. All paths derived from this path
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

# If a SERVER_DATA_DIR was given, get an absolute path before changing
# directories, in case a relative path was given
if [ -n "$SERVER_DATA_DIR" ]; then
    export SERVER_DATA_DIR
    SERVER_DATA_DIR="$(cd "$SERVER_DATA_DIR" && pwd)"
fi

pushd "$ROOT"

# Build the specified architectures.  By default, if PIA_BUILD_ARCHITECTURE
# isn't set, build for the host architecture.  Otherwise, cross-compile for the
# target architecture.
#
# Note that setting ARCHITECTURE implies a cross build, even if it actually is
# the host architecture.

if [ -z "ARCHITECTURE" ]; then
    echo "===Build x86_64==="
    ./scripts/chroot/enter.sh -- rake clean VARIANT=release BRAND="$BRAND"
    ./scripts/chroot/enter.sh -- rake all VARIANT=release BRAND="$BRAND"
else
    echo "===Build $ARCHITECTURE==="
    ./scripts/chroot/enter.sh "$ARCHITECTURE" -- rake clean VARIANT=release BRAND="$BRAND" ARCHITECTURE="$ARCHITECTURE"
    ./scripts/chroot/enter.sh "$ARCHITECTURE" -- rake all VARIANT=release BRAND="$BRAND" ARCHITECTURE="$ARCHITECTURE"
fi

popd
