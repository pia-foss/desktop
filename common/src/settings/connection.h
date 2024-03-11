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

#ifndef SETTINGS_CONNECTION_H
#define SETTINGS_CONNECTION_H

#include "../common.h"
#include "../json.h"
#include "locations.h"

// Bandwidth measurements for one measurement interval, in bytes
class COMMON_EXPORT IntervalBandwidth : public NativeJsonObject
{
    Q_OBJECT
public:
    IntervalBandwidth() {}
    IntervalBandwidth(quint64 receivedVal, quint64 sentVal)
    {
        received(receivedVal);
        sent(sentVal);
    }
    IntervalBandwidth(const IntervalBandwidth &other) {*this = other;}
    IntervalBandwidth &operator=(const IntervalBandwidth &other)
    {
        received(other.received());
        sent(other.sent());
        return *this;
    }
    bool operator==(const IntervalBandwidth &other) const
    {
        return received() == other.received() && sent() == other.sent();
    }
    bool operator!=(const IntervalBandwidth &other) const
    {
        return !(*this == other);
    }

    JsonField(quint64, received, {})
    JsonField(quint64, sent, {})
};

// Transport settings that might vary due to automatic failover.
class COMMON_EXPORT Transport : public NativeJsonObject
{
    Q_OBJECT

public:
    Transport() {}
    Transport(const QString &protocolVal, uint portVal)
    {
        protocol(protocolVal);
        port(portVal);
    }
    Transport(const Transport &other) {*this = other;}
    Transport &operator=(const Transport &other)
    {
        protocol(other.protocol());
        port(other.port());
        return *this;
    }
    bool operator==(const Transport &other) const
    {
        return protocol() == other.protocol() && port() == other.port();
    }
    bool operator!=(const Transport &other) const
    {
        return !(*this == other);
    }

    JsonField(QString, protocol, QStringLiteral("udp"), { "udp", "tcp" })
    JsonField(uint, port, 0)

public:
    // If this transport had port 0 ("use default"), and the actual protocol
    // selected is the same as this transport's protocol, apply the default
    // port from the selected server.  This is used for the "preferred"
    // transport so the client can tell whether a preferred "default" transport
    // is the same as the actual transport.
    void resolveDefaultPort(const QString &selectedProtocol, const Server *pSelectedServer);

    // Based on the transport's protocol and port, select a server from the
    // specified location, and determine the actual port to use to connect.  If
    // no connection is possible, the port is set to 0.
    //
    // - A server is selected from the location given based on the protocol.  If
    //   the transport has a specific port, it will try to get a server with
    //   that port.
    // - If the port is currently 0, the default port from the selected server
    //   is used
    // - If the port is nonzero but that port is not available on this server,
    //   the default port from the selected server is used
    //
    // Returns the appropriate server for this transport's protocol (regardless
    // of whether a port is found).
    const Server *selectServerPort(const Location &location);
    const Server *selectServerPortWithIndex(const Location &location, size_t index);
    std::size_t countServersForLocation(const Location &location) const;
};

// Definition of a custom SOCKS proxy (see DaemonSettings::customProxy)
class COMMON_EXPORT CustomProxy : public NativeJsonObject
{
    Q_OBJECT

public:
    CustomProxy() {}
    CustomProxy(const CustomProxy &other) {*this = other;}
    CustomProxy &operator=(const CustomProxy &other)
    {
        host(other.host());
        port(other.port());
        username(other.username());
        password(other.password());
        return *this;
    }

    bool operator==(const CustomProxy &other) const
    {
        return host() == other.host() && port() == other.port() &&
            username() == other.username() && password() == other.password();
    }

    bool operator!=(const CustomProxy &other) const
    {
        return !(*this == other);
    }

    // Host - domain name or IPv4 address.
    JsonField(QString, host, {})

    // Remote port - 0 just indicates the default (1080)
    JsonField(uint, port, 0)

    // Username/password - both optional
    JsonField(QString, username, {})
    JsonField(QString, password, {})
};

// A single manual server can be specified in DaemonSettings - this is a dev
// tool only to facilitate testing specific servers.  This becomes a region with
// ID "manual".
//
// An IP and CN must be specified for the region to be shown.  Service groups
// and corresponding region ID are optional.
//
// This is similar to the way Dedicated IP servers are integrated into the
// regions model, although manual servers use regular token auth (not DIP auth).
class COMMON_EXPORT ManualServer : public NativeJsonObject
{
    Q_OBJECT
public:
    ManualServer() {}
    ManualServer(const ManualServer &other) {*this = other;}
    ManualServer &operator=(const ManualServer &other)
    {
        ip(other.ip());
        cn(other.cn());
        openvpnNcpSupport(other.openvpnNcpSupport());
        openvpnUdpPorts(other.openvpnUdpPorts());
        openvpnTcpPorts(other.openvpnTcpPorts());
        serviceGroups(other.serviceGroups());
        correspondingRegionId(other.correspondingRegionId());
        return *this;
    }
    bool operator==(const ManualServer &other) const
    {
        return ip() == other.ip() && cn() == other.cn() &&
            openvpnNcpSupport() == other.openvpnNcpSupport() &&
            openvpnUdpPorts() == other.openvpnUdpPorts() &&
            openvpnTcpPorts() == other.openvpnTcpPorts() &&
            serviceGroups() == other.serviceGroups() &&
            correspondingRegionId() == other.correspondingRegionId();
    }
    bool operator!=(const ManualServer &other) const {return !(*this == other);}

    // The manual server's IP address.  This is used for all services included
    // in the service groups.  This should be a valid IPv4 address, but invalid
    // addresses can still be added to the regions list (as with normal regions
    // from the servers list).
    JsonField(QString, ip, {})
    // The certificate CN to expect from this server.
    JsonField(QString, cn, {})
    // Whether to use NCP.  (If this is false, pia-signal-settings is used to
    // indicate ciphers.)  See Server::openvpnNcpSupport().
    JsonField(bool, openvpnNcpSupport, false)
    // Override OpenVPN UDP/TCP ports for this server.
    // If set, this overrides the ports for the OpenVpnTcp / OpenVpnUdp
    // services for this server.  If these are empty, the corresponding port
    // list from the server list group is used.
    JsonField(std::vector<quint16>, openvpnUdpPorts, {})
    JsonField(std::vector<quint16>, openvpnTcpPorts, {})
    // The service groups from the servers list to apply to this server.  If
    // not specified, this defaults to the current set of service groups
    // excluding "meta", which are:
    //  "ovpntcp", "ovpnudp", "wg"
    JsonField(std::vector<QString>, serviceGroups, {})
    // The corresponding PIA region - if specified, 'meta' servers from this
    // region are used for the manual region.  This is optional; if it is not
    // given, then the region does not have any known 'meta' addresses.
    JsonField(QString, correspondingRegionId, {})
};

#endif
