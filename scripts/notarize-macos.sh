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

# Environment variables:
# PIA_APPLE_ID_EMAIL - (required) Email address of Apple ID to notarize with
# PIA_APPLE_ID_PASSWORD - (required) Password to that Apple ID
# PIA_APPLE_NOTARIZE_TIMEOUT - (optional) Override the time to wait for the
#                              package to be approved (time in seconds, default
#                              600 = 10 minutes)

# Zip file for upload to apple
RELEASE_ZIP=$1

# Primary identifier of the bundle
BUNDLE_ID=$2

# Path to the installer app bundle
APP_BUNDLE=$3

echo "Performing notarization."
echo "Release zip: $RELEASE_ZIP"
echo "Bundle ID: $RELEASE_ZIP"
echo "App Bundle: $APP_BUNDLE"
echo "Team ID: $PIA_APPLE_TEAM_ID"
# Outputs are written to stderr
# Upload the tool for notarization
# (Tee through /dev/stderr so the output is logged in case we exit here due to set -e)
notarizeOutput=$( (xcrun notarytool submit --team-id "$PIA_APPLE_TEAM_ID" --apple-id "$PIA_APPLE_ID_EMAIL" --password "$PIA_APPLE_ID_PASSWORD" --wait --timeout "$PIA_APPLE_NOTARIZE_TIMEOUT" --progress "$RELEASE_ZIP") 2>&1 | tee /dev/stderr)
notarizeExit=$?
echo "$notarizeOutput"

if [ $notarizeExit -ne 0 ]
then
    echo "Notarization failed, will exit without stapling"
    exit 3
fi

# Assuming that the package has been approved by this point
stapleOutput=$( (xcrun stapler staple "$APP_BUNDLE") 2>&1)

if [[ $stapleOutput == *"action worked"* ]];
then
    echo "Staple action worked!"
else
    echo "Staple action failed"
    echo "$stapleOutput"
    exit 3
fi
