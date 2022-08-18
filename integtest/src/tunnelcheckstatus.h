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

#include <common/src/common.h>
#line HEADER_FILE("tunnelcheckstatus.h")

#ifndef TUNNELCHECKSTATUS_H
#define TUNNELCHECKSTATUS_H

#include <common/src/jsonrefresher.h>

// TunnelCheckStatus uses the PIA client/status API to detect whether Internet
// traffic is currently routing through the VPN tunnel.
//
// Tests use this to verify routing, for example:
// - after connecting, we should _not_ route any requests outside of the tunnel
//   (would be a leak), but failures are acceptable until we receive a
//   successful response over the VPN tunnel
// - after disconnecting, we wait to ensure that requests route over the
//   Internet again, but initial requests routing over the VPN tunnel might be
//   acceptable
//
// TunnelCheckStatus periodically refreshes the client/status API to detect the
// current state.
class TunnelCheckStatus : public QObject
{
    Q_OBJECT

public:
    enum class Status
    {
        Unknown,    // Haven't received a successful response yet
        OffVPN, // Request succeeded and didn't go over VPN
        OnVPN,  // Request succeeded and did go over VPN
    };
    Q_ENUM(Status)

public:
    TunnelCheckStatus();

public:
    Status status() const {return _status;}

signals:
    void statusChanged(Status newStatus);

private:
    JsonRefresher _refresher;
    Status _status;
};

#endif
