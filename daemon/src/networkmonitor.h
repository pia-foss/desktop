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

#ifndef NETWORKMONITOR_H
#define NETWORKMONITOR_H

#include "common.h"
#include "ipaddress.h"
#include <QHostAddress>
#include <vector>

// NetworkConnection represents a connection to a given network.  These objects
// are returned by NetworkMonitor to identify the current network connections.
class NetworkConnection
{
public:
    NetworkConnection() : NetworkConnection({}, false, false, {}, {}, {}, {}) {}
    // Create NetworkConnection.  Any of the fields could be empty (though it is
    // unusual for the interface to be empty).
    NetworkConnection(const QString &networkInterface,
                      bool defaultIpv4, bool defaultIpv6,
                      const Ipv4Address &gatewayIpv4,
                      const Ipv6Address &gatewayIpv6,
                      std::vector<std::pair<Ipv4Address, unsigned>> addressesIpv4,
                      std::vector<std::pair<Ipv6Address, unsigned>> addressesIpv6);

public:
    bool operator==(const NetworkConnection &other) const
    {
        // Address vectors can be compared directly since the constructor sorts
        // them
        return networkInterface() == other.networkInterface() &&
               defaultIpv4() == other.defaultIpv4() &&
               defaultIpv6() == other.defaultIpv6() &&
               gatewayIpv4() == other.gatewayIpv4() &&
               gatewayIpv6() == other.gatewayIpv6() &&
               addressesIpv4() == other.addressesIpv4() &&
               addressesIpv6() == other.addressesIpv6();
    }
    bool operator!=(const NetworkConnection &other) const {return !(*this == other);}
    // NetworkConnections can be sorted in order to compare sets of network
    // connections.
    bool operator<(const NetworkConnection &other) const;

public:
    // The interface used to connect to this network.  On Windows this is a
    // device GUID; on Mac and Linux it is an interface name.  This isn't
    // necessarily unique among connections.
    const QString &networkInterface() const {return _networkInterface;}
    void networkInterface(QString networkInterface) {_networkInterface = networkInterface;}

    // macOS only - the unique identifier for the primary IPv4 service
    const QString &macosPrimaryServiceKey() const {return _macosPrimaryServiceKey;}
    void macosPrimaryServiceKey(QString key) {_macosPrimaryServiceKey = key;}

    // Whether this connection is the default for IPv4 and/or IPv6.
    bool defaultIpv4() const {return _defaultIpv4;}
    void defaultIpv4(bool defaultIpv4) {_defaultIpv4 = defaultIpv4;}
    bool defaultIpv6() const {return _defaultIpv6;}
    void defaultIpv6(bool defaultIpv6) {_defaultIpv6 = defaultIpv6;}

    // The IPv4 gateway address of this network connection, if present.
    const Ipv4Address &gatewayIpv4() const {return _gatewayIpv4;}
    void gatewayIpv4(const Ipv4Address &gatewayIpv4) {_gatewayIpv4 = gatewayIpv4;}

    // The IPv6 gateway address of this network connection, if present.
    const Ipv6Address &gatewayIpv6() const {return _gatewayIpv6;}
    void gatewayIpv6(const Ipv6Address &gatewayIpv6) {_gatewayIpv6 = gatewayIpv6;}

    // The local IPv4 address(es) of this network connection - zero or more.
    // Provides both the local address and the network prefix length.
    // It's unusual for a connection to have more than one IPv4 address, but
    // platforms don't guarantee this; it's possible.
    const std::vector<std::pair<Ipv4Address, unsigned>> &addressesIpv4() const {return _addressesIpv4;}
    void addressesIpv4(std::vector<std::pair<Ipv4Address, unsigned>> addressesIpv4) {_addressesIpv4 = std::move(addressesIpv4);}

    // The globally-routable IPv6 address(es) of this network connection - zero
    // or more.  Like addressesIpv4(), includes network prefix lengths also.
    //
    // Link-local addresses are excluded, because we don't get these on all
    // platforms (the Mac implementation does not have them since they're not in
    // the dynamic store).  They're not currently needed.
    //
    // Unlike IPv4, it's common to have more than one IPv6 address (there is
    // usually at least a link-local address and a globally routable address,
    // and there may be additional global addresses that rotate).
    const std::vector<std::pair<Ipv6Address, unsigned>> &addressesIpv6() const {return _addressesIpv6;}
    void addressesIpv6(std::vector<std::pair<Ipv6Address, unsigned>> addressesIpv6) {_addressesIpv6 = std::move(addressesIpv6);}

private:
    QString _networkInterface;
    bool _defaultIpv4, _defaultIpv6;
    Ipv4Address _gatewayIpv4;
    Ipv6Address _gatewayIpv6;
    std::vector<std::pair<Ipv4Address, unsigned>> _addressesIpv4;
    std::vector<std::pair<Ipv6Address, unsigned>> _addressesIpv6;
    QString _macosPrimaryServiceKey;
};

// NetworkMonitor monitors the current network connections and identifies the
// networks that we're currently connected to, as a list of NetworkConnection
// objects.
//
// Whenever the connected networks change, it emits the networksChanged()
// signal.
//
// Currently, only the default IPv4 and/or IPv6 connections are required -
// connections other than the defaults do not need to be reported.  The Mac and
// Linux implementations provide all networks, but the Windows implementation
// only provides the defaults.
class NetworkMonitor : public QObject
{
    Q_OBJECT

public:
    NetworkMonitor(){}

protected:
    void updateNetworks(std::vector<NetworkConnection> newNetworks);

public:
    const std::vector<NetworkConnection> &getNetworks() const {return _lastNetworks;}

signals:
    // The network connections have changed
    void networksChanged(const std::vector<NetworkConnection> &newNetworks);

private:
    std::vector<NetworkConnection> _lastNetworks;
};

#endif
