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

MODE=release
BRAND=${BRAND:-pia}

# To sign the output, set PIA_CODESIGN_CERT to the signing identity (see
# codesign -s).
#
# To notarize the output, set the Apple ID credentials in PIA_APPLE_ID_EMAIL /
# PIA_APPLE_ID_PASSWORD.  Notarization is suppressed for feature branch builds
# (PIA_BRANCH_BUILD not empty or 'master'), but it can be forced with
# PIA_ALWAYS_NOTARIZE=1.

ROOT=${ROOT:-"$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"}

pushd "$ROOT"
trap popd EXIT

# Build
export RUBYOPT=-Eutf-8
rake clean VARIANT="$MODE" BRAND="$BRAND" ARCHITECTURE=universal
rake all VARIANT="$MODE" BRAND="$BRAND" ARCHITECTURE=universal

if [ -n "$CI_MERGE_REQUEST_ID" ]; then
    rake tsdiff
fi
