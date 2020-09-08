#!/bin/bash

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

BRAND=${BRAND:-pia}

# ===========================
# Instructions to run locally
# ===========================
#
# First, Qt 5.12 or above must be installed on your system. You cannot use Qt from your
# package manager, get the open source version from https://www.qt.io/
#
# If it's installed to /opt/Qt/ or $HOME/Qt/, the build process will find it automatically.
# Otherwise, set QTROOT to the installation path, such as:
#    export QTROOT=.../Qt/5.12.8
#
# ===============
# Reproducibility
# ===============
#
# Linux build artifacts are reproducible given the same build environment and
# source directory location.
#
# The build script fetches server lists (daemon/res/{servers,shadowsocks}.json)
# for each build by default.
# To override this, build with SERVER_DATA_DIR=<directory> to copy servers.json
# and shadowsocks.json from the specified directory instead.
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

# Build
export RUBYOPT=-Eutf-8
rake clean VARIANT=release BRAND="$BRAND"
rake all VARIANT=release BRAND="$BRAND"

popd
