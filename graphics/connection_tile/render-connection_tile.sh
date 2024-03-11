#!/bin/bash

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

for img in *.svg; do
    echo "$img"
    icon_name=$(basename "$img" .svg)

    /Applications/Inkscape.app/Contents/Resources/bin/inkscape -z -e "$(pwd)/../../client/res/img/common/dashboard/connect/connection-tile/$icon_name.png" -w 32 -h 32 "$(pwd)/$img"
done
