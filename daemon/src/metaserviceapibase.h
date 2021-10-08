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

#include "common.h"
#line HEADER_FILE("metaserviceapibase.h")

#ifndef METASERVICEAPIBASE_H
#define METASERVICEAPIBASE_H

#include "settings/daemonstate.h"
#include "apibase.h"

// ApiBase that generates base URIs from "meta" services in the modern servers
// list.
//
// In the modern infrastructure, "meta" services on the VPN servers provide
// access to APIs from each region.  There are many more servers given than we
// would use for an individual request, so we select a few servers and try
// those.  Fixed bases can also be given as fallbacks added to the end of the
// list.
//
// When not connected to the VPN, the nearest two regions are selected using
// latency measurements:
// 1. Nearest region (best "meta" server in region)
// 2. Second-nearest region (beta "meta" server in region)
// 3+. Fixed base(s)
//
// When connected to the VPN in the modern infra, we can get meta information
// from an internal service on the current server, using 10.0.0.1:443.  In that
// case, fixed bases are only used as a fallback in case of a problem with the
// meta service (10.0.0.1:443 can't be blocked since it is internal to the VPN
// server).
class MetaServiceApiBase : public ApiBase
{
public:
    // Construct MetaServiceApiBase with:
    // - state - used to find the best "meta" services when requested.
    //   MetaServiceApiBase holds a reference to this object until it's
    //   destroyed
    // - dynamicBasePath - The URI path combined with the IP/port from the
    //   'meta' server to form the complete base URI.  For example, if the
    //   base URI should be 'https://<IP>:<PORT>/api/client/', use '/api/client/'
    //   as the base path.  Leading/trailing slashes are accepted but not
    //   required.  This path is not applied to fixed base URIs.
    // - pDynamicBaseCA - The CA to use to validate certificates returned by
    //   'meta' servers
    // - fixedBaseUris - fixed base URIs to include after the meta service bases
    MetaServiceApiBase(const DaemonState &state, QString dynamicBasePath,
                       std::shared_ptr<PrivateCA> pDynamicBaseCA,
                       std::vector<QString> fixedBaseUris);

public:
    // MetaServiceApiBase generates a new set of API bases for each request
    virtual ApiBaseSequence beginAttempt() override;
    // MetaServiceApiBase always estimates that it'll provide 2+fixedBaseUris
    // API bases.  In rare cases, it might provide have fewer API bases.
    virtual unsigned getAttemptCount(unsigned attemptsPerBase) override;

private:
    QString _dynamicBasePath;
    std::shared_ptr<PrivateCA> _pDynamicBaseCA;
    const DaemonState &_state;
    std::vector<QString> _fixedBaseUris;
};

#endif
