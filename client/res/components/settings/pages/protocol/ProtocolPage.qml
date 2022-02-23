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
import "../../"
import "../../inputs"
import PIA.NativeHelpers 1.0
import PIA.BrandHelper 1.0
import "../../stores"
import "../../../common"
import "../../../client"
import "../../../daemon"
import "../../../theme"

Page {

  GridLayout {
    anchors.fill: parent
    columns: 2
    columnSpacing: Theme.settings.controlGridDefaultColSpacing
    rowSpacing: Theme.settings.controlGridDefaultRowSpacing

    PrivacyInput {
      Layout.columnSpan: 2
      itemHeight: 35
      label: uiTr("Protocol")

      readonly property string openvpnTipText: uiTranslate("ConnectionPage",
                                                 "Reliable, stable and secure protocol with 18+ years of open source development.")
      //: Description for WireGuard shown as tip describing available protocols.
      readonly property string wireguardTipText: uiTranslate("ConnectionPage",
                                                   "Newer, more efficient protocol with the potential for increased performance.")
      info: {
        if (Daemon.state.wireguardAvailable)
          return "<strong>OpenVPN®:</strong> " + openvpnTipText
              + "<br/><strong>WireGuard®:</strong> " + wireguardTipText
        else
          return SettingsMessages.wgRequiresWindows8
      }
      tipAccessibleText: {
        if (Daemon.state.wireguardAvailable)
          return "OpenVPN®: " + openvpnTipText + "\nWireGuard®: " + wireguardTipText
        else
          return SettingsMessages.wgRequiresWindows8
      }

      itemList: ["OpenVPN®", "WireGuard®"]
      activeItem: {
        switch (Daemon.settings.method) {
        case 'openvpn':
          return 0
        case 'wireguard':
          return 1
        default:
          return 0
        }
      }

      onUpdated: function (index) {
        var key = ['openvpn', 'wireguard'][index]
        Daemon.applySettings({
                               "method": key
                             })
      }
    }

    Item {
      Layout.columnSpan: 2
      Layout.preferredHeight: 10
    }

    StackLayout {
      Layout.columnSpan: 2
      currentIndex: Daemon.settings.method === "openvpn" ? 0 : 1
      OpenVpnConnection {}
      WireguardConnection {}
    }

    // Spacer
    Item {
      Layout.columnSpan: 2
      Layout.fillHeight: true
    }

    Row {
      Layout.columnSpan: 2
      spacing: 10

      SettingsButton {
        text: uiTranslate("ConnectionPage", "Trademarks")
        Layout.preferredWidth: implicitWidth
        underlined: true
        onClicked: {
          trademarkDialog.open()
        }
      }
      SettingsButton {
        text: uiTranslate("ConnectionPage","What do these settings mean?")
        underlined: true
        link: BrandHelper.getBrandParam("encryptionSettingsLink")
        visible: Daemon.settings.method === 'openvpn'
      }
    }

    OverlayDialog {
      id: trademarkDialog
      buttons: [Dialog.Ok]
      title: uiTranslate("ConnectionPage","Trademarks")

      ColumnLayout {
        StaticText {
          Layout.alignment: Qt.AlignLeft
          Layout.minimumWidth: 200
          Layout.maximumWidth: 400
          wrapMode: Text.WordWrap
          text: uiTranslate("ConnectionPage",
                  "All product and company names are trademarks™ or registered® trademarks of their respective holders. Use of them does not imply any affiliation with or endorsement by them.") + "\n" + "\n" +
                uiTranslate("ConnectionPage", "OpenVPN® is a trademark of OpenVPN Technologies, Inc.") + "\n" +
                uiTranslate("ConnectionPage", "WireGuard® is a trademark of Jason A. Donenfeld, an individual.")
          color: Theme.settings.inputLabelColor
        }
      }
    }
  }
}
