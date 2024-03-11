// Copyright (c) 2024 Private Internet Access, Inc.
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
#line SOURCE_FILE("metaserviceapibase.cpp")

#include "metaserviceapibase.h"
#include <common/src/locations.h>

MetaServiceApiBase::MetaServiceApiBase(const StateModel &state,
                                       QString dynamicBasePath,
                                       std::shared_ptr<PrivateCA> pDynamicBaseCA,
                                       std::vector<QString> fixedBaseUris)
    : _dynamicBasePath{std::move(dynamicBasePath)},
      _pDynamicBaseCA{std::move(pDynamicBaseCA)},
      _state{state}, _fixedBaseUris{std::move(fixedBaseUris)}
{
    // Ensure the base path begins and ends with a '/'
    if(!_dynamicBasePath.startsWith('/'))
        _dynamicBasePath.insert(0, '/');
    if(!_dynamicBasePath.endsWith('/'))
        _dynamicBasePath.append('/');
}

ApiBaseSequence MetaServiceApiBase::beginAttempt()
{
    QSharedPointer<const Location> pFirstBaseRegion{}, pSecondBaseRegion{};

    std::vector<BaseUri> bases;
    bases.reserve(_fixedBaseUris.size() + 2);

    auto appendFixedBases = [&]
    {
        for(const auto &fixedBase : _fixedBaseUris)
            bases.push_back({fixedBase, nullptr, {}});
    };

    // If we're connected check which infra we're using
    if(_state.connectionState() == QStringLiteral("Connected"))
    {
        if(_state.connectedServer())
        {
            // Use a fixed address for the internal meta sevice available in
            // the modern infrastructure.  This is provided by the VPN server,
            // so use the VPN cert's common name
            bases.push_back({QString("https://10.0.0.1:443") + _dynamicBasePath, _pDynamicBaseCA, _state.connectedServer()->commonName()});
            // Fallback addresses
            appendFixedBases();

            qInfo() << "Connected to modern infra, using internal API base";

            QSharedPointer<ApiBaseData> pBaseData{new ApiBaseData{std::move(bases)}};
            return {pBaseData};
        }
    }

    NearestLocations nearest(_state.availableLocations());

    // Find the nearest region that has meta
    if(!pFirstBaseRegion)
    {
        pFirstBaseRegion = nearest.getBestMatchingLocation([](const Location &loc)
        {
            return loc.hasService(Service::Meta);
        });
        if(pFirstBaseRegion)
        {
            qInfo() << "First region for API request is nearest meta region"
                << pFirstBaseRegion->id();
        }
    }

    // If we got a first choice (from either strategy), find a second choice.
    // If we weren't able to get a first choice at all, then we definitely can't
    // get a second choice (no known regions have the meta service).
    if(pFirstBaseRegion)
    {
        pSecondBaseRegion = nearest.getBestMatchingLocation([&](const Location &loc)
        {
            // Exclude the first choice as a possibility regardless of whether
            // it was the connected or nearest location.  (It's also possible it
            // could be both the connected location and the nearest location.)
            return loc.hasService(Service::Meta) && loc.id() != pFirstBaseRegion->id();
        });

        if(pSecondBaseRegion)
        {
            qInfo() << "Second region for API request is next-nearest meta region"
                << pSecondBaseRegion->id();
        }
    }

    // Select a server in the region for the Meta service, and a port on that
    // server.
    auto appendDynamicBase = [&](const QSharedPointer<const Location> &pRegion)
    {
        const Server *pBaseServer{};
        if(pRegion)
            pBaseServer = pRegion->randomServerForService(Service::Meta);
        quint16 basePort{};
        if(pBaseServer)
            basePort = pBaseServer->randomServicePort(Service::Meta);
        if(basePort)
        {
            auto uri = QStringLiteral("https://%1:%2%3")
                .arg(pBaseServer->ip())
                .arg(basePort)
                .arg(_dynamicBasePath);
            bases.push_back({uri, _pDynamicBaseCA, pBaseServer->commonName()});
        }
    };

    appendDynamicBase(pFirstBaseRegion);
    appendDynamicBase(pSecondBaseRegion);
    auto dynamicCount = bases.size();   // For tracing
    // append fixed bases
    appendFixedBases();

    qInfo() << "Selected" << bases.size() << "API bases for request;"
        << dynamicCount << "dynamic and" << _fixedBaseUris.size() << "fixed";

    QSharedPointer<ApiBaseData> pBaseData{new ApiBaseData{std::move(bases)}};
    return {pBaseData};
}

unsigned MetaServiceApiBase::getAttemptCount(unsigned attemptsPerBase)
{
    return (_fixedBaseUris.size() + 2) * attemptsPerBase;
}
