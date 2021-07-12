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

function show_help() {
    echo "usage:"
    echo "  $0 <.../PIA Region and Country Names.zip>"
    echo "  $0 --help"
    echo ""
    echo "Imports the translation ZIP export from OneSky."
    echo "(Extracts the ZIP and rewrites JSON with jq to transform '\u0000'"
    echo "escapes to UTF-8.)"
    echo ""
    echo "jq and unzip are required for this script."
}

if [ "$1" == "--help" ]; then
    show_help
    exit 0
fi

if [ "$#" -ne 1 ]; then
    show_help
    exit 1
fi

function lacks_cmd() {
    if command -v "$1" >/dev/null; then
        return 1 # Doesn't lack the command
    fi
    return 0 # Lacks the command
}

if lacks_cmd jq || lacks_cmd unzip; then
    echo "jq and unzip are required; install these from your package manager" >&2
    exit 1
fi

TOOLDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Get the absolute path to the ZIP since we have to change directory
TRZIP="$(realpath "$1")"

cd "$TOOLDIR/translations"
unzip -o "$TRZIP"
# Rename zh-CN and zh-TW to zh-Hans and zh-Hant
mv zh-CN.json zh-Hans.json
mv zh-TW.json zh-Hant.json

# Rewrite each file with 'jq' to transfor '\u0000' into actual UTF-8
for f in *; do
    echo "Rewrite $f"
    mv "$f" tmp.json
    jq <tmp.json >"$f"
    rm tmp.json
done
