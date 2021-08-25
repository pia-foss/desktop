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
import "../../../core"
import "../../../theme"
import "../../../common"
import "../../../client"
import "../../../daemon"
import "../../inputs"
import "../../stores"
import Qt.labs.platform 1.1
import PIA.SplitTunnelManager 1.0
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "../"


SplitTunnelRowBase {
  id: appRow

  property bool showAppIcons: true
  property string appPath
  property string linkTarget

  // Mode [exclude/include]
  property string appMode
  // App name displayed in the cell
  property string appName

  // Index of app in the list
  property string appIndex

  function removeFromSplitTunnelRules() {
    var updatedRules = Daemon.settings.splitTunnelRules.filter(function(rule) {
      return rule.path !== appPath
    })

    Daemon.applySettings({splitTunnelRules: updatedRules});
  }

  function changeSplitTunnelMode(newMode) {
    var updatedRules = JSON.parse(JSON.stringify(Daemon.settings.splitTunnelRules));
    var appRule = updatedRules.find(rule => rule.path === appPath);
    appRule.mode = newMode;

    console.log(JSON.stringify(updatedRules));

    Daemon.applySettings({splitTunnelRules: updatedRules});
  }

  function keyboardShowModePopup() {
    modeDropDown.showPopup()
  }

  // Select a cell in this row with the keyboard.
  keyboardSelect: function(keyboardColumn) {
    switch(keyboardColumn) {
      case keyColumns.app:
        break // Nothing to do for these columns
      case keyColumns.mode:
        keyboardShowModePopup()
        break
      case keyColumns.remove:
        removeFromSplitTunnelRules();
        break
    }
  }

  // Effective column (app rows have all columns)
  effectiveColumnFor: function(column) {
    return column
  }

  // Screen reader row annotation
  accRow: NativeAcc.TableRow {
    name: appName
    item: appRow
    selected: false
    outlineExpanded: false
    outlineLevel: 0
  }

  readonly property var modes: {
    'exclude': 0,
    'include': 1
  }
  readonly property var modesList: [
    'exclude',
    'include'
  ]

  // Screen reader cell annotations
  accAppCell: NativeAcc.TableCellText {
    name: appName
    item: appNameText
  }
  // Although the path is visually below the app name, it's annotated like a
  // second column.  Annotating the whole name+path as one column would generate
  // really long annotations that are hard to navigate.  This approach is used
  // by most of the similar tables in the Mac OS system tools.
  accPathCell: NativeAcc.TableCellText {
    name: appPath
    item: appPathText
  }
  accModeCell: NativeAcc.TableCellDropDownButton {
    name: modeDropDown.displayText
    item: modeDropDown
    onActivated: {
      appRow.focusCell(keyColumns.mode)
      keyboardShowModePopup()
    }
  }
  accRemoveCell: NativeAcc.TableCellButton {
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
    anchors.left: parent.left
    anchors.leftMargin: iconLeftMargin
    width: iconSize
    height: iconSize
    anchors.verticalCenter: parent.verticalCenter
    appPath: appRow.appPath
  }

  Image {
    visible: !showAppIcons
    source: Theme.settings.splitTunnelRuleTypeImages['no-icon']
    opacity: 1.0

    width: iconSize
    height: (width / sourceSize.width) * sourceSize.height
    anchors.left: parent.left
    anchors.leftMargin: iconLeftMargin
    anchors.verticalCenter: parent.verticalCenter
  }

  Text {
    id: appNameText
    anchors.left: parent.left
    anchors.leftMargin: textLeftMargin
    y: 6
    anchors.right: modeDropDown.left
    anchors.rightMargin: 5
    text: appRow.appName
    color: Theme.settings.inputListItemPrimaryTextColor
    font.pixelSize: labelFontSize
    elide: Text.ElideRight
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
        if (linkTarget) {
          return linkTarget.replace(/\//g, '\\')
        }

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
    color: Theme.settings.inputListItemSecondaryTextColor
    elide: Text.ElideLeft
    anchors.right: modeDropDown.left
    anchors.rightMargin: 5
    anchors.left: appNameText.left
  }

  ThemedComboBox {
    id: modeDropDown
    implicitHeight: 38

    width: dropdownWidth
    model: appModeChoices
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    anchors.rightMargin: 50

    // Not a tabstop, navigation occurs in table
    focusOnDismissFunc: function() { appRow.focusCell(keyColumns.mode) }

    currentIndex: {
      // Hack in a dependency on 'model' - the dropdown forgets our bound value
      // for the current index if the model changes (like retranslation), and
      // this forces the binding to reapply
      var dep = model
      if(modes.hasOwnProperty(appMode)) {
        return modes[appMode]
      }
      return -1;
    }

    onActivated: {
      changeSplitTunnelMode(modesList[currentIndex]);
    }
  }

  // Highlight cue for the mode drop-down
  HighlightCue {
    anchors.fill: modeDropDown
    visible: highlightColumn === keyColumns.mode
  }

  Image {
    id: removeApplicationButtonImg
    height: 14
    width: 14

    source: removeApplicationMouseArea.containsMouse ? Theme.settings.splitTunnelRemoveApplicationButtonHover : Theme.settings.splitTunnelRemoveApplicationButton
    anchors.verticalCenter: parent.verticalCenter
    anchors.right: parent.right
    anchors.rightMargin: 20
  }

  MouseArea {
    id: removeApplicationMouseArea
    anchors.fill: removeApplicationButtonImg
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor

    onClicked: appRow.removeClicked()
  }

  // Highlight cue for the remove button
  HighlightCue {
    anchors.fill: removeApplicationButtonImg
    visible: highlightColumn === keyColumns.remove
  }

  // Highlight cue for the app row
  HighlightCue {
    anchors.fill: parent
    visible: highlightColumn === keyColumns.app
    inside: true
  }
}
