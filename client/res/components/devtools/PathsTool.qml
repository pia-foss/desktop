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

import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import "../../javascript/app.js" as App
import "../client"
import "../daemon"
import PIA.PathInterface 1.0

Item {
  ColumnLayout {
    anchors.fill: parent

    Repeater {
      model: {
          var paths = [
            { name: "Daemon Log File",      path: PathInterface.daemonLogFile() },
            { name: "Client Log File",      path: PathInterface.clientLogFile() },
            { name: "Legacy Region Override", path: PathInterface.legacyRegionOverride() },
            { name: "Legacy Bundled Regions", path: PathInterface.legacyRegionBundle() },
            { name: "Modern Region Override", path: PathInterface.modernRegionOverride() },
            { name: "Modern Bundled Regions", path: PathInterface.modernRegionBundle() },
            { name: "Region Meta Override", path: PathInterface.modernRegionMetaOverride() },
            { name: "Bundled Region meta", path: PathInterface.modernRegionMetaBundle() },
            { name: "Daemon Settings Dir",  path: PathInterface.daemonSettingsDir() },
            { name: "Client Settings Dir",  path: PathInterface.clientSettingsDir() },
            { name: "Daemon Data Dir",      path: PathInterface.daemonDataDir() },
          ]
          if (Qt.platform.os === "linux") paths.push({ name: "Linux AutoStart File", path: PathInterface.linuxAutoStartFile()})
          paths
      }
      delegate: pathsComponent
    }
  }

  Component {
    id: pathsComponent

    Item {
      width: childrenRect.width
      height: childrenRect.height + 2

      Button {
        id: pathButton
        width: 170
        text: modelData.name

        onClicked: {
          PathInterface.showFileInSystemViewer(modelData.path)
        }
      }

       TextArea {
        anchors.left: pathButton.right
        text: modelData.path
        selectByMouse: true
        readOnly: true
      }
    }
  }
}
