#! /bin/bash

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

# Relaunch the app from the installed location after installation.
# Params:
# - PID - If given, kills this PID from a disowned subshell, and waits for it to
#   terminate before relaunching
# - ORIGAPP - Original app bundle path (typically in Downloads or the client
#   update directory) - if given, removes this path after relaunching (to avoid
#   extra copies of the app)
PID="$1"
ORIGAPP="$2"

appDir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# If a PID was passed, kill our parent and relaunch
if [ -n "$PID" ] && [[ $PID =~ ^[0-9]+$ ]] ; then
    echo "Relaunching client"

    # Launch a disowned subshell
    (
        # Wait for parent process to die
        while kill -0 "$PID" 2> /dev/null ; do sleep 0.2 ; done
        # Launch new app as the logged in user (not root)
        open "$appDir"
        if [ -n "$ORIGAPP" ]; then
            rm -rf "$ORIGAPP" || true
        fi
    ) & disown
else
    # Just launch directly
    open "$appDir"
    if [ -n "$ORIGAPP" ]; then
        rm -rf "$ORIGAPP" || true
    fi
fi
