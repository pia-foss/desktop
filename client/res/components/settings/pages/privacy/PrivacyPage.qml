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
  function updateKillSwitchSetting () {
    var value = "off";
    if(onKillSwitchCheckbox.setting.currentValue) {
      value = "on";
    } else if(autoKillSwitchCheckbox.setting.currentValue) {
      value = "auto";
    }

    Daemon.applySettings({killswitch: value});
  }
  GridLayout {
    anchors.fill: parent
    columns: 2
    columnSpacing: Theme.settings.controlGridDefaultColSpacing
    rowSpacing: Theme.settings.controlGridDefaultRowSpacing

    CheckboxInput {
      Layout.columnSpan: 2
      id: autoKillSwitchCheckbox
      label: uiTr("VPN Kill Switch")
      enabled: Daemon.settings.killswitch !== "on"
      warning: Daemon.settings.killswitch === "on" ? uiTr("VPN Kill Switch is always enabled when Advanced Kill Switch is enabled.") : ""
      setting: Setting {
        sourceValue: Daemon.settings.killswitch === "auto" || Daemon.settings.killswitch === "on"
        onCurrentValueChanged: {
          updateKillSwitchSetting();
        }
      }
      desc: uiTr("Once the VPN is turned on, prevent leaks by blocking traffic from going outside the VPN, including during connection loss.")
    }
    CheckboxInput {
      Layout.columnSpan: 2
      id: onKillSwitchCheckbox
      label: uiTr("Advanced Kill Switch")
      setting: Setting {
        sourceValue: Daemon.settings.killswitch === "on"
        onCurrentValueChanged: {
          updateKillSwitchSetting();
        }
      }
      desc: uiTr("Prevent leaks by blocking any traffic from going outside the VPN, even when the VPN is turned off.")
    }
    CheckboxInput {
      uncheckWhenDisabled: true
      Layout.columnSpan: 2
      label: uiTr("PIA MACE")
      setting: DaemonSetting { name: "enableMACE" }
      enabled: Daemon.settings.overrideDNS === "pia"
      warning: enabled ? "" : uiTr("PIA MACE requires using PIA DNS.")
      desc: uiTr("Block domains used for ads, trackers, and malware.")
    }
    Item {
      Layout.columnSpan: 2
      Layout.fillHeight: true
    }
  }
}
