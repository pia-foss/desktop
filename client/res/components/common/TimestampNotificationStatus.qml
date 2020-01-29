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
// TimestampNotificationStatus represents a notification that's expressed by the
// daemon as a timestamp.  These notifications can be dismissible;
// TimestampNotificationStatus keeps track of this dismiss state.
NotificationStatus {
  // The timestamp value for this notification expressed by the daemon (usually
  // a value from DaemonState)
  property real timestampValue

  // If dismissible, this is set when the user dismisses the notification
  property real _dismissedTimestamp: 0

  // Active when a nonzero timestamp is given.
  active: timestampValue > 0
  // Dismissed if this specific instance of the notification has been dismissed
  dismissed: _dismissedTimestamp > 0 && _dismissedTimestamp === timestampValue

  // Dismiss the notification - the notification displayed in the list calls
  // this when the user clicks the "X"
  function dismiss() {
    _dismissedTimestamp = timestampValue
  }
}
