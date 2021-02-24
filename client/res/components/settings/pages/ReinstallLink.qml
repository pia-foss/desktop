// Copyright (c) 2021 Private Internet Access, Inc.
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
import ".." // settings/ for SettingsMessages
import "../../client"
import "../../daemon"
import "../../common"
import "../../theme"
import PIA.NativeHelpers 1.0

// Link for the help page to reinstall a network adapter driver on Windows.
// Used for the TAP and WinTUN adapters.
TextLink {
  id: reinstallLink

  property string linkText
  property string executingText

  // Hook this up to the appropriate status property of NativeHelpers -
  // reinstallTapStatus, etc.
  property string reinstallStatus
  // reinstallAction() is called to trigger the actual reinstallation - this
  // should call NativeHelpers.reinstallTap(), etc.
  property var reinstallAction

  text: {
    if (executing)
      return executingText
    else if (waitingForDisconnect)
      return uiTranslate("HelpPage", "Waiting for Disconnect...")
    else
      return linkText
  }
  underlined: enabled
  enabled: !executing && !waitingForDisconnect
  readonly property bool executing: reinstallStatus === 'working'
  property bool waitingForDisconnect: false
  function startReinstall() {
    if(!enabled)
      return
    if (Daemon.state.connectionState !== 'Disconnected') {
      wSettings.alert(uiTranslate("HelpPage", "The network adapter cannot be reinstalled while connected. Disconnect and reinstall now?"), uiTranslate("HelpPage", "Disconnect needed"), [Dialog.Yes, Dialog.No], function(result) {
        if (result.code == Dialog.Yes) { // enum comparison; intentional coercing ==
          if (Daemon.state.connectionState !== 'Disconnected') {
            waitingForDisconnect = true;
            Daemon.disconnectVPN();
          } else {
            reinstallLink.reinstallAction()
          }
        }
      });
    } else {
      reinstallLink.reinstallAction()
    }
  }
  onClicked: startReinstall()

  // Show a message box when the reinstall status reaches a new state.
  // Though NativeHelpers avoids emitting spurious changes, occasionally
  // these changes seem to queue up and would otherwise cause duplicate
  // message boxes.
  property string lastReinstallStatus

  onReinstallStatusChanged: {
    if(reinstallLink.lastReinstallStatus === reinstallLink.reinstallStatus) {
      console.info('Ignore duplicate state ' + reinstallLink.lastReinstallStatus)
      return
    }
    // This is a new state being observed
    reinstallLink.lastReinstallStatus = reinstallLink.reinstallStatus
    switch (reinstallLink.reinstallStatus)
    {
    case 'success':
      wSettings.alert(uiTranslate("HelpPage", "The network adapter has been successfully reinstalled."), SettingsMessages.titleReinstallSuccessful, 'info');
      break;
    case 'reboot':
      wSettings.alert(uiTranslate("HelpPage", "The network adapter has been successfully reinstalled. You may need to reboot your system."), SettingsMessages.titleReinstallSuccessful, 'warning');
      break;
    case 'error':
      wSettings.alert(uiTranslate("HelpPage", "There was an error while attempting to reinstall the network adapter."), SettingsMessages.titleReinstallError, 'error');
      break;
    }
  }

  Component.onCompleted: reinstallLink.lastReinstallStatus = reinstallLink.reinstallStatus

  Connections {
    target: Daemon.state
    enabled: reinstallLink.waitingForDisconnect
    function onConnectionStateChanged() {
      if (Daemon.state.connectionState === 'Disconnected') {
        reinstallLink.waitingForDisconnect = false;
        reinstallLink.reinstallAction()
      }
    }
  }
}
