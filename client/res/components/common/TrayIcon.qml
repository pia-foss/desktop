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

pragma Singleton
import QtQuick 2.10
import QtQuick.Window 2.11
import PIA.Tray 1.0
import "../client"
import "../daemon"
import "../helpers"

// TrayIcon is a singleton that manages the single tray icon used by the entire
// app.  This is separate from TrayManager to allow the initial "loading" splash
// screen to set up a minimal icon and menu until the daemon connection is
// ready.  Once we connect to the daemon, this is managed by TrayManager.
TrayIconManager {
  property var currentStateIcon: {
    if(connState.snoozeModeEnabled && connState.snoozeDisconnected) {
      return TrayIconManager.Snoozed
    }

    switch (connState.connectionState) {
    default:
    case connState.stateDisconnected:
      return TrayIconManager.Disconnected
    case connState.stateConnected:
      return TrayIconManager.Connected
    case connState.stateConnecting:
      return TrayIconManager.Connecting
    case connState.stateDisconnecting:
      return TrayIconManager.Disconnecting
    }
  }

  icon: {
    // An error/warning from ClientNotifications overrides the icon state
    // right now.  This might not be appropriate for all future
    // errors/warnings, but it is correct for all the warnings we have now:
    // - the auth failure warning occurs only in the disconnected state
    // - the connection lost / connection trouble messages occur only while
    //   connecting or reconnecting
    if(ClientNotifications.worstSeverity >= ClientNotifications.severities.warning)
      return TrayIconManager.Alert

    return currentStateIcon
  }

  toolTip: {
    var base = uiTr("Private Internet Access");
    if(connState.snoozeModeEnabled) {
      switch(connState.snoozeState) {
        case connState.snoozeConnecting:
        return base + ": " + uiTr("Resuming Connection...")
        case connState.snoozeDisconnecting:
        return base + ": " + uiTr("Snoozing...")
        case connState.snoozeDisconnected:
        return base + ": " + uiTr("Snoozed")
      }
    }

    switch (connState.connectionState) {
    case connState.stateConnected:
      return base + ": " + uiTr("Connected to %1").arg(Daemon.getLocationName(Daemon.state.connectedConfig.vpnLocation))
    case connState.stateConnecting:
      if (Daemon.state.connectionState.indexOf('Reconnect') >= 0)
        return base + ": " + uiTr("Reconnecting...")
      else
        return base + ": " + uiTr("Connecting...")
    case connState.stateDisconnecting:
      return base + ": " + uiTr("Disconnecting...")
    default:
      break
    }
    if(Daemon.state.killswitchEnabled)
      return base + ": " + uiTr("Killswitch Active")
    if(ClientNotifications.updateAvailable.showInMenus)
      return base + ": " + uiTr("Update Available")
    return base
  }

  readonly property ConnStateHelper connState: ConnStateHelper {}

  // The dashboard (when it exists) - used to suppress notifications when it's
  // shown.
  property Window dashboard

  // Show a notification.
  // - If the dashboard is visible, or if notifications are disabled, the
  //   message is ignored.
  // - The 'message' line of the popup is set to Private Internet Access.
  //
  // Notifications are annoying when the dashboard is shown, because they
  // usually obscure it.  Anything presented by a notification should be
  // visible in the dashboard anyway, so the user can already see it if the
  // dashboard is open.
  //
  // This should normally only be used after the daemon connection is
  // established - if used before, we don't know the user's notification setting
  // yet (they're enabled by default), and the dashboard doesn't exist yet.
  function showNotification(title, subtitle) {
    hideMessage()
    if(dashboard && dashboard.suppressNotifications)
      console.log('Ignoring notification because dashboard is suppressing it: ' + title)
    else if(!Client.settings.desktopNotifications)
      console.log('Ignoring notification because notifications are disabled: ' + title)
    else
      showMessage(currentStateIcon, title, subtitle)
  }
}
