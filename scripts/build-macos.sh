#!/bin/bash

# Copyright (c) 2019 London Trust Media Incorporated
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

MODE=release
BRAND=${BRAND:-pia}

# Set QTROOT to the path to the Qt 5.12 installation, such as
# "/Users/<username>/Qt/5.12.3/".  The script attempts to find Qt at a default
# location if this isn't set.
#
# macdeployqt and qbs are located automatically using this path.
# MACDEPLOYQT and QBS can be overridden if necessary.
#
# CURL, CODESIGN, and ZIP can also be overridden manually if they aren't in
# PATH.
#
# To sign the output, set PIA_CODESIGN_CERT to the signing identity (see
# codesign -s).
#
# To notarize the output, set the Apple ID credentials in PIA_APPLE_ID_EMAIL /
# PIA_APPLE_ID_PASSWORD.

function die() {
  echo "error:" "$@"
  exit 1
}

function first() {
    local FILES=( "$@" )
    [ -e "${FILES[0]}" ] || die "Unable to match file pattern" "$@"
    echo "${FILES[0]}"
}
function last() {
  local FILES=( "$@" )
  [ -e "${FILES[${#FILES[@]}-1]}" ] || die "Unable to match file pattern" "$@"
  echo "${FILES[${#FILES[@]}-1]}"
}
function verify() {
  hash "$1" 2> /dev/null || die "$@" "executable not found"
}

ROOT=${ROOT:-"$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"}
OUTDIR="$ROOT/out/$BRAND"
BUILD=${BUILD:-"$OUTDIR/build"}
ARTIFACTS=${ARTIFACTS:-"$OUTDIR/artifacts"}

if [ -z "${QTROOT}" ]; then
  if [ -d "$HOME/Qt/5.12.3" ]; then
    QTROOT="$HOME/Qt/5.12.3"
  elif [ -d "$HOME/Qt5.12/5.12.3" ]; then
    QTROOT="$HOME/Qt5.12/5.12.3"
  fi
fi

# Optional paths to tools; can be overridden in environment
MACDEPLOYQT=${MACDEPLOYQT:-"${QTROOT}/clang_64/bin/macdeployqt"}
QBS=${QBS:-"$(cd "${QTROOT}/../Qt Creator.app/Contents/MacOS" && pwd)/qbs"}
CURL=${CURL:-curl}
CODESIGN=${CODESIGN:-codesign}
ZIP=${ZIP:-zip}

pushd "$ROOT"
trap popd EXIT

verify "$QBS"
verify "$MACDEPLOYQT"
verify "$CURL"
verify "$ZIP"

# Set up the build profile for qbs
"$QBS" setup-toolchains --detect
"$QBS" setup-qt "${QTROOT}/clang_64/bin/qmake" pia-desktop-build
"$QBS" config profiles.pia-desktop-build.baseProfile clang

# Wipe the output directory before starting
rm -rf "$OUTDIR"

# Fetch a region list to bundle.
"$CURL" -o "$ROOT/daemon/res/json/servers.json" "https://www.privateinternetaccess.com/vpninfo/servers?version=1001&client=x-alpha" || die "Unable to fetch region list"
"$CURL" -o "$ROOT/daemon/res/json/shadowsocks.json" "https://www.privateinternetaccess.com/vpninfo/shadowsocks_servers" || die "Unable to fetch shadowsocks region list"

# Build all produects in the project in debug/release (use an in-source
# build directory or GitLab won't let us pick up artifacts)
#
# Note: This also executes the autotest runner for all unit tests
"$QBS" build --file "$ROOT/pia_desktop.qbs" --build-directory "$BUILD" profile:pia-desktop-build "config:$MODE" project.brandCode:"$BRAND" project.macdeployqt:true --all-products

# Create the artifacts directory
mkdir -p "$BUILD" "$ARTIFACTS"

# Copy .ts files
cp "$BUILD/release/translations/translations.zip" "$ARTIFACTS/"
cp "$BUILD/release/translations/en_US.onesky.ts" "$ARTIFACTS/"

# Copy code coverage artifacts if they were produced (for local builds unit
# tests might be skipped)
if [ -d "$BUILD/release/llvm-code-coverage" ]; then
    cp -r "$BUILD/release/llvm-code-coverage" "$ARTIFACTS/"
fi

# Perform post-processing on each config
for CONFIG in $MODE; do
    { read -r VERSION; read -r PRODUCTNAME; read -r PACKAGENAME; } < "$BUILD/$CONFIG/version/version.txt"
    { read -r BRAND_NAME; read -r BRAND_CODE; read -r BRAND_BUNDLE_ID; } < "$BUILD/$CONFIG/brand/brand.txt"

    # Copy pia-integtest as an artifact
    cp "$BUILD/$CONFIG"/integtest-dist.*/"$BRAND_CODE-integtest.zip" "$ARTIFACTS/$BRAND_CODE-integtest-$PACKAGENAME.zip"

    cd "$BUILD/$CONFIG/install-root"

    APP="$(first ./*.app)"

    if [ -z "$PIA_BRANCH_BUILD" ] || [ "$PIA_BRANCH_BUILD" == "master" ]; then
        # Zip the app with the original name to notarize it, notarize-macos.sh
        # will build the installer zip after notarizing
        "$ZIP" -y -q -r "$ARTIFACTS/$PACKAGENAME.zip" "$APP"
        "$ROOT/scripts/notarize-macos.sh" "$ARTIFACTS/$PACKAGENAME.zip" "$BRAND_BUNDLE_ID" "$APP"
        # Remove the original zip that was uploaded, then build the installer
        # zip from the stapled app bundle
        rm "$ARTIFACTS/$PACKAGENAME.zip"
    fi

    # Build the Installer zip (same app, but with Installer appended to the
    # name).  Rename the app bundle, zip it, then restore the original name.
    INSTALLER="${APP%.app} Installer.app"
    mv "$APP" "$INSTALLER"
    "$ZIP" -y -q -r "$ARTIFACTS/$PACKAGENAME.zip" "$INSTALLER"
    mv "$INSTALLER" "$APP"

    if [ -z "$PIA_BRANCH_BUILD" ] || [ "$PIA_BRANCH_BUILD" == "master" ]; then
      PLATFORM="mac" BRAND="$BRAND" "$ROOT/scripts/gendebug.sh"
    fi
done

echo "Done."
