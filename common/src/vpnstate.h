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
        // - Try connect up to 2 times
        // - Minimum time between start of each attempt: 1 second
        // - If still not connected, transition to StillConnecting
        Connecting,

        // Still connecting (taking long or many attempts)
        // - Infinite number of attempts
        // - Minimum time between start of each attempt: 10 seconds
        StillConnecting,

        // Successfully connected
        Connected,

        // Connection failed/interrupted after previously being connected
        // - Current OpenVPN process is discarded; wait for process exit
        // - Afterwards, go to Reconnecting
        Interrupted,

        // Waiting for an OpenVPN connection attempt to recover connectivity
        // - Try connect up to 2 times
        // - Minimum time between start of each attempt: 1 second
        Reconnecting,

        // Having trouble regaining connectivity (maybe internet is down)
        // - Infinite number of attempts; just keep relaunching
        // - Minimum time between start of attempts: 10 seconds
        StillReconnecting,

        // Disconnecting in order to reconnect again
        // - Current OpenVPN process is discarded; wait for process exit
        // - Afterwards, go to Connecting
        DisconnectingToReconnect,

        // Disconnecting from server (intentionally)
        Disconnecting,
    };
    Q_ENUM(State)
};

#endif
