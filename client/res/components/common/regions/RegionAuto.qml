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
import "../../daemon"
import "../../common"
import "../../core"
import "../../theme"
import "../../vpnconnection"
import PIA.NativeAcc 1.0 as NativeAcc

Item {
  id: regionAuto

  // RegionListView needs the translated display text to implement keyboard
  // seeking
  readonly property string displayText: uiTr("Choose automatically")

  // Provide a highlightColumn property similar to that of RegionRowBase.
  // RegionAuto can't be favorited, so the specific column here doesn't matter
  // when it's not -1.
  property int highlightColumn

  // Service locations for this region list
  property var serviceLocations

  // The auto region was selected
  signal regionSelected()

  // The regions list should take focus due to a mouse event
  // Like RegionRowBase, includes the column that was selected (always 0 for
  // RegionAuto)
  // Emitted for any click on the row
  signal focusCell(int keyColumn)

  // The 'auto' region only has one column, so the effective column is always
  // that one.
  function effectiveColumnFor(column) {
    return 0 // RegionRowBase.keyColumns.region
  }

  function keyboardSelect() {
    regionSelected()
  }

  readonly property bool selected: !regionAuto.serviceLocations.chosenLocation

  // Screen reader row annotation - corresponds to RegionRowBase.accRow.
  readonly property NativeAcc.TableRow accRow: NativeAcc.TableRow {
    name: displayText
    item: regionAuto
    selected: regionAuto.selected
    outlineExpanded: false
    outlineLevel: 0
  }
  // Screen reader cell annotations - correspond to RegionRowBase properties.
  readonly property NativeAcc.TableCellButton accRegionCell: NativeAcc.TableCellButton {
    name: displayText
    item: chooseAutomaticallyText
    onActivated: regionAuto.clicked()
  }
  // RegionAuto does not have latency or favorite cells.
  readonly property NativeAcc.TableCellText accLatencyCell: null
  readonly property NativeAcc.TableCellCheckButton accFavoriteCell: null

  function clicked() {
    focusCell(0)
    regionSelected()
  }

  Rectangle {
    anchors.fill: parent
    color: {
      if(mouseArea.containsMouse || highlightColumn >= 0)
        return Theme.regions.itemHighlightBackgroundColor
      return Theme.regions.itemBackgroundColor
    }
  }

  FlagImage {
    x: 24
    anchors.verticalCenter: parent.verticalCenter
    countryCode: regionAuto.serviceLocations.bestLocation ? regionAuto.serviceLocations.bestLocation.country : ''
  }

  Text {
    id: chooseAutomaticallyText
    x: 56
    y: 14
    text: displayText
    font.pixelSize: Theme.regions.labelTextPx
    color: regionAuto.selected ? Theme.regions.itemSelectedTextColor : Theme.regions.itemTextColor
  }

  Text {
    x: 56
    y: 34
    text: Daemon.getLocationName(regionAuto.serviceLocations.bestLocation)
    font.pixelSize: Theme.regions.sublabelTextPx
    color: Theme.regions.autoRegionSublabelColor
  }

  Rectangle {
    anchors.bottom: parent.bottom
    anchors.left: parent.left
    anchors.right: parent.right
    height: 4
    color: Theme.regions.itemSeparatorColor
  }

  MouseArea {
    id: mouseArea
    anchors.fill: parent
    hoverEnabled: true
    onClicked: regionAuto.clicked()
  }

  HighlightCue {
    anchors.fill: parent
    visible: highlightColumn >= 0
    inside: true
  }
}
