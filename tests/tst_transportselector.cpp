// Copyright (c) 2026 Private Internet Access, Inc.
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
#include <QtTest>

#include "daemon/src/vpn.h"
#include "common/src/locations.h"

namespace samples
{
    const auto locationJson = QJsonDocument::fromJson(R"(
{
  "groups": {
    "ovpntcp": [{ "name": "openvpn_tcp", "ports": [500, 1003,1004] }],
    "ovpnudp": [{ "name": "openvpn_udp", "ports": [8080, 1001,1002] }],
    "wg": [{ "name": "wireguard", "ports": [1337] }],
    "ikev2": [{ "name": "ikev2", "ports": [500, 4500] }],
    "proxysocks": [{ "name": "socks", "ports": [1080] }],
    "proxyss": [{ "name": "shadowsocks", "ports": [443] }]
  },
  "regions": [
    {
      "id": "nz",
      "name": "New Zealand",
      "country": "NZ",
      "auto_region": true,
      "dns": "nz.privacy.network",
      "port_forward": true,
      "geo": false,
      "offline": false,
      "servers": {
        "ovpnudp": [{ "ip": "43.250.207.86", "cn": "newzealand403" }],
        "ovpntcp": [{ "ip": "43.250.207.85", "cn": "newzealand403" }],
        "ikev2": [{ "ip": "43.250.207.85", "cn": "newzealand403" }],
        "wg": [{ "ip": "43.250.207.85", "cn": "newzealand403" }]
      }
    }
  ]
}
)").object();
    const auto metadataJson = QJsonDocument::fromJson(R"({
        "translations":{},
        "country_groups":{},
        "gps":{}
    })").object();
}

namespace
{

    enum : quint16
    {
        // The order of attempts isn't particularly important, but the tests
        // validate it in order to ensure that all ports are tried.  Currently
        // the ports are checked in descending order.
        firstAltUdp = 1002,
        secondAltUdp = 1001,
        firstAltTcp = 1004,
        secondAltTcp = 1003
    };
    const DescendingPortSet udpPorts{firstAltUdp, secondAltUdp};
    const DescendingPortSet tcpPorts{firstAltTcp, secondAltTcp};

    const uint preferredUdpPort{8080}; // see json
    const uint preferredTcpPort{500};  // see json
}

class tst_transportselector : public QObject
{
    Q_OBJECT

    auto buildRegionsFromJson(const QJsonObject &locationsJson)
    {
        return buildModernLocations({}, locationsJson,
            {}, samples::metadataJson, {}, {}).first;
    }

    // Build the locations with a DIP in "nz" that has its own OpenVPN ports
    auto buildDipLocations(std::vector<quint16> udpPorts = {1197, 1198},
                           std::vector<quint16> tcpPorts = {501})
    {
        AccountDedicatedIp dip;
        dip.id(QStringLiteral("dip-01234567"));
        dip.regionId(QStringLiteral("nz"));
        dip.serviceGroups({QStringLiteral("ovpnudp"), QStringLiteral("ovpntcp")});
        dip.ip(QStringLiteral("191.96.74.2"));
        dip.cn(QStringLiteral("newzealand429"));
        dip.openvpnUdpPorts(std::move(udpPorts));
        dip.openvpnTcpPorts(std::move(tcpPorts));
        return buildModernLocations({}, samples::locationJson,
            {}, samples::metadataJson, {dip}, {}).first;
    }

private slots:

    void testPreferred()
    {
        LocationsById locs{buildRegionsFromJson(samples::locationJson)};
        const Location &location = *locs.at("nz");

        TransportSelector transportSelector;
        QHostAddress dummyAddr{0xC0000201};
        bool delayNext;

        // udp with port 0 should fall back to port in openvpn_udp
        transportSelector.reset(QStringLiteral("udp"), 0, false, udpPorts, tcpPorts);
        transportSelector.beginAttempt(location, dummyAddr, delayNext);
        QVERIFY(transportSelector.lastPreferred().port() == preferredUdpPort);
        QVERIFY(transportSelector.lastPreferred().protocol() == QStringLiteral("udp"));

        // tcp with port 0 should fall back to port in openvpn_tcp
        transportSelector.reset(QStringLiteral("tcp"), 0, false, udpPorts, tcpPorts);
        transportSelector.beginAttempt(location, dummyAddr, delayNext);
        QVERIFY(transportSelector.lastPreferred().port() == preferredTcpPort);
        QVERIFY(transportSelector.lastPreferred().protocol() == QStringLiteral("tcp"));
    }

