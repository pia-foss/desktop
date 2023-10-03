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

#pragma once
#include "server.h"
#include <kapps_core/src/retainshared.h>

namespace kapps::regions {

class KAPPS_REGIONS_EXPORT Region : public core::RetainSharedFromThis<Region>
{
public:
    Region() = default;
    Region(std::string id, bool autoSafe, bool portForward, bool geoLocated,
           core::Ipv4Address dipAddress,
           std::vector<std::shared_ptr<const Server>> servers)
        : _id{std::move(id)}, _autoSafe{autoSafe}, _portForward{portForward},
          _geoLocated{geoLocated}, _dipAddress{dipAddress},
          _servers{std::move(servers)}
    {
        // There are no other invariants to check - a region _could_ have zero
        // servers, which makes it "offline"

        _serversRaw.reserve(_servers.size());
        for(const auto &pServer : _servers)
            _serversRaw.push_back(pServer.get());
    }

    // Like RegionList, we need to be sure the move constructor treats _servers
    // and _serversRaw consistently
    Region(Region &&other) : Region{} {*this = std::move(other);}
    Region &operator=(Region &&other)
    {
        _id = std::move(other._id);
        _autoSafe = std::move(other._autoSafe);
        _portForward = std::move(other._portForward);
        _geoLocated = std::move(other._geoLocated);
        _dipAddress = std::move(other._dipAddress);
        _servers = std::move(other._servers);
        _serversRaw = std::move(other._serversRaw);

        // To guarantee that other is in a valid state (not violating its
        // invariant that _serversRaw corresponds to _servers); just clear both
        // containers.  Although it's unlikely, vector's move assignment is
        // allowed to make different choices moving _servers and _serversRaw;
        // i.e. it could swap one and move+clear the other.  (Maybe it might
        // if there was some sort of "small vector" optimization analogous to
        // the "small string" optimization.)
        other._servers.clear();
        other._serversRaw.clear();
        return *this;
    }

public:
    core::StringSlice id() const {return _id;}
    bool autoSafe() const {return _autoSafe;}
    bool portForward() const {return _portForward;}
    bool geoLocated() const {return _geoLocated;}

    bool offline() const {return _servers.empty();}

    bool isDedicatedIp() const {return dipAddress() != core::Ipv4Address{};}
    core::Ipv4Address dipAddress() const {return _dipAddress;}

    bool hasService(Service service) const {return firstServerFor(service);}
    const Server *firstServerFor(Service service) const;

    core::ArraySlice<const Server * const> servers() const {return _serversRaw;}

private:
    std::string _id;
    bool _autoSafe;
    bool _portForward;
    bool _geoLocated;
    core::Ipv4Address _dipAddress;  // Zero if not a DIP region
    std::vector<std::shared_ptr<const Server>> _servers;
    // Raw pointer array to provide an array slice to API
    std::vector<const Server*> _serversRaw;
};

}
