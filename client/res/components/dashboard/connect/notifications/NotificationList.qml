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

import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import "../../../theme"
import "../../../daemon"
import "../../../common"

// NotificationList displays the current active notifications.  These are laid
// out vertically and influence the NotificationList's implicitHeight.
Rectangle {
  implicitHeight: notifications.implicitHeight
  color: Theme.dashboard.notificationBackgroundColor

  // Note that this must be a Column, not a ColumnLayout, because the height of
  // the contents depends on its width (text wrapping).
  Column {
    id: notifications
    width: parent.width
    spacing: 0
    clip: true // Clip off bottoms of new notifications animating in/out

    // Show top and bottom margins only when at least one notification is
    // active.
    Item {
      id: topMargin
      width: parent.width
      height: 2
      visible: {
        for(var i=0; i<ClientNotifications.notifications.length; ++i) {
          if(ClientNotifications.notifications[i].showMessage)
            return true;
        }
        return false;
      }
    }

    Repeater {
      model: ClientNotifications.notifications

      Notification {
        width: parent.width
        status: modelData
      }
    }

    // Bottom margin
    Item {
      width: parent.width
      // Subtract 1 because of the one-pixel border on the first module
      // immediately below the notifications list.
      height: topMargin.height - 1
      visible: topMargin.visible
    }
  }
}
