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
  Setting {
    id: customDNSSetting
    readonly property DaemonSetting daemonSetting: DaemonSetting {
      name: "overrideDNS"
      onCurrentValueChanged: customDNSSetting.currentValue = currentValue
    }
    property var knownCustomServers: null
    sourceValue: daemonSetting.sourceValue

    function isStringValue(value, str) {
      return typeof value === 'string' && value === str
    }

    function isThirdPartyDNS(value) {
      return !isStringValue(value, "pia") && !isStringValue(value, "handshake")
          && !isStringValue(value, "local")
    }

    onSourceValueChanged: {
      if (Array.isArray(sourceValue) && sourceValue.length > 0) {
        knownCustomServers = sourceValue
      }
    }
    onCurrentValueChanged: {
      if (currentValue === "custom") {
        customDNSDialog.setServers(
              Array.isArray(
                daemonSetting.currentValue) ? daemonSetting.currentValue : [])
      } else if (daemonSetting.currentValue !== currentValue) {
        // When changing from a non-third-party value (pia/handshake) to a
        // third-party value ("existing" since custom was already handled),
        // show the "use existing DNS" prompt.
        if (!isThirdPartyDNS(sourceValue) && isThirdPartyDNS(currentValue)) {
          customDNSDialog.setExisting()
        } else {
          daemonSetting.currentValue = currentValue
        }
      }
    }
  }

  GridLayout {
    anchors.fill: parent
    columns: 2
    columnSpacing: Theme.settings.controlGridDefaultColSpacing
    rowSpacing: Theme.settings.controlGridDefaultRowSpacing

    PrivacyInput {
      id: dnsSelector
      Layout.columnSpan: 2
      label: uiTr("DNS")
      info: ""
      itemHeight: 35
      // The Russian translations are really long but they _barely_ fit if we
      // squish the margins a little.  That's a little bit off from the radii
      // used in the control, so keep the proper margins for other languages.
      textMargin: Client.settings.language === "ru" ? 10 : 20

      itemList: {
        var items = [uiTr("PIA DNS"), uiTr("Built-in Resolver"), uiTr(
                       "Use Existing DNS"), uiTr("Custom")]
        return items
      }
      property string previous: "pia"
      activeItem: {
        if (Array.isArray(Daemon.settings.overrideDNS)) {
          return 3
        }

        switch (Daemon.settings.overrideDNS) {
        case 'pia':
          return 0
        case 'local':
          return 1
        case '':
          return 2
        }
      }

      onUpdated: function (index) {
        var key = ['pia', 'local', '', 'custom'][index]
        if (key === 'custom') {
          if (customDNSSetting.knownCustomServers) {
            customDNSSetting.currentValue = customDNSSetting.knownCustomServers
          } else {
            customDNSSetting.currentValue = "custom"
          }
        } else {
          customDNSSetting.currentValue = key
        }
      }
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
      title = customDNSSetting.currentValue ? uiTr("Use Custom DNS") : uiTr(
                                                "Use Existing DNS")
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

      editHeading.focusButton();
    }
    onRejected: {
      customDNSSetting.currentValue = customDNSSetting.sourceValue
      editHeading.focusButton();
    }
  }
}
