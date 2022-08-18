// Copyright (c) 2022 Private Internet Access, Inc.
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
import "../"
import "../../inputs"
import PIA.NativeHelpers 1.0
import "../../stores"
import "../../../common"
import "../../../client"
import "../../../daemon"
import "../../../theme"
import "../../../common/regions"

Page {
  readonly property bool multiHopAvailable: Daemon.settings.method === "openvpn" && Daemon.settings.proxyEnabled
  GridLayout {
    anchors.fill: parent
    columns: 2
    columnSpacing: Theme.settings.controlGridDefaultColSpacing
    rowSpacing: Theme.settings.controlGridDefaultRowSpacing

    CheckboxInput {
      uncheckWhenDisabled: true
      Layout.columnSpan: 2
      label: uiTr("Multi-hop and Obfuscation")
      desc: uiTr(
              "Add an extra layer of encryption by rerouting your VPN traffic through a proxy. By activating multi-hop, you’ll also obfuscate your connection - you will hide the fact that you're using a VPN.")
      enabled: Daemon.settings.method === "openvpn"
      setting: DaemonSetting{
        name: "proxyEnabled"
      }
      warning: {
        if(Daemon.settings.method !== "openvpn") {
          return uiTr("Multi-hop requires OpenVPN")
        }
        return ""
      }
    }

    PrivacyInput {
      enabled: multiHopAvailable
      id: proxySelector
      Layout.columnSpan: 2
      label: uiTranslate("ConnectionPage", "Proxy")
      info: ""
      itemHeight: 35
      function customProxyHost() {
        var proxyCustomValue = Daemon.settings.proxyCustom
        return proxyCustomValue ? proxyCustomValue.host : ""
      }


      itemList: {
        // "Shadowsocks" is not translated because it is a proper noun
        var items = ["Shadowsocks", uiTranslate("ConnectionPage", "SOCKS5 Proxy")]
        return items
      }
      activeItem: {
        switch (Daemon.settings.proxyType) {
        case 'custom':
          return 1
        case 'shadowsocks':
        default:
          return 0
        }
      }

      onUpdated: function (index) {
        var key = ['shadowsocks', 'custom'][index]

        if (key === 'custom' && !customProxyHost()) {
          customProxyDialog.updateAndOpen();
        } else {
          Daemon.applySettings({'proxyType': key})
        }

      }
    }

    EditHeading {
      id: shadowsocksHeading
      Layout.fillWidth: true
      Layout.topMargin: 5
      visible: Daemon.settings.proxyType === "shadowsocks"
      enabled: multiHopAvailable
      // Not translated
      label: "Shadowsocks"
      onEditClicked: {
          shadowsocksRegionDialog.updateAndOpen();
      }
    }

    Rectangle {
      visible: Daemon.settings.proxyType === "shadowsocks"
      opacity: multiHopAvailable ? 1.0 : 0.6
      Layout.columnSpan: 2
      Layout.fillWidth: true
      Layout.preferredHeight: 80
      radius: 10
      color: Theme.settings.inlayRegionColor

      RowLayout {
        anchors.fill: parent
        anchors.margins: 20

        FlagImage {
          countryCode: {
            let nextId = Daemon.state.shadowsocksLocations.nextLocation.id
            return Daemon.state.getRegionCountryCode(nextId)
          }
        }

        StaticText {
          Layout.leftMargin: 10
          text: {
            if(Daemon.state.shadowsocksLocations.nextLocation)
              return Messages.displayLocation(Daemon.state.shadowsocksLocations.nextLocation, Daemon.settings.proxyShadowsocksLocation === "auto")
            return "";
          }

          color: Theme.dashboard.textColor
        }

        Item {
          Layout.fillWidth: true
        }
      }
    }

    EditHeading {
      id: socksHeading
      Layout.fillWidth: true
      Layout.topMargin: 5
      visible: Daemon.settings.proxyType === "custom"
      enabled: multiHopAvailable
      label: uiTranslate("ConnectionPage", "SOCKS5 Proxy")
      onEditClicked: {
          customProxyDialog.updateAndOpen();
      }
    }

    Rectangle {
      visible: Daemon.settings.proxyType === "custom"
      opacity: multiHopAvailable ? 1.0 : 0.6
      Layout.columnSpan: 2
      Layout.fillWidth: true
      Layout.preferredHeight: {
        // No username or password set
        if(Daemon.settings.proxyCustom.password === "" && Daemon.settings.proxyCustom.username === "") {
          return 60;
        }

        // Only a username or password exists (unlikely, but account for it)
        if(Daemon.settings.proxyCustom.password === "" || Daemon.settings.proxyCustom.username === "") {
          return 80;
        }

        return 105;
      }

      radius: 10
      color: Theme.settings.inlayRegionColor

      GridLayout {
        rowSpacing:  5
        columnSpacing: 8
        columns: 2
        anchors.fill: parent
        anchors.margins: 20

          LabelText {
            id: hostLabel
            color: Theme.dashboard.textColor
            text: uiTr("Host")
            font.bold: true
            opacity:0.6
          }
          ValueText {
            label: hostLabel.text
            Layout.fillWidth: true
            color: Theme.dashboard.textColor
            text: {
              var proxy = Daemon.settings.proxyCustom;
              var result = proxy.host;

              if(proxy.port > 0) {
                result += ":" + proxy.port;
              }

              return result;
            }
          }
          LabelText {
            id: usernameLabel
            visible: Daemon.settings.proxyCustom.username !== ""
            color: Theme.dashboard.textColor
            text: uiTr("Username")
            font.bold: true
            opacity:0.6
          }
          ValueText {
            label: usernameLabel.text
            visible: Daemon.settings.proxyCustom.username !== ""
            color: Theme.dashboard.textColor
            text: Daemon.settings.proxyCustom.username
          }

          LabelText {
            id: passwordLabel
            visible: Daemon.settings.proxyCustom.password !== ""
            color: Theme.dashboard.textColor
            text: uiTr("Password")
            font.bold: true
            opacity:0.6
          }
          ValueText {
            label: passwordLabel.text
            visible: Daemon.settings.proxyCustom.password !== ""
            color: Theme.dashboard.textColor
            text: "********"
          }
        }

      }

    Item {
      Layout.fillWidth: true
      Layout.fillHeight: true
      Layout.columnSpan: 2
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
      if (currentCustomProxy.port > 0 && currentCustomProxy.port <= 65535)
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
        textBoxVerticalPadding: 4
        id: proxyHostname
        Layout.fillWidth: true
        //: The IP address of the SOCKS proxy server to use when
        //: connecting.  Labeled with "IP Address" to indicate that it
        //: can't be a hostname.
        label: uiTranslate("ConnectionPage", "Server IP Address")
        setting: Setting {
          sourceValue: ""
        }
        // Only IP addresses allowed.  This regex allows leading zeros in each
        // part.
        validator: RegExpValidator {
          regExp: /(([0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])\.){3}([0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])/
        }
      }
      TextboxInput {
        textBoxVerticalPadding: 4
        id: proxyPort
        label: uiTranslate("ConnectionPage", "Port")
        setting: Setting {
          sourceValue: ""
        }
        validator: RegExpValidator {
          regExp: /(?:[0-9]{,5})?/
        }
        placeholderText: uiTranslate("ConnectionPage", "Default")
      }
      TextboxInput {
        textBoxVerticalPadding: 4
        id: proxyUsername
        Layout.fillWidth: true
        Layout.columnSpan: 2
        label: uiTranslate("ConnectionPage", "User (optional)")
        setting: Setting {
          sourceValue: ""
        }
      }
      TextboxInput {
        textBoxVerticalPadding: 4
        id: proxyPassword
        Layout.fillWidth: true
        Layout.columnSpan: 2
        label: uiTranslate("ConnectionPage", "Password (optional)")
        masked: true
        setting: Setting {
          sourceValue: ""
        }
      }
    }
    onAccepted: {
      Daemon.applySettings({
                             "proxyType": "custom",
                             "proxyCustom": {
                               "host": proxyHostname.setting.currentValue,
                               "port": Number(proxyPort.setting.currentValue),
                               "username": proxyUsername.setting.currentValue,
                               "password": proxyPassword.setting.currentValue
                             }
                           })
      socksHeading.focusButton();
    }
    onRejected: {
      socksHeading.focusButton();
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
      implicitHeight: 450
      regionFilter: function (serverLocation) {
        // Show regions that have at least one shadowsocks server
        return serverLocation.hasShadowsocks
      }
      // Don't use shadowsocksLocations directly since the chosen location
      // isn't applied until the user clicks OK
      property var chosenLocation
      // assigned in updateAndOpen() or onRegionSelected()
      serviceLocations: ({
                           "bestLocation": Daemon.state.shadowsocksLocations.bestLocation,
                           "chosenLocation": shadowsocksRegionList.chosenLocation
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
      if (shadowsocksRegionList.chosenLocation)
        regionId = shadowsocksRegionList.chosenLocation.id
      Daemon.applySettings({
                             "proxyType": "shadowsocks",
                             "proxyShadowsocksLocation": regionId
                           })
      shadowsocksHeading.focusButton();
    }
    onRejected: {
      shadowsocksHeading.focusButton();
    }
  }
}
