#!/bin/bash

# Copyright (c) 2021 Private Internet Access, Inc.
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

# The root of this script is tools/openvpn_container, and not the PIA source root
ROOT=${ROOT:-"$(cd "$(dirname "${BASH_SOURCE[0]}")/" && pwd)"}
cd "$ROOT" || exit

TEMPDIR="$ROOT/temp"

rm -rf "$TEMPDIR"
mkdir -p "$TEMPDIR/bin"
mkdir -p "$TEMPDIR/lib"

cp "$ROOT/../../deps/openvpn/linux/x86_64/pia-openvpn" "$TEMPDIR/bin/pia-openvpn"
cp "$ROOT/../../deps/openvpn/linux/x86_64/libcrypto.so.1.1" "$TEMPDIR/lib/"
cp "$ROOT/../../deps/openvpn/linux/x86_64/libssl.so.1.1" "$TEMPDIR/lib/"

docker build -t pia-openvpn .
