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
#include <QtTest>

#include "daemon/src/vpn.h"
#include "daemon/src/locations.h"

namespace samples
{
    const auto locationJson = QJsonDocument::fromJson(R"(
{
  "nz": {
    "name": "New Zealand",
    "country": "NZ",
    "dns": "nz.privateinternetaccess.com",
    "port_forward": false,
    "ping": "103.231.91.35:8888",
    "serial": "dummy",
    "openvpn_udp": {
      "best": "103.231.91.35:8080"
    },
    "openvpn_tcp": {
      "best": "103.231.91.35:500"
    },
    "ips": []
  },
  "info":{"vpn_ports":{"udp":[1001,1002],"tcp":[1003,1004]}}
}
   )").object();

    const auto emptyJson = QJsonDocument::fromJson("{}").object();
}

namespace
{
    const LocationsById locs{buildLegacyLocations({}, samples::locationJson, samples::emptyJson)};
    const Location &location = *locs.at(QStringLiteral("nz"));

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

private slots:

    void testPreferred()
    {
        TransportSelector transportSelector;
        QHostAddress dummyAddr{0xC0000201};
        bool delayNext;

        // udp with port 0 should fall back to port in openvpn_udp
        transportSelector.reset({QStringLiteral("udp"), 0}, false, udpPorts, tcpPorts);
        transportSelector.beginAttempt(location, dummyAddr, delayNext);
        QVERIFY(transportSelector.lastPreferred().port() == preferredUdpPort);
        QVERIFY(transportSelector.lastPreferred().protocol() == QStringLiteral("udp"));

        // tcp with port 0 should fall back to port in openvpn_tcp
        transportSelector.reset({QStringLiteral("tcp"), 0}, false, udpPorts, tcpPorts);
        transportSelector.beginAttempt(location, dummyAddr, delayNext);
        QVERIFY(transportSelector.lastPreferred().port() == preferredTcpPort);
        QVERIFY(transportSelector.lastPreferred().protocol() == QStringLiteral("tcp"));
    }

    void testAlternates()
    {
        const QString preferredProtocol{"udp"};
        QHostAddress dummyAddr{0xC0000201};
        bool delayNext;

        // We don't want to wait before trying an alternate transport
        // so we set _transportTimeout to 0 in the constructor
        TransportSelector transportSelector{std::chrono::seconds(0)};
        transportSelector.reset({"udp", 0}, true, udpPorts, tcpPorts);

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
};

QTEST_GUILESS_MAIN(tst_transportselector)
#include TEST_MOC
