// Copyright (c) 2023 Private Internet Access, Inc.
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

#include "common.h"
#line HEADER_FILE("vpnstate.h")

#ifndef VPNSTATE_H
#define VPNSTATE_H

#include <QObject>

// States used by VPNConnection (also used by clients).  The meta object needs
// to be exported, and we can't annotate the Q_NAMESPACE macro.  Instead this
// is in a QObject class that's never instantiated, which allows us to export
// the class.
class COMMON_EXPORT VpnState : public QObject
{
    Q_OBJECT

public:
    enum State
    {
        // Disconnected from server
        Disconnected,

        // Connecting (OpenVPN process launched etc.)
        // - Try connect until we establish a connection, or the user disables
        //   the VPN.  Delays between attempts depend on the number of attempts
        //   made.
        Connecting,

        // Successfully connected
        Connected,

        // Connection failed/interrupted after previously being connected
        // - Current OpenVPN process is discarded; wait for process exit
        // - Afterwards, go to Reconnecting
        Interrupted,

        // Waiting for an OpenVPN connection attempt to recover connectivity -
        // behaves like Connecting state, difference is that we entered this
        // state due to connection loss or a reconnect to apply settings.
        Reconnecting,

        // Disconnecting in order to reconnect again
        // - Current OpenVPN process is discarded; wait for process exit
        // - Afterwards, go to Connecting
        DisconnectingToReconnect,

        // Disconnecting from server (intentionally)
        Disconnecting,
    };
    Q_ENUM(State)

    // Special values for the value of DaemonState::forwardedPort.  Note that
    // these names are part of the CLI interface for "get portforward" and
    // shouldn't be changed.
    enum PortForwardState : int
    {
        // PF not enabled, or not connected to VPN
        Inactive = 0,
        // Enabled, connected, and supported - requesting port
        Attempting = -1,
        // Port forward failed
        Failed = -2,
        // PF enabled, but not available for the connected region
        Unavailable = -3,
    };
    Q_ENUM(PortForwardState)

    enum Limits
    {
        // Maximum connection attempts until "slow connect" mode triggers
        SlowConnectionAttemptLimit = 2,
        // Minimum interval between subsequent connection attempts
        ConnectionAttemptInterval = 1 * 1000,
        SlowConnectionAttemptInterval = 10 * 1000,
    };
};

#endif
