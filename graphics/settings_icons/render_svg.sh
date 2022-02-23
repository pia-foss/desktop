#! /usr/bin/env bash

# Copyright (c) 2022 Private Internet Access, Inc.
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

SIZE="$1"

rm -rf "png_$SIZE" svg_processed
mkdir -p "png_$SIZE"
mkdir -p svg_processed
mkdir svg_processed/light
mkdir svg_processed/dark
mkdir svg_processed/common

cd "$(dirname "${BASH_SOURCE[0]}")"

# Process the original "dark active" SVG, this is the only SVG currently used to
# render icons, all other shades are generated from this one.
# Remove the background circles, these are rendered in-app.
for icon in icon-dark-active-*.svg; do
    sed 's|<circle.*r="50".*></circle>||' "$icon" > "./svg_processed/dark/$icon"
done

# Call with:
# - $1: color for new shade (6-digit hex, like 889099)
# - $2: name of the new shade ('tabinactive', etc.)
# - $3: theme for the new shade ('light'/'dark'/'common')
#
# Call from the original working directory
function generate_shade {
    NEWCOLOR="$1"
    NEWNAME="$2"
    THEME="$3"
    # Custom shades are always generated from dark-active, doesn't really matter
    pushd ./svg_processed/dark >/dev/null
    for icon in icon-dark-active-*.svg; do
        icon_name="$(echo "$icon" | sed -n 's/^icon-[a-z]*-[a-z]*-\(.*\)\.svg/\1/p')"
        sed -e "s|fill=\"#FFFFFF\"|fill=\"#$NEWCOLOR\"|" -e "s|fill:#FFFFFF\([;\"]\)|fill:#$NEWCOLOR\1|" "$icon" > "../$THEME/icon-$THEME-$NEWNAME-$icon_name.svg"
    done
    popd
}


# FFFFFF - selected settings tab and selected quick settings button, both themes
generate_shade FFFFFF color common
# 889099 - inactive settings tab, dark theme
generate_shade 889099 tabinactive dark
# EEEEEE - inactive settings tab, light theme
generate_shade 838389 tabinactive light
# "active dark" is generated above from the original source SVG
# 5B6370 - active quick settings button, light theme
generate_shade 5B6370 active light
# 323642 - inactive quick settings button, dark theme
generate_shade 323642 inactive dark
# 889099 - inactive quick settings button, light theme
generate_shade 889099 inactive light
# 5B6370 - disabled (not currently used), dark theme
generate_shade 5B6370 disabled dark
# D7D8D9 - disabled (not currently used), light theme
generate_shade D7D8D9 disabled light

pushd svg_processed >/dev/null
for type in *; do
    pushd "$type" >/dev/null
    mkdir -p "../../png_$SIZE/$type"
    for img in *.svg; do
        echo "$img"
	img_name="$(echo "$img" | sed -n 's/^icon-[a-z]*-[a-z]*-\(.*\)\.svg/\1/p')"
        img_type="$(echo "$img" | sed -n 's/^icon-[a-z]*-\([a-z]*\)-.*\.svg/\1/p')"
        # Inkscape really wants absolute paths here, it probably changes its
        # working directory or something
        /Applications/Inkscape.app/Contents/Resources/bin/inkscape -z -e "$(pwd)/../../png_$SIZE/$type/$img_name-$img_type.png" -w "$SIZE" -h "$SIZE" "$(pwd)/$img"
    done
    popd
done
popd
