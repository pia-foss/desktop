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
import QtQuick.Window 2.3
import "../../core"
import "../../theme"
import "../../common"
import "../../client"
import "../../daemon"
import Qt.labs.platform 1.1
import PIA.SplitTunnelManager 1.0
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util

// This is the "add application" row of the split tunnel app list.
// Some of the properties here correspond to properties of SplitTunnelAppRow for
// keyboard nav and accessibility.
Item {
  id: addApplicationRow

  // Shows the cell highlight (this row only has one cell)
  property bool showHighlight

  // Select this row (the column is ignored since this row only has one cell)
  function keyboardSelect() {
    addApplicationDialog.openDialog()
  }

  // Effective column (always 0, this row only has one cell)
  function effectiveColumnFor() {
    return 0  // SplitTunnelAppRow.keyColumns.app
  }

  signal focusCell(int column)

  // Screen reader row annotation
  readonly property NativeAcc.TableRow accRow: NativeAcc.TableRow {
    name: displayName
    item: addApplicationRow
    selected: false
    outlineExpanded: false
    outlineLevel: 0
  }

  // Screen reader cell annotations
  readonly property NativeAcc.TableCellButton accAppCell: NativeAcc.TableCellButton {
    name: displayName
    item: addApplicationText
    onActivated: addApplicationRow.clicked()
  }
  // There is no path or remove cell.
  readonly property NativeAcc.TableCellText accPathCell: null
  readonly property NativeAcc.TableCellButton accRemoveCell: null

  // Localized display name (used in list's accessibility table)
  readonly property string displayName: uiTr("Add Application")

  implicitHeight: 50

  function clicked() {
    focusCell(0)
    keyboardSelect()
  }

  SplitTunnelAppDialog {
    id: addApplicationDialog
  }

  Image {
    height: 20
    width: 20

    source: Theme.settings.splitTunnelAddApplicationButtonHover
    anchors.verticalCenter: parent.verticalCenter
    anchors.left: parent.left
    anchors.leftMargin: 10
  }

  Text {
    id: addApplicationText
    text: addApplicationRow.displayName
    color: Theme.settings.hbarTextColor
    font.pixelSize: 16
    x: 40
    anchors.verticalCenter: parent.verticalCenter
  }
  Rectangle {
    anchors.bottom: parent.bottom
    height: 1
    color: Theme.settings.splitTunnelItemSeparatorColor
    opacity: 0.5
    anchors.left: parent.left
    anchors.right: parent.right
  }

  MouseArea {
    id: addApplicationMouseArea
    anchors.fill: parent
    cursorShape: Qt.PointingHandCursor
    hoverEnabled: true
    onClicked: addApplicationRow.clicked()
  }

  HighlightCue {
    anchors.fill: parent
    visible: showHighlight
    inside: true
  }
}
