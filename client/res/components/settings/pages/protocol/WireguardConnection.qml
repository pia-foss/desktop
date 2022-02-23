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
import "."
import "../"
import "../../"
import "../../inputs"
import "../../stores"
import "../../../client"
import "../../../daemon"
import "../../../common"
import "../../../theme"
import "../../../core"
import PIA.BrandHelper 1.0
import PIA.NativeHelpers 1.0

Item {
  ColumnLayout {
      anchors.fill: parent
      spacing: 10

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

      DropdownInput {
        label: uiTr("Connection Timeout")
        setting: DaemonSetting { name: "wireguardPingTimeout" }
        model: [
          { name: uiTr("30 seconds"), value: 30 },
          { name: uiTr("1 minute"), value: 60 },
          { name: uiTr("2 minutes"), value: 120 }
        ]
      }

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

      CheckboxInput {
        id: wgUseKernel

        readonly property bool available: Daemon.state.wireguardKernelSupport

        //: On Linux, the WireGuard kernel module is supported and has
        //: better performance than the userspace implementation.
        //: https://en.wikipedia.org/wiki/Loadable_kernel_module
        label: uiTr("Use Kernel Module")
        setting: DaemonSetting {
          override: !wgUseKernel.available
          overrideValue: false
          name: "wireguardUseKernel"
        }
        visible: Qt.platform.os === "linux"

        enabled: available
        desc: {
          if(!available) {
            //: On Linux, the WireGuard kernel module is supported and has
            //: better performance than the userspace implementation.
            //: https://en.wikipedia.org/wiki/Loadable_kernel_module
            return uiTr("Install the WireGuard kernel module for the best performance.")
          }
          return ""
        }
        descLinks: {
          if(!available) {
            return [{
              text: uiTr("Install"),
              clicked: function(){wgUseKernel.installWireguardKernelModule()}
            }]
          }
          return []
        }

        function installWireguardKernelModule() {
          if(!NativeHelpers.installWireguardKernelModule()) {
            installKmodDialog.open()
          }
        }
      }

      Item {
        Layout.fillHeight: true
      }
    }

    OverlayDialog {
      id: installKmodDialog
      buttons: [Dialog.Ok]
      title: "WireGuard"

      ColumnLayout {
        anchors.fill: parent
        spacing: 10

        DialogMessage {
          id: kmodMessage
          Layout.alignment: Qt.AlignLeft
          Layout.minimumWidth: 200
          Layout.maximumWidth: 400
          text: uiTr("For distribution-specific installation instructions, visit:")
          icon: "info"
        }
        TextLink {
          Layout.alignment: Qt.AlignLeft
          Layout.leftMargin: kmodMessage.textIndent
          text: "https://www.wireguard.com/install/"
          link: "https://www.wireguard.com/install/"
          underlined: true
        }
      }
  }
}
