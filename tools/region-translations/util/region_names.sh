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

function init_check() {
  if [ "$BASH_VERSINFO" -lt 4 ]; then
    echo "Bash 4 is required" >&2
    exit 1
  fi

  if ! hash jq; then
    echo "jq is required" >&2
    echo "Please install the jq package from your package manager" >&2
    exit 1
  fi

  if [ "$1" = '--x' ]; then
    set -x
  fi
}

function load_servers() {
  mkdir -p .tmp
  curl https://serverlist.piaservers.net/vpninfo/servers/v6 | head -1 | jq . >.tmp/servers.json
}

function load_regions() {
  mkdir -p .tmp
  curl https://serverlist.piaservers.net/vpninfo/regions/v2 | head -1 | jq . >.tmp/regions.json
}

function get_translation() {
  # source is already a JSON value obtained from either servers.json or regions.json
  # (it's already quoted)
  source="$1"
  lang="$2"
  # Adding an empty string at the end (+ \"\") coerces 'null' to the empty string.
  jq ".[$source] + \"\"" <"./translations/$lang.json"
}

function check_missing_name() {
  name="$1"
  # Swedish is checked arbitrarily, we translate everything at once anyway
  translation="$(get_translation "$name" sv)"
  if [ "$translation" = '""' ]; then
    echo "missing: $name" >&2
  fi
}

function get_region_fields() {
  FIELD="$1"
  IFS=$'\n'
  for r in $(jq '.regions | .[].'"$FIELD" <.tmp/servers.json); do
    echo "$r"
  done
}

function get_region_names() {
  get_region_fields name
}

function get_country_names() {
  # Find only the country codes that have more than one region (we only need country
  # names for these)
  used_countries=() # Countries that need translations (more than one region)
  # Seen countries - state of each country we've encountered (nothing, "seen", or
  # "used")
  declare -A seen_countries
  function seen_country_state() {
    echo "${seen_countries[${1,,}]}"
  }
  function set_seen_country() {
    seen_countries[${1,,}]="$2"
  }

  for country in $(jq -r '.regions | .[].country' <.tmp/servers.json); do
    case "$(seen_country_state "$country")" in
      seen)
        used_countries+=("$country")
        set_seen_country "$country" used
        ;;
      used)
        ;;
      *)
        set_seen_country "$country" seen
        ;;
    esac
  done

  for country in "${used_countries[@]}"; do
    name="$(jq ".${country,,}" <./country_groups.json)"
    if [ "$name" == "null" ] || [ -z "$name" ]; then
      echo "missing group: ${country,,}" >&2
    else
      echo "$name"
    fi
  done
}
