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

Item {
  id: appRow

  property bool showAppIcons: true
  property string appPath
  // App name displayed in the cell
  property string appName

  // Column index of cell to highlight within this row - -1 for none.
  property int highlightColumn: -1

  // Indices of the keyboard navigation columns in this row.
  // App rows are modeled with two columns: the app and the remove button.
  // The 'app' and 'path' columns don't have any selectable action, but they're
  // split up for a few reasons:
  // - Describing the whole row with one accessibility annotation would require
  //   far too much text (app name, path, remove action)
  // - It would probably be surprising that selecting the entire row would
  //   remove it.  This is a destructive action, we don't want users doing this
  //   accidentally.
  // - We expect to add more controls in the future (rule types, etc.).  This
  //   model extends naturally to more columns.
  readonly property var keyColumns: ({
    app: 0,
    path: 1,
    remove: 2
  })

  function removeFromSplitTunnelRules() {
    var updatedRules = Daemon.settings.splitTunnelRules.filter(function(rule) {
      return rule.path !== appPath
    })

    Daemon.applySettings({splitTunnelRules: updatedRules});
  }

  // Select a cell in this row with the keyboard.
  function keyboardSelect(keyboardColumn) {
    switch(keyboardColumn) {
      case keyColumns.app:
      case keyColumns.path:
        break // Nothing to do for these columns
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
    name: appName
    item: appRow
    selected: false
    outlineExpanded: false
    outlineLevel: 0
  }

  // Screen reader cell annotations
  readonly property NativeAcc.TableCellText accAppCell: NativeAcc.TableCellText {
    name: appName
    item: appNameText
  }
  // Although the path is visually below the app name, it's annotated like a
  // second column.  Annotating the whole name+path as one column would generate
  // really long annotations that are hard to navigate.  This approach is used
  // by most of the similar tables in the Mac OS system tools.
  readonly property NativeAcc.TableCellText accPathCell: NativeAcc.TableCellText {
    name: appPath
    item: appPathText
  }
  readonly property NativeAcc.TableCellButton accRemoveCell: NativeAcc.TableCellButton {
    //: Screen reader annotation for the "remove" button ("X" icon) next to a
    //: split tunnel app rule.  (Should be labeled like a normal command
    //: button.)
    name: uiTr("Remove")
    item: removeApplicationButtonImg
    onActivated: appRow.removeClicked()
  }

  function removeClicked() {
    focusCell(keyColumns.remove)
    keyboardSelect(keyColumns.remove)
  }

  implicitHeight: 50
  SplitTunnelAppIcon {
    visible: showAppIcons
    x: (appNameText.x - width) / 2
    width: (Qt.platform.os === 'windows') ? 32 : 30
    height: width
    anchors.verticalCenter: parent.verticalCenter
    appPath: appRow.appPath
  }

  Text {
    id: appNameText
    x: showAppIcons ? 40 : 5
    y: 4
    text: appRow.appName
    color: Theme.settings.hbarTextColor
    font.pixelSize: 16
  }

  Text{
    id: appPathText
    y: 25
    font.pixelSize: 11
    text: {
      if(Qt.platform.os === 'windows') {
        // There isn't anything meaningful we can display for a UWP app
        if(appPath.startsWith("uwp:"))
          return uiTr("Microsoft Store app")

        // Normalize to backslashes on Windows.  The backend tolerates
        // either, but slashes look out of place on Windows.
        return appPath.replace(/\//g, '\\')
      }

      if(Qt.platform.os === 'osx') {
        if(appPath === SplitTunnelManager.macWebkitFrameworkPath) {
          return uiTr("App Store, Mail, Safari and others")
        }
      }

      return appPath
    }
    color: Theme.settings.inputDropdownTextDisabledColor
    elide: Text.ElideLeft
    anchors.right: parent.right
    anchors.rightMargin: 60
    anchors.left: appNameText.left
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
    id: removeApplicationButtonImg
    height: 16
    width: 16

    source: removeApplicationMouseArea.containsMouse ? Theme.settings.splitTunnelRemoveApplicationButtonHover : Theme.settings.splitTunnelRemoveApplicationButton
    anchors.verticalCenter: parent.verticalCenter
    anchors.right: parent.right
    anchors.rightMargin: 25

    MouseArea {
      id: removeApplicationMouseArea
      anchors.fill: parent
      hoverEnabled: true
      cursorShape: Qt.PointingHandCursor

      onClicked: appRow.removeClicked()
    }

    // Highlight cue for the remove button
    HighlightCue {
      anchors.fill: parent
      visible: highlightColumn === keyColumns.remove
    }
  }

  // Highlight cue for the app row
  HighlightCue {
    anchors.fill: parent
    visible: highlightColumn === keyColumns.app
    inside: true
  }
}
