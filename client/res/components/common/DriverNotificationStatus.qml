// Copyright (c) 2026 Private Internet Access, Inc.
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
import PIA.NativeHelpers 1.0
import "../settings"

NotificationStatus {
  id: adapterMissing
  message: uiTranslate("ClientNotifications", "The virtual network adapter is not installed.")
  severity: severities.error
  dismissible: false
  links: [{
    text: uiTranslate("ClientNotifications", "Reinstall"),
    clicked: function() {reinstallAdapter()}
  }]

  // Whether this adapter is needed for the currently selected connection
  // method.  (The driver status is ignored otherwise.)
  property bool isNeeded
  // Bind this to the appropriate NativeHelpers status property -
  // NativeHelpers.reinstallTapStatus, NativeHelpers.reinstallTunStatus, etc.
  property string reinstallStatus
  // Bind this to the matching Daemon.state property -
  // Daemon.state.tapAdapterMissing, Daemon.state.wintunMissing, etc.
  property bool driverMissing
  // Bind this to a functor that handles reinstalling the driver by signaling
  // the Help page
  property var reinstallAdapter

  // To avoid transient blips during the reinstall itself, we don't change
  // state during while the reinstall status is 'working'.
  property bool lastNonworkingMissing
  function updateMissingState() {
    if(reinstallStatus !== 'working')
      lastNonworkingMissing = driverMissing
    // Otherwise, ignore the current state and keep the state from before the
    // reinstall
  }

  onReinstallStatusChanged: adapterMissing.updateMissingState()
  onDriverMissingChanged: adapterMissing.updateMissingState()

  // When the reinstallation status is 'reboot', show the reboot notification
  // instead.
  active: adapterMissing.isNeeded &&
          lastNonworkingMissing && reinstallStatus !== 'reboot'

  Component.onCompleted: adapterMissing.updateMissingState()
}