    void testAlternates()
    {
        const QString preferredProtocol{"udp"};
        QHostAddress dummyAddr{0xC0000201};
        bool delayNext;

        LocationsById locs{buildRegionsFromJson(samples::locationJson)};
        const Location &location = *locs.at("nz");

        // We don't want to wait before trying an alternate transport
        // so we set _transportTimeout to 0 in the constructor
        TransportSelector transportSelector{std::chrono::seconds(0)};
        transportSelector.reset("udp", 0, true, udpPorts, tcpPorts);

        // First attempt uses our preferred transport
        transportSelector.beginAttempt(location, dummyAddr, delayNext);
        QVERIFY(transportSelector.lastUsed().port() == preferredUdpPort);
        QVERIFY(transportSelector.lastUsed().protocol() == preferredProtocol);

        // Succussive attempts try the provided alternate udpPorts
        transportSelector.beginAttempt(location, dummyAddr, delayNext);
        QVERIFY(transportSelector.lastUsed().port() == firstAltUdp);
        QVERIFY(transportSelector.lastUsed().protocol() == preferredProtocol);

         // We try the preferred port again between trying alternates
        transportSelector.beginAttempt(location, dummyAddr, delayNext);
        QVERIFY(transportSelector.lastUsed().port() == preferredUdpPort);
        QVERIFY(transportSelector.lastUsed().protocol() == preferredProtocol);

        transportSelector.beginAttempt(location, dummyAddr, delayNext);
        QVERIFY(transportSelector.lastUsed().port() == secondAltUdp);
        QVERIFY(transportSelector.lastUsed().protocol() == preferredProtocol);

        // Every other attempt tries the preferred transport
        // (no need to verify it again)
        transportSelector.beginAttempt(location, dummyAddr, delayNext);

        // Now we've exhausted all UDP ports The protocol changes to tcp
        transportSelector.beginAttempt(location, dummyAddr, delayNext);
        QVERIFY(transportSelector.lastUsed().port() == preferredTcpPort);
        QVERIFY(transportSelector.lastUsed().protocol() == "tcp");

        // We call beginAttempt() twice now to skip the preferred transport attempt
        transportSelector.beginAttempt(location, dummyAddr, delayNext);
        transportSelector.beginAttempt(location, dummyAddr, delayNext);

        // Try the first alternate tcp port
        QVERIFY(transportSelector.lastUsed().port() == firstAltTcp);
        QVERIFY(transportSelector.lastUsed().protocol() == "tcp");

        transportSelector.beginAttempt(location, dummyAddr, delayNext);
        transportSelector.beginAttempt(location, dummyAddr, delayNext);
        QVERIFY(transportSelector.lastUsed().port() == secondAltTcp);
        QVERIFY(transportSelector.lastUsed().protocol() == "tcp");

        transportSelector.beginAttempt(location, dummyAddr, delayNext);
        transportSelector.beginAttempt(location, dummyAddr, delayNext);

        // We've now exhausted all the tcp options, we start
        // again from the beginning with udp
        QVERIFY(transportSelector.lastUsed().protocol() == "udp");
    }

