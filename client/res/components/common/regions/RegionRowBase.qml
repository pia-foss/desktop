// Copyright (c) 2024 Private Internet Access, Inc.
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
import "../../daemon"
import "../../theme"
import "../../common"
import "../../core"
import "../../settings/stores"
import PIA.NativeAcc 1.0 as NativeAcc

RegionRowConstants {
  id: regionRowBase

  property alias label: labels.label
  property alias labelTextPx: labels.labelTextPx
  property alias showLatency: labels.showLatency
  property alias latency: labels.latency
  property alias selected: labels.selected
  property alias canFavorite: labels.canFavorite
  property alias lacksPortForwarding: labels.lacksPortForwarding
  property alias pfWarningTipText: labels.pfWarningTipText
  property alias geoLocation: labels.geoLocation
  property alias offline: labels.offline

  property alias backgroundColor: background.backgroundColor
  property alias hoverBackgroundColor: background.hoverBackgroundColor
  property alias separatorColor: background.separatorColor

  // The location ID used for the favorite setting
  property string favoriteRegionId

  // Whether the row is expanded.  (Only on RegionRowBase because it's expressed
  // in the row's screen reader annotation.)
  property bool expanded
  // The row's outline level (for the screen reader annotation)
  property int outlineLevel

  // Children of the region row go into the opacity wrapper, so they're dimmed
  // if the region is dimmed
  default property alias contents: labels.contents

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
    name: regionRowBase.label
    item: regionRowBase
    selected: regionRowBase.selected
    outlineExpanded: regionRowBase.expanded
    outlineLevel: regionRowBase.outlineLevel
  }
  accRegionCell: labels.accRegionCell
  accDetailCell: null // Normal region rows have no details
  accLatencyCell: labels.accLatencyCell
  accFavoriteCell: labels.accFavoriteCell

  property ClientSetValueSetting favoriteSetting: ClientSetValueSetting {
    name: 'favoriteLocations'
    settingValue: favoriteRegionId
  }

  function toggleFavorite() {
    favoriteSetting.currentValue = !favoriteSetting.currentValue
  }

  RegionRowBackground {
    id: background
    anchors.fill: parent
    showRowHighlight: labels.showRowHighlight ||
      effectiveColumnFor(highlightColumn) === keyColumns.region
    onClicked: {
      focusCell(keyColumns.region)
      regionRowBase.clicked()
    }
  }

  RegionRowLabels {
    id: labels
    anchors.fill: parent

    isFavorite: favoriteSetting.currentValue
    highlightFavorite: effectiveColumnFor(highlightColumn) === keyColumns.favorite
    onRegionClicked: regionRowBase.clicked()
    onFavoriteClicked: {
      focusCell(keyColumns.favorite)
      toggleFavorite()
    }
  }

  // Keyboard highlight cue for the region itself
  HighlightCue {
    anchors.fill: parent
    visible: effectiveColumnFor(highlightColumn) === keyColumns.region
    inside: true
  }
}
