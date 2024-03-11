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
import "../../client"
import "../../common"
import "../../core"
import "../../theme"
import "../../vpnconnection"
import PIA.NativeAcc 1.0 as NativeAcc

RegionRowConstants {
  id: regionAuto

  // RegionListView needs the translated display text to implement keyboard
  // seeking
  readonly property string displayText: uiTr("Choose automatically")

  // Service locations for this region list
  property var serviceLocations

  // The 'auto' region only has one column, so the effective column is always
  // that one.
  function effectiveColumnFor(column) {
    return keyColumns.region
  }

  function keyboardSelect() {
    clicked()
  }

  readonly property bool selected: !regionAuto.serviceLocations.chosenLocation

  accRow: NativeAcc.TableRow {
    name: displayText
    item: regionAuto
    selected: regionAuto.selected
    outlineExpanded: false
    outlineLevel: 0
  }
  accRegionCell: labels.accRegionCell
  accDetailCell: NativeAcc.TableCellText {
    name: currentAutoRegionText.text
    item: currentAutoRegionText
  }
  accLatencyCell: labels.accLatencyCell
  accFavoriteCell: labels.accFavoriteCell

  function regionClicked() {
    focusCell(keyColumns.region)
    clicked()
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
    countryCode: regionAuto.serviceLocations.bestLocation ?
      Daemon.state.getRegionCountryCode(regionAuto.serviceLocations.bestLocation.id) : ''
  }

  RegionRowLabels {
    id: labels
    width: parent.width
    height: 46
    label: displayText
    labelTextPx: Theme.regions.labelTextPx
    showLatency: true
    latency: regionAuto.serviceLocations.bestLocation ? regionAuto.serviceLocations.bestLocation.latency : -1
    selected: regionAuto.selected
    canFavorite: false
    // N/A, auto region never shows this warning (only PF regions are chosen
    // automatically when PF is enabled)
    lacksPortForwarding: false
    pfWarningTipText: ""
    // N/A, auto regions never show this (only non-geo regions are chosen automatically)
    geoLocation: false
    // N/A, auto can't be favorited
    isFavorite: false
    highlightFavorite: false
    onRegionClicked: regionAuto.regionClicked()
  }

  Text {
    id: currentAutoRegionText
    x: 56
    y: 34
    text: Client.getRegionAutoName(regionAuto.serviceLocations.bestLocation && regionAuto.serviceLocations.bestLocation.id)
    font.pixelSize: Theme.regions.sublabelTextPx
    color: Theme.regions.autoRegionSublabelColor
  }

  Rectangle {
    anchors.bottom: parent.bottom
    anchors.left: parent.left
    anchors.right: parent.right
    height: 1
    color: Theme.regions.itemSeparatorColor
  }

  MouseArea {
    id: mouseArea
    anchors.fill: parent
    hoverEnabled: true
    onClicked: regionAuto.regionClicked()
  }

  HighlightCue {
    anchors.fill: parent
    visible: highlightColumn >= 0
    inside: true
  }
}
