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

AutomationRowBase {
  id: ruleRow

  implicitHeight: 45

  property var ruleCondition
  property var ruleAction
  property int ruleIndex

  //: Screen reader annotation for the remove button in the automation list table
  readonly property string removeButton: uiTr("Remove")

  readonly property string textName: Messages.getAutomationRuleName(ruleCondition)

  readonly property var modes: {
    'enable': 0,
    'disable': 1
  }
  readonly property var modesList: [
    'enable',
    'disable'
  ]

  effectiveColumnFor: function(column){return column}
  keyboardSelect: function(column) {
    switch(column) {
      case keyColumns.condition:
        break;
      case keyColumns.action:
        actionDropDown.showPopup();
        break;
      case keyColumns.remove:
        removeRuleDialog.show()
        break
    }
  }

  function removeClicked() {
    focusCell(keyColumns.remove)
    keyboardSelect(keyColumns.remove)
  }

  function changeRuleAction(value) {
    var updatedRules = JSON.parse(JSON.stringify(Daemon.settings.automationRules));
    for(var i = 0; i < updatedRules.length; i++) {
      if(ruleId(updatedRules[i].condition) === ruleId(ruleCondition)) {
        updatedRules[i].action.connection = value;
      }
    }
    Daemon.applySettings({automationRules: updatedRules});
  }

  Image {
    id: connectionImage
    anchors.verticalCenter: parent.verticalCenter
    x: 10
    height: 12
    width: 18
    source: Theme.settings.ruleTypeImages[ruleCondition.ruleType]
  }

  Text {
    id: nameText

    anchors.verticalCenter: parent.verticalCenter
    anchors.left: parent.left
    anchors.leftMargin: 35

    text: textName
    color: Theme.settings.inputListItemPrimaryTextColor
    font.pixelSize: 12
    elide: Text.ElideRight
  }

  Rectangle {
    id: activeLabel
    visible: Daemon.settings.automationEnabled &&
      !! Daemon.state.automationCurrentMatch &&
      ruleId(Daemon.state.automationCurrentMatch.condition) === ruleId(ruleCondition)

    color: Theme.settings.inputPrivacySelectedBackgroundColor
    anchors.verticalCenter: parent.verticalCenter
    width: labelText.implicitWidth + 6
    height: labelText.implicitHeight + 2
    anchors.left: nameText.right
    anchors.leftMargin: 5
    opacity: 0.8
    radius: 3


    Text {
      id: labelText
      //: "ACTIVE" Indicates that you are connected to the network corresponding to this rule item
      text: uiTr("ACTIVE")
      font.pixelSize: 11
      font.bold: true
      anchors.centerIn: parent
      color: Theme.settings.inputPrivacyTextColor
    }
  }

  ThemedComboBox {
    id: actionDropDown
    width: 145
    height: 24
    model: [
      {name: Messages.getAutomationActionName("enable")},
      {name: Messages.getAutomationActionName("disable")},
    ]
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    anchors.rightMargin: 45

    // Not a tabstop, navigation occurs in table
    focusOnDismissFunc: function() {
      ruleRow.focusCell(keyColumns.mode)
    }

    currentIndex: {
      // Hack in a dependency on 'model' - the dropdown forgets our bound value
      // for the current index if the model changes (like retranslation), and
      // this forces the binding to reapply
      var dep = model
      if(modes.hasOwnProperty(ruleAction.connection)) {
        return modes[ruleAction.connection]
      }
      return -1;
    }

    onActivated: {
      console.log("Changed", currentIndex);
      changeRuleAction(modesList[currentIndex]);
    }
  }

  Image {
    id: removeRuleButtonImg
    height: 16
    width: 16

    source: removeRuleMouseArea.containsMouse ? Theme.settings.splitTunnelRemoveApplicationButtonHover : Theme.settings.splitTunnelRemoveApplicationButton
    anchors.verticalCenter: parent.verticalCenter
    anchors.right: parent.right
    anchors.rightMargin: 20
  }

  MouseArea {
    id: removeRuleMouseArea
    anchors.fill: removeRuleButtonImg
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor

    onClicked: ruleRow.removeClicked()
  }

  HighlightCue {
    anchors.fill: removeRuleButtonImg
    visible: highlightColumn === keyColumns.remove
  }


  accRow: NativeAcc.TableRow {
    name: textName
    item: ruleRow
    selected: false
    outlineExpanded: false
    outlineLevel: 0
  }

  OverlayDialog {
    id: removeRuleDialog

    buttons: [{code: Dialog.Ok, text: removeButton}, Dialog.Cancel]
    contentWidth: 300
    title: uiTr("Remove Automation Rule")

    DialogMessage {
      width: parent.width
      icon: 'info'
      //: Message shown when removing an automation rule.  "%1" is the name of a
      //: rule, which can be a standard rule like "Protected Wi-Fi",
      //: "Open Wi-Fi", or "Wired", or it can be a network SSID for a rule
      //: created for a specific wireless network.  For example -
      //: "Remove Protected Wi-Fi?", or "Remove Steve's House?"
      text: uiTr("Remove %1?").arg(textName)
      color: Theme.settings.inputLabelColor
    }

    function show() {
      visible = true
      focus = true
      open()
    }
    onAccepted: {
      var updatedRules = JSON.parse(JSON.stringify(Daemon.settings.automationRules));
      var index = -1
      for(var i = 0; i < updatedRules.length; i++) {
        if(ruleId(updatedRules[i].condition) === ruleId(ruleCondition)) {
          index = i;
        }
      }

      if(index >= 0) {
        updatedRules.splice(index, 1);
        Daemon.applySettings({automationRules: updatedRules});
      }
    }
  }


//  property var accConditionCell
//  property var accActionCell
//  property var accRemoveCell

  accConditionCell: NativeAcc.TableCellText {
    name: textName
    item: nameText
  }
  accActionCell: NativeAcc.TableCellDropDownButton {
    name: actionDropDown.displayText
    item: actionDropDown
  }
  accRemoveCell: NativeAcc.TableCellButton {
    name: removeButton
    item: removeRuleButtonImg
    onActivated: ruleRow.removeClicked()
  }

  HighlightCue {
    anchors.fill: actionDropDown
    visible: highlightColumn === keyColumns.action
  }

  HighlightCue {
    anchors.fill: parent
    visible: highlightColumn === keyColumns.condition
    inside: true
  }
}
