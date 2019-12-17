#! /usr/bin/env bash

# Copyright (c) 2019 London Trust Media Incorporated
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

if [ $# -lt 1 ]; then
    echo "usage: $0 <img-size>"
    echo "run inside icons-updated directory (with all the original SVG files)"
    echo "requires Inkscape to be installed, path is hard-coded, will only work on Mac right now"
    echo ""
    echo "generates:"
    echo " - svg_processed/{common,dark,light} - processed SVG files"
    echo " - png_##/{common,dark,light} - rendered PNG assets"
    echo ""
    echo "Changes made to SVG files:"
    echo " - background circles are removed"
    exit 1
fi

rm -rf png_$1 svg_processed
mkdir -p png_$1
mkdir -p svg_processed
mkdir svg_processed/light
mkdir svg_processed/dark
mkdir svg_processed/common

# Light/dark variations - remove the background circles, render these in the app
# This includes removing the 'disabled' background circles; the stroke on these
# is too small to see at ~60 px anyway
for type in dark light; do
    for state in disabled inactive active; do
        for icon in icon-$type-$state-*.svg; do
            sed 's|<circle.*r="50"></circle>||' $icon > ./svg_processed/$type/$icon
        done
    done
done

# 'color' is the same for light and dark
for icon in icon-light-color-*.svg; do
    sed 's|<circle.*r="50"></circle>||' $icon > ./svg_processed/common/$icon
done

# Call with:
# - $1: color for new shade (6-digit hex, like 889099)
# - $2: name of the new shade ('tabinactive', etc.)
# - $3: theme for the new shade ('light'/'dark'/'common')
#
# Call from the original working directory
function generate_shade {
    # Custom shades are always generated from dark-active, doesn't really matter
    cd ./svg_processed/dark
    for icon in icon-dark-active-*.svg; do
        icon_name=`echo $icon | sed -n 's/^icon-[a-z]*-[a-z]*-\(.*\)\.svg/\1/p'`
        sed "s|fill=\"#323642\"|fill=\"#$1\"|" $icon > ../$3/icon-$3-$2-$icon_name.svg
    done
    cd ../..
}

# Generate a #889099 version for the inactive dark-theme settings tabs
generate_shade 889099 tabinactive dark

# Generate a #eeeeee version for the inactive light-theme settings tabs
generate_shade EEEEEE tabinactive light

cd svg_processed
for type in *; do
    cd $type
    mkdir -p ../../png_$1/$type
    for img in *.svg; do
        echo $img
	img_name=`echo $img | sed -n 's/^icon-[a-z]*-[a-z]*-\(.*\)\.svg/\1/p'`
        img_type=`echo $img | sed -n 's/^icon-[a-z]*-\([a-z]*\)-.*\.svg/\1/p'`
        # Inkscape really wants absolute paths here, it probably changes its
        # working directory or something
        /Applications/Inkscape.app/Contents/Resources/bin/inkscape -z -e `pwd`/../../png_$1/$type/$img_name-$img_type.png -w $1 -h $1 `pwd`/$img
    done
    cd ..
done
