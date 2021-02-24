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

# Generates debug information which can be used

set -e


ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BRAND=${BRAND:-pia}
PLATFORM=${PLATFORM:-win}
ARCH=${ARCH:-x64}
MODE=release

# Setup dirs
OUT_DIR="$ROOT/out/$BRAND"
ARTIFACT_DIR="$OUT_DIR/artifacts"
DEBUG_DIR="$OUT_DIR/debug"
if [ "$PLATFORM" == "win" ]; then
    DEBUG_DIR="$DEBUG_DIR/$ARCH"
fi
rm -rf "$DEBUG_DIR"
mkdir -p "$DEBUG_DIR"

echo "Running gendebug."
echo "BRAND=$BRAND"
echo "PLATFORM=$PLATFORM"
echo "ARCH=$ARCH"
echo "MODE=$MODE"

if [ "$PLATFORM" == "win" ]; then
    _7Z=$(cygpath -u "$PROGRAMFILES\7-Zip\7z.exe")
else
    ZIP=${ZIP:-zip}
fi

# Archive the original PDB symbols on Windows in case we need to analyze a dump
# with WinDbg
function archive_pdb_win() {
    SYMBOLS_DIR="$OUT_DIR/symbols/symbols_${BRAND}_${ARCH}"
    rm -rf "$SYMBOLS_DIR"
    mkdir -p "$SYMBOLS_DIR"

    echo "Archiving target symbols"
    find "$1" -type f \
        \( -name "*.exe" -o -name "*.pdb" -o -name "*.dll" \) \
        -and ! \( -name "test-*.exe" -o -name "test-*.pdb" -o -name "test-*.dll" \) \
        -exec cp {} "$SYMBOLS_DIR" \;

    "$_7Z" a -mx=9 "$DEBUG_DIR/symbols-$2.zip" "$SYMBOLS_DIR"
}

function make_debug_win () {
    BUILD_ROOT="$OUT_DIR/build/$ARCH/$MODE"
    VERSION_FILE="$BUILD_ROOT/version/version-unix.txt"

    # Normalize line endings and create a new version file
    tr -d '\15\32' < "$BUILD_ROOT/version/version.txt" > "$VERSION_FILE"
    { read -r VERSION; read -r PRODUCTNAME; read -r PACKAGENAME; } < "$VERSION_FILE"

    archive_pdb_win "$BUILD_ROOT" "$PACKAGENAME"

    "$ROOT/deps/dump_syms/dump_syms.exe" "$(cygpath -w "$BUILD_ROOT/install-root/$BRAND-client.exe")" > "$DEBUG_DIR/client.sym"
    "$ROOT/deps/dump_syms/dump_syms.exe" "$(cygpath -w "$BUILD_ROOT/install-root/$BRAND-service.exe")" > "$DEBUG_DIR/daemon.sym"
    cp "$VERSION_FILE" "$DEBUG_DIR/version.txt"
    cp "$ARTIFACT_DIR/$PACKAGENAME.exe" "$DEBUG_DIR/$PACKAGENAME.exe"

    # Zip the debug artifacts
    "$_7Z" a -mx=9 "$ARTIFACT_DIR/debug-$PACKAGENAME.zip" "$DEBUG_DIR"/*
}

function make_debug_mac () {
    VERSION_FILE="$OUT_DIR/build/$MODE/version/version.txt"

    { read -r VERSION; read -r PRODUCTNAME; read -r PACKAGENAME; } < "$VERSION_FILE"

    APP="$OUT_DIR/build/$MODE/install-root/$PRODUCTNAME.app"
    DUMP_SYMS="$ROOT/deps/dump_syms/dump_syms_mac"

    $DUMP_SYMS "$APP/Contents/MacOS/$PRODUCTNAME" > "$DEBUG_DIR/client.sym"
    $DUMP_SYMS "$APP/Contents/MacOS/$BRAND-daemon" > "$DEBUG_DIR/daemon.sym"
    cp "$ARTIFACT_DIR/$PACKAGENAME.zip" "$DEBUG_DIR/$PACKAGENAME.zip"
    cp "$VERSION_FILE" "$DEBUG_DIR/version.txt"

    "$ZIP" -y -q -r "$ARTIFACT_DIR/debug-$PACKAGENAME.zip" "$DEBUG_DIR"/*
}

function make_debug_linux () {
    BUILD_PACKAGE="$OUT_DIR/build"
    VERSION_FILE="$BUILD_PACKAGE/$MODE/version/version.txt"
    { read -r VERSION; read -r PRODUCTNAME; read -r PACKAGENAME; } < "$BUILD_PACKAGE/$MODE/version/version.txt"
    PIA_FILES="$BUILD_PACKAGE/$PACKAGENAME/piafiles"
    DUMP_SYMS="$ROOT/deps/dump_syms/dump_syms_linux.bin"
    "$DUMP_SYMS" "$PIA_FILES/bin/$BRAND-client" > "$DEBUG_DIR/client.sym"
    "$DUMP_SYMS" "$PIA_FILES/bin/$BRAND-daemon" > "$DEBUG_DIR/daemon.sym"
    cp "$ARTIFACT_DIR/$PACKAGENAME.run" "$DEBUG_DIR/$PACKAGENAME.run"
    cp "$VERSION_FILE" "$DEBUG_DIR/version.txt"

    "$ZIP" -y -q -r "$ARTIFACT_DIR/debug-$PACKAGENAME.zip" "$DEBUG_DIR"/*
}

"make_debug_$PLATFORM"

echo "gendebug finished"
