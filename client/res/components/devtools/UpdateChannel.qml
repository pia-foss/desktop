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
import "../client"
import "../daemon"

Row {
  id: updateChannel

  property string label

  // Name of the Daemon.settings property that defines this channel
  // (updateChannel or betaUpdateChannel)
  property string channelPropName

  // Value of the property identified by channelPropName, needed to be able to
  // connect to a change signal for it.
  readonly property string channelPropValue: Daemon.settings[channelPropName]

  spacing: 5

  Text {
    anchors.verticalCenter: parent.verticalCenter
    width: 40
    text: label
  }
  TextField {
    id: updateChannelValue
    anchors.verticalCenter: parent.verticalCenter
    Connections {
      target: updateChannel
      function onChannelPropValueChanged() {
        updateChannelValue.text = updateChannel.channelPropValue
      }
    }
    Component.onCompleted:  {
      updateChannelValue.text = updateChannel.channelPropValue
    }
  }
  Button {
    text: "Set channel"
    onClicked: {
      var settings = {}
      settings[updateChannel.channelPropName] = updateChannelValue.text
      Daemon.applySettings(settings)
    }
  }
}
