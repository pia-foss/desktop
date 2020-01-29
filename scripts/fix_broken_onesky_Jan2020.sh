#! /usr/bin/env bash

# Copyright (c) 2020 Private Internet Access, Inc.
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

# OneSky's export to Qt .ts files is currently broken (as of Jan 17, 2020).
# This script fixes up the broken comment and extracomment tags it is producing.

SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPTS_DIR/.." && pwd)"

for ts in "$REPO_DIR/client/ts"/*.ts; do
    echo "$ts"
    sed -e "s|<extracomment><comment>|<extracomment>|" -e "s|<comment><comment>|<comment>|" -e "s|</comment></comment>|</comment>|" -i "" "$ts"
done
