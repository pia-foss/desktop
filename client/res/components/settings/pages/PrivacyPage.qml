// Copyright (c) 2019 London Trust Media Incorporated
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
import "../../daemon"
import "../../theme"

Page {
  ColumnLayout {
    anchors.fill: parent
    anchors.leftMargin: Theme.settings.narrowPageLeftMargin
    spacing: 12

    PrivacyInput {
      Layout.preferredWidth: parent.width
      minItemWidth: 80
      //: Label for the setting that controls the VPN killswitch, a privacy feature
      //: that prevents network traffic from leaving the user's computer unless it
      //: is going through the VPN. The term "killswitch" is a recognizable term in
      //: the VPN industry that gets used in marketing and can be left unlocalized
      //: if there is no clear translation for the concept.
      label: uiTr("VPN Killswitch")
      //: Descriptive label for the VPN killswitch setting.
      desc: uiTr("Prevent leaks by blocking traffic outside the VPN")
      // \u2022 is a bullet, \xA0 is a non-breaking space
      // This is broken up into multiple parts to ensure that lupdate doesn't
      // choke or corrupt the bullets.
      info: [
        //: Setting value description for when the VPN killswitch is set to "Off".
        //: No network traffic is blocked and the user's privacy can potentially
        //: be compromised if the VPN connection unexpectedly drops.
        uiTr("Off: Don't block any traffic"),
        //: Setting value description for when the VPN killswitch is set to "Auto".
        //: Network traffic that tries to go outside the VPN is blocked as long as
        //: the user has turned on the VPN, including if the actual VPN connection
        //: itself unexpectedly drops.
        uiTr("Auto: Block outside traffic when the VPN is on"),
        //: Setting value description for when the VPN killswitch is set to "Always".
        //: Network traffic that tries to go outside the VPN is always blocked, even
        //: when the user has switched off the VPN. This effectively disables the
        //: user's internet connection whenever they are not connected to the VPN.
        uiTr("Always: Also block all traffic when the VPN is off"),
      ].map(function (i) { return "\u2022\xA0\xA0" + i; }).join("\n")
      //: These values are used for the Killswitch setting.
      itemList: [uiTr("Off", "privacy-killswitch"), uiTr("Auto"), uiTr("Always")]
      activeItem: {
        switch(Daemon.settings.killswitch) {
        case 'off': return 0
        case 'auto': return 1
        case 'on': return 2
        }
      }

      onUpdated: function(index){
        var key = ['off', 'auto', 'on'][index];
        Daemon.applySettings({'killswitch': key});
      }
    }
    PrivacyInput {
      Layout.preferredWidth: parent.width
      inputEnabled: Daemon.settings.overrideDNS === 'pia'
      warning: Daemon.settings.overrideDNS !== 'pia' ? uiTr("PIA MACE requires using PIA DNS in order to function.") : ""
      label: uiTr("PIA MACE")
      desc: uiTr("Block domains used for ads, trackers, and malware")
      // Can directly set to `enableMace` instead of inferring index,
      // but to prevent any issues with future versions or
      // options being moved around, it's better to be explicit
      activeItem: inputEnabled && Daemon.settings.enableMACE ? 1 : 0
      //: These values are used for the MACE setting.
      itemList: [uiTr("Off", "privacy-mace"), uiTr("On")]
      onUpdated: function (index) {
        Daemon.applySettings({'enableMACE': index === 1})
      }
    }
    Rectangle {
      Layout.fillHeight: true
      Layout.preferredHeight: 0
      color: "transparent"
    }
  }

  Component.onCompleted: {

  }
}
