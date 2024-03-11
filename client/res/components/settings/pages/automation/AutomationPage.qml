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
import "."
import "../"
import "../../"
import "../../../"
import "../../../core/"
import "../../inputs"
import "../../stores"
import "../../../client"
import "../../../daemon"
import "../../../settings"
import "../../../common"
import "../../../common/regions"
import "../../../theme"
import PIA.BrandHelper 1.0


Page {
  GridLayout {
    anchors.fill: parent
    columns: 2
    columnSpacing: Theme.settings.controlGridDefaultColSpacing
    rowSpacing: Theme.settings.controlGridDefaultRowSpacing

    CheckboxInput {
      Layout.columnSpan: 2
      id: enableAutomationCheckbox
      label: uiTr("Connection Automation")
      desc: {
        //: Text displayed in a tooltip for connection automation
        return uiTr("Create rules to automatically connect or disconnect the VPN when you join a particular network.")
      }
      enabled: true
      setting: DaemonSetting {
        name: 'automationEnabled'
      }
    }

    StaticText {
      Layout.topMargin: 5
      color: Theme.dashboard.textColor
      text: uiTr("Your Automation Rules")
    }

    // The button's size affects the layout when it's present.  Wrap this in a
    // same-size Item so we still have a placeholder at that size when the
    // button is not visible; that avoids the layout jumping as the button
    // appears.
    Item {
      width: addAutomationRuleButton.width
      height: addAutomationRuleButton.height
      Layout.alignment: Qt.AlignRight
      SettingsButton {
        id: addAutomationRuleButton
        text: uiTranslate("AutomationAddRuleRow", "Add Automation Rule")
        icon: "add"
        visible: Daemon.settings.automationRules.length > 0
        enabled: Daemon.settings.automationEnabled
        onClicked: {
          addRuleDialog.show();
        }
      }
    }

    Rectangle {
      visible: Daemon.settings.automationRules.length === 0
      Layout.columnSpan: 2
      Layout.fillWidth: true
      Layout.preferredHeight: 220
      Layout.topMargin: 5
      color: Theme.settings.inlayRegionColor
      radius: 20


        Rectangle {
          id: automationIcon
          anchors.horizontalCenter: parent.horizontalCenter
          width: 60
          height: 60
          anchors.top: parent.top
          anchors.topMargin: 40
          radius: 30
          color: "transparent"
          border.color: Theme.settings.vbarTextColor
          opacity: 0.2

          Image {
            anchors.centerIn: parent
            // Use the active state for dark theme, inactive for light theme
            source: Theme.dark ? Theme.settings.pageImages['automation'][0] : Theme.settings.pageImages['automation'][1]
            width: 40
            height: 40
          }
        }

        StaticText {
          id: automationText
          text: uiTr("You don't have any automation rules")
          color: Theme.settings.vbarTextColor
          opacity: 0.7
          font.pixelSize: 14
          anchors.horizontalCenter: parent.horizontalCenter
          anchors.top: automationIcon.bottom
          anchors.topMargin: 10
        }

        SettingsButton {
          id: defaultAddButton
          text: uiTranslate("AutomationAddRuleRow", "Add Automation Rule")
          icon: "add"
          anchors.top: automationText.bottom
          anchors.topMargin: 10
          enabled: Daemon.settings.automationEnabled
          anchors.horizontalCenter: parent.horizontalCenter
          onClicked: {
            addRuleDialog.show();
          }
        }

    }

    Item {
      visible: Daemon.settings.automationRules.length === 0
      Layout.columnSpan: 2
      Layout.fillWidth: true
      Layout.fillHeight: true
    }

    AutomationTable {
      visible: Daemon.settings.automationRules.length > 0
      id: automationTable
      Layout.columnSpan: 2
      Layout.fillWidth: true
      Layout.fillHeight: true
      enabled: Daemon.settings.automationEnabled
    }

    AutomationAddRuleDialog {
      id: addRuleDialog
    }
  }

}
