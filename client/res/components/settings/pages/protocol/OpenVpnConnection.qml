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
import "."
import "../"
import "../../"
import "../../inputs"
import "../../stores"
import "../../../client"
import "../../../daemon"
import "../../../common"
import "../../../common/regions"
import "../../../theme"
import PIA.BrandHelper 1.0

// Translation note - various elements on this page deliberately do not
// translate:
// - Ports (local/remote) - always uses Arabic numerals
// - Procols (UDP/TCP)
// - Cryptographic settings (ciphers, hashes, signature algorithms)
// Settings like 'auto'/'none' in those lists _are_ translated though.
Item {
  id: openVpnConnection

    function portSelection(ports) {
      return [
              { name: SettingsMessages.defaultRemotePort, value: 0 }
             ].concat(ports.map(function(x) { return { name: x.toString(), value: x } }));
    }

  // Shadowsocks forces transport to TCP.  The PIA Shadowsocks servers do not
  // have UDP proxying enabled.  If SS obfuscation is needed, we usually need to
  // use TCP 443 anyway to penetrate the firewall too.  (Shadowsocks uses UDP
  // when encapsulating UDP traffic.)
  readonly property bool usingShadowsocksProxy: Daemon.settings.proxyType === "shadowsocks" && Daemon.settings.proxyEnabled
  readonly property bool usingTcpTransport: usingShadowsocksProxy || Daemon.settings.protocol === "tcp"

  GridLayout {
    anchors.fill: parent
    columns: 2
    columnSpacing: Theme.settings.controlGridDefaultColSpacing
    rowSpacing: Theme.settings.controlGridDefaultRowSpacing

    // Variant 1 of 2 [Transport Dropdown]
    DropdownInput {
      visible: !openVpnConnection.usingShadowsocksProxy
      label: SettingsMessages.connectionTypeSetting
      setting: DaemonSetting {
        name: "protocol"
      }
      model: [{
          "name": "UDP",
          "value": "udp"
        }, {
          "name": "TCP",
          "value": "tcp"
        }]
    }
    // Variant 2 of 2 [Transport Dropdown]
    DropdownInput {
      visible: openVpnConnection.usingShadowsocksProxy
      label: SettingsMessages.connectionTypeSetting
      enabled: false
      setting: Setting {
        sourceValue: 0
      }
      info: uiTranslate("ConnectionPage",
                        "The Shadowsocks proxy setting requires TCP.")
      model: [{
          "name": "TCP",
          "value": 0
        }]
    }

    DropdownInput {
      label: SettingsMessages.dataEncryptionSetting
      setting: DaemonSetting {
        name: "cipher"
      }
      model: [{
          "name": "AES-128 (GCM)",
          "value": "AES-128-GCM"
        }, {
          "name": "AES-256 (GCM)",
          "value": "AES-256-GCM"
        }]
      warning: setting.sourceValue === 'none' ? uiTranslate(
                                                  "ConnectionPage",
                                                  "Warning: Your traffic is sent unencrypted and is vulnerable to eavesdropping.") : ""
    }

    // Row 3
    // Variant 1 of 2 [ Remote port ]
    DropdownInput {
      label: SettingsMessages.remotePortSetting
      visible: !openVpnConnection.usingTcpTransport
      setting: DaemonSetting {
        name: "remotePortUDP"
      }
      model: portSelection(Daemon.state.openvpnUdpPortChoices)
    }
    // Variant 1 of 2 [ Remote port ]
    DropdownInput {
      label: SettingsMessages.remotePortSetting
      visible: openVpnConnection.usingTcpTransport
      setting: DaemonSetting {
        name: "remotePortTCP"
      }
      model: portSelection(Daemon.state.openvpnTcpPortChoices)
    }
    TextboxInput {
      visible: !Daemon.data.flags.includes("remove_local_port_setting")
      textBoxVerticalPadding: 4
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

    // Row 4
    DropdownInput {
      label: SettingsMessages.mtuSetting
      setting: DaemonSetting {
        name: "mtu"
      }
      model: [{
          "name": SettingsMessages.mtuSettingAuto,
          "value": -1
        }, {
          "name": SettingsMessages.mtuSettingLargePackets,
          "value": 0
        }, {
          "name": SettingsMessages.mtuSettingSmallPackets,
          "value": 1250
        }]
      info: SettingsMessages.mtuSettingDescription
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


    // Spacer between drop downs and check boxes.  With columnSpan: 2, this also
    // forces the next check box to start a new line (the items in the prior
    // row vary since Configuration Method only applies on Windows)
    Item {
      Layout.columnSpan: 2
      Layout.preferredHeight: 5
    }

    // Row 5
    CheckboxInput {
      uncheckWhenDisabled: true
      Layout.alignment: Qt.AlignTop
      Layout.columnSpan: 2
      label: uiTranslate("ConnectionPage", "Try Alternate Settings")
      setting: DaemonSetting { name: "automaticTransport" }
      readonly property bool hasProxy: Daemon.settings.proxyEnabled
      enabled: !hasProxy
      desc: {
        //: Tip for the automatic transport setting.  Refers to the
        //: "Connection Type" and "Remote Port" settings above on the
        //: Connection page.
        return uiTranslate("ConnectionPage", "If the connection type and remote port above do not work, try other settings automatically.")
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
  }
}
