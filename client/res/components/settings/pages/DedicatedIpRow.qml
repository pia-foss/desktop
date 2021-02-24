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
import QtQuick.Window 2.3
import "../../core"
import "../../theme"
import "../../common"
import "../../client"
import "../../daemon"
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc

DedicatedIpRowBase {
  id: dedicatedIpRow

  implicitHeight: 45

  // Properties from the dedicated IP region
  property string regionCountry
  property string regionName
  property string regionIp
  property string regionId

  //: "Remove" button label - used as the screen reader annotation for the "X"
  //: button next to a dedicated IP, and for the confirmation button on the
  //: prompt presented when removing a dedicated IP.
  readonly property string removeButton: uiTr("Remove")

  // This row has all columns
  effectiveColumnFor: function(column){return column}
  keyboardSelect: function(column) {
    switch(column) {
      case keyColumns.region:
        // Nothing to do
        break
      case keyColumns.ip:
        ipText.copyClicked()
        break
      case keyColumns.remove:
        removeDipDialog.show()
        break
    }
  }

  accRow: NativeAcc.TableRow {
    name: regionName
    item: dedicatedIpRow
    selected: false
    outlineExpanded: false
    outlineLevel: 0
  }

  accRegionCell: NativeAcc.TableCellText {
    name: regionName
    item: nameText
  }
  accIpCell: NativeAcc.TableCellValueText {
    name: dedicatedIpRow.ipAddressColumnName
    value: regionIp
    item: ipText
    copiable: true
    onActivated: ipText.copyClicked()
  }
  accRemoveCell: NativeAcc.TableCellButton {
    name: removeButton
    item: removeImg
    onActivated: dedicatedIpRow.removeClicked()
  }

  function removeClicked() {
    focusCell(keyColumns.remove)
    removeDipDialog.show()
  }

  OverlayDialog {
    id: removeDipDialog

    buttons: [{code: Dialog.Ok, text: removeButton}, Dialog.Cancel]
    contentWidth: 300
    title: uiTr("Remove Dedicated IP")

    DialogMessage {
      width: parent.width
      icon: 'info'
      //: Confirmation prompt to remove a dedicated IP.  %1 is the translated
      //: region name, %2 is an IP address, such as 100.200.100.200.
      text: uiTr("Remove %1 - %2?").arg(regionName).arg(regionIp)
      color: Theme.settings.inputLabelColor
    }

    function show(name, ip, id) {
      visible = true
      focus = true
      open()
    }
    onAccepted: Daemon.removeDedicatedIp(regionId)
  }

  // Highlight cue for the entire row, used for the region column
  HighlightCue {
    anchors.fill: parent
    visible: highlightColumn === keyColumns.region
    inside: true
  }

  FlagImage {
    id: flag
    anchors.verticalCenter: parent.verticalCenter
    x: 6
    countryCode: dedicatedIpRow.regionCountry
  }

  Text {
    id: nameText

    anchors.verticalCenter: parent.verticalCenter
    anchors.left: flag.right
    anchors.leftMargin: 10
    anchors.right: ipText.left
    anchors.rightMargin: 10

    text: regionName
    color: Theme.settings.inputListItemPrimaryTextColor
    font.pixelSize: 12
    elide: Text.ElideRight
  }

  CopiableValueText {
    id: ipText

    anchors.verticalCenter: parent.verticalCenter
    anchors.right: removeImg.left
    anchors.rightMargin: 10

    // The value text itself is not annotated for accessibility; we create a
    // button cell instead since this is inside a table.
    label: ""
    copiable: true
    showKeyboardHighlight: highlightColumn === keyColumns.ip

    text: regionIp
    color: Theme.settings.inputListItemSecondaryTextColor
    font.pixelSize: 12
    elide: Text.ElideRight
  }

  // Highlight cue for the IP cell
  HighlightCue {
    anchors.fill: ipText
    visible: ipText.showKeyboardHighlight
  }

  Image {
    id: removeImg
    height: 16
    width: 16
    source: removeMouseArea.containsMouse ?
      Theme.settings.splitTunnelRemoveApplicationButtonHover :
      Theme.settings.splitTunnelRemoveApplicationButton
    anchors.verticalCenter: parent.verticalCenter
    anchors.right: parent.right
    anchors.rightMargin: 20
  }

  MouseArea {
    id: removeMouseArea
    anchors.fill: removeImg
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor

    onClicked: dedicatedIpRow.removeClicked()
  }

  // Highlight cue for the remove cell
  HighlightCue {
    anchors.fill: removeImg
    visible: highlightColumn === keyColumns.remove
  }
}
