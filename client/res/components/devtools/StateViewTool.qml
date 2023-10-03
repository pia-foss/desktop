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

import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import "../../javascript/app.js" as App
import "../client"
import "../daemon"

Item {
  id: tool

  // Populate the diagnostic output with a JSON representation of some object.
  // - obj - the object to display
  // - except - if specified, array of property names to exclude (the object is
  //   cloned, and the properties are deleted from the clone)
  // - only - if specified, array of property names to include (otherwise all
  //   properties are included)
  function populate(obj, except, only) {
    // If only some properties are desired, just take those properties
    if(only) {
      let orig = obj
      obj = {}
      for(let i=0; i<only.length; ++i)
        obj[only[i]] = orig[only[i]]
    }
    // If excluded properties were given, clone the object and remove those
    // properties
    if(except) {
      obj = Object.assign({}, obj)
      for(let i=0; i<except.length; ++i)
        delete obj[except[i]]
    }
    output.text = JSON.stringify(obj, null, 2)
  }

  ToolBar {
    id: stateToolbar
    anchors.top: parent.top
    width: parent.width

    RowLayout {
      ToolButton {
        id: locationButton
        text: "Locations"
        onClicked: tool.populate(Daemon.state.availableLocations)
      }
      ToolButton {
        text: "Settings"
        onClicked: tool.populate(Daemon.settings)
      }
      ToolButton {
        text: "ClientSettings"
        onClicked: tool.populate(Client.settings)
      }
      ToolButton {
        text: "State"
        // Locations, grouped locations, and the service locations are shown
        // separately, they're huge and usually not important when viewing state.
        // Location metadata is mostly static.
        onClicked: tool.populate(Daemon.state,
                                 ["availableLocations", "regionsMetadata",
                                  "groupedLocations", "vpnLocations",
                                  "shadowsocksLocations"])
      }
      ToolButton {
        text: "ClientState"
        onClicked: tool.populate(Client.state)
      }
      ToolButton {
        text: "Data"
        onClicked: tool.populate(Daemon.data)
      }
      ToolButton {
        text: "Grouped Locations"
        onClicked: tool.populate(Daemon.state.groupedLocations)
      }
      ToolButton {
        text: "Service Locations"
        onClicked: tool.populate(Daemon.state, [],
                                 ["vpnLocations", "shadowsocksLocations"])
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
  }
}
