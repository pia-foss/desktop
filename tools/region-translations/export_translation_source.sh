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

TOOLDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$TOOLDIR"

source ./util/region_names.sh

init_check "$@" || exit 1

load_servers

IFS=$'\n'

function print_translation_source() {
  key="$1"
  # A few strings already have en-US translations that differ from the regions list name.
  # Use those as the en-US sources in that case, otherwise use the key by default
  en="$(get_translation "$key" "en-US")"
  if [ "$en" == '""' ]; then
    en="$key"
  fi
  echo "  $key: $en,"
}

mkdir -p out/export
(
  echo "{"

  (
    for r in $(get_region_names); do
      print_translation_source "$r"
    done

    for r in $(get_country_names); do
      print_translation_source "$r"
    done
  ) | sort | sed '$s/,$//'

  echo "}"
) >out/export/en-US.json

mv out/export/en-US.json translations/en-US.json

echo "Generated translations/en-US.json"
