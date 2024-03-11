// Copyright (c) 2024 Private Internet Access, Inc.
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
  GridLayout {
    anchors.fill: parent
    columns: 2
    columnSpacing: 10
    rowSpacing: 20

  // Row 1
  CheckboxInput {
    id: loginItemEnabled
    label: uiTr("Launch on System Startup")
    setting: Setting {
      onCurrentValueChanged: {
        if(currentValue !== sourceValue) {
          NativeHelpers.setStartOnLogin(currentValue);
          sourceValue = currentValue;
        }
      }
    }
    // There's no change signal or property for this setting
    function initCurrentVal() {
      setting.sourceValue = NativeHelpers.getStartOnLogin()
    }
    Component.onCompleted: initCurrentVal()
  }
  CheckboxInput {
    label: uiTr("Show Desktop Notifications")
    setting: ClientSetting { name: "desktopNotifications" }
  }

  // Row 2:
  CheckboxInput {
    label: uiTr("Connect on Launch")
    setting: ClientSetting { name: "connectOnLaunch" }
  }
  CheckboxInput {
    label: uiTr("Show Service Communication Messages")
    setting: DaemonSetting { name: 'showAppMessages' }
  }

  // Row 3: [ colSpan: 2 ]
  CheckboxInput {
    Layout.columnSpan: 2
    label: uiTr("Show Geo-Located Regions")
    setting: DaemonSetting { name: 'includeGeoOnly' }
  }

  Item {
    Layout.columnSpan: 2
    Layout.preferredHeight: 10
    Layout.fillWidth: true
  }

  // Row 4:
  DropdownInput {
    label: uiTr("Language")
    setting: ClientSetting {
      name: "language"
    }
    model: {
      var langModel = []
      for(var i=0; i<Client.state.languages.length; ++i) {
        var lang = Client.state.languages[i]
        langModel.push({name: lang.displayName,
             value: lang.locale,
             disabled: !lang.available})
      }
      return langModel
    }
  }
  DropdownInput {
    //: This setting allows the user to choose a style for the icon shown in
    //: the system tray / notification area.  It should use the typical
    //: desktop terminology for the "tray".
    label: uiTr("Tray Icon Style")

    setting: ClientSetting { name: "iconSet" }
    info: {
      if(Client.settings.iconSet === "auto")
        return uiTr("The 'System' setting chooses an icon based on your desktop theme.")
      return ""
    }
    model: {
      var themeNames = {
        auto: uiTr("System", "icon-theme"),
        light: uiTr("Light", "icon-theme"),
        dark: uiTr("Dark", "icon-theme"),
        colored: uiTr("Colored", "icon-theme"),
        classic: uiTr("Classic", "icon-theme")
      }

      var themeChoices = NativeHelpers.iconThemeValues()

      return themeChoices.map(function(choice){
        return {
          name: themeNames[choice],
          value: choice,
          icon: NativeHelpers.iconPreviewResource(choice),
          backdropFlag: true
        }
      })
    }
  }

  // Row 5:
  DropdownInput {
    label: uiTr("Theme")
    setting: ClientSetting { name: "themeName" }
    model: [
      { name: uiTr("Dark"), value: "dark" },
      { name: uiTr("Light"), value: "light" }
    ]
  }
  DropdownInput {
    //: Setting controlling how the dashboard is displayed - either as a popup
    //: attached to the system tray or as an ordinary window.
    label: uiTr("Dashboard Appearance")
    setting: ClientSetting { name: "dashboardFrame" }
    model: [
      //: Setting value indicating that the dashboard is a popup attached to
      //: the system tray.
      { name: uiTr("Attached to Tray"), value: "popup" },
      //: Setting value indicating that the dashboard is an ordinary window
      { name: uiTr("Window"), value: "window" },
    ]
    warning: {
      if(Qt.platform.os === "linux" && setting.currentValue === "popup") {
        return uiTr("Attached mode may not work with all desktop environments, and it requires a system tray. If you can't find the dashboard, start Private Internet Access again to show it, and switch back to Window mode in Settings.")
      }
      return ""
    }
    tipBelow: false
  }



  // Spacer
  Item {
      Layout.columnSpan: 2
      Layout.fillHeight: true
    }

  SettingsButton{
    id: resetLink
    text: uiTr("Reset All Settings")

    onClicked: {
      Window.window.alert(uiTr("Reset all settings to their default values?"),
              resetLink.text, [Dialog.Yes, Dialog.No],
              handleResetResult)
    }
    function handleResetResult(result) {
      // Coercing != to handle enum comparison
      if(result.code != Dialog.Yes)
        return

      // The alert dialog can't handle a language change during the "yes"
      // button press - this retranslates the button strings, which recreates
      // the buttons, which prevents the dialog from closing properly.
      // Delay the actual reset to get out of the button click signal.
      resetTimer.start()
    }

    Timer {
      id: resetTimer
      repeat: false
      interval: 1
      onTriggered: resetLink.resetSettings()
    }

    function resetSettings() {
      // Start-on-login isn't in ClientSettings or DaemonSettings since it's
      // stored by the system - reset it too.
      // If we ever have more of these, we should group them together, but
      // right now this is the only one.
      NativeHelpers.setStartOnLogin(false)
      loginItemEnabled.initCurrentVal()
      Client.resetSettings()
      Daemon.resetSettings()
    }
    Layout.alignment: Qt.AlignLeft
  }
}}
