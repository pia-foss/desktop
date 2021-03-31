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
import "../inputs"
import "../stores"
import "../../client"
import "../../daemon"
import "../../settings"
import "../../common"
import "../../common/regions"
import "../../theme"
import PIA.BrandHelper 1.0


Page {
  ColumnLayout {
    anchors.fill: parent
    spacing: 0

    CheckboxInput {
      id: enableAutomationCheckbox
      label: uiTr("Connection Automation")
      info: {
        var bullet = "\u2022\xA0\xA0"
        var endl = "\n"

        //: Text displayed in a tooltip for connection automation
        return uiTr("Create rules to automatically connect or disconnect the VPN when you join a particular network.")
      }
      //: Help link for automation page
      linkText: uiTr("Help")
      linkTarget: BrandHelper.getBrandParam("automationHelpLink")
      enabled: true
      setting: DaemonSetting {
        name: 'automationEnabled'
      }
    }

    AutomationTable {
      Layout.fillWidth: true
      Layout.fillHeight: true
      Layout.topMargin: 10
      enabled: Daemon.settings.automationEnabled
    }
  }
}
