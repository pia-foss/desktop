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
// NotificationStatus stores the definition and status of one notification.
QtObject {
  // These properties define the notification - these are constants that usually
  // don't change.  (A few notifications, like the 'update available'
  // notification, do change message/links/etc.)

  // The title displayed for this notification in the header
  property string title
  // The message displayed for this notification on the connect page
  property string message
  // Links to display with this message - array of objects with 'text' and
  // 'clicked' per MessageWithLinks
  // By default, no links.
  property var links: []
  // Tip to display in an InfoTip in the client UI, if set
  // The InfoTip and dismiss X occupy the same part of the notification; a
  // notification shouldn't have both.
  property string tipText: ""
  // The severity to use when this notification is active (use a value from
  // ClientNotifications.severities)
  property int severity
  // Whether the notification can be dismissed - false by default
  property bool dismissible: false
  // The entire notification is clickable and will emit the clicked signal.
  // Should only be used if the notification has a very clear intuitive
  // action associated with it (e.g. "Reconnect to apply settings").
  property bool clickable: false

  // These properties indicate the current state of the notification.  These are
  // updated by ClientNotifications as the notification activates, is dismissed,
  // etc.

  // Whether the notification is active right now (meaning this condition is
  // present, regardless of whether it has been dismissed).  This is bound by
  // ClientNotifications or by a more specific type of notification like
  // TimestampNotificationStatus.
  property bool active

  // Whether the notification has been dismissed.  This is normally bound by
  // more specific types like TimestampNotificationStatus, use that type's
  // dismiss() method when the user wants to dismiss the notification.
  property bool dismissed: false

  // Progress of the state represented by the notification - if this is in the
  // range 0-100, the notification list displays it as a progress bar with a
  // "stop" button.
  property int progress: -1
  // If the progress bar is displayed, and the user clicks the 'stop' button,
  // this signal is emitted
  signal stop()

  // Signal emitted for clickable notifications
  signal clicked()

  // Whether the notification's message should be shown (it is active and hasn't
  // been dismissed)
  readonly property bool showMessage: active && !dismissed
}
