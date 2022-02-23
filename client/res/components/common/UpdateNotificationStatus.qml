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
import "../daemon"
import "../../javascript/util.js" as Util
import PIA.NativeHelpers 1.0

// UpdateNotificationStatus is a NotificationStatus for piaX updates.
//
// It's a specialized object instead of just a NotificationStatus bound by
// ClientNotifications because the update notification transitions through
// multiple states as an update is available, downloading, and ready to install.
//
// It's preferred here to have one notification go through several states rather
// than several notifications that each represent one of the states, because
// the notifications list will transition smoothly between states rather than
// hiding the prior state and showing the next.
NotificationStatus {
  readonly property var states: {
    // Nothing to show in this notification
    'inactive': 0,
    // An update is available to download.
    'available': 1,
    // An update is being downloaded right now.
    'downloading': 2,
    // An update is ready to install.
    'ready': 3
  }

  readonly property int state: {
    // Show the 'ready to install' state if an installer has been downloaded,
    // and that version is still the version advertised by the server.
    //
    // Ignore the downloaded installer if the advertised version is changed -
    // there might be a newer version already, or the server could have rolled
    // back if this version was bad, etc.  (We haven't forgotten about this
    // installer though, if the server returns to this version for some reason
    // we go back to the 'ready' state and use the existing download.)
    if(Daemon.state.updateInstallerPath) {
      if(Daemon.state.updateVersion === Daemon.state.availableVersion)
        return states.ready
      else {
        // This case is somewhat obscure, log that we're ignoring a download
        // that we completed.
        console.log('Ignoring downloaded version ' + Daemon.state.updateVersion +
                    ', version ' + Daemon.state.availableVersion + ' is advertised now')
      }
    }
    if(Daemon.state.updateDownloadProgress >= 0)
      return states.downloading
    if(Daemon.state.availableVersion)
      return states.available
    return states.inactive
  }

  function startDownload() {
    // If the download completes successfully or fails due to error, show a
    // notification if the dashboard is hidden.
    // Only do this on the client that initiated the download.
    // Don't show any notification if the download is canceled.
    //
    // For any of these notifications, use the version provided with the result,
    // because that's the version that was being downloaded.  (Daemon defers
    // DaemonData updates until idle, so we can't guarantee that updateVersion
    // is up-to-date in this callback.)
    Daemon.downloadUpdate(function(error, result){
      if(error) {
        // If an error occurred, we failed to send the request at all (the
        // daemon connection might be down).  The daemon never rejects a
        // download request this way, it resolves it in order to provide the
        // downloaded version.
        console.log('Download request rejected: ' + error)
      }
      else if(result.failed)
      {
        // The download failed.
        TrayIcon.showNotification(uiTr("Download of v%1 failed").arg(Util.shortVersion(result.version)))
      }
      else if(result.succeeded)
      {
        // The download failed.
        TrayIcon.showNotification(uiTr("Ready to install v%1").arg(Util.shortVersion(result.version)))
      }
      else
      {
        // The download was canceled, or no update was available.
        console.log('Download of version \"' + result.version + '\" was canceled')
      }
    })
  }

  function launchInstaller(installerPath) {
    if(!NativeHelpers.launchInstaller(installerPath)) {
      // TODO - We should show a modal for this error (immediate failure to
      // perform a user action)
      console.error("Failed to execute installer " + installerPath)
    }
  }

  // Used by header menu and tray menu
  readonly property bool showInMenus: state !== states.inactive
  readonly property bool enableInMenus: {
    return state === states.available || state === states.ready
  }
  readonly property string menuText: {
    switch(state) {
      default:
      case states.inactive:
        return ""
      case states.available:
        return uiTr("Download v%1").arg(Util.shortVersion(Daemon.state.availableVersion))
      case states.downloading:
        return uiTr("(%2%) Install v%1").arg(Util.shortVersion(Daemon.state.updateVersion))
          .arg(Daemon.state.updateDownloadProgress)
      case states.ready:
        return uiTr("Install v%1").arg(Util.shortVersion(Daemon.state.updateVersion))
    }
  }
  function menuSelected() {
    switch(state) {
      default:
      case states.inactive:
      case states.downloading:
        break
      case states.available:
        startDownload()
        break
      case states.ready:
        launchInstaller(Daemon.state.updateInstallerPath)
        break
    }
  }

  property string dismissedVersion

  title: "" // Not used for info-level notifications
  message: {
    switch(state)
    {
    default:
    case states.inactive:
      return ""
    case states.available:
      return uiTr("Version %1 is available.").arg(Util.shortVersion(Daemon.state.availableVersion))
    case states.downloading:
      return uiTr("Downloading v%1...").arg(Util.shortVersion(Daemon.state.updateVersion))
    case states.ready:
      return uiTr("Ready to install v%1.").arg(Util.shortVersion(Daemon.state.updateVersion))
    }
  }

  links: {
    switch(state)
    {
    default:
    case states.inactive:
      return []
    case states.available:
      return [{
        text: uiTr("Download"),
        clicked: function(){startDownload()}
      }]
    case states.downloading:
      return []
    case states.ready:
      return [{
        text: uiTr("Install"),
        clicked: function(){launchInstaller(Daemon.state.updateInstallerPath)}
      }]
    }
  }

  dismissible: state === states.available

  active: state !== states.inactive

  dismissed: state === states.available &&
    Daemon.state.availableVersion === dismissedVersion

  progress: Daemon.state.updateDownloadProgress
  onStop: Daemon.cancelDownloadUpdate()

  // If at any point the notification is no longer dismissed, clear
  // dismissedVersion so we won't implicitly dismiss later.
  // For example, if the user dismisses a version, then starts downloading it,
  // but cancels the download, the notification should remain visible instead of
  // hiding again.  Or, if the server rolls back a dismissed version to a prior
  // version, then back to the newer version that was dismissed, we should show
  // the newer version again.
  onStateChanged: {
    if(state !== states.available)
      dismissedVersion = ""
  }
  property var stateConnections: Connections {
    target: Daemon.state
    function onAvailableVersionChanged() {
      if(Daemon.state.availableVersion !== dismissedVersion)
        dismissedVersion = ""
    }
  }

  function dismiss() {
    dismissedVersion = Daemon.state.availableVersion
  }

  // Show a notification for a new update version only when it is first
  // observed.  (Don't re-show it every time the client starts.)
  property string lastAvailableVersion: ""
  function checkInitialConnection() {
    if(Daemon.connected) {
      Daemon.onConnectedChanged.disconnect(checkInitialConnection)
      initVersionNotification()
    }
  }
  function initVersionNotification() {
    lastAvailableVersion = Daemon.state.availableVersion
    Daemon.state.onAvailableVersionChanged.connect(function(){
      if(Daemon.state.availableVersion !== lastAvailableVersion) {
        lastAvailableVersion = Daemon.state.availableVersion
        if(Daemon.state.availableVersion)
          TrayIcon.showNotification(uiTr("Version %1 is available").arg(Util.shortVersion(Daemon.state.availableVersion)))
      }
    })
  }
  Component.onCompleted: {
    // Init the version notification once the connection is established
    Daemon.onConnectedChanged.connect(checkInitialConnection)
    // We might already be connected, check now
    checkInitialConnection()
  }
}
