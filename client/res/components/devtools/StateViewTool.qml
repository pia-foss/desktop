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

Item {

  ToolBar {
    id: stateToolbar
    anchors.top: parent.top
    width: parent.width
    RowLayout {
      ToolButton {
        id: locationButton
        text: "Locations"
        onClicked: {
          output.text = JSON.stringify(Daemon.data.locations, null, 2);
        }
      }
      ToolButton {
        text: "Settings"
        onClicked: {
          output.text = JSON.stringify(Daemon.settings, null, 2);
        }
      }
      ToolButton {
        text: "ClientSettings"
        onClicked: {
          output.text = JSON.stringify(Client.settings, null, 2);
        }
      }
      ToolButton {
        text: "State"
        onClicked: {
          output.text = JSON.stringify(Daemon.state, null, 2);
        }
      }
      ToolButton {
        text: "ClientState"
        onClicked: {
          output.text = JSON.stringify(Client.state, null, 2);
        }
      }
      ToolButton {
        text: "Data"
        onClicked: {
          output.text = JSON.stringify(Daemon.data, null, 2);
        }
      }
    }
  }

  ScrollView {
    anchors.top: stateToolbar.bottom
    anchors.bottom: parent.bottom
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.margins: 5

    TextArea {
      id: output

      selectByMouse: true
      readOnly: true
    }

    Component.onCompleted: {
      locationButton.clicked();
    }
  }
}
