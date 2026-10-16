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
import PIA.NativeHelpers 1.0

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

  // Multi-hop applies both proxy types with OpenVPN's socks-proxy option, and
  // OpenVPN disables DCO when that option is used, so DCO can't be selected
  // while multi-hop is enabled.
  readonly property bool usingProxy: Daemon.settings.proxyEnabled

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
      label: {
        if(reinstallTap.executing) {
          //: Label for the Virtual Network Driver setting when it cannot be
          //: changed due to an ongoing installation.
          return uiTr("Virtual Network Driver (Installing...)")
        }
        return uiTr("Virtual Network Driver")
      }
      visible: Qt.platform.os === 'windows'
      enabled: !reinstallTap.executing
      setting: DaemonSetting {
        name: "windowsIpMethod"
        function isTap(value) { return value === "dhcp" || value === "static" }
        onCurrentValueChanged: {
          if(isTap(currentValue) && !isTap(sourceValue) &&
            Daemon.state.tapAdapterMissing) {
            // We're switching to TAP, and the driver isn't installed yet.
            // Trigger a TAP install.
            reinstallTap.startReinstall()
          }
        }
      }
      model: [
        //: "DCO" stands for Data Channel Offload and is the name of the
        //: OpenVPN driver; it should not be translated.
        { name: uiTr("DCO (recommended)"), value: "dco",
          disabled: openVpnConnection.usingProxy },
        // 'WinTUN' is the driver name and is not translated
        { name: "WinTUN", value: "wintun" },
        //: "TAP" is the name of the OpenVPN virtual network adapter driver
        //: and should not be translated.  "DHCP" refers to Dynamic Host
        //: Configuration Protocol, a network configuration technology.
        //: This probably is not translated for most languages.
        { name: uiTranslate("ConnectionPage", "TAP (DHCP)"), value: "dhcp" },
        //: "TAP" is the name of the OpenVPN virtual network adapter driver
        //: and should not be translated. "Static" is an alternative to DHCP
        //: - instead of using dynamic configuration on the network adapter,
        //: it is configured with static addresses.
        { name: uiTranslate("ConnectionPage", "TAP (Static)"), value: "static" }
      ]
      info: [
          uiTranslate("ConnectionPage", "Determines which virtual network driver is used and how it is configured."),
          //: "DCO" stands for Data Channel Offload and should not be
          //: translated.  WinTUN is the name of the other driver and should
          //: also not be translated.
          uiTr("DCO: Offloads the data channel to the kernel for the best performance.  WinTUN is used automatically when DCO can't be used."),
          //: WinTUN is the name of the driver and should not be translated.
          uiTranslate("ConnectionPage", "WinTUN: Modern driver, offers the best speeds (try this first)"),
          //: TAP is the name of the driver and should not be translated.
          //: "DHCP" refers to Dynamic Host Configuration Protocol, a network
          //: configuration technology.  This probably is not translated for
          //: most languages.
          uiTranslate("ConnectionPage", "TAP (DHCP): Older driver, use if WinTUN isn't working"),
          //: "TAP" is the name of the OpenVPN virtual network adapter driver
          //: and should not be translated. "Static" is an alternative to DHCP
          //: - instead of using dynamic configuration on the network adapter,
          //: it is configured with static addresses.
          uiTranslate("ConnectionPage", "TAP (Static): Older driver, use if TAP (DHCP) isn't working")
      ].join("\n\u2022\xA0\xA0")
      //: Shown when DCO can't be selected because multi-hop is enabled.
      //: "DCO" stands for Data Channel Offload and is the name of the driver;
      //: it should not be translated.
      warning: openVpnConnection.usingProxy ? uiTr("DCO can't be used with Multi-hop. Turn off Multi-hop to select DCO.") : ""

      // This ReinstallLink is used to handle TAP installation when switching
      // to TAP - it triggers the installation and shows dialogs indicating
      // success/failure/reboot/etc.
      //
      // It is never actually shown; the reinstall state is added to the
      // dropdown's label.
      ReinstallLink {
        id: reinstallTap
        visible: false
      
        linkText: ""
        executingText: ""

        reinstallStatus: NativeHelpers.reinstallTapStatus
        reinstallAction: function(){NativeHelpers.reinstallTap()}
      }
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
