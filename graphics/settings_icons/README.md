# Settings icons - used for Settings window tabs and Quick Settings buttons

## General notes

Render to a particular size using `./render_svg <size>` in this directory.

`<size>` is the size of the image bounds (including the circles, although these
are removed from the image output and rendered in the app instead).

Only works on Mac at the moment due to the hard-coded path to Inkscape, and
requires Inkscape to be installed.

## Settings tabs

Render with `./render_settings_tabs.sh` - renders at 72px and copies into app
resources.

## Quick settings buttons

Render with `./render_quicksettings.sh` - renders at 76px and copies into app
resources.
