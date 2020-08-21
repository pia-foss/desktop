// Copyright (c) 2020 Private Internet Access, Inc.
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

import QtQuick 2.10
import "./settings"
import "./dashboard"
import "./common"
import "./devtools"
import "../javascript/app.js" as App
import PIA.NativeHelpers 1.0
import "./theme"
import "./client"
import "./daemon"
import "./onboarding"
import "./changelog"

QtObject {
  property var wSettings: SettingsWindow {
    objectName: "wSettings"
    visible: false
  }

  property var dashboard: DashboardFrameLoader {
    windowed: Client.settings.dashboardFrame === 'window'

    content.pageHeight: wrapper.pageHeight
    content.maxPageHeight: wrapper.maxPageHeight
    content.headerTopColor: wrapper.headerTopColor

    settingsWindow: wSettings

    DashboardWrapper {
      id: wrapper
      anchors.fill: parent
      contentRadius: dashboard.window ? dashboard.window.contentRadius : 0
      layoutHeight: dashboard.window ? dashboard.window.layoutHeight : 500
    }

    onDashboardLoaded: {
      TrayIcon.dashboard = dashboard.window
    }
  }

  property var trayManager: TrayManager {
    dashPopup: dashboard.window
  }

  property var wDevTools: DevToolsWindow {
    visible: false
  }

  property var wChangeLog: ChangelogWindow {
  }

  property var wOnboarding: OnboardingWindow {

  }

  property Connections showDashboardHandler: Connections {
    target: NativeHelpers
    onDashboardOpenRequested: dashboard.window.showDashboard(trayManager.getIconMetrics())
  }

  property Component macMenuBarComponent: Component {
    MacMenuBar{}
  }
  property Loader macMenuBarLoader: Loader {
    active: Qt.platform.os === 'osx'
    sourceComponent: macMenuBarComponent
  }

  // We have to do the initial show after a slight delay so the icon bounds
  // will be set up correctly.
  property var initialShowTimer: Timer {
    interval: 1
    repeat: false
    running: false
    onTriggered: dashboard.window.showDashboard(trayManager.getIconMetrics())
  }

  property var centerChangelogTimer: Timer {
    interval: 1
    repeat: false
    running: false
    onTriggered: {
      wChangeLog.show();
      wChangeLog.centerOnActiveScreen();
    }
  }

  // Qt has issues when the theme is changed while the dashboard is hidden
  // (usually while connected and with the llvmpipe backend on Windows).  This
  // seems to be a scene graph bug - the next time the dashboard is shown, it
  // seems to try to update image nodes that have been destroyed.
  //
  // To work around it, if the theme is changed while the dashboard is hidden,
  // force it to be reloaded.
  property var themeChangeHandler: Connections {
    target: Client.settings
    onThemeNameChanged: {
      if(!dashboard.window.visible)
        dashboard.forceReload()
    }
  }

  Component.onCompleted: {
    TrayIcon.dashboard = dashboard.window

    if(!Client.state.quietLaunch && !Client.state.firstRunFlag || Client.state.clientHasBeenUpdated) {
      // Not a quiet launch or first run - initially show the dashboard near the tray icon
      initialShowTimer.start()
    }

    if(Client.state.firstRunFlag) {
      wOnboarding.showOnboarding();
    }

    if(Client.state.clientHasBeenUpdated) {
      centerChangelogTimer.start();
    }

    // Check location metadata against the server list.  We only do this once at
    // startup, this could be a relatively expensive check if we applied it
    // every time the location list changed, and there's no sense repeating
    // these logs over and over anyway.
    //
    // Do this here because we know the locations have been loaded from the
    // daemon at this point (although it's also possible that the daemon itself
    // has not loaded the locations yet, the point of this is mainly to see
    // which locations are missing for devs).
    Daemon.state.checkLocationMetadata(Daemon.state.groupedLocations)
  }
}
