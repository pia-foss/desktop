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
import QtQuick.Window 2.11
import "../../theme"
import "../../daemon"
import "../../client"
import "../../common"

Rectangle {
  color: Theme.dashboard.automationRuleBackgroundColor


  readonly property bool enabled: !!Daemon.state.automationLastTrigger
  readonly property var activeRule: {
    if(enabled) {
      return Daemon.state.automationLastTrigger;
    } else {
      return {
        condition: {
          "ruleType": "ssid",
          "ssid": ""
        },
        action: {
          "connection": "disable"
        }
      }
    }
  }

  readonly property string ruleType: enabled ? activeRule.condition.ruleType : "wired"
  readonly property string ruleName: {
    if(enabled) {
      return Messages.getAutomationRuleName(activeRule.condition);
    }
    return ""
  }
  readonly property string ruleActionName: {
    if(enabled) {
      return Messages.getAutomationActionName(activeRule.action.connection)
    }
    return ""
  }

  Image {
    id: ruleTypeIcon
    x: 5
    anchors.verticalCenter: parent.verticalCenter
    height: 12
    width: 18
    source: Theme.settings.ruleTypeImages[ruleType]

    visible: enabled
  }

  Item {
    id: ruleTextContainer
    anchors.left: ruleTypeIcon.right
    anchors.leftMargin: 5
    anchors.right: configureIcon.left
    anchors.rightMargin: 5
    anchors.top: parent.top
    anchors.bottom: parent.bottom

    readonly property int spacing: 5

    // The text for "Rule > Action" is split up so that if any elision is
    // necessary, it will elide the rule text and keep the action visible.  This
    // can happen for long wireless SSIDs.
    Text {
      id: ruleNameText
      anchors.verticalCenter: parent.verticalCenter

      width: {
        let actionWidth = 2*ruleTextContainer.spacing + chevronText.width + ruleActionText.width
        return Math.min(implicitWidth, ruleTextContainer.width - actionWidth)
      }

      color: Theme.dashboard.textColor
      text: ruleName
      elide: Text.ElideRight
    }

    Text {
      id: chevronText
      anchors.left: ruleNameText.right
      anchors.leftMargin: ruleTextContainer.spacing
      anchors.verticalCenter: parent.verticalCenter
      color: Theme.dashboard.textColor
      text: "»"
    }

    Text {
      id: ruleActionText
      anchors.left: chevronText.right
      anchors.leftMargin: ruleTextContainer.spacing
      anchors.verticalCenter: parent.verticalCenter
      color: Theme.dashboard.textColor
      text: ruleActionName
    }
  }

  Image {
    id: configureIcon
    anchors.right: parent.right
    anchors.rightMargin: 5
    anchors.verticalCenter: parent.verticalCenter
    height: 25
    width: 25
    source: Theme.settings.buttonIcons['configure']
    opacity: configureRuleArea.containsMouse ? 0.6 : 1

    ButtonArea {
      id: configureRuleArea
      anchors.fill: parent
      cursorShape: Qt.PointingHandCursor
      name: uiTr("Open Automation Rule Settings")

      onClicked: {
        wSettings.showSettings()
        wSettings.selectPage('automation')
      }
    }
  }
}