    // A DIP connects on the selected DIP port.  0, or a port the DIP doesn't
    // offer (like a regular OpenVPN port), uses the DIP's first port.
    void testDedicatedIpPorts()
    {
        LocationsById locs{buildDipLocations()};
        const Location &dip = *locs.at("dip-01234567");
        const DescendingPortSet dipUdpPorts = dip.allPortsForService(Service::OpenVpnUdp);
        const DescendingPortSet dipTcpPorts = dip.allPortsForService(Service::OpenVpnTcp);
        QVERIFY(dipUdpPorts == (DescendingPortSet{1197, 1198}));
        QVERIFY(dipTcpPorts == (DescendingPortSet{501}));

        TransportSelector transportSelector;
        QHostAddress dummyAddr{0xC0000201};
        bool delayNext;
        const auto usedPort = [&](const QString &protocol, uint port)
        {
            transportSelector.reset(protocol, port, false, dipUdpPorts, dipTcpPorts);
            transportSelector.beginAttempt(dip, dummyAddr, delayNext);
            return transportSelector.lastUsed().port();
        };

        // Multiple UDP ports - the selected one is used
        QCOMPARE(usedPort(QStringLiteral("udp"), 1198), 1198u);
        QCOMPARE(usedPort(QStringLiteral("udp"), 1197), 1197u);
        // Default or a port not offered by the DIP - first DIP port
        QCOMPARE(usedPort(QStringLiteral("udp"), 0), 1197u);
        QCOMPARE(usedPort(QStringLiteral("udp"), preferredUdpPort), 1197u);

        // Single TCP port - always used
        QCOMPARE(usedPort(QStringLiteral("tcp"), 501), 501u);
        QCOMPARE(usedPort(QStringLiteral("tcp"), 0), 501u);
        QCOMPARE(usedPort(QStringLiteral("tcp"), preferredTcpPort), 501u);
    }

    // The default DIP port is the first port in API order, not the lowest
    // port, and it matches dedicatedIpPortToUse()
    void testDedicatedIpDefaultPortApiOrder()
    {
        const std::vector<quint16> apiUdpPorts{1198, 1197};
        LocationsById locs{buildDipLocations(apiUdpPorts, {502, 501})};
        const Location &dip = *locs.at("dip-01234567");

        TransportSelector transportSelector;
        QHostAddress dummyAddr{0xC0000201};
        bool delayNext;
        transportSelector.reset(QStringLiteral("udp"), 0, false,
                                dip.allPortsForService(Service::OpenVpnUdp),
                                dip.allPortsForService(Service::OpenVpnTcp));
        transportSelector.beginAttempt(dip, dummyAddr, delayNext);
        QCOMPARE(transportSelector.lastUsed().port(), 1198u);
        QCOMPARE(transportSelector.lastUsed().port(), uint{dedicatedIpPortToUse(0, apiUdpPorts)});

        transportSelector.reset(QStringLiteral("tcp"), 0, false,
                                dip.allPortsForService(Service::OpenVpnUdp),
                                dip.allPortsForService(Service::OpenVpnTcp));
        transportSelector.beginAttempt(dip, dummyAddr, delayNext);
        QCOMPARE(transportSelector.lastUsed().port(), 502u);
    }

    // With alternate settings, a DIP only tries the DIP ports, never the
    // regular OpenVPN ports of the corresponding region
    void testDedicatedIpAlternates()
    {
        LocationsById locs{buildDipLocations()};
        const Location &dip = *locs.at("dip-01234567");

        TransportSelector transportSelector{std::chrono::seconds(0)};
        QHostAddress dummyAddr{0xC0000201};
        bool delayNext;
        transportSelector.reset(QStringLiteral("udp"), 1197, true,
                                dip.allPortsForService(Service::OpenVpnUdp),
                                dip.allPortsForService(Service::OpenVpnTcp));

        std::set<std::pair<QString, uint>> used;
        for(int i = 0; i < 20; ++i)
        {
            transportSelector.beginAttempt(dip, dummyAddr, delayNext);
            used.insert({transportSelector.lastUsed().protocol(),
                         transportSelector.lastUsed().port()});
        }

        const std::set<std::pair<QString, uint>> expected{
            {QStringLiteral("udp"), 1197}, {QStringLiteral("udp"), 1198},
            {QStringLiteral("tcp"), 501}};
        QVERIFY(used == expected);
    }
};

QTEST_GUILESS_MAIN(tst_transportselector)
#include TEST_MOC
