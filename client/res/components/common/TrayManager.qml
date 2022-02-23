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
import QtQuick.Window 2.11
import "../../javascript/app.js" as App
import "../common"
import "../helpers"
import "../daemon"
import "../client"
import PIA.Tray 1.0

Item {
  // The dashboard window that this tray icon will control.  (This is either a
  // DashboardWindow or DashboardPopup.)
  property Window dashPopup

  TrayMenuBuilder {
    id: builder
    dashboard: dashPopup
  }
  function getIconMetrics() {return TrayIcon.getIconMetrics()}

  // This stores the last notification that was (or would have been) shown
  // so we don't re-display it on duplicate state-change notifications.
  // This is the message's title, the subtitle does not influence this (for
  // example, if the killswitch state changed and we get a duplicate
  // 'disconnected' state change, we would not want to re-show the
  // disconnected notification).
  property string lastNotification: ''

  function showConnectionMsg(title, subtitle) {
    // Show the notification only if it changed since the last notification
    // We can get duplicate state-change notifications (such as if another
    // client connects and causes the state to be sent out again), don't send
    // spurious notifications for these.
    if (lastNotification === title) {
      return
    }

    // Update the last notification.  (Do this even if we don't actually
    // display it, since the point is that the user has seen this information
    // one way or another.)
    lastNotification = title

    // TrayIcon might not actually show this notification if the user has
    // disabled them or the dashboard says to suppress them.
    // It's important that we still update the last notification message though,
    // as if we had displayed it, because we could receive a duplicate state
    // transition later.
    TrayIcon.showNotification(title, subtitle)
  }

  // Get the current message to show.  This returns a title and subtitle packed
  // into an object.
  function getCurrentMessage() {
    var message = {title: "", subtitle: ""}

    if(connState.snoozeModeEnabled) {
      switch(connState.snoozeState) {
      case connState.snoozeDisconnecting:
        message.title = uiTr("Snoozing")
        break;
      case connState.snoozeConnecting:
        message.title = uiTr("Resuming Connection")
        break;
      case connState.snoozeDisconnected:
        message.title = uiTr("Snoozed")
        break;
      }

      if(message.title)
        return message;
    }

    switch (connState.connectionState) {
    default:
    case connState.stateDisconnected:
      // The daemon does not necessarily transition to the Disconnected state
      // and update killswitchEnabled at exactly the same time.  Check if
      // killswitch is set to 'on' (not 'auto') and include the killswitch
      // message if it is - we trust that the daemon will activate/deactivate
      // the killswitch according to this setting.
      if(Daemon.settings.killswitch === 'on')
        message.subtitle = uiTr("Internet access has been disabled by the killswitch.")

      message.title = Daemon.state.connectedConfig.vpnLocation ?
                        uiTr("Disconnected from %1").arg(Daemon.getLocationName(Daemon.state.connectedConfig.vpnLocation)) :
                        uiTr("Disconnected")
      break
    case connState.stateConnected:
      message.title = uiTr("Connected to %1").arg(Daemon.getLocationName(Daemon.state.connectedConfig.vpnLocation))

      // When connected with port forwarding enabled, check the port forward
      // status.
      // Note that the daemon may not update from inactive -> attempting
      // immediately when the connection occurs if PF is on, so we check whether
      // the option is enabled in settings.
      if(Daemon.settings.portForward) {
        // If port forwarding is turned on when we connect, we defer the
        // notification until the port forward completes.  If it's turned on
        // later, we don't show a duplicate 'connected' notification, since it was
        // already shown (it's in lastNotification).
        switch(Daemon.state.forwardedPort) {
          default:
            // A port has been forwarded.
            message.subtitle = uiTr("Forwarded port %1").arg(Daemon.state.forwardedPort)
            break
          case Daemon.state.portForward.inactive:
          case Daemon.state.portForward.attempting:
            // Still waiting, do not show any notification yet.
            // Note that if we connected to a region that doesn't support PF,
            // but PF is on, we might initially hit this case.  If we do, we'll
            // show the connect notification when the daemon changes to
            // portForward.unavailable
            message.title = ""
            break
          case Daemon.state.portForward.failed:
            // The port forward failed
            message.subtitle = uiTr("Port forward request failed")
            break
          case Daemon.state.portForward.unavailable:
            // No subtitle when connecting to a region that doesn't support PF -
            // we display this in the regions list and in the IP block, including
            // a subtitle here would likely be more annoying than useful.
            break
        }
      }

      break
    case connState.stateConnecting:
      // If we're reconnecting to the same location, display the reconnecting
      // message.
      // Otherwise, display the connecting message, even if this is a reconnect;
      // the phrasing "reconnecting to <name>" is misleading then - although
      // we're reconnecting, it's the first connection to that location
      if (Daemon.state.connectionState.indexOf("Reconnect") >= 0 &&
          Daemon.state.connectingConfig.vpnLocation.id === Daemon.state.connectedConfig.vpnLocation.id)
        message.title = uiTr("Reconnecting to %1...").arg(Daemon.getLocationName(Daemon.state.connectingConfig.vpnLocation))
      else
        message.title = uiTr("Connecting to %1...").arg(Daemon.getLocationName(Daemon.state.connectingConfig.vpnLocation))
      break
    case connState.stateDisconnecting:
      message.title = Daemon.state.connectedConfig.vpnLocation ?
                        uiTr("Disconnecting from %1...").arg(Daemon.getLocationName(Daemon.state.connectedConfig.vpnLocation)) :
                        uiTr("Disconnecting...")
      break
    }
    return message
  }

  ConnStateHelper {
    id: connState
  }

  // Whenever the dashboard becomes visible, hide any notification that might
  // be shown.  (It annoyingly obscures the dashboard otherwise, at least on
  // OS X.)
  // In general, do this whenever the dashboard says we should be suppressing
  // notifications (for the windowed dashboard, this depends on focus, not
  // visibility).
  Connections {
    target: dashPopup
    function onSuppressNotificationsChanged() {
      if (dashPopup.suppressNotifications)
        TrayIcon.hideMessage()
    }
  }

  Connections {
    target: TrayIcon
    function onTrayClicked(metrics) {
      dashPopup.trayClicked(metrics)
    }

    function onItemActivated(code) {
      builder.handleSelection(code);
    }
  }

  function showCurrentMessage() {
    var message = getCurrentMessage()
    if(message.title)
      showConnectionMsg(message.title, message.subtitle)
  }

  Component.onCompleted: {
    // Set the message that would have been shown due to the initial state, so
    // any duplicate state changes after this will not show a redundant
    // notification.
    lastNotification = getCurrentMessage().title
    // Hook up notifications with a signal handler, now that the initial state
    // is set.
    connState.onConnectionStateChanged.connect(showCurrentMessage)
    connState.onSnoozeStateChanged.connect(showCurrentMessage)
    Daemon.state.onForwardedPortChanged.connect(showCurrentMessage)
  }
}
