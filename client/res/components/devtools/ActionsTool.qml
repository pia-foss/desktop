// Copyright (c) 2020 Private Internet Access, Inc.
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
      title: "Connect Actions"
      Layout.fillWidth: true
      Layout.preferredHeight: 120
      ColumnLayout {
        anchors.fill: parent
        spacing: 2
        Row {
          spacing: 5
          Button {
            text: "Connect"
            onClicked:  {
              Daemon.connectVPN()
            }
          }
          Button {
            text: "Disconnect"
            onClicked:  {
              Daemon.disconnectVPN()
            }
          }
          ComboBox {
            textRole: "name"
            currentIndex: {

              switch(Daemon.settings.killswitch) {
              case 'off': return 0
              case 'auto': return 1
              case 'on': return 2
              }
            }
            model: ListModel {
              ListElement { value: "off"; name: "Killswitch - Off" }
              ListElement { value: "auto"; name: "Killswitch - Auto" }
              ListElement { value: "on"; name: "Killswitch - On" }
            }
            onActivated: function(index){
              var key = model.get(index).value
              console.log("Current Key", key)
              Daemon.applySettings({'killswitch': key});
            }

          }
          Button {
            text: "Clear recents"
            onClicked: {
              Client.applySettings({recentLocations: []})
            }
          }
        }
        Row {
          Text {
            text: "State: " + Daemon.state.connectionState + "  |   Traffic: U " + Daemon.state.bytesSent + " D " + Daemon.state.bytesReceived + "  |  Protocol: " + Daemon.settings.protocol
          }
        }
      }
    }

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
          Text {
            text: "User: " + Daemon.account.username
            anchors.verticalCenter: parent.verticalCenter
          }
          Button {
            text: "Reset User"
            onClicked: {
              // TODO: Read default user from filesystem and
            }
          }
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
          visible: Qt.platform.os === 'osx'
          Button {
            text: "Open Security Prefs"
            onClicked: NativeHelpers.openSecurityPreferencesMac()
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
          Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "<b>Restart Client after reset</b>"
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
        // Spacer (put the Check for Updates button at the top-right of the
        // group box)
        Item {
          Layout.fillWidth: true
        }
        Button {
          Layout.alignment: Qt.AlignTop|Qt.AlignRight
          text: "Check for updates"
          onClicked: Daemon.refreshUpdate()
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
