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
import "../inputs"
import "../stores"
import "../../client"
import "../../daemon"
import "../../settings"
import "../../common"
import "../../common/regions"
import "../../theme"

Page {
  ColumnLayout {
    anchors.fill: parent
    spacing: 2

    RowLayout {
      InputLabel {
        text: uiTranslate("ConnectionPage", "Proxy")
      }
      InfoTip {
        icon: icons.warning
        tipText: SettingsMessages.requiresOpenVpnMessage
        visible: Daemon.settings.method !== "openvpn"
      }
    }


    InputDescription {
      text: uiTr("Redirect the VPN connection through an additional location")
    }

    ThemedRadioGroup {
      id: proxyInput
      Layout.fillWidth: true
      groupEnabled: Daemon.settings.method === "openvpn"

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
        var shadowsocksName = uiTr("Shadowsocks - %1").arg(shadowsocksRegion)

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
          customName = uiTr("SOCKS5 Proxy - %1").arg(customHost)
        }

        return [
          {name: uiTranslate("ConnectionPage", "None"), value: 'none'},
          {
            name: shadowsocksName,
            value: 'shadowsocks',
            actionName: uiTr("Configure..."),
            action: function() {shadowsocksRegionDialog.updateAndOpen()}
          },
          {
            name: customName,
            value: 'custom',
            //: Opens a dialog to specify the custom proxy host/port/credentials.
            actionName: uiTr("Configure..."),
            action: function() {customProxyDialog.updateAndOpen()}
          }
        ]
      }

      readonly property DaemonSetting daemonProxy: DaemonSetting {
        name: "proxy"
        onCurrentValueChanged: proxyInput.setSelection(currentValue)
      }

      function customProxyHost() {
        var proxyCustomValue = Daemon.settings.proxyCustom
        return proxyCustomValue ? proxyCustomValue.host : ""
      }

      onSelected: {
        // If the user selected "custom", but no custom proxy has been
        // configured yet, configure it now.
        if(value === 'custom' && !proxyInput.customProxyHost())
          customProxyDialog.updateAndOpen()
        else
          proxyInput.daemonProxy.currentValue = value
      }
      Component.onCompleted: setSelection(daemonProxy.currentValue)
    }

    // Spacer - push contents to top
    Item {
      Layout.fillHeight: true
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
        proxyInput.setSelection(proxyInput.daemonProxy.sourceValue)
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
          return !!serverLocation.shadowsocks
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
        onRegionSelected: {
          // Update chosenLocation - null if 'auto' or an unknown region was
          // selected
          shadowsocksRegionList.chosenLocation = Daemon.data.locations[locationId]
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
}
