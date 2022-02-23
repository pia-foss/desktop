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
import ".."
import "../../../../client"
import "../../../../core"
import "../../../../daemon"
import "../../../../theme"
import "../../../../settings/stores"
import PIA.NativeAcc 1.0 as NativeAcc

MovableModule {
  implicitHeight: 80
  moduleKey: 'settings'

  //: Screen reader annotation for the Quick Settings tile.
  tileName: uiTr("Quick Settings tile")
  NativeAcc.Group.name: tileName

  Text {
    text: {
      var hoveredSetting = settingButtons.hoveredSettingName
      if(hoveredSetting)
        return Client.localeUpperCase(hoveredSetting)
      return uiTr("QUICK SETTINGS")
    }

    color: Theme.dashboard.moduleTitleColor
    font.pixelSize: Theme.dashboard.moduleLabelTextPx
    x: 20
    y: 10
    width: 260
    elide: Text.ElideRight
  }

  RowLayout {
    id: settingButtons

    x: 20
    y: 35
    width: 260
    height: 32
    spacing: 0

    property string hoveredSettingName: {
      for(var i=0; i<children.length; ++i) {
        if(children[i].hovered)
          return children[i].displayName
      }

      return ""
    }

    SettingsToggleButton {
      displayName: uiTr("Desktop Notifications")
      setting: ClientSetting{name: "desktopNotifications"}
      iconResourceType: 'notifications'
    }

    SettingsToggleButton {
      displayName: uiTr("MACE")
      setting: DaemonSetting{name: "enableMACE"}
      iconResourceType: 'mace'
      settingEnabled: Daemon.settings.overrideDNS === 'pia'
    }

    SettingsToggleButton {
      displayName: uiTr("Port Forwarding")
      setting: DaemonSetting{name: "portForward"}
      iconResourceType: 'port-forwarding'
    }

    SettingsToggleButton {
      displayName: uiTr("Allow LAN")
      setting: DaemonSetting{name: "allowLAN"}
      iconResourceType: 'lan'
    }

    SettingsToggleButton {
      id: debugModeToggle
      displayName: uiTr("Debug Logging")
      setting: Setting {
        sourceValue: Daemon.settings.debugLogging !== null
        onCurrentValueChanged: {
          if(currentValue !== undefined && currentValue !== (Daemon.settings.debugLogging !== null)) {
            if(!currentValue) {
              Daemon.applySettings({'debugLogging': null})
            }
            else {
              Daemon.applySettings({'debugLogging': Daemon.settings.defaultDebugLogging})
            }
          }
        }
      }
      iconResourceType: 'debug'
    }

    SettingsToggleButton {
      displayName: uiTr("Light Theme")
      setting: Setting {
        sourceValue: Client.settings.themeName === 'light'
        onCurrentValueChanged: {
          Client.applySettings({themeName: currentValue ? 'light' : 'dark'})
        }
      }
      iconResourceType: 'theme'
    }

    SettingsActionButton {
      displayName: uiTr("View All Settings...")
      iconResourceType: 'more'
      onClicked: wSettings.showSettings()
    }

    Item {
      Layout.fillWidth: true
    }
  }
}
