#! /usr/bin/env bash

# Copyright (c) 2022 Private Internet Access, Inc.
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
SCRIPTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# If the user passed --help, skip sudo and pass straight through to the
# inner script to show help
for p in "$@"; do
    if [ "$p" = "--help" ]; then
        "$SCRIPTDIR/setup-as-root.sh" "$@"
        exit 0
    fi
done

echo "Will include user $USER in build chroot user list"

sudo "$SCRIPTDIR/setup-as-root.sh" --user "$USER" "$@"
