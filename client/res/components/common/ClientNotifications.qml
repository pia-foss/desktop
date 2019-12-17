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

pragma Singleton
import QtQuick 2.9
import "../daemon"
import PIA.NativeHelpers 1.0
import "../client"
import "../settings"
import PIA.BrandHelper 1.0

// ClientNotifications determines which notifications should be shown by the
// client and how they are shown.
//
// It provides a list of notification statuses used by NotificationList, as well
// as aggregate information used to control the states of the tray icon,
// connect button, and header bar.
Item {
  // Possible severities for notifications.  These are in ascending order so
  // worse severities are greater.
  readonly property var severities: {
    // 'none' indicates in ClientNotifications.worstSeverity that no
    // notifications are active.  It is not a valid value for an individual
    // NotificationStatus's severity
    'none': -1,
    'info': 0, // Just informative, not a problem (used for updates)
    'warning': 1,
    'error': 2
  }

  // The header title for all error-level notifications is currently "ERROR".
  // It's generally difficult to put any meaningful description in the very
  // limited header space, the user has to read the notification anyway.
  //
  //: Header bar title used for all "error" statuses - serious installation
  //: problems, etc.  This means that there is currently an error condition
  //: active now.
  readonly property var errorHeaderTitle: uiTr("ERROR", "header-title")

  // === Notification objects ===
  // These define each possible notification.  Keep these in the same order as
  // 'notifications' below.

  // Update available
  // This notification is read directly by the header menu, tray menu, etc., so
  // it's exposed as a property.
  property UpdateNotificationStatus updateAvailable: UpdateNotificationStatus {
    severity: severities.info
  }

  // Downloading an update failed
  TimestampNotificationStatus {
    id: downloadFailed
    title: uiTr("UPDATE FAILED")
    message: uiTr("Download of version %1 failed.").arg(Daemon.state.updateVersion)
    severity: severities.warning
    dismissible: true
    timestampValue: Daemon.state.updateDownloadFailure
  }

  // TAP adapter isn't found (Windows only)
  //
  // This isn't dismissible - it's unlikely that we detect this incorrectly, and
  // it completely breaks connectivity.  If somehow we did fail to detect a
  // change in the adapter, a successful connection would remove it when it
  // finds the adapter.
  //
  // This is before auth failure - it's unlikely that both of these would appear
  // at once, but if they do, the auth failure can't be resolved until the TAP
  // adapter is back so we can connect again.
  NotificationStatus {
    id: tapAdapterMissing
    title: errorHeaderTitle
    message: uiTr("The virtual network adapter is not installed.")
    //: "TAP" is the type of virtual network adapter used on Windows and is not
    //: generally localized.
    tipText: uiTr("The TAP adapter for the VPN tunnel is not installed.  You can reinstall it from Settings.")
    severity: severities.error
    dismissible: false
    links: [{
      text: uiTr("Reinstall"),
      clicked: function() {reinstallTapAdapter()}
    }]

    // To avoid transient blips during the reinstall itself, we don't change
    // state during while the reinstall status is 'working'.
    property bool lastNonworkingMissing
    function updateMissingState() {
      if(NativeHelpers.reinstallTapStatus !== 'working')
        lastNonworkingMissing = Daemon.state.tapAdapterMissing
      // Otherwise, ignore the current state and keep the state from before the
      // reinstall
    }
    property var nhConns: Connections {
      target: NativeHelpers
      onReinstallTapStatusChanged: tapAdapterMissing.updateMissingState()
    }
    property var stateConns: Connections {
      target: Daemon.state
      onTapAdapterMissingChanged: tapAdapterMissing.updateMissingState()
    }

    // When the reinstallation status is 'reboot', show the reboot notification
    // instead.
    active: lastNonworkingMissing && NativeHelpers.reinstallTapStatus !== 'reboot'

    Component.onCompleted: tapAdapterMissing.updateMissingState()
  }

  // A reboot is required to complete installation of the TAP adapter.
  NotificationStatus {
    id: installationRestart
    title: errorHeaderTitle
    message: uiTr("Restart to complete installation.")
    tipText: uiTr("The system must be restarted before you can connect.")
    severity: severities.error
    dismissible: false
    active: NativeHelpers.reinstallTapStatus === 'reboot'
  }

  // Warn if the split tunnel feature is enabled, but the driver is missing.
  // This could happen if the driver was somehow uninsta..ed
  NotificationStatus {
    id: splitTunnelUninstalled
    title: errorHeaderTitle
    message: uiTr("The split tunnel filter is not installed.")
    tipText: uiTr("The App Exclusion feature requires the split tunnel filter.  Reinstall it from Settings.")
    severity: severities.error
    // Not dismissible - completely breaks the app exclusion feature.  If the
    // driver can't be installed, disable the Split Tunnel setting to clear this
    // error.
    dismissible: false
    active: Daemon.state.netExtensionState === "NotInstalled" &&
      Daemon.settings.splitTunnelEnabled &&
      NativeHelpers.reinstallWfpCalloutStatus !== "reboot" &&
      Qt.platform.os === 'windows'
    links: [{
      text: uiTr("Reinstall"),
      clicked: function() {reinstallSplitTunnel()}
    }]
  }

  // Split tunnel filter is enabled, but installation requires reboot.
  // This doesn't show up if the normal installation flow ends up requiring a
  // reboot - the setting is still disabled in that state.  This occurs if the
  // driver is unexpectedly uninstalled, then the reinstallation requires a
  // reboot.
  NotificationStatus {
    id: splitTunnelReboot
    title: errorHeaderTitle
    message: SettingsMessages.installReboot
    tipText: uiTr("The App Exclusion feature requires the split tunnel filter.  Restart to finish installation.")
    severity: severities.error
    dismissible: false
    active: Daemon.settings.splitTunnelEnabled &&
      NativeHelpers.reinstallWfpCalloutStatus === "reboot"
  }

  // Notification for the OpenVPN "authorization failure" error.
  // Although this OpenVPN error has a very specific meaning, for PIA this does
  // not mean that the user's credentials are incorrect.  For example, if the
  // user's account has expired, OpenVPN returns this error.  (They can still
  // authenticate with the web API in this state.)
  TimestampNotificationStatus {
    id: authFailure
    title: errorHeaderTitle
    message: uiTr("Connection refused.")
    //: This error could be caused by incorrect credentials or an expired
    //: account, but it could have other causes too.  The message should suggest
    //: checking those things without implying that they're necessarily the
    //: cause (to avoid frustrating users who are sure their account is
    //: current).
    tipText: uiTr("The server refused the connection.  Please check your username and password, and verify that your account is not expired.")
    severity: severities.error
    dismissible: false
    timestampValue: Daemon.state.openVpnAuthFailed
  }

  TimestampNotificationStatus {
    id: dnsConfigFailed
    title: errorHeaderTitle
    message: uiTr("Could not configure DNS.")
    tipText: uiTr("Enable debug logging and check the daemon log for specific details.")
    severity: severities.error
    // Used for both a link title and an error title
    readonly property string daemonLogStr: uiTr("Daemon Log")
    links: [{
      text: uiTr("Settings"),
      clicked: function() {showHelpPage()}
    },
    {
      text: dnsConfigFailed.daemonLogStr,
      clicked: function() {
        var failedLogFilePath = NativeHelpers.openDaemonLog()
        if(failedLogFilePath) {
          var msg = uiTr("Failed to run /usr/bin/xdg-open.  Please open the daemon log file from:")
          msg += "<br><pre>" + failedLogFilePath + "</pre>"
          showHelpAlert(msg, dnsConfigFailed.daemonLogStr, 'error')
        }
      }
    }]
    dismissible: false
    timestampValue: Daemon.state.dnsConfigFailed
  }

  // hnsd is failing - only occurs in Connected state, prevents any DNS
  // resolution when active
  TimestampNotificationStatus {
    id: hnsdFailing
    title: errorHeaderTitle
    //: Indicates that we can't connect to the Handshake name-resolution
    //: network.  "Handshake" is a brand name and should be left as-is.
    message: uiTr("Can't connect to Handshake.")
    //: Detailed message about failure to connect to the Handshake name-
    //: resolution network.  "Handshake" is a brand name and should be left
    //: as-is.
    tipText: uiTr("Can't set up name resolution with Handshake.  Continue waiting, or try a different Name Server setting.")
    severity: severities.error
    links: [{
      text: uiTr("Settings"),
      clicked: function() {showNetworkPage()}
    }]
    dismissible: false
    // Daemon reports these two conditions indicating problems in hnsd.  We show
    // the same warning for both of them since they overlap.
    active: Daemon.state.hnsdFailing > 0 || Daemon.state.hnsdSyncFailure > 0
  }

  NotificationStatus {
    id: winIsElevated
    message: uiTr("Running PIA as administrator is not recommended.")
    tipText: uiTr("Running PIA as administrator can prevent Launch on System Startup from working and may cause other problems.")
    severity: severities.info
    active: Client.state.winIsElevated
    //dismissible: true
    function dismiss() {
      dismissed = true;
    }
  }

  // Killswitch active
  NotificationStatus {
    id: killswitchEnabled
    title: uiTr("KILLSWITCH ENABLED")
    message: uiTr("Killswitch is enabled.")
    tipText: uiTr("Access to the Internet is blocked because the killswitch feature is enabled in Settings.")
    severity: severities.warning
    links: [{
      text: uiTr("Change"),
      clicked: function() {showPrivacyPage()}
    }]
    // The killswitch rules are active even while connecting/connected/etc., but
    // we only display it while disconnected (this is the only state when it
    // might be surprising that the Internet is not reachable).
    active: Daemon.state.killswitchEnabled &&
            Daemon.state.connectionState === "Disconnected"
  }

  // Connection lost
  TimestampNotificationStatus {
    id: connectionLost
    title: uiTr("RECONNECTING...")
    message: uiTr("The connection to the VPN server was lost.")
    severity: severities.warning
    dismissible: true
    timestampValue: Daemon.state.connectionLost
  }

  // Notification for the "proxy unreachable" error.
  TimestampNotificationStatus {
    id: proxyUnreachable
    title: uiTr("CONNECTING...")
    //: Warning message used when the app is currently trying to connect to a
    //: proxy, but the proxy can't be reached.
    message: uiTr("Can't connect to the proxy.")
    tipText: uiTr("The proxy can't be reached.  Check your proxy settings, and check that the proxy is available.")
    severity: severities.warning
    links: [{
      text: uiTr("Settings"),
      clicked: function() {showConnectionPage()}
    }]
    dismissible: false
    timestampValue: Daemon.state.proxyUnreachable
  }

  // Unable to connect.  This is the most generic error message, only show it if
  // no other more specific error applies.
  NotificationStatus {
    id: connectionTrouble
    title: uiTr("CONNECTING...")
    message: uiTr("Can't reach the VPN server.  Please check your connection.")
    severity: severities.warning
    active: Daemon.state.connectingStatus !== "Connecting" &&
            Daemon.state.connectionLost === 0 &&
            Daemon.state.proxyUnreachable === 0
  }

  NotificationStatus {
    id: reconnectNeeded
    title: uiTr("RECONNECT NEEDED")
    message: uiTr("Reconnect to apply settings.")
    tipText: uiTr("Some settings changes won't take effect until the next time you connect. Click to reconnect now.")
    clickable: true
    onClicked: { Daemon.connectVPN(); }
    severity: severities.info
    active: Daemon.state.needsReconnect
  }

  NotificationStatus {
    id: alternateTransport

    readonly property string chosenProtocol: Daemon.state.chosenTransport ? Daemon.state.chosenTransport.protocol : ""
    readonly property string chosenPort: Daemon.state.chosenTransport ? Daemon.state.chosenTransport.port : 0
    readonly property string actualProtocol: Daemon.state.actualTransport ? Daemon.state.actualTransport.protocol : ""
    readonly property string actualPort: Daemon.state.actualTransport ? Daemon.state.actualTransport.port : 0

    //: Message when the client automatically uses a transport other than the
    //: user's chosen transport (because the user's settings did not work).
    //: "Connected" means the client is currently connected right now using this
    //: setting.  %1 is the protocol used ("UDP" or "TCP"), and %2 is the port
    //: number.  For example: "UDP port 8080" or "TCP port 443".
    message: uiTr("Connected using %1 port %2.").arg(actualProtocol.toUpperCase()).arg(actualPort)
    tipText: {
      //: Detailed message when the client automatically uses an alternate
      //: transport.  "%1 port %2" refers to the chosen transport, and
      //: "%3 port %4" refers to the actual transport; for example
      //: "TCP port 443" or "UDP port 8080".  The "Try Alternate Settings"
      //: setting is on the Connection page.
      return uiTr("Try Alternate Settings is enabled.  The server could not be reached on %1 port %2, so %3 port %4 was used instead.")
        .arg(chosenProtocol.toUpperCase()).arg(chosenPort).arg(actualProtocol.toUpperCase()).arg(actualPort)
    }
    severity: severities.info
    links: [{
      text: uiTr("Settings"),
      clicked: function() { showConnectionPage() }
    }]
    active: Daemon.state.connectionState === "Connected" && (chosenProtocol !== actualProtocol || chosenPort !== actualPort)
  }

  // Account expiring with a manual renewal needed
  NotificationStatus {
    id: accountExpiring
    message: uiTr("Subscription expires in %1 days.").arg(Daemon.account.daysRemaining)
    severity: severities.warning
    links: [{
      text: uiTr("Renew"),
      clicked: function() { Qt.openUrlExternally(Daemon.account.renewURL) }
    }]
    active: (Daemon.account.loggedIn && Daemon.account.expireAlert && !Daemon.account.recurring && Daemon.account.renewURL) && BrandHelper.brandCode === 'pia'
  }


  NotificationStatus {
    id: accountTokenNotAvailable
    title: ''
    //: Dashboard notification for being unable to reach our main API server
    //: in order to authenticate the user's account. The phrase should convey
    //: that the problem is network related and that we are merely offline or
    //: "out of touch" rather than there being any account problem.
    message: uiTr("Unable to reach login server.")
    //: Infotip to explain to the user that a login authentication failure is
    //: not necessarily a critical problem, but that the app will have reduced
    //: functionality until this works.
    tipText: uiTr("Your account details are unavailable, but you may still be able to connect to the VPN.")
    severity: severities.info
    active: Daemon.account.loggedIn && Daemon.account.token === ''
    links: [{
      text: uiTr("Retry"),
      clicked: function() { Daemon.login(Daemon.account.username, Daemon.account.password); }
    }]
  }

  NotificationStatus {
    id: changelogAvailable
    severity: severities.info
    message: uiTr("Private Internet Access was updated.")
    dismissible: true
    links: [{
      text: uiTr("See what's new"),
      clicked: function() {
        showChangelog();
      }
    }]
    active: Client.state.clientHasBeenUpdated
    function dismiss() {
      dismissed = true;
    }
  }

  NotificationStatus {
    id: invalidClientExit
    severity: severities.info
    //: Indicates that Private Internet Access had previously crashed or
    //: otherwise stopped unexpectedly - shown the next time the user starts the
    //: app.
    message: uiTr("The application quit unexpectedly. Your VPN connection was preserved.")
    dismissible: true
    active: Daemon.state.invalidClientExit
    function dismiss() {
      dismissed = true;
    }
  }

  // This property holds a list of all notifications, whether they are active,
  // their messages, severities, etc.  This is in the order they're displayed in
  // the list.
  //
  // Generally, these are ordered by severity.  If there's a problem, and
  // several of these notifications are active, which ones should the user read
  // first?  (Imagine a mysterious undiagnosed showstopping problem.)
  // - Available update is first (even though it's "info") because the first
  //   troubleshooting step is usually "update to the latest version".
  //   "Download failed" follows it since it's directly related.
  // - Showstopping errors are next - auth failure, TAP adapter problem, etc.,
  //   we know these are bad.
  // - Possibly-unexpected conditions are next (accidental killswitch,
  //   connection loss, etc.)  Some of these may be resolved automatically if we
  //   reconnect.
  // - Informational conditions are last (account expiring, can't verify
  //   account, changelog)
  //
  // The general headings are here, but for readability any detailed notes on
  // order are with each notification definition above.
  //
  // Note that since this is a plain array, the notification object is available
  // as 'modelData' when this is used as a view model.  In this case, this is
  // preferable over using ListModel (which injects all the object's properties
  // as "roles") for a few reasons:
  // - to give the delegate a name for the entire NotificationStatus object
  // - to give some readability to the delegate code
  // - to reduce name collisions
  readonly property var notifications: [
    // Update - first because this could resolve a known issue
    updateAvailable,
    downloadFailed,
    // Showstopping errors - prevent all connections or seriously impact
    // functionality
    tapAdapterMissing,
    installationRestart,
    splitTunnelUninstalled,
    splitTunnelReboot,
    authFailure,
    dnsConfigFailed,
    hnsdFailing,
    // Possibly unexpected conditions
    winIsElevated, // May cause other issues, list first
    killswitchEnabled,
    connectionLost,
    proxyUnreachable,
    connectionTrouble,
    // Informational
    reconnectNeeded,
    alternateTransport,
    accountExpiring,
    accountTokenNotAvailable,
    changelogAvailable,
    invalidClientExit
  ]

  // This is the worst severity among any active notification.  If no
  // notifications are active, it is severities.none.  Otherwise, it is the
  // worst severity among active notifications.
  readonly property int worstSeverity: {
    var worst = severities.none
    for(var i=0; i<notifications.length; ++i) {
      if(notifications[i].active)
        worst = Math.max(worst, notifications[i].severity)
    }
    return worst
  }

  function firstTitleAt(severity) {
    for(var i=0; i<notifications.length; ++i) {
      if(notifications[i].active &&
         notifications[i].severity === severity &&
         notifications[i].title) {
        return notifications[i].title
      }
    }
    return ""
  }

  // ClientNotifications is a singleton (it's used throughout the client), but
  // some notifications interact with other windows.
  // Maybe the client's main.qml could also be a singleton, but for the moment
  // we just emit these signals when we want to show other windows, and we let
  // the other windows handle them.
  //
  // Show the privacy page (for the killswitch notification)
  signal showPrivacyPage()
  // Show the network page
  signal showNetworkPage()
  // Show the connection page
  signal showConnectionPage()
  // Show the help page
  signal showHelpPage()
  // Changelog (for the post-update notification)
  signal showChangelog()
  // Show help page & trigger TAP reinstall (for the TAP adapter notification)
  signal reinstallTapAdapter()
  // Show help page & trigger split tunnel filter reinstall
  signal reinstallSplitTunnel()
  // Emitted when a notification needs to show an alert on the Settings Help
  // page (due to an error handling a user action)
  signal showHelpAlert(string msg, string title, string level)

  // The title of the first active error-level notification if there is one, or
  // "" if there are no active errors.
  readonly property string firstErrorTitle: firstTitleAt(severities.error)

  // The title of the first active warning, or "" if there is none.
  readonly property string firstWarningTitle: firstTitleAt(severities.warning)

  // The title of the first active info notification if there is one,
  // or "" if there are no active info notifications.
  readonly property string firstInfoTitle: firstTitleAt(severities.info)
}
