// Copyright (c) 2019 London Trust Media Incorporated
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
import "../../../javascript/util.js" as Util
import "../../daemon"
import "../../theme"
import "../../common"
import "../../core"

// RegionDelegate is used by RegionListView to display each group of server
// locations in a particular region (country).
//
// If a region just has one server location, it displays a single item with the
// flag and latency that can be selected to choose that region.  If it has more
// than one, they're referred to as "subregions" and are represented as a list
// that can be collapsed.
Item {
  id: regionDelegate

  property bool expanded: true
  readonly property int countryHeight: 40
  readonly property int regionHeight: 36
  readonly property bool hasSubRegions: regionChildren.length > 0

  // The ServerLocation representing the single region, if there is only one
  property var region
  // The single region's ID, if there is only one, '' otherwise
  readonly property string regionKey: region ? region.id : ''
  // The country code (valid for both single regions and country groups)
  property string regionCountry
  // The subregions, as an array of objects with:
  // - subregion: The ServerLocation for this region
  // For a single region, this is an empty array
  property var regionChildren
  // Configuration properties forwarded from RegionListView
  property var serviceLocations
  property bool portForwardEnabled
  property bool canFavorite

  readonly property string singleRegionPfWarning: uiTr("Port forwarding is not available for this location.")
  readonly property string regionGroupPfWarning: uiTr("Port forwarding is not available for this country.")

  // highlightRow and highlightColumn are bound in from RegionListView when it
  // shows the keyboard navigation highlight.
  // (highlightRow is always bound to keyboardRow, highlightColumn is bound only
  // when the cue should be shown, -1 otherwise)
  // RegionDelegate uses this to indicate the keyboard highlight to
  // RegionRowBase, either in the region or in subregions.
  property var highlightRow
  property int highlightColumn

  // A region has been selected
  signal regionSelected(string locationId)

  // The regions list should take focus due to a mouse event on a cell
  // Includes the row and cell that were clicked
  // Emitted for any click on the row (region or favorite parts)
  signal focusCell(var row, int keyColumn)

  // Find a row for the specified keyboard row.  Used by RegionListView for
  // keyboard navigation.
  // Returns a RegionRowBase, or undefined if no row in this delegate matches.
  function findKeyboardRow(row) {
    if(row.country !== regionCountry)
      return
    if(row.location === regionKey)
      return countryWrapper

    for(var i=0; i<subregionsRepeater.count; ++i) {
      var subregion = subregionsRepeater.itemAt(i)
      if(row.location === subregion.locationId)
        return subregion
    }
  }

  // Get the RegionRowBase for the delegate's "main region" - either the country
  // if this is a country group, or the single region otherwise.
  function getRegionItem() {
    return countryWrapper
  }
  // Get the RegionRowBase that displays a child row (used to build cell
  // accessibility elements).  The index corresponds to the subregions array.
  function getSubregionItem(index) {
    // itemAt() doesn't introduce a dependency on the repeater's children.
    var childrenDep = subregionsRepeater.children
    return subregionsRepeater.itemAt(index)
  }

  Layout.fillWidth: true

  height: countryHeight + (expanded ? 1 : 0) * regionChildren.length * regionHeight
  Layout.preferredHeight: height
  clip: true
  Behavior on height {
    SmoothedAnimation {
      duration: Theme.animation.normalDuration
    }
  }
  RegionRowBase {
    id: countryWrapper

    anchors.left: parent.left
    anchors.top: parent.top
    anchors.right: parent.right
    height: countryHeight

    label: hasSubRegions ? Daemon.getCountryName(regionCountry) : Daemon.getLocationName(region)
    labelTextPx: Theme.regions.labelTextPx
    showLatency: !hasSubRegions
    latency: {
      if(region && Util.isFiniteNumber(region.latency))
        return region.latency
      return -1
    }
    selected: !!(regionDelegate.serviceLocations.chosenLocation && regionKey === regionDelegate.serviceLocations.chosenLocation.id && !hasSubRegions)
    backgroundColor: Theme.regions.itemBackgroundColor
    hoverBackgroundColor: Theme.regions.itemHighlightBackgroundColor
    separatorColor: Theme.regions.itemSeparatorColor
    canFavorite: regionDelegate.canFavorite
    favoriteRegionId: {
      hasSubRegions ? "auto/" + regionDelegate.regionCountry : regionKey
    }
    lacksPortForwarding: {
      if(!regionDelegate.portForwardEnabled)
        return false  // PF not enabled, do not show indicators
      if(region)
        return !region.portForward
      // For a group, the group supports PF if any subregion supports PF
      for(var i=0; i<regionChildren.length; ++i) {
        if(regionChildren[i].subregion.portForward)
          return false  // Supports PF, no indicator
      }
      return true // Lacks PF
    }
    pfWarningTipText: hasSubRegions ? regionGroupPfWarning : singleRegionPfWarning
    highlightColumn: {
      // If this row is currently highlighted, apply the highlighted column
      if(regionDelegate.highlightRow.country === regionCountry &&
         regionDelegate.highlightRow.location === regionKey) {
        return regionDelegate.highlightColumn
      }
      return -1
    }
    expanded: regionDelegate.expanded
    outlineLevel: 0

    onFocusCell: {
      regionDelegate.focusCell({country: regionCountry, location: regionKey},
                               keyColumn)
    }

    onClicked: {
      if (hasSubRegions) {
        regionDelegate.expanded = !regionDelegate.expanded
      } else {
        regionSelected(regionKey)
      }
    }

    // Expander triangle
    Image {
      source: Theme.regions.regionExpander
      height: 10
      width: 6
      anchors.verticalCenter: parent.verticalCenter
      x: 9
      visible: hasSubRegions
      rotation: regionDelegate.expanded ? 90 : 0
      Behavior on rotation {
        RotationAnimation {
          duration: Theme.animation.normalDuration
        }
      }
    }

    FlagImage {
      anchors.verticalCenter: parent.verticalCenter
      x: 24
      countryCode: regionCountry
    }
  }

  ColumnLayout {
    visible: hasSubRegions
    anchors.top: countryWrapper.bottom
    anchors.left: parent.left
    width: parent.width
    spacing: 0

    Repeater {
      id: subregionsRepeater
      model: regionChildren

      delegate: RegionRowBase {
        // The ServerLocation object for this location
        property var subregion: modelData.subregion
        // The location's ID
        property string subregionKey: modelData.subregion.id

        Layout.fillWidth: true
        height: regionHeight
        label: Daemon.getLocationName(subregion)
        labelTextPx: Theme.regions.sublabelTextPx
        showLatency: true
        latency: Util.isFiniteNumber(subregion.latency) ? subregion.latency : -1
        selected: !!(regionDelegate.serviceLocations.chosenLocation && subregionKey === regionDelegate.serviceLocations.chosenLocation.id)
        backgroundColor: Theme.regions.subRegionBackground
        hoverBackgroundColor: Theme.regions.subRegionHighlightBackgroundColor
        separatorColor: Theme.regions.subRegionSeparatorColor
        canFavorite: regionDelegate.canFavorite
        favoriteRegionId: subregionKey
        lacksPortForwarding: regionDelegate.portForwardEnabled && !subregion.portForward
        pfWarningTipText: singleRegionPfWarning
        highlightColumn: {
          // If this row is currently highlighted, apply the highlighted column
          if(regionDelegate.highlightRow.country === regionDelegate.regionCountry &&
             regionDelegate.highlightRow.location === subregionKey) {
            return regionDelegate.highlightColumn
          }
          return -1
        }
        expanded: false
        outlineLevel: 1

        // Needed for RegionDelegate to locate the row in findKeyboardRow()
        readonly property string locationId: subregionKey

        onFocusCell: {
          regionDelegate.focusCell({country: regionDelegate.regionCountry,
                                    location: subregionKey}, keyColumn)
        }
        onClicked: regionSelected(subregionKey)
      }
    }
  }
}
