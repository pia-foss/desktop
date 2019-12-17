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

import QtQuick 2.10
import "../dashboard"
import "../theme"
import "../common"
import PIA.Tray 1.0

DashboardFrameLoader {
  id: popup

  // The 'splash' dashboard is always windowed on Linux, popup on other
  // platforms. The windowed-dashboard preference does not affect the splash
  // dashboard.
  windowed: Qt.platform.os === 'linux'

  content.pageHeight: Theme.splash.height
  content.maxPageHeight: Theme.splash.height
  content.headerTopColor: Theme.dashboard.backgroundColor

  SplashContent {
    id: content

    anchors.fill: parent
    contentRadius: popup.window.contentRadius

    Connections {
      target: TrayIcon

      onTrayClicked: function(metrics) {
        popup.window.trayClicked(metrics)
      }
      onItemActivated: {
        if(code === 'quit')
        {
          console.info("Quit from splash dashboard tray menu")
          Qt.quit()
        }
      }

      Component.onCompleted: {
        TrayIcon.setMenuItems([{ text: uiTr("Quit"), code: 'quit' }])
      }
    }
  }
}
