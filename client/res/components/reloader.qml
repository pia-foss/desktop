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

import QtQuick 2.10
import PIA.NativeHelpers 1.0

QtObject {
  property var rootLoader: Loader {
    active: false
  }

  property var reloadTimer: Timer {
    interval: 500
    onTriggered: {
      console.log("Reloading file: ", params.qml_reload_entry)
      rootLoader.source = params.qml_reload_entry;
      rootLoader.active = true;
      rootLoader.item.reloaderActive = true;
    }
  }

  property var reloadShortcut: Shortcut {
    sequence: "Ctrl+Shift+R"
    context: Qt.ApplicationShortcut
    onActivated: {
      console.log("Reloading")
      rootLoader.active = false;
      NativeHelpers.clearComponentCache();
      reloadTimer.start();
    }
  }

  Component.onCompleted: {
    reloadTimer.start();
  }

  property var params: QtObject {
    id: params
    objectName: "params"
    property string qml_reload_entry: ""
  }
}
