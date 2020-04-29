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

MODE=(release)
BRAND=${BRAND:-pia}

# ===========================
# Instructions to run locally
# ===========================
#
# First, Qt 5.12 or above must be installed on your system. You cannot use Qt from your
# package manager. It has to be downloaded from https://www.qt.io/
# Let's say they're installed at $HOME/Qt/
# You should add the following line to your .bashrc/.zshrc:
#
# export QT_ROOT=$HOME/Qt/5.12.1/gcc_64/
#
# QBS would then be used from $HOME/Qt/Tools/QtCreator/bin/qbs.  If necessary,
# you can override the QBS variable too.
#
# Next, install dependencies. This is for Ubuntu, but you can install the corresponding
# packages for your distribution
#
# $ sudo apt install build-essential curl clang mesa-common-dev
#
# $ cd desktop # PIA source code directory
#
# Run the script in configure mode to setup the compiler toolchain. You only need to do this once.
# $ ./scripts/build-linux.sh --configure
#
# Run the daemon (make sure any previously installed piavpn daemon is stopped by calling `sudo service piavpn stop` before running your own daemon)
# $ sudo --preserve-env=QT_ROOT ./scripts/build-linux.sh --run-daemon
#
# Run the client in a separate terminal
# $ ./scripts/build-linux.sh --run-client
#
# To build your own installer:
# $ ./scripts/build-linux.sh --package
#
# ===============
# Reproducibility
# ===============
#
# Linux build artifacts are reproducible given the same build environment and
# source directory location.  A few caveats apply to the --package step.
#
# The build script fetches server lists (daemon/res/{servers,shadowsocks}.json)
# for each build by default.
# To override this, use `--server-data <dir>` to copy servers.json and
# shadowsocks.json from the specified directory instead.
#
# The build uses the last Git commit timestamp by default.  To override this,
# you can set SOURCE_DATE_EPOCH=<timestamp> in the environment.

# Utility functions
function echoPass() {
    printf '\e[92m\xE2\x9C\x94\e[0m %s\n' "$@"
}
function echoFail() {
    printf '\e[91m\xE2\x9C\x98\e[0m %s\n' "$@"
}
function fail() {
    echoFail "$@"
    exit 1
}

function last() {
    local FILES=( "$@" )
    [ -e "${FILES[-1]}" ] || die "Unable to match file pattern" "$@"
    echo "${FILES[-1]}"
}

TASK_CONFIGURE=0
TASK_CLEAN=0
TASK_RUN_CLIENT=0
TASK_RUN_DAEMON=0
TASK_PACKAGE=0

SERVER_DATA_DIR=

function canRunTask() {
    [ $TASK_CONFIGURE -eq 1 ] && fail "Cannot run other tasks while passing --configure"
    [ $TASK_RUN_CLIENT -eq 1 ] && fail "Cannot run other tasks while passing --run-client"
    [ $TASK_RUN_DAEMON -eq 1 ] && fail "Cannot run other tasks while passing --run-daemon"

  # Return true so set -e doesn't assume function failed
  true
}

if [ $# -eq 0 ]; then
  echo "Usage: ./scripts/build-linux.sh [--configure] [--clean] [--run-client] [--run-daemon] [--package]"
  exit 1
fi

echo "========================================="
echo "Private Internet Access: Linux Build"
echo "========================================="

while (( "$#" )); do
  case "$1" in
    --configure)
      canRunTask
      TASK_CONFIGURE=1
      shift
      ;;
    --package)
      TASK_PACKAGE=1
      shift
      ;;
    --clean)
      TASK_CLEAN=1
      shift
      ;;
    --run-client)
      canRunTask
      TASK_RUN_CLIENT=1
      shift
      ;;
    --run-daemon)
      canRunTask
      TASK_RUN_DAEMON=1
      shift
      ;;
    --server-data)
      shift
      SERVER_DATA_DIR="$1"
      if [ -z "$SERVER_DATA_DIR" ]; then
        fail "--server-data requires a path to the directory containing servers.json and shadowsocks.json"
      fi
      shift
      ;;
    *)
      fail "Unknown option $1"
      ;;
  esac
done

# ===========
# Setup paths
# ===========

