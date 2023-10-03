// Copyright (c) 2023 Private Internet Access, Inc.
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
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util
import PIA.NativeHelpers 1.0
import "../"



TableBase {
  id: automationRulesTable
  keyboardRow: {
    return buildRowId("add")
  }
  color: "transparent"
  border.color: "transparent"
  property bool enabled: true
  keyboardColumnCount: 3
  verticalScrollPolicy: ScrollBar.AsNeeded
  contentHeight: automationRulesLayout.implicitHeight
  hideTableFocusCue: true

  //: Screen reader annotation for the name of the table
  label: uiTr("Automation Rules")

  // Remove button name, used by the Remove button in rule rows and by the
  // shared dialog below
  //: Screen reader annotation for the remove button in the automation list table
  readonly property string removeButtonText: uiTranslate("AutomationRuleRow", "Remove")

  // Create a string key representing an automation rule
  // - rule is the rule object with {"condition": {"ruleType: "", "ssid": ""}}
  //   or {"ruleType: "", "ssid": ""}
  function ruleId(rule) {
    var key = "";
    if(rule.condition) {
      rule = rule.condition;
    }

    if(rule.ruleType && typeof(rule.ruleType) === "string") {
      key += rule.ruleType;

      if(rule.ruleType === "ssid" && rule.ssid) {
        key += "/" + NativeHelpers.encodeUriComponent(rule.ssid);
      }
    } else {
      console.warn("Unable to generate unique key for rule");
    }

    return key;
  }

  function ruleRowId (rule) {
    return buildRowId("rule", ruleId(rule));
  }

  ColumnLayout {
    id: automationRulesLayout

    spacing: 0
    width: parent.width
    enabled: automationRulesTable.enabled
    opacity: enabled ? 1.0 : 0.6

    Repeater {
      id: automationRuleRepeater
      model: Daemon.settings.automationRules
      delegate: AutomationRuleRow {
        Layout.fillWidth: true
        ruleCondition: modelData.condition
        ruleAction: modelData.action
        parentTable: automationRulesTable
        rowId: ruleRowId(modelData.condition)
      }
    }

  }

  property NativeAcc.TableColumn conditionColumn: NativeAcc.TableColumn {
    //: Name of the "Condition" table column, in the Automation table
    name: uiTr("Condition")
    item: automationRulesTable
  }

  property NativeAcc.TableColumn actionColumn: NativeAcc.TableColumn {
    //: Name of the "Action" table column, in the automation table
    name: uiTr("Action")
    item: automationRulesTable
  }

  property NativeAcc.TableColumn removeColumn: NativeAcc.TableColumn {
    //: Name of the "Remove" table column, containing the remove button, in the automation table
    name: uiTr("Remove")
    item: automationRulesTable
  }

  accColumns: [{
      "property": "accConditionCell",
      "column": conditionColumn
    }, {
      "property": "accActionCell",
      "column": actionColumn
    }, {
      "property": "accRemoveCell",
      "column": removeColumn
    }]

  accTable: {
    var childrenDep = automationRuleRepeater.children
    var table = []

    // Rule row
    for (var i = 0; i < automationRuleRepeater.count; ++i) {
      var ruleItem = automationRuleRepeater.itemAt(i)
      if (!ruleItem)
        continue

      table.push({
                   "row": ruleRowId(ruleItem.ruleCondition),
                   "name": "ssid",
                   "item": ruleItem
                 })
    }

    return table
  }

  function promptRemoveRule(textName, removeRuleId) {
    removeRuleDialog.textName = textName
    removeRuleDialog.removeRuleId = removeRuleId
    removeRuleDialog.show()
  }

  // Dialog used to confirm removing a rule.  This must be outside of the
  // AutomationRuleRow object since removing a rule destroys the rows (by
  // causing a list rebuild), which can cause issues in the UI (mouseover
  // events, like InfoTips, stop working for some reason)
  OverlayDialog {
    id: removeRuleDialog

    property string textName
    property string removeRuleId

    buttons: [{code: Dialog.Ok, text: removeButtonText}, Dialog.Cancel]
    contentWidth: 300
    title: uiTranslate("AutomationRuleRow", "Remove Automation Rule")

    DialogMessage {
      width: parent.width
      icon: 'info'
      //: Message shown when removing an automation rule.  "%1" is the name of a
      //: rule, which can be a standard rule like "Protected Wi-Fi",
      //: "Open Wi-Fi", or "Wired", or it can be a network SSID for a rule
      //: created for a specific wireless network.  For example -
      //: "Remove Protected Wi-Fi?", or "Remove Steve's House?"
      text: uiTranslate("AutomationRuleRow", "Remove %1?").arg(removeRuleDialog.textName)
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
        if(ruleId(updatedRules[i].condition) === removeRuleId) {
          index = i;
        }
      }

      if(index >= 0) {
        updatedRules.splice(index, 1);
        Daemon.applySettings({automationRules: updatedRules});
      }
    }
  }
}
