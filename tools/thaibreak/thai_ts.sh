#! /usr/bin/env bash

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

function show_help() {
    echo "usage:"
    echo "  $0 <.../th.ts>"
    echo "  $0 --help"
    echo ""
    echo "Processes th.ts (Thai translation file) by adding word breaks to each Thai"
    echo "translation, using the thaibreak utility".
}

if [ "$1" == "--help" ]; then
    show_help
    exit 0
fi

if [ "$#" -ne 1 ]; then
    show_help
    exit 1
fi

THAI_TS="$1"

TOOLSBIN="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

THAI_TS_TMP="$(mktemp)"
"$TOOLSBIN"/thaibreak --regex ' *<translation>(.*)</translation>' <"$THAI_TS" >"$THAI_TS_TMP"
mv "$THAI_TS_TMP" "$THAI_TS"