# PIA root directory. All paths derived from this path
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUTDIR="$ROOT/out/$BRAND"
# Place to build program during development
BUILD_DAEMON="$OUTDIR/build-daemon"
BUILD_CLIENT="$OUTDIR/build-client"
BUILD_PACKAGE="$OUTDIR/build"

# Look for manually specified QT_ROOT and QBS
if [ -n "$QT_ROOT" ]; then
  if [ -z "$QBS" ]; then
    QBS="$(cd "$QT_ROOT/../../Tools/QtCreator/bin" && pwd)/qbs"
  fi
  echo "Using qbs at $QBS"
  echo "Using Qt at $QT_ROOT [ specified via env variable ]"
# Look for Qt installed in known locations
elif [ -e /opt/Qt/ ]; then
  # Use the latest Qt version available, 5.1*.*
  # (Intentional word-splitting)
  # shellcheck disable=SC2046
  QT_ROOT=$(last $(echo "/opt/Qt/5.*/gcc_64/" | sort --version-sort))
  QBS="/opt/Qt/Tools/QtCreator/bin/qbs"
  echo "Found qbs at $QBS"
  echo "Found Qt at $QT_ROOT"
# If all else fails, try PATH, but this is unlikely to work if it picks up a
# system-installed Qt
elif hash qmake 2>/dev/null && hash qbs 2>/dev/null; then
  # Get the qbs from $PATH
  QBS="$(command -v qbs)"

  # Get the QT Root path
  QT_ROOT=$(dirname "$(command -v qmake)")/../
  QT_ROOT=$(cd "$QT_ROOT" && pwd)
  echo "Found qbs at $QBS"
  echo "Found Qt at $QT_ROOT"
else
  fail "Cannot find qmake and qbs.  Please ensure Qt is installed from qt.io, and set QT_ROOT/QBS if necessary."
fi

if ! hash clang 2>/dev/null; then
  fail "Cannot find clang in your PATH"
fi

# Check Qt version
QTVER=$("$QT_ROOT/bin/qmake" --version | tail -n +2 | awk '{print $4}')

SUPPORTED_VERS="5.12.2 5.12.3 5.12.4 5.12.5 5.12.6 5.12.7 5.12.8"

echo "Detected Qt version $QTVER"

if [[ ! $SUPPORTED_VERS =~ $QTVER ]]; then
  fail "Invalid version. Supported Qt versions: $SUPPORTED_VERS"
fi

# We need to "stage" the installation for setting up the artifacts etc
STAGE_ROOT="$OUTDIR/stage"
# Place to store the final package
ARTIFACT_PATH="$OUTDIR/artifacts"


# ============
# Tasks
# ============


if [ $TASK_CONFIGURE -eq 1 ]; then
  echo "Running task: configure"
  "$QBS" config --unset profiles.piaqt512
  "$QBS" setup-toolchains --detect
  "$QBS" setup-qt "$QT_ROOT/bin/qmake" piaqt512
  "$QBS" config profiles.piaqt512.baseProfile clang
  echoPass "Configured QBS. Profile config:"
  "$QBS" config --list profiles.piaqt512
fi

if [ $TASK_CLEAN -eq 1 ]; then
  echo "Running task: clean"
  rm -rf "$OUTDIR"
fi

if [ $TASK_RUN_CLIENT -eq 1 ]; then
    echo "Running task: run-client"
    "$QBS" build profile:piaqt512 -f "$ROOT/pia_desktop.qbs" -d "$BUILD_CLIENT" config:debug
    "$QBS" run profile:piaqt512 -f "$ROOT/pia_desktop.qbs" -d "$BUILD_CLIENT" config:debug --products client
fi

if [ $TASK_RUN_DAEMON -eq 1 ]; then
  echo "Running task: run-daemon"
  # Ensure dir exists for daemon socket.
  sudo mkdir -p /opt/piavpn/var/
  "$QBS" build profile:piaqt512 -f "$ROOT/pia_desktop.qbs" -d "$BUILD_DAEMON" config:debug
  "$QBS" run profile:piaqt512 -f "$ROOT/pia_desktop.qbs" -d "$BUILD_DAEMON" config:debug --products daemon
fi



