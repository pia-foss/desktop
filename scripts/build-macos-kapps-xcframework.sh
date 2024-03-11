#! /usr/bin/env bash

# Copyright (c) 2024 Private Internet Access, Inc.
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
  echo "  $0 [--skip-build] <brand>"
  echo "  $0 --help"
  echo
  echo "Generates a combined xcframework from the macOS, iOS, and iOS Simulator"
  echo "universal release builds.  Rake is invoked for each Apple platform to"
  echo "build each platform framework, then the results are merged with"
  echo "xcodebuild."
  echo ""
  echo "Parameters:"
  echo "  --skip-build: Don't invoke Rake, assume the products have been built"
  echo "  <brand>: The PIA brand to pull artifacts from - defaults to 'pia'."
  echo "    The framework builds aren't brand-specific, but all rake builds are"
  echo "    associated with a brand."
  echo ""
}

# Build combined xcframeworks for the kapps frameworks after having built
# macOS, iOS, iOS Simulator.  This is an exceptional case because it involves
# combining builds for multiple platforms; the Rake build system is designed to
# target a single platform at a time.  (Don't proliferate more postprocessing
# steps in build.sh except in this exceptional case, keep everything in rake.)

DO_BUILD=1

while [ "$#" -gt 0 ]; do
    case "$1" in
        --help)
            show_usage
            exit 0
            ;;
        --skip-build)
            DO_BUILD=0
            shift
            ;;
        --)
            shift
            break
            ;;
        --*)
            show_usage
            exit 1
            ;;
        *)
            break
            ;;
    esac
done

if [ "$#" -gt 1 ]; then
    show_usage
    exit 1
fi

BRAND="$1"
BRAND="${BRAND:-pia}"

if [ "$DO_BUILD" -ne 0 ]; then
    # Build the libs_stage target; on macos this means we don't build the
    # desktop app too.  For ios/iossim it's almost the same as artifacts/all but
    # skips zipping the libraries.
    for p in macos ios iossim; do
        rake PLATFORM="$p" VARIANT=release ARCHITECTURE=universal BRAND="$BRAND" frameworks
    done
fi

rm -rf out/xcframework
rm -rf out/swift-package
mkdir -p out/xcframework/kapps-frameworks
mkdir -p out/xcframework/artifacts
mkdir -p out/swift-package/artifacts

targets=(
    "${BRAND}_release_universal" # macOS
    "${BRAND}_release_ios_universal"
    "${BRAND}_release_iossim_universal"
)

first_target="${targets[0]}"

function show_exec {
  echo "$@"
  "$@"
}

for file in "out/$first_target/kapps-frameworks"/*; do
  if [[ $file =~ .*\.framework\.dSYM ]]; then
    # Ignore, we pick up dSYMs along with the frameworks
    true
  elif [[ $file =~ .*\.framework ]]; then
    target_args=()
    for t in "${targets[@]}"; do
      target_args+=("-framework")
      target_args+=("$(pwd)/out/$t/kapps-frameworks/$(basename "$file")")
      target_args+=("-debug-symbols")
      target_args+=("$(pwd)/out/$t/kapps-frameworks/$(basename "$file").dSYM")
    done

    show_exec xcodebuild -create-xcframework \
      "${target_args[@]}" \
      -output "out/xcframework/kapps-frameworks/$(basename "$file" .framework).xcframework"
  else
    # Not a framework - this applies to README.md, VERSION.txt, etc.  Just copy
    # the first one; these are the same as long as all of the targets were
    # really just built.
    show_exec cp "$file" "out/xcframework/kapps-frameworks"
  fi
done

# Zip up the combined xcframeworks
pushd "out/xcframework"
version="$(head -1 kapps-frameworks/version.txt)"
show_exec zip -y -9 -r "artifacts/kapps-frameworks-$version.zip" "kapps-frameworks"
popd

# Make a Swift package containing the xcframeworks, using the Swift package
# template.  The resulting package can be used as a local package dependency,
# but it's most useful if it's then committed to a Git repo, so it can be added
# as a remote dependency.

# When using a local build as an override in a downstream project, the folder
# name has to match the repo name - 'kapps-swift-package'.  Build with that name
# so the build output can be dragged into Xcode as an override.
cp -r kapps-swift-package/KApps out/swift-package/kapps-swift-package
cp -r out/xcframework/kapps-frameworks/* out/swift-package/kapps-swift-package/
pushd out/swift-package
show_exec zip -y -9 -r "artifacts/KApps-swift-package-$version.zip" "kapps-swift-package"
popd
