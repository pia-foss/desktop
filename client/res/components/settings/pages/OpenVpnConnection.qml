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
import "."
import ".."
import "../inputs"
import "../stores"
import "../../client"
import "../../daemon"
import "../../common"
import "../../common/regions"
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
          // Qt 5.15.2 seems to have broken the way these nested layouts interact,
          // we have to explicitly set the dropdown's Layout.preferredWidth/
          // preferredHeight to its implicitWidth/implicitHeight.
          //
          // Looks like this commit was trying to optimize the way layouts are
          // processed, but it seems to prevent the implicitHeight changes in the
          // dropdowns from correctly propagating up to the ColumnLayout
          // https://code.qt.io/cgit/qt/qtdeclarative.git/commit/src/imports/layouts/qquickstacklayout.cpp?id=a5d2fd816bcbee6026894927ae5d049536bfc7ea
          Layout.preferredWidth: implicitWidth
          Layout.preferredHeight: implicitHeight
          label: SettingsMessages.connectionTypeSetting
          setting: DaemonSetting { name: "protocol" }
          model: [
            { name: "UDP", value: "udp" },
            { name: "TCP", value: "tcp" }
          ]
        }
        DropdownInput {
          Layout.preferredWidth: implicitWidth
          Layout.preferredHeight: implicitHeight
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
          Layout.preferredWidth: implicitWidth
          Layout.preferredHeight: implicitHeight
          label: SettingsMessages.remotePortSetting
          setting: DaemonSetting { name: "remotePortUDP" }
          model: portSelection(Daemon.state.openvpnUdpPortChoices)
        }
        DropdownInput {
          Layout.preferredWidth: implicitWidth
          Layout.preferredHeight: implicitHeight
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
          { name: "AES-256 (CBC)", value: "AES-256-CBC" }
        ]
        warning: setting.sourceValue === 'none' ? uiTranslate("ConnectionPage", "Warning: Your traffic is sent unencrypted and is vulnerable to eavesdropping.") : ""
      }

      DropdownInput {
        id: proxyInput
        hasConfigureButton: setting.currentValue !== "none"
        configureButtonHelp: uiTranslate("ProxyPage", "Configure...")

        function customProxyHost() {
          var proxyCustomValue = Daemon.settings.proxyCustom
          return proxyCustomValue ? proxyCustomValue.host : ""
        }

        onConfigureClicked: {
          if(setting.currentValue === "shadowsocks") {
            shadowsocksRegionDialog.updateAndOpen();
          } else if(setting.currentValue === "custom") {
            customProxyDialog.updateAndOpen();
          }
        }

        label: uiTranslate("ConnectionPage", "Proxy")
        setting: Setting {
          id: customProxySetting

          readonly property DaemonSetting daemonSetting: DaemonSetting { name: "proxy"; }
          sourceValue: daemonSetting.sourceValue

          onCurrentValueChanged: {
            if(currentValue === 'custom' && !proxyInput.customProxyHost())
              customProxyDialog.updateAndOpen()
            else
              daemonSetting.currentValue = currentValue
          }
        }

        model: {
          // Determine the name for the Shadowsocks item based on the selected
          // region
          var shadowsocksRegion = Messages.displayLocationSelection(Daemon.state.shadowsocksLocations.chosenLocation,
                                                                    Daemon.state.shadowsocksLocations.bestLocation)
          //: Label for the Shadowsocks proxy choice.  "Shadowsocks" is a proper
          //: noun and shouldn't be translated, but the dash should match the
          //: other proxy choice labels.  %1 is a description of the selected
          //: region, such as "Japan" or "Auto (US East)", this uses the
          //: localizations defined for the region module.
          var shadowsocksName = uiTranslate("ProxyPage", "Shadowsocks - %1").arg(shadowsocksRegion)

          // Determine the name for the custom item based on whether it is
          // configured
          var customHost = customProxyHost()
          var customName = uiTranslate("ConnectionPage", "SOCKS5 Proxy...")
          if(customHost) {
            if(Daemon.settings.proxyCustom.port)
              customHost += ":" + Daemon.settings.proxyCustom.port
            //: Label for the custom SOCKS5 proxy choice when a proxy has been
            //: configured.  %1 is the configured proxy (host or host:port), such
            //: as "SOCKS5 Proxy: 127.0.0.1" or "SOCKS5 Proxy: 172.16.24.18:9080"
            customName = uiTranslate("ProxyPage", "SOCKS5 Proxy - %1").arg(customHost)
          }

          return [
            {name: uiTranslate("ConnectionPage", "None"), value: 'none'},
            {
              name: shadowsocksName,
              value: 'shadowsocks',
            },
            {
              name: customName,
              value: 'custom',
            }
          ]
        }
      }

      OverlayDialog {
        id: customProxyDialog
        buttons: [Dialog.Ok, Dialog.Cancel]
        canAccept: proxyHostname.acceptableInput
        contentWidth: 300
        title: uiTranslate("ConnectionPage", "SOCKS5 Proxy")

        function updateAndOpen() {
          var currentCustomProxy = Daemon.settings.proxyCustom
          proxyHostname.setting.currentValue = currentCustomProxy.host
          if(currentCustomProxy.port > 0 && currentCustomProxy.port <= 65535)
            proxyPort.setting.currentValue = currentCustomProxy.port.toString()
          else
            proxyPort.setting.currentValue = ""

          proxyUsername.setting.currentValue = currentCustomProxy.username
          proxyPassword.setting.currentValue = currentCustomProxy.password

          open()
        }

        GridLayout {
          width: parent.width
          columns: 2
          TextboxInput {
            id: proxyHostname
            Layout.fillWidth: true
            //: The IP address of the SOCKS proxy server to use when
            //: connecting.  Labeled with "IP Address" to indicate that it
            //: can't be a hostname.
            label: uiTranslate("ConnectionPage", "Server IP Address")
            setting: Setting { sourceValue: "" }
            // Only IP addresses allowed.  This regex allows leading zeros in each
            // part.
            validator: RegExpValidator {
              regExp: /(([0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])\.){3}([0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])/
            }
          }
          TextboxInput {
            id: proxyPort
            label: uiTranslate("ConnectionPage", "Port")
            setting: Setting { sourceValue: "" }
            validator: RegExpValidator { regExp: /(?:[0-9]{,5})?/ }
            placeholderText: uiTranslate("ConnectionPage", "Default")
          }
          TextboxInput {
            id: proxyUsername
            Layout.fillWidth: true
            Layout.columnSpan: 2
            label: uiTranslate("ConnectionPage", "User (optional)")
            setting: Setting { sourceValue: "" }
          }
          TextboxInput {
            id: proxyPassword
            Layout.fillWidth: true
            Layout.columnSpan: 2
            label: uiTranslate("ConnectionPage", "Password (optional)")
            masked: true
            setting: Setting { sourceValue: "" }
          }
        }
        onAccepted: {
          Daemon.applySettings({
            proxy: "custom",
            proxyCustom: {
              host: proxyHostname.setting.currentValue,
              port: Number(proxyPort.setting.currentValue),
              username: proxyUsername.setting.currentValue,
              password: proxyPassword.setting.currentValue
            }
          })
          proxyInput.forceActiveFocus(Qt.MouseFocusReason)
        }
        onRejected: {
          // Revert the proxy setting choice
          proxyInput.setting.currentValue = proxyInput.setting.sourceValue
          proxyInput.forceActiveFocus(Qt.MouseFocusReason)
        }
      }

      OverlayDialog {
        id: shadowsocksRegionDialog
        buttons: [Dialog.Ok, Dialog.Cancel]
        canAccept: true
        contentWidth: 350
        title: "Shadowsocks" // Not translated
        topPadding: 0
        bottomPadding: 0
        leftPadding: 0
        rightPadding: 0

        function updateAndOpen() {
          shadowsocksRegionList.chosenLocation = Daemon.state.shadowsocksLocations.chosenLocation
          shadowsocksRegionList.clearSearch()
          shadowsocksRegionList.reevalSearchPlaceholder()
          open()
        }

        RegionList {
          id: shadowsocksRegionList
          width: parent.width
          implicitHeight: 300
          regionFilter: function(serverLocation) {
            // Show regions that have at least one shadowsocks server
            for(var i=0; i<serverLocation.servers.length; ++i) {
              if(serverLocation.servers[i].shadowsocksPorts.length > 0 &&
                 serverLocation.servers[i].shadowsocksKey &&
                 serverLocation.servers[i].shadowsocksCipher)
                return true
            }
            return false
          }
          // Don't use shadowsocksLocations directly since the chosen location
          // isn't applied until the user clicks OK
          property var chosenLocation // assigned in updateAndOpen() or onRegionSelected()
          serviceLocations: ({
            bestLocation: Daemon.state.shadowsocksLocations.bestLocation,
            chosenLocation: shadowsocksRegionList.chosenLocation
          })
          portForwardEnabled: false
          canFavorite: false
          collapsedCountriesSettingName: "shadowsocksCollapsedCountries"
          onRegionSelected: {
            // Update chosenLocation - null if 'auto' or an unknown region was
            // selected
            shadowsocksRegionList.chosenLocation = Daemon.state.availableLocations[locationId]
          }
        }

        onAccepted: {
          var regionId = 'auto'
          if(shadowsocksRegionList.chosenLocation)
            regionId = shadowsocksRegionList.chosenLocation.id
          Daemon.applySettings({
            proxy: "shadowsocks",
            proxyShadowsocksLocation: regionId
          })
          proxyInput.forceActiveFocus(Qt.MouseFocusReason)
        }
        onRejected: {
          // Just focus the proxy input.  Nothing to revert, since the proxy input
          // never shows this dialog due to a radio button selection.
          proxyInput.forceActiveFocus(Qt.MouseFocusReason)
        }
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
