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
import "../../javascript/app.js" as App
import "../client"
import "../core"
import "../daemon"
import PIA.NativeHelpers 1.0

Item {
  ColumnLayout {
    anchors.fill: parent
    anchors.margins: 10
    GroupBox {
      title: "Misc"
      Layout.fillWidth: true
      Layout.fillHeight: true
      ColumnLayout {
        id: miscContent
        anchors.fill: parent
        spacing: 2
        Row {
          spacing: 5
          Button {
            text: "Crash Client(!)"
            onClicked: {
              NativeHelpers.crashClient();
            }
          }
          Button {
            text: "Crash Daemon(!)"
            onClicked: {
              Daemon.crash();
            }
          }

          Button {
            text: "Write Diagnostics"
            onClicked: {
              Daemon.writeDiagnostics();
            }
          }
          Button {
            text: "Dummy Logs"
            onClicked: {
              Daemon.writeDummyLogs();
              NativeHelpers.writeDummyLogs();
            }
          }
        }
        Row {
          spacing: 5
          TextField {
            id: dllName
            width: 120
          }
          Button {
            text: "Load DLL"
            onClicked: {
              NativeHelpers.loadDummyCrashDll(dllName.text)
            }
          }
        }
        Row {
          CheckBox {
            id: enableQuickLanguages
            text: "F6 Cycle Languages"
          }
        }
        Row {
          spacing: 5
          visible: Qt.platform.os === 'windows'
          Button {
            text: "Install WFP callout"
            onClicked: NativeHelpers.installWfpCallout()
          }
          Button {
            text: "Reinstall WFP callout"
            onClicked: NativeHelpers.reinstallWfpCallout()
          }
          Button {
            text: "Uninstall WFP callout"
            onClicked: NativeHelpers.uninstallWfpCallout()
          }
          Button {
            text: "Force reboot state"
            onClicked: NativeHelpers.forceWfpCalloutRebootState()
          }
        }
        Row {
          spacing: 5
          Text {
            Layout.fillHeight: true
            visible: Qt.platform.os === 'windows'
            text: "Reinstall status: " + NativeHelpers.reinstallWfpCalloutStatus
          }
          Text {
            Layout.fillHeight: true
            text: "Driver state: " + Daemon.state.netExtensionState
          }
        }
        Row {
          spacing: 5
          Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "RatingSessionCount: <b>" + Daemon.settings.sessionCount + "</b>"
          }
          Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "RatingEnabled: <b>" + Daemon.settings.ratingEnabled + "</b>"
          }
          Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "RatingFlag: " + (Daemon.data.flags.includes("ratings_1") ? "<b>yes</b>" : "<b>no</b>")
          }

          Button {
            text: "Reset: 0 Sessions"
            onClicked: {
              Daemon.applySettings({"sessionCount": 0, "ratingEnabled": true})
            }
          }
          Button {
            text: "Reset: 8 Sessions"
            onClicked: {
              Daemon.applySettings({"sessionCount": 8, "ratingEnabled": true})
            }
          }
        }
        Row {

          spacing: 5
          Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "SurveySessionCount: <b>" + Daemon.settings.successfulSessionCount+ "</b>"
          }
          Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "SurveyEnabled: <b>" + Daemon.settings.surveyRequestEnabled + "</b>"
          }

          Button {
            text: "Reset: 0 Sessions"
            onClicked: {
              Daemon.applySettings({"successfulSessionCount": 0, "surveyRequestEnabled": true})
            }
          }
          Button {
            text: "Reset: 12 Sessions"
            onClicked: {
              Daemon.applySettings({"successfulSessionCount": 12, "surveyRequestEnabled": true})
            }
          }
        }
        Row {
          spacing: 5
          Button {
            Layout.alignment: Qt.AlignTop|Qt.AlignRight
            text: "Check for updates"
            onClicked: Daemon.refreshMetadata()
          }
          Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "Refreshes client updates, notification data, dedicated IPs, etc."
          }
        }
        Row {
          spacing: 5
          Button {
            Layout.alignment: Qt.AlignTop|Qt.AlignRight
            text: "Send events"
            onClicked: Daemon.sendServiceQualityEvents()
          }
          Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "Send service quality events early (if events are enabled)"
          }
        }

        Row {
          // spacer
          Layout.fillHeight: true
          Layout.fillWidth: true
        }
      }
    }
    GroupBox {
      title: "Updates"
      Layout.fillWidth: true
      GridLayout {
        id: updateChannels
        columns: 3
        width: parent.width
        UpdateChannel {
          label: "GA"
          channelPropName: "updateChannel"
        }
        UpdateChannel {
          label: "Beta"
          channelPropName: "betaUpdateChannel"
        }
      }
    }
  }

  Shortcut {
    sequence: enableQuickLanguages.checked ? "F6" : ""
    context: Qt.ApplicationShortcut
    onActivated: {
      var i=Client.state.languages.findIndex(function(lang){return lang.locale === Client.settings.language})
      i = (i + 1) % Client.state.languages.length
      console.info('lang ' + i + ' ' + Client.state.languages[i].locale)
      Client.applySettings({language: Client.state.languages[i].locale})
    }
  }
}
