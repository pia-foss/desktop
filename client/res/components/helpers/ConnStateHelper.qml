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
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import "../../javascript/app.js" as App
import "../daemon"

Item {
  // This is a simplified version of Daemon.state.connectionState.
  // The actual connectionState has a number of detailed values that are not
  // relevant to many client components (like the distinctions between
  // Disconnecting/DisconnectingToReconnect, Reconnecting/StillReconnecting, and
  // Connecting/StillConnecting).
  //
  // Currently, the actual states exposed here are fixed, but potentially they
  // could be configurable if some objects are interested in a few of the
  // detailed states.

  // Possible states:
  readonly property string stateConnecting: 'Connecting'
  readonly property string stateConnected: 'Connected'
  readonly property string stateDisconnecting: 'Disconnecting'
  readonly property string stateDisconnected: 'Disconnected'

  readonly property string snoozeDisconnecting: 'SnoozeDisconnecting'
  readonly property string snoozeDisconnected: 'SnoozeDisconnected'
  readonly property string snoozeConnecting: 'SnoozeConnecting'
  readonly property string snoozeDisabled: 'SnoozeDisabled'

  // Current state:
  readonly property string connectionState: {
    switch(Daemon.state.connectionState) {
      case 'Disconnected':
        return stateDisconnected
      case 'Connecting':
      case 'StillConnecting':
      case 'Reconnecting':
      case 'StillReconnecting':
      case 'DisconnectingToReconnect':
      case 'Interrupted':
        return stateConnecting
      case 'Connected':
        return stateConnected
      case 'Disconnecting':
        return stateDisconnecting
      default:
        // This is not good, the component will not show the correct state
        console.error('Unhandled connection state:', Daemon.state.connectionState)
        return stateDisconnected
    }
  }

  // Use a separate property for snoozes, so we don't interfere with anything that depends on connectionState
  readonly property string snoozeState: {
    if(snoozeModeEnabled) {
      var snoozeEndTime = Daemon.state.snoozeEndTime;
      if(snoozeEndTime > 0 && connectionState !== stateDisconnected)
        return snoozeConnecting;

      if(snoozeEndTime > 0 && connectionState === stateDisconnected)
        return snoozeDisconnected;

      if(snoozeEndTime === 0)
        return snoozeDisconnecting;
    } else {
      return snoozeDisabled;
    }

  }

  readonly property bool snoozeModeEnabled: Daemon.state.snoozeEndTime > -1

  readonly property bool canSnooze: {
    return connectionState === stateConnected && snoozeState === snoozeDisabled
  }

  readonly property bool canResumeFromSnooze: {
    return connectionState === stateDisconnected && snoozeState === snoozeDisconnected
  }
}
