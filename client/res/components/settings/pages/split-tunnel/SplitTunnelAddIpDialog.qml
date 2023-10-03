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
import QtQuick.Window 2.3
import "../../inputs"
import "../../stores"
import "../../../core"
import "../../../theme"
import "../../../common"
import "../../../client"
import "../../../daemon"
import Qt.labs.platform 1.1
import PIA.SplitTunnelManager 1.0
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util
import "../"

OverlayDialog {
  id: ipSubnetDialog
  buttons: [ Dialog.Ok, Dialog.Cancel ]
  canAccept: (!ipSubnet.visible || (ipSubnet.text.length > 0 && ipSubnet.acceptableInput))
  contentWidth: 300
  RegExpValidator {
    id: ipValidator;
    regExp: {
        // Regex for ipv4 section (the part between successive dots)
        var ipv4Section = /(?:(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?))/.source
        return new RegExp(
            // IPv6 CIDR Regex
            "(?:(?:(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|(["+
            "0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:["+
            "0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA"+
            "-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-"+
            "F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F"+
            "]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]"+
            "{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){"+
            "1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)|" +
            "([0-9a-fA-F]{1,4}:){1,4}:((25[0-5]|(2[0-4]|1{0,1}["+
            "0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-"+
            "9]){0,1}[0-9]))(\\/((1(1[0-9]|2[0-8]))|([0-9][0-9])"+
            "|([0-9])))?)|" +

            // IPv4 CIDR Regex (supports compressed form too, i.e 192.168/16)
            "(" +
            ipv4Section + "(\\/[1-8]))|(" + ipv4Section + "\\." +
            ipv4Section + ")(\\/(1[0-6]|[0-9]))|(" + ipv4Section + "\\." +
            ipv4Section + "\\." + ipv4Section + ")(\\/(2[0-4]|1[0-9]|[0-9]))|(" +
            ipv4Section + "\\." + ipv4Section + "\\." + ipv4Section + "\\." +
            ipv4Section + ")(\\/(3[0-2]|2[0-9]|1[0-9]|[0-9]))?)");
        }
  }
  ColumnLayout {
    width: parent.width
    TextboxInput {
      textBoxVerticalPadding: 4
      id: ipSubnet
      label: uiTranslate("SplitTunnelAddIpRow", "IP Address or Subnet")
      setting: Setting { sourceValue: "" }
      validator: ipValidator
      Layout.fillWidth: true
    }
    DialogMessage {
      icon: 'info'
      text: uiTranslate("SplitTunnelAddIpRow", "You can enter an IPv4/IPv6 address, or a subnet in CIDR notation:") + "\n" +
                 ["192.0.2.5",
                  "198.51.100.0/24",
                  "2001:db8::8e7d:9002",
                  "2001:db8::/32"
                 ].map(ip => "\u2022\xA0\xA0" + ip).join("\n")

      color: Theme.settings.inputDescriptionColor
    }
  }
  function setSubnets() {
    title = uiTranslate("SplitTunnelAddIpRow", "Add IP Address")
    ipSubnet.setting.currentValue = ""
    ipSubnet.visible = true;
    ipSubnet.focus = true
    open();
  }
  onAccepted: {
    var subnets = Daemon.settings.bypassSubnets
    var normalizedSubnets = subnets.map(rule => SplitTunnelManager.normalizeSubnet(rule.subnet))
    if(ipSubnet.currentValue !== '')
    {
      var newSubnet = SplitTunnelManager.normalizeSubnet(ipSubnet.currentValue)
      if(newSubnet !== '' && !normalizedSubnets.includes(newSubnet))
        subnets.push({mode: "exclude", subnet: newSubnet})
    }

    if(subnets.length > 0)
      Daemon.applySettings({bypassSubnets: subnets})
  }
  onRejected: {
    ipSubnet.setting.currentValue = ipSubnet.setting.sourceValue;
  }
}
