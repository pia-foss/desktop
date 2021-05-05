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
import "../inputs"
import PIA.Error 1.0
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc

OverlayDialog {
  id: addRuleDialog
  buttons: [
    { code: Dialog.Ok, clicked: function(){addRuleDialog.dialogAccept()}},
    Dialog.Cancel
  ]
  canAccept: ruleTypesTable.selectedRuleType !== ""
  title: addRow.displayName
  contentWidth: 300
  contentHeight: ruleTypesTable.contentHeight + 34 + actionInput.implicitHeight
  visible: false

  function dialogAccept () {
    var newRule = {
      condition: {
        ruleType: ruleTypesTable.selectedRuleType,
        ssid: ruleTypesTable.selectedRuleSsid
      },
      action: {
        connection: actionInput.currentValue
      }
    }

    var updatedRules = JSON.parse(JSON.stringify(Daemon.settings.automationRules));
    updatedRules.push(newRule)
    Daemon.applySettings({automationRules: updatedRules})
  }

  ColumnLayout {
    width: parent.width
    spacing: 0

    Text {
      text: uiTr("Network")
      color: Theme.dashboard.textColor
      Layout.bottomMargin: 5
    }

    Rectangle {
      id: ruleTypesTable
      readonly property int rowHeight: 30
      color: Theme.settings.hbarBackgroundColor
      border.color: Theme.settings.hbarBottomBorderColor
      border.width: 1
      activeFocusOnTab: true
      Layout.fillWidth: true
      readonly property int contentHeight: ruleTypeRepeater.count * rowHeight + 10
      Layout.preferredHeight: contentHeight
      property string selectedRuleType: ""
      property string selectedRuleSsid: ""  // Only relevant when an SSID rule is selected

      ColumnLayout {
        spacing: 0
        width: parent.width


        Repeater {
          id: ruleTypeRepeater
          model: {
            var existingRules = Daemon.settings.automationRules;
            // model for table that will be built
            var model = [];

            // build options as rules first, so we can compare rule IDs
            var newRules = [
                  {
                    condition: {
                      ruleType: "protectedWifi",
                      ssid: ""
                    },
                    action: {}
                  },
                  {
                    condition: {
                      ruleType: "openWifi",
                      ssid: ""
                    },
                    action: {}
                  },
                  {
                    condition: {
                      ruleType: "wired",
                      ssid: ""
                    },
                    action: {}
                  },
                ];

            // If currently connected to a wifi network, add it to the rules
            for(let i=0; i<Daemon.state.automationCurrentNetworks.length; ++i) {
              newRules.push({
                              condition: {
                                ruleType: "ssid",
                                ssid: Daemon.state.automationCurrentNetworks[i].ssid
                              },
                              action: {}
                            })
            }

            newRules.forEach(function (item) {
              var enabled = true;

              for(var i = 0; i < existingRules.length; i++) {
                if(ruleId(existingRules[i]) === ruleId(item)) {
                  enabled = false;
                  break;
                }
              }

              model.push({
                name: Messages.getAutomationRuleName(item.condition),
                type: item.condition.ruleType,
                ssid: item.condition.ssid,
                enabled: enabled
              })
            })

            return model;
          }

          delegate: Item {
            id: ruleRow
            Layout.fillWidth: true
            Layout.preferredHeight: ruleTypesTable.rowHeight
            property bool ruleEnabled: modelData.enabled
            property string ruleName: modelData.name
            property string ruleType: modelData.type
            property string ruleSsid: modelData.ssid
            function keyboardSelect(column) {
              markRowSelected();
            }

            readonly property bool isSelected: {
              return ruleTypesTable.selectedRuleType === ruleType &&
                ruleTypesTable.selectedRuleSsid === ruleSsid
            }

            function markRowSelected () {
              if(ruleEnabled) {
                ruleTypesTable.selectedRuleType = ruleType;
                ruleTypesTable.selectedRuleSsid = ruleSsid;
              }
            }


            Rectangle {
              color: Theme.settings.inputDropdownSelectedColor
              anchors.fill: parent
              anchors.leftMargin: 1
              anchors.rightMargin: 1
              visible: isSelected
              opacity: 0.4
            }

            Image {
              id: connectionImage
              anchors.verticalCenter: parent.verticalCenter
              x: 6
              height: 12
              width: 18
              opacity: ruleEnabled ? 1 : 0.5
              source: Theme.settings.ruleTypeImages[ruleType]
            }

            Text {
              id: nameTextItem
              text: ruleName

              anchors.verticalCenter: parent.verticalCenter
              x: 30
              readonly property int minSpacing: 10
              width: {
                let ruleExistsWidth = 0
                if(ruleExistsLabel.visible)
                  ruleExistsWidth = ruleExistsLabel.width + 2*minSpacing
                let maxWidth = parent.width - x - ruleExistsWidth
                return Math.min(implicitWidth, maxWidth)
              }
              opacity: ruleEnabled ? 1 : 0.5
              color: Theme.dashboard.textColor
              elide: Text.ElideRight
            }

            Rectangle {
              id: ruleExistsLabel
              visible: !ruleEnabled
              color: Theme.dashboard.pushButtonBackgroundColor
              opacity: 0.6
              anchors.verticalCenter: parent.verticalCenter
              width: labelText.implicitWidth + 6
              height: labelText.implicitHeight + 2
              anchors.left: nameTextItem.right
              anchors.leftMargin: nameTextItem.minSpacing
              radius: 3


              Text {
                id: labelText
                text: uiTr("RULE EXISTS")
                font.pixelSize: 10
                anchors.centerIn: parent
                color: Theme.dashboard.textColor
              }
            }



            RadioButtonArea {
              //: Screen reader annotation used when an automation rule already exists.
              //: Here, "%1" will refer to a network type (Wired Network), etc.
              name: ruleEnabled ? ruleName : uiTr("%1 - rule exists").arg(ruleName)
              anchors.fill: parent
              checked: isSelected
              enabled: ruleEnabled

              onClicked: {
                markRowSelected()
              }
            }

            Rectangle {
              anchors.bottom: parent.bottom
              height: 1
              color: Theme.settings.splitTunnelItemSeparatorColor
              opacity: 0.5
              anchors.left: parent.left
              anchors.right: parent.right
            } // Rectangle separator

            HighlightCue {
              anchors.fill: parent
              visible: highlightColumn === keyColumns.name
              inside: true
            }
          }
        } // Repeater
      } // ColumnLayout
    }

    Text {
      Layout.topMargin: 6
      text: uiTr("Action")
      color: Theme.dashboard.textColor
    }

    ThemedRadioGroup {
      id: actionInput
      Layout.bottomMargin: 5
      function reset () {
        actionInput.setSelection("" + defaultValue);
        currentValue = "" + defaultValue;
      }
      readonly property string defaultValue: "enable"

      verticalOrientation: true
      property string currentValue: "" + defaultValue
      rowSpacing: -10
      model: [{
          "name": Messages.getAutomationActionName("enable"),
          "value": 'enable'
        }, {
          "name": Messages.getAutomationActionName("disable"),
          "value": 'disable',
        }]
      onSelected: function(value){
        currentValue = value;
      }
      Component.onCompleted: {

      }
    }
  }

  function show() {
    visible = true
    focus = true
    ruleTypesTable.selectedRuleType = "";
    ruleTypesTable.selectedRuleSsid = "";
    actionInput.reset();
    open()
  }
}
