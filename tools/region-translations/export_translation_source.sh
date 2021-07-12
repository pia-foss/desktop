#! /usr/bin/env bash

# Copyright (c) 2021 Private Internet Access, Inc.
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

source ./util/region_names.sh

init_check "$@" || exit 1

load_servers

IFS=$'\n'

first=1

function print_translation_source() {
  key="$1"
  # A few strings already have en-US translations that differ from the regions list name.
  # Use those as the en-US sources in that case, otherwise use the key by default
  en="$(get_translation "$key" "en-US")"
  if [ "$en" == '""' ]; then
    en="$key"
  fi
  # Print the prior line's comma and line ending if this isn't the first line
  if [ -z "$first" ]; then
    echo ","
  else
    first=
  fi
  echo -n "  $key: $en"
}

mkdir -p out/onesky
(
  echo "{"

  for r in $(get_region_names); do
    print_translation_source "$r"
  done

  for r in $(get_country_names); do
    print_translation_source "$r"
  done

  echo ""	# Last line's line ending (no comma)
  echo "}"
) >out/onesky/regions.json

echo "Generated out/onesky/regions.json"
