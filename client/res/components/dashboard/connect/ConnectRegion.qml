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
import QtQuick.Layouts 1.3
import "../../daemon"
import "../../theme"
import "../../vpnconnection"
import "./notifications"

// ConnectRegion is the part of the ConnectPage containing the connect button
// and notifications.  This part of the page slides out of view when the page is
// expanded.
Item {
  implicitHeight: notifications.y + notifications.height

  ActiveRuleDisplay {
    id: activeRuleDisplay
    height: enabled ? 34 : 0
    width: parent.width
    visible: enabled
  }

  // Place margins around the connect button (the connect button sizes itself)
  Item {
    id: buttonWrapper
    width: parent.width
    anchors.top: activeRuleDisplay.bottom
    height: cb.height + 2*Theme.dashboard.connectButtonVMarginPx

    ConnectButton {
      id: cb
      anchors.centerIn: parent
      onClicked: {
        if (Daemon.state.connectionState === "Disconnected") {
          VpnConnection.connectCurrentLocation()
        }
        else {
          Daemon.disconnectVPN()
        }
      }
    }
  }

  NotificationList {
    id: notifications
    anchors.top: buttonWrapper.bottom
    width: parent.width
  }
}
