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

import QtQuick 2.10
import QtQml 2.11
import PIA.NativeHelpers 1.0
import PIA.PreConnectStatus 1.0
import PIA.Tray 1.0
import "./splash"
import "./dashboard"
import "./theme"
import "./common"

QtObject {
  // The SplashPopup has to be created dynamically so we can destroy it
  // dynamically
  property Component splashComponent: Component {
    SplashPopup {
    }
  }
  property SplashPopup splash

  property var showSplashHandler: Connections {
    target: NativeHelpers
    function onDashboardOpenRequested() {
      if(splash)  {
        splash.window.trayClicked(TrayIcon.getIconMetrics());
      }
    }
  }


  Component.onCompleted: {
    var splashObj = splashComponent.createObject()
    splash = splashObj
    PreConnectStatus.initialConnect.connect(function(){
      splash = null
      splashObj.destroy()
    })
  }
}
