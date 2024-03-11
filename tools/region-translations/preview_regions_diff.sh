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

TOOLDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if ! [ -f "$TOOLDIR/out/regions.json" ]; then
    echo "Generate regions.json with build_regions_json.rb before previewing the diff" >&2
    exit 1
fi

cd "$TOOLDIR"
source ./util/region_names.sh
load_regions

jq --sort-keys . <"out/regions.json" >".tmp/regions-new.json"
jq --sort-keys . <".tmp/regions.json" >".tmp/regions-current.json"

diff -u ".tmp/regions-current.json" ".tmp/regions-new.json"
