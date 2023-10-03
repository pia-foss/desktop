#! /usr/bin/env bash

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
cd "$(dirname "${BASH_SOURCE[0]}")/.."

function show_usage() {
  echo "usage:"
  echo "  $0 [-t] <target_architecture>"
  echo
  echo "Prepares the android dependency repository artifact by setting up the required namespace"
  echo "and creating the zip containing the specified target architectures shareable objects."
  echo ""
  echo "Parameters:"
  echo "  -t: <target_architecture> Prepare the dependency with the pre-build target architecture."
  echo ""
}

# Build combined dependency for Android after having built all supported architectures.  
# This is an exceptional case because it involves combining builds for multiple 
# platforms; the Rake build system is designed to target a single platform at a time.  
# (Don't proliferate more postprocessing steps in build.sh except in this 
# exceptional case, keep everything in rake.)
#
# == CI Variables ==
#
# This script uses the following CI variables:
# * Standard:
#   * CI_COMMIT_SHORT_SHA
#

readonly ARTIFACT_VERSION=  # Set only if we will make an official release.
readonly ARTIFACT_NAME="kapps"
readonly ARTIFACT_PACKAGE="pia.appcomponents.mobile"
readonly PACKAGE_DIRECTORY=$(echo "$ARTIFACT_PACKAGE" | sed 's/\./\//g')

while getopts "t:" opt; do
  case $opt in
    t)
      TARGETS+=("$OPTARG")
      ;;
    :)
      echo "Error: -${OPTARG} requires an argument."
      show_usage
      exit 1
      ;;
    *)
      show_usage
      exit 1
      ;;
  esac
done

if [ -z "${TARGETS}" ]; then
  show_usage
  exit 1
fi

# By default use the short HEAD value as version.
version="$CI_COMMIT_SHORT_SHA"

# If we are making an official release. Replace it with that version instead.
if [ -n "$ARTIFACT_VERSION" ]; then
  version=$ARTIFACT_VERSION
fi

# Prepare the directory structure expected by the dependency repository.
# e.g. pia/appcomponents/mobile/kapps/1.0.0/
#
# The version directory must have the ivy.xml along with the named zip
# in the following format "ARTIFACT_NAME-version" containing the target architectures. 
readonly TARGET_DIRECTORY=out/android-package/artifacts/kapps-libs/$PACKAGE_DIRECTORY/$ARTIFACT_NAME/$version

# Bootstrap directories.
rm -rf out/tmp
rm -rf out/android-package/artifacts
mkdir -p out/tmp/kapps
mkdir -p "$TARGET_DIRECTORY"

# Prepare the ivy.xml file for the target version.
echo "<ivy-module version=\"2.0\">
  <info organisation=\"$ARTIFACT_PACKAGE\" module=\"$ARTIFACT_NAME\" revision=\"$version\"></info>
</ivy-module>" >> "$TARGET_DIRECTORY"/ivy.xml

# Post-process the output to match android needs.
for target in "${TARGETS[@]}"; do
  for file in "out/pia_release_android_$target/kapps-libs/lib"/*; do
    filename=$(basename "${file}")

    # Android needs shareable objects with the prefix `lib`
    # in order to load them via `System.loadLibrary`.
    if [[ ! $filename =~ ^lib.* ]]; then
      file_path=$(dirname "${file}")
      mv "$file" "$file_path/lib$filename"
    fi
  done

  # Update the target architecture folder name to match android expectations.
  target_folder_name=$target
  if [[ $target == "armhf" ]]; then
    target_folder_name="armeabi-v7a"
  elif [[ $target == "arm64" ]]; then
    target_folder_name="arm64-v8a"
  fi

  # Move `kapps-libs` to its tmp location in preparation for the zip 
  # containing all architectures.
  cp -r "out/pia_release_android_$target/kapps-libs" "out/tmp/kapps/$target_folder_name"
done

function show_exec {
  echo "$@"
  "$@"
}

# Zip all architectures together and move it into the target repository directory.
pushd out/tmp
show_exec zip -9 -r "$ARTIFACT_NAME-$version.zip" "kapps"
popd
mv "out/tmp/$ARTIFACT_NAME-$version.zip" "$TARGET_DIRECTORY"

# Cleanup.
rm -rf out/tmp
