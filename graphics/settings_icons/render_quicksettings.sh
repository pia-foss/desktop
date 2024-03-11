#! /bin/bash

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

# size should be a multiple of 4 for proper scaling and centering
size=72

./render_svg.sh $size || exit 1

for type in debug mace notifications port-forwarding theme lan; do
    cp ./png_$size/common/$type-color.png ../../client/res/img/common/dashboard/connect/settings/$type-color.png
    cp ./png_$size/dark/$type-active.png ../../client/res/img/dark/dashboard/connect/settings/$type-active.png
    cp ./png_$size/dark/$type-inactive.png ../../client/res/img/dark/dashboard/connect/settings/$type-inactive.png
    cp ./png_$size/light/$type-active.png ../../client/res/img/light/dashboard/connect/settings/$type-active.png
    cp ./png_$size/light/$type-inactive.png ../../client/res/img/light/dashboard/connect/settings/$type-inactive.png
done
