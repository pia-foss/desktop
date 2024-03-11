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
cd "$TOOLDIR"

# Checks en_US translations to see if any differ from the en_US name in the regions list.

source ./util/region_names.sh

init_check "$@" || exit 1

load_servers

IFS=$'\n'

function check_region_gps() {
  region="$1"
  if [ "$(jq ".$region" <./gps.json )" = "null" ]; then
    echo "missing GPS: $region"
  fi
}

for r in $(get_region_fields id); do
  check_region_gps "$r"
done
