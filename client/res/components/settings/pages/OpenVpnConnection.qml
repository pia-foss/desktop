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
import PIA.BrandHelper 1.0

// Translation note - various elements on this page deliberately do not
// translate:
// - Ports (local/remote) - always uses Arabic numerals
// - Procols (UDP/TCP)
// - Cryptographic settings (ciphers, hashes, signature algorithms)
//
// Settings like 'auto'/'none' in those lists _are_ translated though.

Item {

  // Generates the values for the port selection drop down (either udp/tcp)
  function portSelection(ports) {
    return [
            { name: SettingsMessages.defaultRemotePort, value: 0 }
           ].concat(ports.map(function(x) { return { name: x.toString(), value: x } }));
  }

  // This page contains two columns of smaller controls, followed by a single
  // column of wider controls.
  //
  // In English this doesn't make much difference (the sections are not visually
  // separated), but in other languages the wide controls are much longer, so
  // the layout is important for reasonable wrapping behavior.
  //
  // .--------------------------.
  // | Proto------  Encryption- |  ^
  // | R. Port----  Auth------- |  | 2 columns
  // | L. Port----  Handshake-- |  |
  // | C. Method--              |  V
  // | Use Small Packets------- |  ^
  // | Try Alternate Settings-- |  | 1 column
  // |    <spacer>              |  |
  // | -----What do these mean? |  V
  // '--------------------------'
  //
  GridLayout {
    id: outerGrid
    anchors.fill: parent
    columnSpacing: 30
    rowSpacing: 10
    columns: 2

    ColumnLayout {
      Layout.fillWidth: true
      Layout.alignment: Qt.AlignTop
      spacing: outerGrid.rowSpacing
      StackLayout {
        Layout.fillWidth: false
        Layout.fillHeight: false
        currentIndex: Daemon.settings.proxy === "shadowsocks" ? 1 : 0
        DropdownInput {
          label: SettingsMessages.connectionTypeSetting
          setting: DaemonSetting { name: "protocol" }
          model: [
            { name: "UDP", value: "udp" },
            { name: "TCP", value: "tcp" }
          ]
        }
        DropdownInput {
          label: SettingsMessages.connectionTypeSetting
          enabled: false
          setting: Setting { sourceValue: 0 }
          info: uiTranslate("ConnectionPage", "The Shadowsocks proxy setting requires TCP.")
          model: [
            { name: "TCP", value: 0 }
          ]
        }
      }
      StackLayout {
        currentIndex: {
          if(Daemon.settings.protocol === "tcp" || Daemon.settings.proxy === "shadowsocks")
            return 1
          return 0
        }
        Layout.fillWidth: false
        Layout.fillHeight: false
        DropdownInput {
          label: SettingsMessages.remotePortSetting
          setting: DaemonSetting { name: "remotePortUDP" }
          model: portSelection(Daemon.state.openvpnUdpPortChoices)
        }
        DropdownInput {
          label: SettingsMessages.remotePortSetting
          setting: DaemonSetting { name: "remotePortTCP" }
          model: portSelection(Daemon.state.openvpnTcpPortChoices)
        }
      }
      TextboxInput {
        label: uiTranslate("ConnectionPage", "Local Port")
        setting: Setting {
          readonly property DaemonSetting actual: DaemonSetting { name: "localPort" }
          sourceValue: actual.sourceValue > 0 && actual.sourceValue <= 65535 ? actual.sourceValue.toString() : ""
          onCurrentValueChanged: {
            var newValue = Number(currentValue); // "" becomes 0
            if (newValue !== actual.currentValue) {
              actual.currentValue = newValue;
            }
          }
        }
        validator: RegExpValidator { regExp: /(?:[0-9]{,5})?/ }
        placeholderText: uiTranslate("ConnectionPage", "Auto")
      }

      DropdownInput {
        label: uiTranslate("ConnectionPage", "Configuration Method")
        visible: Qt.platform.os === 'windows'
        setting: DaemonSetting { name: "windowsIpMethod" }
        model: [
          //: "DHCP" refers to Dynamic Host Configuration Protocol, a network
          //: configuration technology.  This probably is not translated for
          //: most languages.
          { name: uiTranslate("ConnectionPage", "DHCP"), value: "dhcp" },
          //: "Static" is an alternative to DHCP - instead of using dynamic
          //: configuration on the network adapter, it is configured with
          //: static addresses.
          { name: uiTranslate("ConnectionPage", "Static"), value: "static" }
        ]
        //: Description of the configuration method choices for Windows.
        //: This should suggest that the only reason to change this setting
        //: is if you have trouble connecting.
        info: uiTranslate("ConnectionPage", "Determines how addresses are configured on the TAP adapter.  If you have trouble connecting, a different method may be more reliable.")
      }
    }

    ColumnLayout {
      Layout.fillWidth: true
      Layout.alignment: Qt.AlignTop
      spacing: outerGrid.rowSpacing
      DropdownInput {
        label: SettingsMessages.dataEncryptionSetting
        setting: DaemonSetting { name: "cipher" }
        model: [
          { name: "AES-128 (GCM)", value: "AES-128-GCM" },
          { name: "AES-128 (CBC)", value: "AES-128-CBC" },
          { name: "AES-256 (GCM)", value: "AES-256-GCM" },
          { name: "AES-256 (CBC)", value: "AES-256-CBC" },
          { name: uiTranslate("ConnectionPage", "None"), value: "none" }
        ]
        warning: setting.sourceValue === 'none' ? uiTranslate("ConnectionPage", "Warning: Your traffic is sent unencrypted and is vulnerable to eavesdropping.") : ""
      }
      StackLayout {
        currentIndex: Daemon.settings.cipher.endsWith("GCM") ? 1 : 0
        Layout.fillWidth: false
        Layout.fillHeight: false
        DropdownInput {
          label: SettingsMessages.dataAuthenticationSetting
          setting: DaemonSetting { name: "auth" }
          model: [
            { name: "SHA1", value: "SHA1" },
            { name: "SHA256", value: "SHA256" },
            { name: uiTranslate("ConnectionPage", "None"), value: "none" }
          ]
          warning: setting.sourceValue === 'none' ? uiTranslate("ConnectionPage", "Warning: Your traffic is unauthenticated and is vulnerable to manipulation.") : ""
        }
        DropdownInput {
          label: SettingsMessages.dataAuthenticationSetting
          enabled: false
          setting: Setting { sourceValue: 0 }
          model: [
            { name: "GCM", value: 0 }
          ]
        }
      }
      DropdownInput {
        label: SettingsMessages.handshakeSetting
        setting: DaemonSetting { name: "serverCertificate" }
        model: [
          { name: "RSA-2048", value: "RSA-2048" },
          { name: "RSA-3072", value: "RSA-3072" },
          { name: "RSA-4096", value: "RSA-4096" },
          { name: "ECC-256k1", value: "ECDSA-256k1" },
          { name: "ECC-256r1", value: "ECDSA-256r1" },
          { name: "ECC-521", value: "ECDSA-521" }
        ]
        warning: setting.sourceValue.startsWith("ECDSA-") && !setting.sourceValue.endsWith("k1") ? uiTranslate("ConnectionPage", "This handshake relies on an Elliptic Curve endorsed by US standards bodies.") : ""
      }
    }

    ColumnLayout {
      Layout.fillWidth: true
      Layout.alignment: Qt.AlignTop
      Layout.columnSpan: 2
      spacing: outerGrid.rowSpacing

      CheckboxInput {
        label: uiTranslate("ConnectionPage", "Use Small Packets")
        onValue: 1250
        offValue: 0
        setting: DaemonSetting { name: "mtu" }
        info: uiTranslate("ConnectionPage", "Set a smaller MTU for the VPN connection. This can result in lower transfer speeds but improved reliability on poor connections.")
      }

      CheckboxInput {
        label: uiTranslate("ConnectionPage", "Try Alternate Settings")
        setting: DaemonSetting { name: "automaticTransport" }
        readonly property bool hasProxy: Daemon.settings.proxy !== 'none'
        enabled: !hasProxy
        info: {
          if(!hasProxy) {
            //: Tip for the automatic transport setting.  Refers to the
            //: "Connection Type" and "Remote Port" settings above on the
            //: Connection page.
            return uiTranslate("ConnectionPage", "If the connection type and remote port above do not work, try other settings automatically.")
          }
          return ""
        }
        warning: {
          if(hasProxy) {
            //: Tip used for the automatic transport setting when a proxy is
            //: configured - the two settings can't be used together.
            return uiTranslate("ConnectionPage", "Alternate settings can't be used when a proxy is configured.")
          }
          return ""
        }
      }

      Item {
        Layout.fillHeight: true // spacer
      }
    }
  }
}
