#!/bin/bash

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

cd "$(dirname "${BASH_SOURCE[0]}")"

case "$(uname)" in
  Darwin)
    INKSCAPE="/Applications/Inkscape.app/Contents/Resources/bin/inkscape"
    ;;
  Linux)
    INKSCAPE="$(which inkscape)"
    ;;
esac

if ! [[ "$("$INKSCAPE" --version 2>/dev/null)" =~ Inkscape\ 1\. ]]; then
    echo "This script requires inkscape 1.0+.  You have:" >&2
    "$INKSCAPE" --version >&2 2>/dev/null
    exit 1
fi

# Generate dark icons from light icons
rm -rf dark_gen
mkdir -p dark_gen
for icon in light/*; do
  dark_gen_svg="dark_gen/$(basename "$icon")"
  sed -e "s|fill=\"#000000\"|fill=\"#ffffff\"|" -e "s|fill:#000000\([;\"]\)|fill:#ffffff\1|" "$icon" > "$dark_gen_svg"
done

function render_icons() {
  SVG_DIR="$1"
  PNG_DIR="$2"
  rm -rf "$PNG_DIR"
  mkdir -p "$PNG_DIR"
  for img in "$SVG_DIR"/*.svg; do
    img_name="$(basename "$img" .svg)"
    echo "Rendering: $img_name"
    "$INKSCAPE" --export-filename="$(pwd)/$PNG_DIR/$img_name.png" -h "24" "$(pwd)/$img"
  done
}

echo "Rendering light theme icons"
render_icons "light" "light_out"
echo "Rendering dark theme icons"
render_icons "dark_gen" "dark_out"
