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
import "."
import ".."
import "../inputs"
import "../stores"
import "../../client"
import "../../daemon"
import "../../common"
import "../../theme"
import "../../core"
import PIA.BrandHelper 1.0
import PIA.NativeHelpers 1.0

Page {
  ColumnLayout {
    anchors.fill: parent

    Item {
      Layout.fillWidth: true
      Layout.preferredHeight: 30
      RowLayout {
        Text {
          text: uiTr("Protocol: ")
          color: Theme.settings.inputLabelColor
        }

        ThemedRadioGroup {
          id: methodInput

          readonly property DaemonSetting daemonMethod: DaemonSetting {
            name: "method"
            onCurrentValueChanged: methodInput.setSelection(currentValue)
          }

          verticalOrientation: false
          columnSpacing: 10
          model: [{
              "name": "OpenVPN®",
              "value": 'openvpn'
            }, {
              "name": "WireGuard®",
              "value": 'wireguard',
              disabled: !Daemon.state.wireguardAvailable
            }]
          onSelected: {
            daemonMethod.currentValue = value;
          }
          Component.onCompleted: setSelection(daemonMethod.currentValue)
        }

        InfoTip {
          Layout.leftMargin: 2
          showBelow: true
          //: Description for OpenVPN shown as tip describing available protocols.
          readonly property string openvpnTipText: uiTr("Reliable, stable and secure protocol with 18+ years of open source development.")
          //: Description for WireGuard shown as tip describing available protocols.
          readonly property string wireguardTipText: uiTr("Newer, more efficient protocol with the potential for increased performance.")
          tipText: {
            if(Daemon.state.wireguardAvailable)
              return "<strong>OpenVPN®:</strong> " + openvpnTipText + "<br/><strong>WireGuard®:</strong> " + wireguardTipText
            else
              return SettingsMessages.wgRequiresWindows8
          }
          accessibleText: {
            if(Daemon.state.wireguardAvailable)
              return "OpenVPN®: " + openvpnTipText + "\nWireGuard®: " + wireguardTipText
            else
              return SettingsMessages.wgRequiresWindows8
          }
          icon: icons.settings
        }
      }
    }

    RowLayout{
      Layout.fillWidth: true
      Layout.preferredHeight: 15
      Layout.topMargin: 3

      Rectangle {
        Layout.alignment: Qt.AlignVCenter
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Theme.settings.inputLabelColor
        opacity: 0.1
      }
      Text {
        Layout.alignment: Qt.AlignVCenter
        text: {
          switch(Daemon.settings.method) {
          case 'openvpn':
            return uiTr("OpenVPN® Settings")
          case 'wireguard':
            return uiTr("WireGuard® Settings")
          default:
            return ""
          }
        }
        color: Theme.settings.inputLabelColor
        opacity: 0.6
      }

      Rectangle {
        Layout.alignment: Qt.AlignVCenter
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Theme.settings.inputLabelColor
        opacity: 0.1
      }
    }


    Item {
      Layout.fillWidth: true
      Layout.fillHeight: true
      Layout.topMargin: 5

      OpenVpnConnection {
        visible: Daemon.settings.method === 'openvpn'
        anchors.fill: parent
      }

      WireguardConnection{
        visible: Daemon.settings.method === 'wireguard'
        anchors.fill: parent
      }
    }

    RowLayout {
      TextLink {
        text: uiTr("Trademarks")
        underlined: true
        onClicked: {
          trademarkDialog.open();
        }
      }

      Item {
        Layout.fillWidth: true
      }

      TextLink {
        id: infoLink
        Layout.alignment: Qt.AlignRight
        text: uiTr("What do these settings mean?")
        link: BrandHelper.getBrandParam("encryptionSettingsLink")
        visible: Daemon.settings.method === 'openvpn'
        underlined: true
      }
    }
  }
  OverlayDialog {
    id: trademarkDialog
    buttons: [Dialog.Ok]
    title: uiTr("Trademarks")

    ColumnLayout {
      StaticText {
        Layout.alignment: Qt.AlignLeft
        Layout.minimumWidth: 200
        Layout.maximumWidth: 400
        wrapMode: Text.WordWrap
        text: uiTr("All product and company names are trademarks™ or registered® trademarks of their respective holders. Use of them does not imply any affiliation with or endorsement by them.") + "\n" +
              "\n" +
              uiTr("OpenVPN® is a trademark of OpenVPN Technologies, Inc.") + "\n" +
              uiTr("WireGuard® is a trademark of Jason A. Donenfeld, an individual.")
        color: Theme.settings.inputLabelColor
      }
    }
  }
}
