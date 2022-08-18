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

#ifndef SETTINGS_DEDICATEDIP_H
#define SETTINGS_DEDICATEDIP_H

#include "../common.h"
#include "../json.h"

// Information for a Dedicated IP stored in DaemonAccount.  The daemon uses
// this to populate "dedicated IP regions" in the regions list, and to refresh
// the dedicated IP information periodically.
class COMMON_EXPORT AccountDedicatedIp : public NativeJsonObject
{
    Q_OBJECT
public:
    AccountDedicatedIp() {}
    AccountDedicatedIp(const AccountDedicatedIp &other) {*this = other;}
    AccountDedicatedIp &operator=(const AccountDedicatedIp &other)
    {
        dipToken(other.dipToken());
        expire(other.expire());
        id(other.id());
        regionId(other.regionId());
        serviceGroups(other.serviceGroups());
        ip(other.ip());
        cn(other.cn());
        lastIpChange(other.lastIpChange());
        return *this;
    }

    bool operator==(const AccountDedicatedIp &other) const
    {
        return dipToken() == other.dipToken() &&
            expire() == other.expire() &&
            id() == other.id() &&
            regionId() == other.regionId() &&
            serviceGroups() == other.serviceGroups() &&
            ip() == other.ip() &&
            cn() == other.cn() &&
            lastIpChange() == other.lastIpChange();
    }
    bool operator!=(const AccountDedicatedIp &other) const {return !(*this == other);}

    // This is the DIP token.  The token is functionally a password - it must
    // not be exposed in the region information provided in DaemonState.
    JsonField(QString, dipToken, {})

    // This is the expiration timestamp - UTC Unix time in milliseconds.
    // Note that the API returns this in seconds; it's stored in milliseconds
    // to match all other timestamps in Desktop.
    JsonField(quint64, expire, 0)

    // The "ID" created by the client to represent this region in the region
    // list - of the form "dip-###", where ### is a random number.
    //
    // A random identifier is used rather than the token or IP address to meet
    // the following requirements:
    // * DIP tokens can't be exposed in the region information in DaemonState
    //   (they're functionally passwords)
    // * The region ID must remain the same even in the rare event that the
    //   dedicated IP changes (we can't use the IP address itself)
    // * The region ID should not be likely to duplicate a prior region ID that
    //   was added and expired/removed (could cause minor oddities like a
    //   favorite reappearing, etc.)
    //
    // This is not the 'id' field from the DIP API, that's regionId.
    JsonField(QString, id, {})

    // The ID of the corresponding PIA region where this server is.  We get the
    // country code, region name, geo flag, and PF flag from the corresponding
    // PIA region.  We also pull auxiliary service servers from this region,
    // like meta and Shadowsocks.
    JsonField(QString, regionId, {})

    // These are the server groups (from the server list) provided by the DIP
    // server.  Note that like the server list, the client does not attribute
    // any significance to the group names themselves, which can be freely
    // changed by Ops.  This is just used to find the service and port
    // information from the servers list.
    JsonField(std::vector<QString>, serviceGroups, {})

    // The dedicated IP itself.  This is both the VPN endpoint and the VPN IP.
    // It provides all services identified by serviceGroups.
    JsonField(QString, ip, {})

    // The common name for certificates corresponding to this server.
    JsonField(QString, cn, {})

    // If the daemon observes a change in the dedicated IP's IP address, this
    // field is set to the most recent change timestamp.  This triggers a
    // notification in the client.  It's important to keep track of this
    // individually for each dedicated IP to ensure that the notification is
    // cleared if the changed dedicated IP is removed.
    JsonField(quint64, lastIpChange, 0)
};


#endif
