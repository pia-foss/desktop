// Copyright (c) 2020 Private Internet Access, Inc.
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
import QtQuick.Window 2.3
import "../../core"
import "../../theme"
import "../../common"
import "../../client"
import "../../daemon"
import "../inputs"
import "../stores"
import Qt.labs.platform 1.1
import PIA.SplitTunnelManager 1.0
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc

SplitTunnelRowBase {
  id: ipRow

  property bool showAppIcons: true

  // Subnet displayed in the cell
  property string subnet

  // Column index of cell to highlight within this row - -1 for none.
  property int highlightColumn: -1

  function removeFromSplitTunnelRules() {
    var updatedRules = Daemon.settings.bypassSubnets.filter(subnetRule => subnetRule.subnet !== subnet)
    Daemon.applySettings({bypassSubnets: updatedRules});
  }

  // Select a cell in this row with the keyboard.
  function keyboardSelect(keyboardColumn) {
    switch(keyboardColumn) {
    case keyColumns.remove:
      removeFromSplitTunnelRules();
      break
    }
  }

  // Effective column (app rows have all columns)
  function effectiveColumnFor(column) {
    return column
  }

  signal focusCell(int column)

  // Screen reader row annotation
  readonly property NativeAcc.TableRow accRow: NativeAcc.TableRow {
    name: subnet
    item: ipRow
    selected: false
    outlineExpanded: false
    outlineLevel: 0
  }

  // Screen reader cell annotations
  readonly property NativeAcc.TableCellText accAppCell: NativeAcc.TableCellText {
    name: subnet
    item: subnetText
  }

  readonly property NativeAcc.TableCellText accModeCell: NativeAcc.TableCellText {
    name: bypassText.text
    item: bypassText
  }

  readonly property NativeAcc.TableCellButton accRemoveCell: NativeAcc.TableCellButton {
    //: Screen reader annotation for the "remove" button ("X" icon) next to a
    //: split tunnel ip rule.  (Should be labeled like a normal command
    //: button.)
    name: uiTr("Remove")
    item: removeIpButtonImg
    onActivated: ipRow.removeClicked()
  }

  function removeClicked() {
    focusCell(keyColumns.remove)
    keyboardSelect(keyColumns.remove)
  }

  implicitHeight: 50

  Image {
    id: image
    visible: showAppIcons
    source: Theme.dashboard.subnetImage
    opacity: 1.0

    width: 20
    height: (width / sourceSize.width) * sourceSize.height
    anchors.left: parent.left
    anchors.leftMargin: 10
    anchors.verticalCenter: parent.verticalCenter
  }

  Text {
    id: subnetText
    anchors.left: parent.left
    anchors.leftMargin: showAppIcons ? 40 : 5
    y: 4
    anchors.verticalCenter: parent.verticalCenter
    anchors.right: bypassText.left
    anchors.rightMargin: 5
    text: ipRow.subnet
    color: Theme.settings.hbarTextColor
    font.pixelSize: 16
    elide: Text.ElideRight
  }

  // This layout mimics the "mode" dropdown in app rules, the margins
  // approximate the layout of the dropdown.  (Only bypass is available for IP
  // rules currently.)
  Text {
    id: bypassText
    width: 114
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    anchors.rightMargin: 69

    text: appModeChoices[0].name // Corresponds to "Bypass VPN" text
    color: Theme.settings.hbarTextColor
    font.pixelSize: 13
  }

  Rectangle {
    anchors.bottom: parent.bottom
    height: 1
    anchors.left: parent.left
    anchors.right: parent.right
    color: Theme.settings.splitTunnelItemSeparatorColor
    opacity: 0.5
  }

  Image {
    id: removeIpButtonImg
    height: 16
    width: 16

    source: removeApplicationMouseArea.containsMouse ? Theme.settings.splitTunnelRemoveApplicationButtonHover : Theme.settings.splitTunnelRemoveApplicationButton
    anchors.verticalCenter: parent.verticalCenter
    anchors.right: parent.right
    anchors.rightMargin: 20
  }

  MouseArea {
    id: removeApplicationMouseArea
    anchors.fill: removeIpButtonImg
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor

    onClicked: ipRow.removeClicked()
  }

  // Highlight cue for bypass text
  HighlightCue {
    anchors.fill: bypassText
    visible: highlightColumn === keyColumns.mode
  }

  // Highlight cue for the remove button
  HighlightCue {
    anchors.fill: removeIpButtonImg
    visible: highlightColumn === keyColumns.remove
  }

  // Highlight cue for the app row
  HighlightCue {
    anchors.fill: parent
    visible: highlightColumn === keyColumns.app
    inside: true
  }
}
