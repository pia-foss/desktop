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

function show_help() {
    echo "usage:"
    echo "  $0 <.../PIA Desktop.zip>"
    echo "  $0 --help"
    echo ""
    echo "Imports the translation ZIP export from Translation Platforms."
    echo "(Extracts the ZIP, renames the ts files, and runs thaibreak)"
    echo ""
    echo "unzip is required for this script."
}

if [ "$1" == "--help" ]; then
    show_help
    exit 0
fi

if [ "$#" -ne 1 ]; then
    show_help
    exit 1
fi

TOOLSBIN="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$TOOLSBIN/../../../.." && pwd)"

if ! [ -e "$TOOLSBIN"/thai_ts.sh ] || ! [ -e "$TOOLSBIN/thaibreak" ]; then
    echo "Didn't find thai_ts.sh or thaibreak" >&2
    echo "" >&2
    echo " - Run this on Linux (thaibreak is Linux-only)" >&2
    echo " - Ensure libthai-dev is installed" >&2
    echo " - Run 'rake tools' to build thaibreak and copy scripts to tools/bin/" >&2
    echo " - Run this from ./out/pia_debug_x86_64/tools/bin/, not the tools source directory" >&2
    exit 1
fi

function lacks_cmd() {
    if command -v "$1" >/dev/null; then
        return 1 # Doesn't lack the command
    fi
    return 0 # Lacks the command
}

if lacks_cmd unzip; then
    echo "unzip is required; install from your package manager" >&2
    exit 1
fi

# Get the absolute path to the ZIP since we have to change directory
TRZIP="$(realpath "$1")"

cd "$REPO/client/ts/"
unzip -o "$TRZIP"
# Rename all files
mv ar/ar.ts ar.ts
mv da/da.ts da.ts
mv de/de.ts de.ts
mv es-MX/es_MX.ts es_MX.ts
mv fr/fr.ts fr.ts
mv it/it.ts it.ts
mv ja/ja.ts ja.ts
mv ko/ko.ts ko.ts
mv nb/nb.ts nb.ts
mv nl/nl.ts nl.ts
mv pl/pl.ts pl.ts
mv pt-BR/pt_BR.ts pt_BR.ts
mv ru/ru.ts ru.ts
mv th/th.ts th.ts
mv tr/tr.ts tr.ts
mv zh-CN/zh_Hans.ts zh_Hans.ts
mv zh-TW/zh_Hant.ts zh_Hant.ts

# Empty directories are left behind for each of the languages. Delete them.
rm -Rf -- */

"$TOOLSBIN/thai_ts.sh" th.ts
