# Region translation scripts

These scripts address translations for region and country names in the PIA regions list:

1. Region and country names are exported from the PIA regions list for import into OneSky
2. OneSky translations are imported and processed into the region translations JSON
3. Additional scripts check for missing GPS coordinates and can preview the regions.json difference

## 1. Exporting region/country names

TL;DR:

```
./export_translation_source.sh
# Output produced in ./out/onesky/regions.json
```

This script fetches the PIA regions list and exports the following:

1. en-US names of all regions
2. Names of country groups that apply to two or more regions

If a country group applies to more than one region, but doesn't have a name in `country_groups.json`, the script prints an error.  (Add the country's en-US name to `country_groups.json` and run again.)

Then import `out/onesky/regions.json` into OneSky and order new translations.

## 2. Importing translations from OneSky

TL;DR:

```
./import_onesky.sh <.../path/to/PIA Region and Country Names.zip>
# Check if any new GPS coordinates are needed:
./check_gps.sh
./build_regions_json.rb
# Output produced in ./out/regions.json
# Recommended, preview difference against prod:
./preview_regions_diff.sh
# Both JSONs are sorted with jq to produce a reasonable diff
```

Use `./import_onesky.sh` to import the ZIP downloaded from OneSky.  This extracts the ZIP and re-processes the JSON with jq to convert '\u1234' into UTF-8 text (mainly for legibility).  This updates the JSON files in translations/; check those in.

Use `./check_gps.sh` to check if any regions are missing GPS coordinates.  If they are, add them to `gps.json` and re-check.

Then use `./build_regions_json.rb` to build a new region translation data JSON blob.  This includes translations, country group names, and GPS coordinates.  This should replace `https://serverlist.piaservers.net/vpninfo/regions`.

You can preview the difference wtih `./preview_regions_diff.sh`.  This creates `.tmp/regions-current.json` and `.tmp/regions-new.json`; then shows the difference with `diff -u`.

## All scripts

| Script | Inputs | Purpose |
|========|========|=========|
| build_regions_json.rb | Translation data, `gps.json`, `country_groups.json` | Builds the final regions JSON blob to post |
| check_en_us.sh | Translation data | Checks if any en-US translations differ from the regions' names in the servers list (a few do) |
| check_gps.sh | Prod servers list, `gps.json` | Checks if any regions are missing GPS coordinates |
| export_translation_source.sh | Prod servers list, `country_groups.json`, `translations/en-US.json` | Builds the en-US translation data for import into OneSky |
| import_onesky.sh | OneSky export ZIP | Imports translations downloaded from OneSky |
| preview_regions_diff.sh | Prod region data, built region data | Displays the diff between the built region data and production |
| show_untranslated_regions.sh | Prod servers list, `country_groups.json`, `translations/sv.json` | Shows region or country names that lack translations (just checks `sv`) |


