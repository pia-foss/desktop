#! /bin/bash

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

# size should be a multiple of 4 for proper scaling and centering
size=68

./render_svg.sh $size || exit 1

# $1: source type name
# $2: dest type name
# (usually the same, can be different for using a substitute icon as a
# placeholder)
function copy_tab_icon
{
    cp ./png_$size/common/$1-color.png ../../client/res/img/common/settings/$2-active.png
    cp ./png_$size/dark/$1-tabinactive.png ../../client/res/img/dark/settings/$2-inactive.png
    cp ./png_$size/light/$1-tabinactive.png ../../client/res/img/light/settings/$2-inactive.png
}

for type in connection general help privacy proxy dedicatedip network automation; do
    copy_tab_icon $type $type
done
