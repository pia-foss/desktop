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

# Environment variables:
# PIA_APPLE_ID_EMAIL - (required) Email address of Apple ID to notarize with
# PIA_APPLE_ID_PASSWORD - (required) Password to that Apple ID
# PIA_APPLE_ID_PROVIDER - (optional) If the Apple ID is a member of multiple
#                         teams, specify the provider ID to use (see
#                         `xcrun altool --help`; `--asc-provider` option)
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

NOTARIZE_EXTRA_ARGS=()
if [ -n "$PIA_APPLE_ID_PROVIDER" ]; then
    NOTARIZE_EXTRA_ARGS+=("--asc-provider=$PIA_APPLE_ID_PROVIDER")
fi

# Outputs are written to stderr
# Upload the tool for notarization
# (Tee through /dev/stderr so the output is logged in case we exit here due to set -e)
notarizeOutput=$( (xcrun altool --notarize-app -t osx -f "$RELEASE_ZIP" --primary-bundle-id="$BUNDLE_ID" -u "$PIA_APPLE_ID_EMAIL" -p "$PIA_APPLE_ID_PASSWORD" "${NOTARIZE_EXTRA_ARGS[@]}") 2>&1 | tee /dev/stderr)

# notarizeOutput="2019-07-09 10:20:48.926 altool[70312:1478958] No errors uploading './out/artifacts/pia-macos-1.3-mac-notarization-20190701210616-b2471ea6.zip'.
# RequestUUID = 6e6265a3-03e8-42e6-9ff6-146780ae8952
# "

if [[ $notarizeOutput == *"No errors uploading"* ]];
then
    REQUEST_ID=$(echo "$notarizeOutput"| grep RequestUUID |awk '{print $NF}')
    echo "Request ID is: $REQUEST_ID"
else
    echo "Notarization upload failed."
    echo "$notarizeOutput"
    exit 3
fi

# Wait 20 seconds otherwise we may get the error "Could not find the RequestUUID"
echo "Waiting 20 seconds"
sleep 20

# Wait up to 10 minutes (or PIA_APPLE_NOTARIZE_TIMEOUT)
deadline=$(($(date "+%s") + ${PIA_APPLE_NOTARIZE_TIMEOUT:-600}))

while :
do
    notarizationStatus=$( (xcrun altool --notarization-info "$REQUEST_ID" -u "$PIA_APPLE_ID_EMAIL" -p "$PIA_APPLE_ID_PASSWORD") 2>&1 | tee /dev/stderr)

    if [[ $notarizationStatus == *"in progress"* ]];
    then
        now="$(date "+%s")"
        if [[ "$now" -ge "$deadline" ]];
        then
            echo "Timed out waiting for approval"
            echo "Last status:"
            echo "$notarizationStatus"
            exit 3
        fi
        # In progress
        echo "Waiting for notarization"
    elif [[ $notarizationStatus == *"Package Approved"* ]];
    then
        echo "Package has been approved"
        break;
    elif [[ $notarizationStatus == *"invalid"* ]];
    then
        echo "Notarization invalid"
        echo "$notarizationStatus"
        exit 3
    else
        echo "Unknown notarization status"
        echo "$notarizationStatus"
    fi

    sleep 5
done

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
