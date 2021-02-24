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
import "../../common"
import "../../theme"
import "../../daemon"
import "../../settings/stores"
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util
import PIA.NativeAcc 1.0 as NativeAcc

RegionRowConstants {
  id: dedicatedIpRegion

  property var region
  property var serviceLocations
  property bool canFavorite
  property bool portForwardEnabled

  implicitHeight: 55

  // Get the effective column for a specified highlight or selection column.
  // (If a row doesn't have a favorite icon, highlighting/selecting the favorite
  // column applies to the region column instead.)
  function effectiveColumnFor(column) {
    if(column < 0)
      return -1
    if(column === keyColumns.favorite && canFavorite)
      return keyColumns.favorite
    return keyColumns.region
  }

  // Select the row with the keyboard.  'column' is the column that is selected.
  function keyboardSelect(column) {
    switch(effectiveColumnFor(column)) {
      case keyColumns.region:
        clicked()
        break
      case keyColumns.favorite:
        toggleFavorite()
        break
    }
  }

  // Screen reader row annotation
  // RegionAuto has a corresponding property.
  accRow: NativeAcc.TableRow {
    name: labels.label
    item: dedicatedIpRegion
    selected: labels.selected
    outlineExpanded: false
    outlineLevel: 0
  }
  accRegionCell: labels.accRegionCell
  accDetailCell: NativeAcc.TableCellText {
    //: Screen reader annotation for the "detail" line of a Dedicated IP row,
    //: which displays the IP address and the "Dedicated IP" tag.  %1 is an IPv4
    //: address, such as 100.200.100.200.
    name: uiTr("%1 (Dedicated IP)").arg(region.dedicatedIp)
    item: dipSubtitle
  }
  accLatencyCell: labels.accLatencyCell
  accFavoriteCell: labels.accFavoriteCell

  property ClientSetValueSetting favoriteSetting: ClientSetValueSetting {
    name: 'favoriteLocations'
    settingValue: region.id
  }
  function toggleFavorite() {
    favoriteSetting.currentValue = !favoriteSetting.currentValue
  }

  RegionRowBackground {
    id: background
    anchors.fill: parent

    backgroundColor: Theme.regions.itemBackgroundColor
    hoverBackgroundColor: Theme.regions.itemHighlightBackgroundColor
    separatorColor: Theme.regions.itemSeparatorColor

    showRowHighlight: labels.showRowHighlight ||
      effectiveColumnFor(highlightColumn) === keyColumns.region
    onClicked: {
      focusCell(keyColumns.region)
      dedicatedIpRegion.clicked()
    }
  }

  // The normal labels are positioned with a height of 36 to make room for
  // the dedicated IP-specific UI.
  RegionRowLabels {
    id: labels
    width: parent.width
    height: 36

    label: Daemon.getLocationName(region)
    labelTextPx: Theme.regions.labelTextPx
    showLatency: true
    latency: (region && Util.isFiniteNumber(region.latency)) ? region.latency : -1
    selected: !!(serviceLocations.chosenLocation && region.id === serviceLocations.chosenLocation.id)
    canFavorite: dedicatedIpRegion.canFavorite
    lacksPortForwarding: portForwardEnabled && !region.portForward
    pfWarningTipText: singleRegionPfWarning
    geoLocation: region.geoOnly

    isFavorite: favoriteSetting.currentValue
    highlightFavorite: effectiveColumnFor(dedicatedIpRegion.highlightColumn) === keyColumns.favorite
    onRegionClicked: dedicatedIpRegion.clicked()
    onFavoriteClicked: {
      focusCell(keyColumns.favorite)
      dedicatedIpRegion.toggleFavorite()
    }

    FlagImage {
      x: 24
      y: dedicatedIpRegion.height / 2 - height / 2
      countryCode: region.country
    }

    DedicatedIpSubtitle {
      id: dipSubtitle
      x: 56
      y: 30
      dedicatedIp: region.dedicatedIp
      // Don't need to fill the DIP tag background for this element, and this
      // allows the "hover" background color to show through
      dipTagBackground: "transparent"
    }
  }

  // Keyboard highlight cue for the region itself
  HighlightCue {
    anchors.fill: parent
    visible: effectiveColumnFor(highlightColumn) === keyColumns.region
    inside: true
  }
}
