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

Page {
  //: "Handshake" is a brand name, not translated.
  readonly property string hdnsName: uiTr("Handshake DNS")

  function isStringValue(value, str) {
    return typeof value === 'string' && value === str
  }

  GridLayout {
    anchors.fill: parent
    columns: 2
    columnSpacing: Theme.settings.controlGridDefaultColSpacing
    rowSpacing: Theme.settings.controlGridDefaultRowSpacing

    DropdownInput {
      id: dnsDropdown
      //: Label for the setting that controls which DNS servers are used to look up
      //: domain names and translate them to IP addresses when browsing the internet.
      //: This setting is also present in OS network settings, so this string should
      //: preferably match whatever localized term the OS uses.
      label: uiTr("DNS")
      setting: Setting {
        id: customDNSSetting
        readonly property DaemonSetting daemonSetting: DaemonSetting { name: "overrideDNS"; onCurrentValueChanged: customDNSSetting.currentValue = currentValue }
        sourceValue: daemonSetting.sourceValue

        function isThirdPartyDNS(value) {
          return !isStringValue(value, "pia") && !isStringValue(value, "local")
        }

        onSourceValueChanged: {
          if (Array.isArray(sourceValue) && sourceValue.length > 0) {
            Client.uiState.settings.knownCustomServers = sourceValue;
          }
        }
        onCurrentValueChanged: {
          if (currentValue === "custom") {
            customDNSDialog.setServers(Array.isArray(daemonSetting.currentValue) ? daemonSetting.currentValue : []);
          } else if (daemonSetting.currentValue !== currentValue) {
            // When changing from a non-third-party value (pia/handshake) to a
            // third-party value ("existing" since custom was already handled),
            // show the "use existing DNS" prompt.
            if (!isThirdPartyDNS(sourceValue) && isThirdPartyDNS(currentValue)) {
              customDNSDialog.setExisting();
            } else {
              daemonSetting.currentValue = currentValue;
            }
          }
        }
      }
      warning: {
        if(setting.isThirdPartyDNS(setting.sourceValue) && !isStringValue(setting.sourceValue, "hdns"))
          return uiTr("Warning: Using a third party DNS could compromise your privacy.")
        return ""
      }
      info: {
        if(setting.sourceValue === "hdns") {
          //: "Handshake" is a brand name and should not be translated.
          return uiTr("Handshake is a decentralized naming protocol.  For more information, visit handshake.org.")
        }
        return ""
      }
      model: {
        var items = [
              { name: uiTr("PIA DNS"), value: "pia" },
              //: Indicates that we will run a built-in DNS resolver locally
              //: on the user's computer.
              { name: uiTr("Built-in Resolver"), value: "local" },
              { name: hdnsName, value: "hdns" },
              { name: uiTr("Use Existing DNS"), value: "" }
            ];


        if (Array.isArray(setting.currentValue) && setting.currentValue.length > 0) {
          items.push({ name: uiTr("Custom") + " [" +
                              setting.currentValue.join(" / ")
                              + "]", value: setting.currentValue });
        } else if (Client.uiState.settings.knownCustomServers) {
          items.push({ name: uiTr("Custom") + " [" +
                              Client.uiState.settings.knownCustomServers.join(" / ")
                              + "]", value: Client.uiState.settings.knownCustomServers });
        } else {
          items.push({ name: uiTr("Custom"), value: "custom" });
        }

        return items;
      }
    }

    Item {
      Layout.fillWidth: true
    }

    EditHeading {
      id: editHeading
      Layout.fillWidth: true
      Layout.topMargin: 5
      visible: Array.isArray(Daemon.settings.overrideDNS)
      label: uiTr("Custom DNS")
      onEditClicked: {
          customDNSSetting.currentValue = "custom";
      }
    }

    Rectangle {
      visible: Array.isArray(Daemon.settings.overrideDNS)
      color: Theme.settings.inlayRegionColor
      radius: 10
      Layout.preferredHeight: 70
      Layout.columnSpan: 2
      Layout.fillWidth: true

      RowLayout {
        anchors.margins: 20
        anchors.fill: parent

        RowLayout {
          LabelText {
            id: primaryDnsLabel
            color: Theme.dashboard.textColor
            font.pixelSize: 14
            text: uiTr("Primary DNS:")
            font.bold: true
            opacity: 0.6
          }
          ValueText {
            Layout.leftMargin: 10
            label: primaryDnsLabel.text
            text: Daemon.settings.overrideDNS[0] || ""
            color: Theme.dashboard.textColor
          }
        }

        RowLayout {
          visible: Daemon.settings.overrideDNS.length > 1
          LabelText {
            id: secondaryDnsLabel
            color: Theme.dashboard.textColor
            font.pixelSize: 14
            text: uiTr("Secondary DNS:")
            font.bold: true
            opacity: 0.6
          }
          ValueText {
            Layout.leftMargin: 10
            label: secondaryDnsLabel.text
            text: Daemon.settings.overrideDNS.length > 1 ? Daemon.settings.overrideDNS[1] : ""
            color: Theme.dashboard.textColor
          }
        }
      }
    }

    CheckboxInput {
      Layout.topMargin: 10
      Layout.columnSpan: 2
      //: Label for the setting that controls whether the application tries to
      //: forward a port from the public VPN IP to the user's computer. This
      //: feature is not guaranteed to work or be available, therefore we label
      //: it as "requesting" port forwarding.
      label: uiTr("Request Port Forwarding")
      //: Tooltip for the port forwarding setting. The user can not choose which
      //: port to forward; a port will be automatically assigned by our servers.
      //: The user should further be made aware that only some of our servers
      //: support forwarding. The string contains embedded linebreaks to prevent
      //: it from being displayed too wide on the user's screen - such breaks
      //: should be preserved at roughly the same intervals.
      desc: uiTr("Forwards a port from the VPN IP to your computer. The port will be selected for you. Not all locations support port forwarding.")
      setting: DaemonSetting {
        name: "portForward"
      }
    }

    CheckboxInput {
      Layout.columnSpan: 2
      label: uiTr("Allow LAN Traffic")
      desc: uiTr("Always permits traffic between devices on your local network, even when using the VPN killswitch.")
      setting: DaemonSetting { name: 'allowLAN' }
    }


    // Spacer
    Item {
      Layout.columnSpan: 2
      Layout.fillHeight: true
    }
  }

  OverlayDialog {
    id: customDNSDialog
    buttons: [{
        "text": uiTr("Proceed"),
        "role": DialogButtonBox.AcceptRole
      }, {
        "text": uiTr("Cancel"),
        "role": DialogButtonBox.RejectRole
      }]
    canAccept: (!customPrimaryDNS.visible
                || (customPrimaryDNS.text.length > 0
                    && customPrimaryDNS.acceptableInput))
               && (!customSecondaryDNS.visible
                   || customSecondaryDNS.acceptableInput)
    contentWidth: 400
    RegExpValidator {
      id: ipValidator
      regExp: /(?:(?:[0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])(?:\.(?:[0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])){3})?/
    }
    ColumnLayout {
      width: parent.width
      TextboxInput {
        textBoxVerticalPadding: 4

        id: customPrimaryDNS
        label: uiTr("Primary DNS")
        setting: Setting {
          sourceValue: ""
        }
        validator: ipValidator
        Layout.fillWidth: true
      }
      TextboxInput {
        textBoxVerticalPadding: 4

        id: customSecondaryDNS
        label: uiTr("Secondary DNS (optional)")
        setting: Setting {
          sourceValue: ""
        }
        validator: ipValidator
        Layout.fillWidth: true
        Layout.bottomMargin: 5
      }
      DialogMessage {
        icon: 'warning'
        text: uiTr(
                "<b>Warning:</b> Using non-PIA DNS servers could expose your DNS traffic to third parties and compromise your privacy.")
        color: Theme.settings.inputDescriptionColor
      }
    }
    function setServers(servers) {
      title = uiTr("Set Custom DNS")
      customPrimaryDNS.setting.currentValue = servers[0] || ""
      customSecondaryDNS.setting.currentValue = servers[1] || ""
      customPrimaryDNS.visible = true
      customSecondaryDNS.visible = true
      customPrimaryDNS.focus = true
      open()
    }
    function setExisting() {
      if(isStringValue(customDNSSetting.currentValue, "hdns")) {
        title = uiTr("Handshake DNS")
      } else {
        title = customDNSSetting.currentValue ? uiTr("Use Custom DNS") : uiTr(
              "Use Existing DNS")
      }

      customPrimaryDNS.visible = false
      customSecondaryDNS.visible = false
      open()
    }
    onAccepted: {
      if (customPrimaryDNS.visible) {
        var servers = []
        if (customPrimaryDNS.currentValue !== '')
          servers.push(customPrimaryDNS.currentValue)
        if (customSecondaryDNS.currentValue !== '')
          servers.push(customSecondaryDNS.currentValue)
        if (servers.length > 0) {
          customDNSSetting.daemonSetting.currentValue = servers
        } else {
          customDNSSetting.currentValue = customDNSSetting.sourceValue
        }
      } else {
        customDNSSetting.daemonSetting.currentValue = customDNSSetting.currentValue
      }

      dnsDropdown.forceActiveFocus(Qt.MouseFocusReason)
    }
    onRejected: {
      customDNSSetting.currentValue = customDNSSetting.sourceValue
      dnsDropdown.forceActiveFocus(Qt.MouseFocusReason)
    }
  }
}
