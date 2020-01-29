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
import QtQuick 2.0
import "../client"
import "../daemon"

// VpnConnection provides methods to connect to the VPN and update the recent
// location history.
//
// Client code (other than VpnConnection) generally should not use
// Daemon.connectVPN() directly, since that wouldn't update the recent
// locations.
QtObject {
  // Add a recently-used connection (internal to VpnConnection)
  function _addRecentLocation(locationId) {
    // Never add 'auto' as a recent.
    if(locationId === 'auto')
      return

    var recents = Client.settings.recentLocations || []

    // Nothing to do if this was already the most recent region
    if(recents.length > 0 && recents[0] === locationId)
      return;

    // Remove the region if it was already there
    recents = recents.filter(function(item) {return item !== locationId})

    // Put this region at the beginning
    recents = [locationId].concat(recents)

    Client.applySettings({recentLocations: recents}, false, function(error) {
      if(error) {
        console.error('Could not set most recent region to ' + locationId, error)
      }
    })
  }

  // Select a new location.  Reconnect if already connected.
  // Used for interactions that choose a region, like the region selection list.
  function selectLocation(locationId) {
    Daemon.applySettings({location: locationId}, true, function(error) {
      if(error) {
        console.error('Could not set location to ' + locationId, error)
      }
      else {
        // Only add the recent location if the VPN is enabled (meaning we just
        // connected to the new location)
        if(Daemon.state.vpnEnabled) {
          _addRecentLocation(locationId)
        }
      }
    })
  }

  // Connect to a specific location.  Reconnect if already connect, otherwise
  // connect now.
  // Used for interactions intended to connect to a specific region, like the
  // Quick Connect buttons.
  function connectLocation(locationId) {
    // If this isn't the currently selected location, change to this location
    Daemon.applySettings({location: locationId}, true, function(error) {
      if(error) {
        console.error('Could not set location to ' + locationId, error)
      }
      else {
        _addRecentLocation(locationId)
        // If we are not already connected, connect to this location.
        //
        // If we were connected, the daemon started a reconnection due to
        // specifying reconnectIfNeeded=true to applySettings().  This call has
        // no effect.
        //
        // This is preferable than specifying reconnectIfNeeded=false though to
        // ensure that we don't briefly show "reconnect to apply settings" in
        // the UI.
        Daemon.connectVPN()
      }
    })
  }

  // Connect to the currently-selected location.
  // Used for interactions that connect without specifying a region, like the
  // Connect button and automatic login on startup.
  function connectCurrentLocation() {
    if (Daemon.state.vpnLocations.chosenLocation) {
      _addRecentLocation(Daemon.state.vpnLocations.chosenLocation.id)
    }
    Daemon.connectVPN()
  }
}
