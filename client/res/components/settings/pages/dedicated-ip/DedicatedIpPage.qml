// Copyright (c) 2026 Private Internet Access, Inc.
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
import "../../inputs"
import "../../stores"
import "../../../client"
import "../../../daemon"
import "../../../settings"
import "../../../common"
import "../../../core"
import "../../../common/regions"
import "../../../theme"

import "../"
import PIA.BrandHelper 1.0


Page {
  id: dedicatedIpPage

  readonly property bool dedicatedIpExists: Array.isArray(Daemon.state.dedicatedIpLocations) && Daemon.state.dedicatedIpLocations.length > 0

  // The OpenVPN port for a dedicated IP has its own settings, separate from
  // the Remote Port setting on the Protocols page.  The TCP ports are listed
  // when the protocol is TCP, or when the Shadowsocks proxy is enabled, since
  // OpenVPN always connects over TCP through that proxy.
  readonly property bool usingTcpTransport: (Daemon.settings.proxyType === "shadowsocks" && Daemon.settings.proxyEnabled) || Daemon.settings.protocol === "tcp"
  readonly property var dipPortChoices: usingTcpTransport ? Daemon.state.dedicatedIpOpenvpnTcpPortChoices : Daemon.state.dedicatedIpOpenvpnUdpPortChoices

  GridLayout {
    anchors.fill: parent
    columns: 2
    columnSpacing: Theme.settings.controlGridDefaultColSpacing
    rowSpacing: Theme.settings.controlGridDefaultRowSpacing

    Item {
      Layout.fillWidth: true
      Layout.columnSpan: 2
      Layout.preferredHeight: dipDescription.y + dipDescription.implicitHeight

      Image {
        id: dipIcon
        source: Theme.settings.dedicatedIpPageImage
        width: sourceSize.width/sourceSize.height*height
        height: 24
      }

      InputLabel {
        id: dipLabel
        anchors.left: dipIcon.right
        anchors.leftMargin: 10
        text: uiTr("Dedicated IP")
      }

      MessageWithLinks {
        id: dipDescription
        anchors.left: dipLabel.left
        anchors.top: dipLabel.bottom
        anchors.topMargin: 8
        anchors.right: parent.right
        message: uiTr("Secure your remote connections to any asset with a dedicated IP from a country of your choice.  During your subscription, this IP will be yours and yours alone, protecting your data transfers with the strongest encryption out there.")
        links: []
        wrapMode: Text.WordWrap
        color: Theme.settings.inputDescriptionColor

        linkColor: Theme.settings.inputDescLinkColor
        linkHoverColor: Theme.settings.inputDescLinkHoverColor
        linkFocusColor: Theme.settings.backgroundColor
        linkFocusBgColor: Theme.popup.focusCueColor
      }
    }

    Item {
      width: 1
      height: 5
    }

    SettingsButton {
      text: uiTr("Get Your Dedicated IP")
      visible: !dedicatedIpExists
      link: BrandHelper.getBrandParam("getDedicatedIpLink")
    }

    Item {
      width: 1
      visible: !dedicatedIpExists
      height: 15
    }


    DedicatedIpList {
      id: dedicatedIpList
      visible: dedicatedIpExists
      Layout.columnSpan: 2
      Layout.fillWidth: true
      // Sized to its rows so the port dropdown below sits right under the IP
      Layout.preferredHeight: contentHeight
    }

    DropdownInput {
      Layout.columnSpan: 2
      //: Label for the dropdown that selects the OpenVPN remote port used for
      //: dedicated IP connections.
      label: uiTr("OpenVPN Remote Port")
      visible: dedicatedIpExists && Daemon.settings.method === "openvpn" &&
        dedicatedIpPage.dipPortChoices.length > 0
      // With a single port there is nothing to choose
      enabled: dedicatedIpPage.dipPortChoices.length > 1
      // There is no "Default" choice.  The selected port is the one the
      // daemon uses for the connection, which can differ from the setting (see
      // DaemonState::dedicatedIpOpenvpnUdpPort).  The setting is only written
      // when the user picks a port.
      setting: Setting {
        readonly property string name: dedicatedIpPage.usingTcpTransport ? "dipRemotePortTCP" : "dipRemotePortUDP"
        sourceValue: dedicatedIpPage.usingTcpTransport ?
          Daemon.state.dedicatedIpOpenvpnTcpPort : Daemon.state.dedicatedIpOpenvpnUdpPort
        onCurrentValueChanged: {
          if(currentValue !== undefined && currentValue !== sourceValue) {
            var param = {}
            param[name] = currentValue
            Daemon.applySettings(param, function(error) {
              if(error) {
                console.error("Error applying setting " + name + ":", error)
                currentValue = sourceValue
              }
            })
          }
        }
      }
      model: dedicatedIpPage.dipPortChoices.map(function(x) { return { name: x.toString(), value: x } })
    }

    Rectangle {
      visible: !dedicatedIpExists
      color: Theme.settings.inlayRegionColor
      Layout.columnSpan: 2
      Layout.fillWidth: true
      Layout.preferredHeight: 134 + dedicatedIpAdd.errorHeight + dedicatedIpAdd.messageHeight
      radius: 5

      DedicatedIpAdd {
        id: dedicatedIpAdd
        anchors.fill: parent
        anchors.margins: 20

        Connections {
          target: dedicatedIpPage
          function onDedicatedIpExistsChanged () {
            if(dedicatedIpExists)
              dedicatedIpAdd.clearText();
          }
        }
      }
    }

    Item {
      Layout.columnSpan: 2
      Layout.fillHeight: true
    }
  }
}