function addQtLib() {
    libname=$1
    echo "Adding $libname"
    libPath="$QT_ROOT/lib/$libname"
    [ -f "$libPath" ] && cp -H "$libPath" "$STAGE_ROOT/lib/$libname"
    true
}

function addQtPlugin() {
    libname=$1
    echo "Adding plugin $libname"
    libPath="$QT_ROOT/plugins/$libname"
    [ -d "$libPath" ] && cp -r "$libPath" "$STAGE_ROOT/plugins/$libname"
    true
}

function addQmlImport () {
    libname=$1
    echo "Adding QML Import $libname"
    cp -r "$QT_ROOT/qml/$libname" "$STAGE_ROOT/qml/$libname"
}

if [ $TASK_PACKAGE -eq 1 ]; then
  # Clear the stage path and create a new one
  [ -d "$STAGE_ROOT" ] && rm -rf "$STAGE_ROOT"
  mkdir -p "$STAGE_ROOT"

  rm -rf "$ARTIFACT_PATH"
  mkdir -p "$ARTIFACT_PATH"

  # Create required dirs
  mkdir -p "$STAGE_ROOT/lib"
  mkdir -p "$STAGE_ROOT/plugins"
  mkdir -p "$STAGE_ROOT/qml"

  echoPass "Created staging root"
  addQtLib "libicudata.so.56"
  addQtLib "libicui18n.so.56"
  addQtLib "libicuuc.so.56"
  addQtLib "libQt5Core.so.5"
  addQtLib "libQt5DBus.so.5"
  addQtLib "libQt5Gui.so.5"
  addQtLib "libQt5Network.so.5"
  addQtLib "libQt5Qml.so.5"
  addQtLib "libQt5QuickControls2.so.5"
  addQtLib "libQt5Quick.so.5"
  addQtLib "libQt5QuickShapes.so.5"
  addQtLib "libQt5QuickTemplates2.so.5"
  addQtLib "libQt5Widgets.so.5"
  addQtLib "libQt5XcbQpa.so.5"

  addQtPlugin "platforms"
  addQtPlugin "egldeviceintegrations"
  addQtPlugin "xcbglintegrations"

  addQmlImport "builtins.qmltypes"
  addQmlImport "QtGraphicalEffects"
  addQmlImport "QtQml"
  addQmlImport "Qt"
  addQmlImport "QtQuick.2"
  addQmlImport "QtQuick"

  if [ -z "$SERVER_DATA_DIR" ]; then
    curl -o "$ROOT/daemon/res/json/servers.json" "https://www.privateinternetaccess.com/vpninfo/servers?version=1001&client=x-alpha" || fail "unable to fetch region list"
    curl -o "$ROOT/daemon/res/json/shadowsocks.json" "https://www.privateinternetaccess.com/vpninfo/shadowsocks_servers" || fail "unable to fetch shadowsocks region list"
  else
    echo "Using servers.json and shadowsocks.json from $SERVER_DATA_DIR"
    cp "$SERVER_DATA_DIR/servers.json" "$ROOT/daemon/res/json/servers.json"
    cp "$SERVER_DATA_DIR/shadowsocks.json" "$ROOT/daemon/res/json/shadowsocks.json"
  fi
  # Set the LD_LIBRARY_PATH so it's picked up by rpath
  export LD_LIBRARY_PATH="$STAGE_ROOT/lib"


  for CONFIG in "${MODE[@]}"; do
    # We want to use "$ORIGIN" in the string including the "$" so we single quote it, shellcheck assumes it's an error
    # shellcheck disable=SC2016
    "$QBS" build profile:piaqt512 -f "$ROOT/pia_desktop.qbs" -d "$BUILD_PACKAGE" config:"$CONFIG" modules.cpp.rpaths:\[\"'$ORIGIN/../lib'\"\] project.brandCode:"$BRAND" --all-products

    { read -r VERSION; read -r PRODUCTNAME; read -r PACKAGENAME; read -r BUILDTIMESTAMP; } < "$BUILD_PACKAGE/$CONFIG/version/version.txt"
    { read -r BRAND_NAME; read -r BRAND_CODE; read -r BRAND_ID; read -r BRAND_HELPDESK_LINK; } < "$BUILD_PACKAGE/$CONFIG/brand/brand.txt"

    echo "Building package for version $CONFIG"
    # Create a folder to stage the new build
    PKGROOT="$BUILD_PACKAGE/$PACKAGENAME"

    [ -d "$PKGROOT" ] && rm -rf "$PKGROOT"
    # The piafiles folder which would contain all the pia files to deploy
    PIA_FILES="$PKGROOT/piafiles"

    mkdir -p "$PIA_FILES"

    # Copy installer to root of package
    cp "$ROOT/extras/installer/linux/linux_installer.sh" "$PKGROOT/install.sh"
    chmod +x "$PKGROOT/install.sh"

    # Copy compiled binaries
    cp -r "$BUILD_PACKAGE/$CONFIG/install-root/"* "$PIA_FILES"


    # Copy the qt.conf to the same path as all binaries
    cp "$ROOT/extras/installer/linux/linux-qt.conf" "$PIA_FILES/bin/qt.conf"

    # Copy the version file as package.txt so the installer can use it
    # Also copy all the install files
    cp "$BUILD_PACKAGE/$CONFIG/version/version.txt" "$PKGROOT/package.txt"
    cp -r "$ROOT/extras/installer/linux/installfiles/" "$PKGROOT/installfiles/"
    # Apply the brand code to the uninstall script
    [ "$BRAND_CODE" = "pia" ] || mv "$PKGROOT/installfiles/pia-uninstall.sh" "$PKGROOT/installfiles/$BRAND_CODE-uninstall.sh"
    cp "$ROOT/brands/$BRAND_CODE/icons/app.png" "$PKGROOT/installfiles/app.png"

    # Use a different separator for brand link, because we'd expect the slashes of the URL to interfere with the sed command
    BRAND_SUBSTITUTION="s/{{BRAND_CODE}}/${BRAND_CODE}/g; s/{{BRAND_NAME}}/${BRAND_NAME}/g; s/{{BRAND_ID}}/${BRAND_ID}/g; s#{{BRAND_HELPDESK_LINK}}#${BRAND_HELPDESK_LINK}#g"

    # Recursively find all files in PKGROOT and perform `sed` to do an in-place replace. This touches
    # various files in `installfiles`.
    #
    # It's important to do this before copying all the PIA_FILES
    find "$PKGROOT/installfiles/" -type f -exec sed -i "$BRAND_SUBSTITUTION" {} \;

    # Perform the substitution on other files
    sed -i "$BRAND_SUBSTITUTION" "$PKGROOT/install.sh"
    sed -i "$BRAND_SUBSTITUTION" "$PIA_FILES/bin/qt.conf"

    cp -r "$STAGE_ROOT/"* "$PIA_FILES"

    cd "$BUILD_PACKAGE"
    # Don't embed a timestamp when gzipping
    export GZIP=-n
    "$ROOT/extras/installer/linux/makeself/makeself.sh" --tar-quietly --keep-umask --tar-extra "--mtime=@${BUILDTIMESTAMP} --sort=name --owner=0 --group=0 --numeric-owner" --packaging-date "$(date -d @"${BUILDTIMESTAMP}")" "$PACKAGENAME" "$PACKAGENAME.run" "$PRODUCTNAME" ./install.sh
    mv "$PACKAGENAME.run" "$ARTIFACT_PATH/$PACKAGENAME.run"
    echoPass "Built installer at $ARTIFACT_PATH/$PACKAGENAME.run"

    # Preserve unit test code coverage artifacts
    if [ -d "$BUILD_PACKAGE/$CONFIG/llvm-code-coverage" ]; then
        cp -r "$BUILD_PACKAGE/$CONFIG/llvm-code-coverage" "$ARTIFACT_PATH"
    fi

    # Copy pia-integtest as an artifact
    cp "$BUILD_PACKAGE/$CONFIG"/integtest-dist.*/"$BRAND_CODE-integtest.zip" "$ARTIFACT_PATH/$BRAND_CODE-integtest-$PACKAGENAME.zip"

    if [ -z "$PIA_BRANCH_BUILD" ] || [ "$PIA_BRANCH_BUILD" == "master" ]; then
      PLATFORM="linux" BRAND="$BRAND" "$ROOT/scripts/gendebug.sh"
    fi
  done
fi
