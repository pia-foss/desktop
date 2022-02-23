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
import QtQuick.Window 2.10
import PIA.NativeHelpers 1.0
import "../core"

QtObject {
  id: dashFrameLoader

  // Whether to use the windowed or popup frame.  Changing this property will
  // destroy and re-create the dashboard.
  property bool windowed: false

  property alias window: frameLoader.item
  property Window settingsWindow

  // Emitted whenever the dashboard is loaded - including the initial load and
  // for any change to 'windowed'
  signal dashboardLoaded()

  // Force the dashboard to be reloaded.  Used in main.qml to work around Qt
  // scene graph bugs - see main.qml.
  function forceReload() {
    console.log('Forcing unload of dashboard')
    forcedUnload = true
    console.log('Reloading dashboard')
    forcedUnload = false
  }

  property bool forcedUnload: false

  property Component windowComponent: Component {
    DashboardWindow {
      content: dashFrameLoader.content
      contents: dashFrameLoader.contents
    }
  }
  property Component popupComponent: Component {
    DashboardPopup {
      content: dashFrameLoader.content
      contents: dashFrameLoader.contents
      settingsWindow: dashFrameLoader.settingsWindow
    }
  }

  // Load either a DashboardWindow or DashboardPopup depending on the platform
  property Loader loader: Loader {
    id: frameLoader
    sourceComponent: {
      if(forcedUnload)
        return undefined

      return windowed ? windowComponent : popupComponent
    }
    onLoaded: dashboardLoaded()
  }

  // The parent binds the content parameters that define the frame size and
  // state, and it adds children that will be displayed inside the frame.
  //
  // We can't alias these properties, since we don't have a static object ID
  // for the DashboardWindow/DashboardPopup that we can reference.  Instead,
  // define equivalent properties here and bind them into the frame chosen.
  // This works because the frame doesn't need to bind its own values here that
  // our parent observes, it just reads the values bound into it.
  readonly property DashboardContentInterface content: DashboardContentInterface{}
  default property var contents
}
