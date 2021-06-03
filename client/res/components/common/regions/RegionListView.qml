// Copyright (c) 2021 Private Internet Access, Inc.
//
// This file is part of the Private Internet Access Desktop Client.
//
// The Private Internet Access Desktop Client is free software: you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// The Private Internet Access Desktop Client is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Private Internet Access Desktop Client.  If not, see
// <https://www.gnu.org/licenses/>.

import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import "../../common"
import "../../client"
import "../../daemon"
import "../../theme"
import "../../settings/stores"
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util
import PIA.NativeAcc 1.0 as NativeAcc

// RegionListView is used to select a region from the list of available regions.
// It's used to select the VPN region, as well as the Shadowsocks region.
//
// Several parts of the RegionListView can be customized for different contexts,
// such as:
// - filtering regions (display only regions that have a given service)
// - Favorite region buttons
// - Port forwarding hints
// - What the 'Auto' region displays
//
// Although this has quite a bit in common with TableBase, it is somewhat
// different since the country groups can be expanded/collapsed - this affects
// keyboard navigation, and there are differences in the accessibility
// annotations since the rows are annotated with outline levels.
Rectangle {
  id: regionListView

  // Customization properties

  // Specify a function used to filter regions.  The functor is passed a
  // ServerLocation and returns true/false to include or exclude a region.
  property var regionFilter

  // The service locations for this list view - includes best/chosen locations
  property var serviceLocations
  // Whether port forwarding is enabled for this service - causes non-PF regions
  // to show an indicator
  property bool portForwardEnabled
  // Whether regions in this list can be favorited (only applicable to VPN
  // regions currently; settings for this are fixed in RegionListView)
  property bool canFavorite
  // The client setting name used to store the collapsed country groups
  property string collapsedCountriesSettingName

  // A region has been selected
  signal regionSelected(string locationId)

  property string searchTerm: ""

  readonly property Setting sortKey: ClientSetting{name: "regionSortKey"}

  color: Theme.dashboard.backgroundColor

  // Keyboard navigation in the regions list acts like a table.  The user can
  // focus the whole list, then use the arrow keys to navigate the regions.
  //
  // The table has two "columns" - the region itself and the favorite icon.  The
  // "no-port-forwarding" arrow isn't reachable from keyboard navigation, it
  // provides virtually no additional information when pointed.
  activeFocusOnTab: true
  // The row/column currently highlighted with the keyboard.  Initially, 'auto'
  // is highlighted.
  //
  // The highlighted row is conceptually a <country, location> tuple, since both
  // countries and locations can be highlighted.  This is stored as an object
  // with those two properties.
  //
  // The tuple is used instead of a plain index so it behaves as expected when
  // regions are added/removed/re-sorted.  An index also does not map onto the
  // heirarchical regions list very well, we'd have to do much of the same work
  // to figure out what row an index refers to.
  //
  // The 'auto' row is treated as country='auto', location='auto'.
  //
  // Dedicated IP rows are treated as country='dip', location='<id>'.
  //
  // Any invalid row choice behaves as if no row is highlighted (including
  // cases like:
  // - country='bogus'
  // - country='us', location'bogus'
  // - country='ja', location='' (Japan is not a country group, would be 'ja'/'ja')
  property var keyboardRow: ({country: 'auto', location: 'auto'})
  property int keyboardColumn: 0
  // When the highlight cue is shown, this is keyboardColumn, otherwise it's -1.
  // We still preserve keyboardColumn even when the highlight cue is shown.
  property int highlightColumn: {
    // Show the keyboard highlight when the focus cue is shown
    return focusCue.show ? keyboardColumn : -1
  }

  // keyboardColumnCount indicates how many "columns" the region rows provide
  // for keyboard navigation.  (The screen reader columns are different because
  // they include non-interactive items.)
  // Some rows may not actually have this many cells (some rows can't be
  // favorited for example), but the actual column position is still preserved
  // so it behaves as expected if the user navigates up to a row that does have
  // that many columns.
  //
  // This really would make more sense in RegionRowBase since it's directly tied
  // to that implementation (RegionListView only cares about it as a limit), but
  // QML does not have any concept of a "static property".
  readonly property int keyboardColumnCount: canFavorite ? 2 : 1

  // Screen reader annotation for the region list
  property string regionListLabel

  ThemedScrollView {
    id: scrollView
    ScrollBar.vertical.policy: ScrollBar.AlwaysOn
    label: regionListView.regionListLabel
    anchors.fill: parent
    clip: true

    Flickable {
      id: regionListFlickable
      boundsBehavior: Flickable.StopAtBounds
      // don't allow horizontal scrolling.
      contentWidth: parent.width

      // The scroll view should keep its scroll position when regions are
      // updated.  This usually doesn't change the size of the view; the most
      // common change is just updating latencies.
      //
      // When the regions are rebuilt, the ColumnLayout calculates a new height
      // after each region is added by the repeater.  Normally this would cause
      // the scroll view to go back to the top, because it would briefly observe
      // a short height of ~68 or so when the first region is added.
      //
      // Instead, defer height updates for one frame to collect all these
      // updates.  This means that the scroll view only observes a height change
      // after all regions have been built, so it is able to keep its scroll
      // position.
      contentHeight: 1

        function applyViewHeight() {
          if(scrollViewColumn.implicitHeight !== regionListFlickable.contentHeight) {
            regionListFlickable.contentHeight = scrollViewColumn.implicitHeight
          }
        }

        Timer {
          id: heightDeferralTimer
          repeat: false
          interval: 0
          onTriggered: regionListFlickable.applyViewHeight()
          Component.onCompleted: regionListFlickable.applyViewHeight()
        }

        Connections {
          target: scrollViewColumn
          function onImplicitHeightChanged()  {
            heightDeferralTimer.start()
          }
        }

        ColumnLayout {
          id: scrollViewColumn
          width: parent.width
          spacing: 0
          RegionAuto {
            id: regionAuto
            // As in other cases, reading 'visible' isn't the same as reading
            // the value it's bound too; avoid spurious dependencies in the
            // separator's visible binding
            readonly property bool show: searchTerm === ""
            visible: show
            height: 65
            Layout.fillWidth: true
            highlightColumn: {
              if(keyboardRow.country === 'auto' && keyboardRow.location === 'auto') {
                return regionListView.highlightColumn
              }
              return -1
            }
            serviceLocations: regionListView.serviceLocations
            onClicked: regionListView.regionSelected('auto')
            onFocusCell: mouseFocusCell({country: 'auto', location: 'auto'}, keyColumn)
          }
          Repeater {
            id: dedicatedIpsRepeater
            model: displayDedicatedIpsArray
            delegate: DedicatedIpRegion {
              Layout.fillWidth: true
              region: modelData
              serviceLocations: regionListView.serviceLocations
              portForwardEnabled: regionListView.portForwardEnabled
              canFavorite: regionListView.canFavorite
              highlightColumn: {
                if(keyboardRow.country === "dip" && keyboardRow.location === region.id) {
                  return regionListView.highlightColumn
                }
                return -1
              }
              onClicked: regionListView.regionSelected(modelData.id)
              onFocusCell: mouseFocusCell({country: "dip", location: region.id}, keyColumn)
            }
          }
          // Separator between the "auto" + dedicated regions and the normal
          // regions.  Show if any region above the separator would be shown.
          Rectangle {
            Layout.fillWidth: true
            height: 3
            visible: regionAuto.show || displayDedicatedIpsArray.length > 0
            color: Theme.regions.itemSeparatorColor
          }
          Repeater {
            id: regionsRepeater
            model: displayRegionsArray
            delegate: RegionDelegate {
              region: modelData.region
              regionCountry: modelData.regionCountry
              regionChildren: modelData.regionChildren
              portForwardEnabled: regionListView.portForwardEnabled
              serviceLocations: regionListView.serviceLocations
              canFavorite: regionListView.canFavorite
              collapsedCountriesSettingName: regionListView.collapsedCountriesSettingName
              highlightRow: keyboardRow
              highlightColumn: regionListView.highlightColumn
              onRegionSelected: regionListView.regionSelected(locationId)
              onFocusCell: mouseFocusCell(row, keyColumn)
            }
          }
        }
    }
  }

  // RegionListView's focus cues are displayed as a subtle cue around the whole
  // control, and a strong cue around the specific focused "cell".
  // This emphasizes the focused cell and the fact that this is a list, while
  // still providing some indication of the focus if that cell isn't visible.
  // This is similar to the scheme used on Mac, but it differs from Windows,
  // which normally just shows the focused row.
  OutlineFocusCue {
    id: focusCue
    anchors.fill: parent
    control: regionListView
    inside: true
    opacity: 0.6
  }

  function mouseFocusCell(row, column) {
    keyboardRow = row
    keyboardColumn = column
    regionListView.forceActiveFocus(Qt.MouseFocusReason)
  }

  // Find the RegionRowBase for the current keyboard-highlighted row.  Used to
  // scroll it into view and to select it.
  // Returns undefined if there isn't one.
  function findKeyboardRow() {
    var idx = findKeyboardRowIndex(accessibilityTable)
    if(idx >= 0)
      return accessibilityTable[idx].regionItem
  }

  function findKeyboardRowIndex(accTable) {
    return accTable.findIndex(function(row) {
      return row.country === keyboardRow.country && row.location === keyboardRow.location
    })
  }

  // Ensure that a row is visible by scrolling to it if necessary.
  // 'row' is a RegionRowBase, either a country/single region or a
  // subregion within a group.
  function revealRow(row) {
    // Get the highlighted row's bounds in the content of the scroll view
    var rowBounds = row.mapToItem(scrollViewColumn, 0, 0, row.width, row.height)

    Util.ensureScrollViewVertVisible(scrollView, scrollView.ScrollBar.vertical,
                                     rowBounds.y, rowBounds.height)
  }

  Keys.onPressed: {
    // Arrow keys - simple navigation
    if(event.key === Qt.Key_Left) {
      keyboardColumn = Math.max(0, keyboardColumn-1)
    }
    else if(event.key === Qt.Key_Right) {
      keyboardColumn = Math.min(keyboardColumnCount-1, keyboardColumn+1)
    }
    else if(event.key === Qt.Key_Up) {
      keyboardUp()
    }
    else if(event.key === Qt.Key_Down) {
      keyboardDown()
    }
    // Home/End - seek to ends
    else if(event.key === Qt.Key_Home) {
      // Highlight the first item by clearing the highlight and then navigating
      // down
      keyboardRow = {country: '', location: ''}
      keyboardDown()
    }
    else if(event.key === Qt.Key_End) {
      // Highlight the last item by clearing the highlight and then navigating
      // up
      keyboardRow = {country: '', location: ''}
      keyboardUp()
    }
    // Space/Enter/Return - select
    else if(event.key === Qt.Key_Space || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
      // Select the current row/column
      var highlightRowItem = findKeyboardRow()
      if(highlightRowItem)
        highlightRowItem.keyboardSelect(keyboardColumn)
    }
    // Letters - seek by letter
    else if(event.text) {
      // Text key, look for items starting with this text
      if(!keyboardText(event.text)) {
        // Wasn't a key that made sense for these items, do nothing
        return
      }
      // Otherwise, we selected a new item, apply the normal post-selection logic
    }
    else {
      // Some other key that we don't care about.  Do nothing.
      return
    }
    // Page Up/Down might be nice too, these are pretty nontrivial though, would
    // require locating items by position within the ScrollView since they have
    // non-uniform heights.

    // The event was a key that we handled.
    focusCue.reveal()
    event.accepted = true
    // Scroll the current row into view
    var revealItem = findKeyboardRow()  // Returns a RegionRowBase
    if(revealItem)
      revealRow(revealItem)
  }

  function keyboardUp() {
    // Find the row that's currently highlighted
    var accTable = accessibilityTable
    var highlightIdx = findKeyboardRowIndex(accTable)

    // If there isn't a valid country highlighted, select the bottom one
    if(highlightIdx < 0) {
      highlightIdx = accTable.length-1
    }
    // Otherwise, move up if we're not on the top row
    else if(highlightIdx > 0) {
      highlightIdx = highlightIdx-1
    }

    // If the row we are now on is buried, move up to the next non-buried row.
    // Note that the top row can never be buried (it's 'auto')
    while(accTable[highlightIdx].buried)
      --highlightIdx

    // Store the new highlighted row
    keyboardRow = {country: accTable[highlightIdx].country,
                   location: accTable[highlightIdx].location}
  }

  function keyboardDown() {
    // Find the row that's currently highlighted
    var accTable = accessibilityTable
    var highlightIdx = findKeyboardRowIndex(accTable)

    // If there isn't a valid country highlighted, select the top one
    if(highlightIdx < 0) {
      highlightIdx = 0
    }
    // Otherwise, move down if we're not on the bottom
    else if(highlightIdx < accTable.length-1) {
      highlightIdx = highlightIdx+1
    }

    // If the row we are now on is buried, move down to the next non-buried row.
    // We could be in the last group though, so we might still end up on a
    // buried row.
    while(accTable[highlightIdx].buried && highlightIdx < accTable.length-1)
      ++highlightIdx

    // If we're still on a row that's buried, we're in the bottom group, so find
    // the last row that's visible.
    // This will always find a valid row since the 'auto' row at the top is
    // never buried.
    while(accTable[highlightIdx].buried)
      --highlightIdx

    // Store the new highlighted row
    keyboardRow = {country: accTable[highlightIdx].country,
                   location: accTable[highlightIdx].location}
  }

  function keyboardText(keyText) {
    // Find the row that's currently highlighted
    var accTable = accessibilityTable
    var highlightIdx = findKeyboardRowIndex(accTable)

    // Find the next index to select
    highlightIdx = KeyUtil.seekChoiceByKey(accTable, 'display', 'buried',
                                           highlightIdx, keyText)

    if(highlightIdx >= 0) {
      // Found a new selection
      keyboardRow = {country: accTable[highlightIdx].country,
                     location: accTable[highlightIdx].location}
      return true
    }

    // The key wasn't one that made sense for these items
    return false
  }

  function matchesSearchTerm(value) {
    return value.toLowerCase().indexOf(searchTerm.toLowerCase()) >= 0
  }

  // Filter the 'dedicatedIpLocations' array from DaemonState, which is an array
  // of Locations representing dedicated IP regions.
  // These are not grouped, so they're just filtered by the region names.
  function filterDedicatedIps() {
    var dedicatedIps = Daemon.state.dedicatedIpLocations

    if(!searchTerm && !regionFilter)
      return dedicatedIps

    var filteredDedicatedIps = dedicatedIps

    // Filter with the region filter if there is one
    if(regionFilter)
      filteredDedicatedIps = filteredDedicatedIps.filter(regionFilter)

    // Filter by the search term if present
    if(searchTerm) {
      filteredDedicatedIps = filteredDedicatedIps.filter(function(dip) {
        return matchesSearchTerm(Daemon.getLocationName(dip))
      })
    }

    return filteredDedicatedIps
  }

  // Filter the 'countries' object from DaemonState.groupedLocations,
  // which is an array of CountryLocations (which has a 'locations' property,
  // which is an array of locations).
  //
  // - Single regions or regions in a group are included if the region name
  //   contains the search string
  // - An entire group is included if the group name contains the search string
  function filterCountryLocations() {
    var countries = Daemon.state.groupedLocations
    if(!searchTerm && !regionFilter)
      return countries

    var filteredCountries = []
    for(var i=0; i<countries.length; ++i) {
      // Filter the locations in this country using the region filter.  This
      // could cause a country group to become a single location (which affects
      // the search term matching below)
      var filteredLocations = countries[i].locations
      if(regionFilter)
        filteredLocations = filteredLocations.filter(regionFilter)

      if(filteredLocations.length <= 0)
        continue  // Nothing to display in this country

      // If the country contains exactly one region, just include it if the
      // region's name matches.  Don't look at the country name, because we
      // don't display it for single regions.
      if(filteredLocations.length === 1) {
        if(matchesSearchTerm(Daemon.getLocationName(filteredLocations[0])))
          filteredCountries.push({locations: filteredLocations})
      }
      // The country contains more than one region.  If the country name
      // matches, include everything.
      else if(matchesSearchTerm(Daemon.getCountryName(filteredLocations[0].country)))
        filteredCountries.push({locations: filteredLocations})
      // The country has more than one region, and the country name doesn't
      // match.  Filter the individual regions.
      else {
        var filteredRegions = filteredLocations.filter(function(loc) {
          return matchesSearchTerm(Daemon.getLocationName(loc))
        })
        // If at least one region matched, include the country with the filtered
        // regions
        if(filteredRegions.length > 0)
          filteredCountries.push({locations: filteredRegions})
      }
    }

    return filteredCountries
  }

  function localeCompareRegions(first, second) {
    return first.localeCompare(second, Client.state.activeLanguage.locale)
  }

  function countrySortName(country) {
    if(country.locations.length === 1)
      return Daemon.getLocationName(country.locations[0]).toLowerCase()
    return Daemon.getCountryName(country.locations[0].country).toLowerCase()
  }

  function sortLocations(locations) {
    var sortedLocations = locations.slice()
    sortedLocations.sort(function(first, second) {
      var nameComp = localeCompareRegions(Daemon.getLocationName(first).toLowerCase(),
                            Daemon.getLocationName(second).toLowerCase())
      if(nameComp !== 0)
        return nameComp
      // For dedicated IP regions, the names could be exactly the same if the
      // user has more than one dedicated IP.  Sort by IP next.  This is a
      // lexical sort - a numeric sort by IP might be nice, but it really
      // doesn't make that much difference, we just need a consistent order.
      //
      // If this somehow happens for regions that aren't dedicated IP regions,
      // this won't do anything (both will be empty strings).
      return localeCompareRegions(first.dedicatedIp, second.dedicatedIp)
    })
    return sortedLocations
  }

  // Sort the (possibly filtered) country locations by name.
  function sortCountriesByName(countries) {
    // Sort the country groups by either the country name or single-region name.
    // Sort a copy of the array because this could be the actual array from
    // DaemonState.
    countries = countries.slice()
    countries.sort(function(first, second) {
      return localeCompareRegions(countrySortName(first), countrySortName(second))
    })

    // Sort each country's regions
    for(var i=0; i<countries.length; ++i) {
      // Create new country objects, because these could still be the actual
      // objects from DaemonState.  sortLocations() creates a new array with the
      // sorted locations.
      countries[i] = {locations: sortLocations(countries[i].locations)}
    }

    return countries
  }

  // Build an array of regions from the country map.
  function buildRegionArray(countries) {
    var result = []
    for(var i=0; i<countries.length; ++i) {
      var rlist = countries[i].locations
      var countryId = rlist[0].country.toLowerCase()
      // Single location
      if (rlist.length === 1) {
        result.push({
                      region: rlist[0],
                      regionCountry: countryId,
                      regionChildren: []
                    })
      }
      // Group of locations in a country
      else {
        var subregions = rlist.map(function(rgn) {
          return {
            subregion: rgn
          }});

        // Country groups have a null region (they don't represent a single
        // location).
        result.push({
                      region: null,
                      regionCountry: countryId,
                      regionChildren: subregions
                    })
      }
    }
    return result
  }

  property var displayDedicatedIpsArray: {
    // Get the filtered dedicated IP locations
    var dedicatedIps = filterDedicatedIps()

    // If sorting by name, re-sort the locations by name
    if(sortKey.currentValue === "name")
      dedicatedIps = sortLocations(dedicatedIps)

    return dedicatedIps
  }

  // The array of country groups and single regions used as the model for the
  // regions repeater.
  //
  // This consists of objects with the following properties:
  // - region: null for a country group, or a Location object for a single region
  // - regionCountry: country code for the group or location
  // - regionChildren: array of {subregion: <Location>} objects for a country
  //   group, or empty array for a single region
  property var displayRegionsArray: {
    // Get the filtered country locations
    var countries = filterCountryLocations()

    // If sorting by name, re-sort the countries and their locations.
    // (If sorting by latency, Daemon has already sorted them.)
    if(sortKey.currentValue === "name")
      countries = sortCountriesByName(countries)

    // Build the actual model into a list with children
    return buildRegionArray(countries)
  }

  // Build a flat tabular representation of the contents that we use for
  // accessibility.  Keyboard navigation uses this, and screen reader
  // representation probably will too.  (The heirarchical representation is
  // necessary for the UI presentation, but it does not fit the table model very
  // well.)
  //
  // This is an array of objects with the following properties:
  // - country: Country code (or 'auto' for the auto region) - country/location
  //   identify the highlighted row in keyboardRow
  // - location: Location ID ('' for a country group heading, 'auto' for the
  //   auto region)
  // - level: The outline level for this row (1 for children of a country group,
  //   0 otherwise)
  // - expanded: Whether this row displays its children (rows with no children
  //   have expanded === false).
  // - buried: Whether this row is hidden due to being in a collapsed group.
  // - display: The display text for this row (used for keyboard seeking)
  // - regionItem: The RegionRowBase (or RegionAuto) that displays this row.
  //   (Used to build accessibility elements for cells.)
  //
  // Note that 'buried' could in principle be determined by finding the parent
  // (using 'level') and checking its 'expanded' attribute.  Storing it though
  // simplifies algorithms greatly, and is necessary to use the
  // KeyUtil.seekChoiceByKey() algorithm (note that the sense of the parameter
  // is important for this - it acts like a 'disabled' flag, so navigable
  // choices must have 'false').
  property var accessibilityTable: {
    // This calculation depends on regionRepeater.children - itemAt() does not
    // add this dependency.
    var childrenDependency = regionsRepeater.children
    var dipDependency = dedicatedIpsRepeater.children

    var table = []

    // Add the 'auto' region at the top
    table.push({country: 'auto', location: 'auto', level: 0, expanded: false,
                buried: false, display: regionAuto.displayText,
                regionItem: regionAuto})

    // The accessibility table is built from the actual items that represent the
    // regions, not the displayRegionsArray, so it can include references to the
    // row items.  These are used to build the accessibility elements in
    // NativeAcc.Table.rows.
    //
    // This also gives it a good way to access regionItem.expanded (which stores
    // the 'expanded' state, although this should probably move out of the items
    // themselves eventually, since this causes it to reset whenever the array
    // is rebuilt.

    var i;
    for(i=0; i<dedicatedIpsRepeater.count; ++i) {
      var dipItem = dedicatedIpsRepeater.itemAt(i)
      if(!dipItem)
        continue

      // DIPs are all single locations
      table.push({country: "dip", location: dipItem.region.id, level: 0,
                  expanded: false, buried: false,
                  display: Daemon.getLocationName(dipItem.region),
                  regionItem: dipItem})
    }

    for(i=0; i<regionsRepeater.count; ++i) {
      var regionItem = regionsRepeater.itemAt(i)
      if(!regionItem)
        continue

      if(regionItem.regionChildren.length > 0) {
        var countryCollapsed = !regionItem.expanded

        // Country heading
        table.push({country: regionItem.regionCountry, location: '', level: 0,
                    expanded: !countryCollapsed, buried: false,
                    display: Daemon.getCountryName(regionItem.regionCountry),
                    regionItem: regionItem.getRegionItem()})

        for(var j=0; j<regionItem.regionChildren.length; ++j) {
          var subregion = regionItem.regionChildren[j].subregion
          var subregionLoc = Daemon.state.availableLocations[subregion.id]
          table.push({country: regionItem.regionCountry,
                      location: subregion.id,
                      level: 1,
                      expanded: false,
                      buried: countryCollapsed,
                      display: subregionLoc ? Daemon.getLocationName(subregionLoc) : subregion.id,
                      regionItem: regionItem.getSubregionItem(j)})
        }
      }
      else {
        // Single location
        var regionId = regionItem.region.id
        var regionLoc = Daemon.state.availableLocations[regionId]
        table.push({country: regionItem.regionCountry,
                    location: regionId,
                    level: 0, expanded: false, buried: false,
                    display: regionLoc ? Daemon.getLocationName(regionLoc) : regionId,
                    regionItem: regionItem.getRegionItem()})
      }
    }

    return table
  }

  NativeAcc.Table.name: regionListView.regionListLabel

  property NativeAcc.TableColumn regionColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the column in the region list that displays
    //: the region names and flags.
    name: uiTr("Region")
    item: regionListView
  }

  property NativeAcc.TableColumn detailColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the column in the region list that displays
    //: additional details about the region, when present.  This includes
    //: the IP address for "dedicated IP" regions, and the current nearest
    //: region for the "auto" row.
    name: uiTr("Detail")
    item: regionListView
  }
  property NativeAcc.TableColumn latencyColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the column in the region list that displays
    //: the regions' latency measurements.
    name: uiTr("Latency")
    item: regionListView
  }
  property NativeAcc.TableColumn favoriteColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the column in the region list that displays
    //: the regions' "favorite region" buttons.
    name: uiTr("Favorite")
    item: regionListView
  }
  NativeAcc.Table.columns: {
    var cols = [
      { property: "accRegionCell", column: regionColumn },
      { property: "accDetailCell", column: detailColumn },
      { property: "accLatencyCell", column: latencyColumn }
    ]
    // Only provide the favorite column when enabled.  Row objects will still
    // have the "favorite" property, but it'll be ignored by NativeAcc.Table
    if(regionListView.canFavorite)
      cols.push({ property: "accFavoriteCell", column: favoriteColumn })
    return cols
  }
  NativeAcc.Table.rows: {
    var tblRows = []
    var accRow
    var accTable = accessibilityTable
    var rowId
    for(var i=0; i<accTable.length; ++i) {
      if(!accTable[i].buried) {
        accRow = accTable[i]
        rowId = accRow.country + '/' + accRow.location

        tblRows.push({id: rowId,
                      row: accRow.regionItem.accRow,
                      accRegionCell: accRow.regionItem.accRegionCell,
                      accDetailCell: accRow.regionItem.accDetailCell,
                      accLatencyCell: accRow.regionItem.accLatencyCell,
                      accFavoriteCell: accRow.regionItem.accFavoriteCell})
      }
    }

    return tblRows
  }

  NativeAcc.Table.navigateRow: {
    var accTable = accessibilityTable
    return findKeyboardRowIndex(accTable)
  }
  NativeAcc.Table.navigateCol: {
    var keyboardRowObj = findKeyboardRow()
    if(!keyboardRowObj)
      return -1
    var actualKeyColIdx = keyboardRowObj.effectiveColumnFor(keyboardColumn)
    // Map the key columns to screen reader columns
    switch(actualKeyColIdx) {
      // RegionRowBase.keyColumns.region
      case 0:
        return 0
      // RegionRowBase.keyColumns.favorite
      case 1:
        return 3
    }
    return -1
  }
}
