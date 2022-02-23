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

#include "common.h"
#include "connection.h"

void Transport::resolveDefaultPort(const QString &selectedProtocol, const Server *pSelectedServer)
{
    // Find the default port only if the default port was selected, the selected
    // protocol is the same as this transport's protocol, and a server was
    // found
    if(port() == 0 && protocol() == selectedProtocol && pSelectedServer)
    {
        Service selectedService{Service::OpenVpnUdp};
        if(protocol() == QStringLiteral("tcp"))
            selectedService = Service::OpenVpnTcp;
        port(pSelectedServer->defaultServicePort(selectedService));
    }
}

const Server *Transport::selectServerPort(const Location &location)
{
    Service selectedService{Service::OpenVpnUdp};
    if(protocol() == QStringLiteral("tcp"))
        selectedService = Service::OpenVpnTcp;

    // Select a server.  If a port has been selected, try to get that port,
    // otherwise take any server for this service.
    const Server *pSelectedServer = location.randomServer(selectedService, port());

    // If no connection is possible, set port to 0
    if(!pSelectedServer)
        port(0);
    // Otherwise, if the default port was selected, or if the selected port is
    // not available, use the default
    else if(port() == 0 || !pSelectedServer->hasPort(selectedService, port()))
        port(pSelectedServer->defaultServicePort(selectedService));
    return pSelectedServer;
}

std::size_t Transport::countServersForLocation(const Location &location) const
{
    Service service{Service::OpenVpnUdp};
    if(protocol() == QStringLiteral("tcp"))
        service = Service::OpenVpnTcp;

    if(port())
        // If a port is specified limit our servers to those with that port
        return location.countServersForPort(service, port());
    else
        // Otherwise, all servers for that service are game
        return location.countServersForService(service);
}

const Server *Transport::selectServerPortWithIndex(const Location &location, size_t index)
{
    Service selectedService{Service::OpenVpnUdp};
    if(protocol() == QStringLiteral("tcp"))
        selectedService = Service::OpenVpnTcp;

    // Select a server.  If a port has been selected, try to get that port,
    // otherwise take any server for this service.
    const Server *pSelectedServer = location.serverWithIndex(index, selectedService, port());

    // If no connection is possible, set port to 0
    if(!pSelectedServer)
        port(0);
    // Otherwise, if the default port was selected, or if the selected port is
    // not available, use the default
    else if(port() == 0 || !pSelectedServer->hasPort(selectedService, port()))
        port(pSelectedServer->defaultServicePort(selectedService));
    return pSelectedServer;
}
