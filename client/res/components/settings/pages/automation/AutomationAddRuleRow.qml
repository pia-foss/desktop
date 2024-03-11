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
import QtQuick.Window 2.3
import "../"

import "../../../core"
import "../../../theme"
import "../../../common"
import "../../../client"
import "../../../daemon"
import "../../inputs"
import PIA.Error 1.0
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util

AutomationRowBase {
  id: addRow

  implicitHeight: 45
  readonly property string displayName: uiTr("Add Automation Rule")

  effectiveColumnFor: function(column){return keyColumns.condition}
  keyboardSelect: function(column){
    clicked();
  }

  accRow: NativeAcc.TableRow {
    name: displayName
    item: addRow
    selected: false
    outlineExpanded: false
    outlineLevel: 0
  }

  accConditionCell: NativeAcc.TableCellButton {
    name: displayName
    item: addText
    onActivated: addRow.clicked()
  }
  accActionCell: null
  accRemoveCell: null

  function clicked () {
    addRow.focusCell(0)
    addRuleDialog.show()
  }

  Image {
    height: 15
    width: 15

    source: Theme.settings.splitTunnelAddApplicationButtonHover
    anchors.verticalCenter: parent.verticalCenter
    anchors.left: parent.left
    anchors.leftMargin: 11
  }

  Text {
    id: addText
    text: addRow.displayName
    color: Theme.settings.inputListItemPrimaryTextColor
    font.pixelSize: 12
    x: 35
    anchors.verticalCenter: parent.verticalCenter
  }

  MouseArea {
    id: addApplicationMouseArea
    anchors.fill: parent
    cursorShape: Qt.PointingHandCursor
    hoverEnabled: true
    onClicked: addRow.clicked()
  }

  HighlightCue {
    anchors.fill: parent
    visible: highlightColumn >= 0 // There's only one cell in this row
    inside: true
  }


  AutomationAddRuleDialog {
    id: addRuleDialog
  }

}
