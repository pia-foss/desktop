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
#include "locations.h"
#include <QRandomGenerator>

bool Server::hasNonLatencyService() const
{
    return !openvpnTcpPorts().empty() || !openvpnUdpPorts().empty() ||
        !wireguardPorts().empty() || !shadowsocksPorts().empty() ||
        !metaPorts().empty();
}

bool Server::hasService(Service service) const
{
    return !servicePorts(service).empty();
}

bool Server::hasVpnService() const
{
    return hasService(Service::OpenVpnTcp) || hasService(Service::OpenVpnUdp) ||
        hasService(Service::WireGuard);
}

bool Server::hasPort(Service service, quint16 port) const
{
    const auto &ports = servicePorts(service);
    return std::find(ports.begin(), ports.end(), port) != ports.end();
}

const std::vector<quint16> &Server::servicePorts(Service service) const
{
    switch(service)
    {
        default:
        {
            Q_ASSERT(false);
            static const std::vector<quint16> dummy{};
            return dummy;
        }
        case Service::OpenVpnTcp:
            return openvpnTcpPorts();
        case Service::OpenVpnUdp:
            return openvpnUdpPorts();
        case Service::WireGuard:
            return wireguardPorts();
        case Service::Shadowsocks:
            return shadowsocksPorts();
        case Service::Meta:
            return metaPorts();
    }
}

void Server::servicePorts(Service service, std::vector<quint16> ports)
{
    switch(service)
    {
        default:
            Q_ASSERT(false);
            return;
        case Service::OpenVpnTcp:
            openvpnTcpPorts(std::move(ports));
            return;
        case Service::OpenVpnUdp:
            openvpnUdpPorts(std::move(ports));
            return;
        case Service::WireGuard:
            wireguardPorts(std::move(ports));
            return;
        case Service::Shadowsocks:
            shadowsocksPorts(std::move(ports));
            return;
        case Service::Meta:
            metaPorts(std::move(ports));
            return;
    }
}

quint16 Server::defaultServicePort(Service service) const
{
    const auto &ports = servicePorts(service);
    if(ports.empty())
        return 0;
    return ports.front();
}

quint16 Server::randomServicePort(Service service) const
{
    const auto &ports = servicePorts(service);
    if(ports.empty())
        return 0;

    std::size_t idx = QRandomGenerator::global()->bounded(static_cast<quint32>(ports.size()));
    return ports[idx];
}

template<class PredicateFuncT>
std::size_t Location::countServersFor(const PredicateFuncT &predicate) const
{
    std::size_t matches = 0;
    for(const auto &server : servers())
    {
        if(predicate(server))
            ++matches;
    }
    return matches;
}

std::size_t Location::countServersForService(Service service) const
{
    return countServersFor([service](const Server &server){return server.hasService(service);});
}

std::size_t Location::countServersForPort(Service service, quint16 port) const
{
    return countServersFor([service, port](const Server &server){return server.hasPort(service, port);});
}

template<class PredicateFuncT>
const Server *Location::randomServerFor(const PredicateFuncT &predicate) const
{
    // This implementation is O(N) in the number of servers in the region.  It
    // could be O(1) if we kept separate lists of the servers that have each
    // service / port, but the number of servers offered per region isn't very
    // large.

    // Count the matching servers
    std::size_t matches = countServersFor(predicate);

    if(matches >= 1)
    {
        std::size_t idx = QRandomGenerator::global()->bounded(static_cast<quint32>(matches));
        // Count off that index among the matching servers
        for(const auto &server : servers())
        {
            if(predicate(server))
            {
                if(idx == 0)
                    return &server;
                --idx;
            }
        }
    }

    // No servers match
    return nullptr;
}

template<class PredicateFuncT>
const Server *Location::serverWithIndexFor(std::size_t desiredIndex, const PredicateFuncT &predicate) const
{
    // This implementation is O(N) in the number of servers in the region.  It
    // could be O(1) if we kept separate lists of the servers that have each
    // service / port, but the number of servers offered per region isn't very
    // large.

    // Count the matching servers
    std::size_t matches = countServersFor(predicate);

    if(matches >= 1)
    {
        std::size_t idx = 0;
        // Count off that index among the matching servers
        for(const auto &server : servers())
        {
            if(predicate(server))
            {
                if(idx == desiredIndex)
                    return &server;
                ++idx;
            }
        }
    }

    // No servers match
    return nullptr;
}

bool Location::hasService(Service service) const
{
    for(const auto &server : servers())
    {
        if(server.hasService(service))
            return true;
    }
    return false;
}

const Server *Location::randomIcmpLatencyServer() const
{
    return randomServerFor([](const Server &server)
    {
        return server.hasVpnService();
    });
}

const Server *Location::randomServerForService(Service service) const
{
    return randomServerFor([service](const Server &server){return server.hasService(service);});
}

const Server *Location::randomServerForPort(Service service, quint16 port) const
{
    return randomServerFor([service, port](const Server &server){return server.hasPort(service, port);});
}

const Server *Location::serverWithIndexForService(std::size_t index, Service service) const
{
    return serverWithIndexFor(index, [service](const Server &server){return server.hasService(service);});
}

const Server *Location::serverWithIndexForPort(std::size_t index, Service service, quint16 port) const
{
    return serverWithIndexFor(index, [service, port](const Server &server){return server.hasPort(service, port);});
}

const Server *Location::randomServer(Service service, quint16 tryPort) const
{
    const Server *pSelected{nullptr};

    // If a port was requested, try to find that port.
    if(tryPort)
        pSelected = randomServerForPort(service, tryPort);
    // If that port was not available (or no port was requested), select any
    // server for this service.
    if(!pSelected)
        pSelected = randomServerForService(service);
    return pSelected;
}

const Server *Location::serverWithIndex(std::size_t index, Service service, quint16 tryPort) const
{
    const Server *pSelected{nullptr};

    // If a port was requested, try to find that port.
    if(tryPort)
        pSelected = serverWithIndexForPort(index, service, tryPort);
    // If that port was not available (or no port was requested), select any
    // server for this service.
    if(!pSelected)
    {
        pSelected = serverWithIndexForService(index, service);
    }

    return pSelected;
}

std::vector<Server> Location::allServersForService(Service service) const
{
    std::vector<Server> serversForService;
    serversForService.reserve(countServersForService(service));
    for(const auto &server : servers())
    {
        if(server.hasService(service))
            serversForService.push_back(server);
    }

    return serversForService;
}

DescendingPortSet Location::allPortsForService(Service service) const
{
    DescendingPortSet ports;
    allPortsForService(service, ports);
    return ports;
}

void Location::allPortsForService(Service service, DescendingPortSet &ports) const
{
    for(const auto &server : servers())
    {
        const auto &serverPorts{server.servicePorts(service)};
        ports.insert(serverPorts.begin(), serverPorts.end());
    }
}
