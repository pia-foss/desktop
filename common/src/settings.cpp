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
#line SOURCE_FILE("settings.cpp")

#include "settings.h"
#include "brand.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QSharedPointer>
#include <iterator>

bool Server::hasNonLatencyService() const
{
    return !openvpnTcpPorts().empty() || !openvpnUdpPorts().empty() ||
        !wireguardPorts().empty() || !shadowsocksPorts().empty();
}

bool Server::hasService(Service service) const
{
    return !servicePorts(service).empty();
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
        case Service::Latency:
            return latencyPorts();
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
        case Service::Latency:
            latencyPorts(std::move(ports));
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
        return server.hasService(Service::OpenVpnTcp) ||
            server.hasService(Service::OpenVpnUdp) ||
            server.hasService(Service::WireGuard);
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

DaemonData::DaemonData()
    : NativeJsonObject(DiscardUnknownProperties)
{

}

DaemonAccount::DaemonAccount()
    : NativeJsonObject(DiscardUnknownProperties)
{

}

SplitTunnelRule::SplitTunnelRule()
    : NativeJsonObject(DiscardUnknownProperties)
{

}

SplitTunnelSubnetRule::SplitTunnelSubnetRule()
    : NativeJsonObject(DiscardUnknownProperties)
{

}

DaemonSettings::DaemonSettings()
    : NativeJsonObject(SaveUnknownProperties)
{
}

DaemonState::DaemonState()
    : NativeJsonObject(DiscardUnknownProperties)
{
    // If any property of a ServiceLocations object changes, consider the
    // ServiceLocations property changed also.  (Daemon does not monitor for
    // nested property changes.)
    auto connectServiceLocations = [this](ServiceLocations &locs, auto func)
    {
        connect(&locs, &ServiceLocations::chosenLocationChanged, this, func);
        connect(&locs, &ServiceLocations::bestLocationChanged, this, func);
        connect(&locs, &ServiceLocations::nextLocationChanged, this, func);
    };

    connectServiceLocations(_vpnLocations, [this](){
        emitPropertyChange({[this](){emit vpnLocationsChanged();},
                            QStringLiteral("vpnLocations")});
    });
    connectServiceLocations(_shadowsocksLocations, [this](){
        emitPropertyChange({[this](){emit shadowsocksLocationsChanged();},
                            QStringLiteral("shadowsocksLocations")});
    });

    auto connectConnectionInfo = [this](ConnectionInfo &info, auto func)
    {
        connect(&info, &ConnectionInfo::vpnLocationChanged, this, func);
        connect(&info, &ConnectionInfo::vpnLocationAutoChanged, this, func);
        connect(&info, &ConnectionInfo::proxyChanged, this, func);
        connect(&info, &ConnectionInfo::proxyCustomChanged, this, func);
        connect(&info, &ConnectionInfo::proxyShadowsocksChanged, this, func);
        connect(&info, &ConnectionInfo::proxyShadowsocksLocationAutoChanged, this, func);
    };

    connectConnectionInfo(_connectingConfig, [this](){
        emitPropertyChange({[this](){emit connectingConfigChanged();},
                            QStringLiteral("connectingConfig")});
    });
    connectConnectionInfo(_connectedConfig, [this](){
        emitPropertyChange({[this](){emit connectedConfigChanged();},
                            QStringLiteral("connectedConfig")});
    });
}

bool DaemonAccount::validateToken(const QString& token)
{
    static const QRegularExpression validToken(QStringLiteral("^[0-9A-Fa-f]+$"));
    return token.isEmpty() || validToken.match(token).hasMatch();
}

const QString DaemonSettings::defaultReleaseChannelGA{QStringLiteral(BRAND_RELEASE_CHANNEL_GA)};
const QString DaemonSettings::defaultReleaseChannelBeta{QStringLiteral(BRAND_RELEASE_CHANNEL_BETA)};
const QStringList DaemonSettings::defaultDebugLogging
{
    QStringLiteral("*.debug=true"),
    QStringLiteral("qt*.debug=false"),
    QStringLiteral("latency.*=false"),
    QStringLiteral("qt.scenegraph.general*=true")
};

QJsonValue DaemonSettings::getDefaultDebugLogging()
{
    QJsonValue value;
    json_cast(defaultDebugLogging, value);
    return value;
}

bool DaemonSettings::validateDNSSetting(const DaemonSettings::DNSSetting& setting)
{
    static const QStringList validDNSSettings {
        "pia",
        "handshake",
        "local",
    };
    static const QRegularExpression validIP(QStringLiteral("^(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)$"));
    QString value;
    if (setting.get(value))
        return value.isEmpty() || validDNSSettings.contains(value);
    QStringList servers;
    if (setting.get(servers))
    {
        if (servers.isEmpty() || servers.length() > 2)
            return false;
        for (const QString& server : servers)
        {
            auto match = validIP.match(server);
            if (!match.hasMatch())
                return false;
            for (int i = 1; i <= 4; i++)
            {
                bool ok;
                if (match.captured(i).toUInt(&ok) > 255 || !ok)
                    return false;
            }
        }
        return true;
    }
    return false;
}

#if defined(PIA_DAEMON) || defined(UNIT_TEST)

// Local 127/8 address used by a local resolver.  We can't put this on 127.0.0.1
// because port 53 may already be in use on that address.
const QString resolverLocalAddress{QStringLiteral("127.80.73.65")};
const QString piaLegacyDnsPrimary{QStringLiteral("209.222.18.222")};
const QString piaLegacyDnsSecondary{QStringLiteral("209.222.18.218")};
const QString piaModernDnsVpnMace{QStringLiteral("10.0.0.241")};
const QString piaModernDnsVpn{QStringLiteral("10.0.0.243")};

#endif
