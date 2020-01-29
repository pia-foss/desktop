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

for theme in $(ls); do
    if [ -d $theme ]; then
    	
        for file in $(ls $theme/*.svg); do
            convert -density 384 -background transparent $file -define icon:auto-resize -colors 256 `basename $file .svg`.ico
        done
    fi
done
