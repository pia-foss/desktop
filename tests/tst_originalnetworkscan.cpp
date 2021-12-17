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
#include <QtTest>

#include "daemon/src/vpn.h"

namespace
{
    QString gatewayIp{"192.168.1.1"};
    QString interfaceName{"eth0"};
    QString ipAddress{"192.168.1.50"};
    unsigned prefixLen{24};
    QString ipAddress6{"2001:db8::123"};
    QString gatewayIp6{"2001:db8::1"};
}

class tst_originalnetworkscan : public QObject
{
    Q_OBJECT

private slots:

    void testEquality()
    {
        OriginalNetworkScan scan1{gatewayIp, interfaceName, ipAddress, prefixLen,
                                  1500, ipAddress6, gatewayIp6, 1500};
        OriginalNetworkScan scan2{gatewayIp, interfaceName, ipAddress, prefixLen,
                                  1500, ipAddress6, gatewayIp6, 1500};
        QVERIFY(scan1 == scan2);

        OriginalNetworkScan scan3{};
        scan3 = scan1;
        QVERIFY(scan1 == scan3);
    }

    void testInequality()
    {
        OriginalNetworkScan scan1{gatewayIp, "eth1", ipAddress, prefixLen, 1500,
                                ipAddress6, gatewayIp6, 1500};
        OriginalNetworkScan scan2{};

        // Tweak each field in turn; verify each field participates in equality
        // comparison
        scan2 = scan1;
        scan2.gatewayIp(QStringLiteral("10.0.0.1"));
        QVERIFY(scan1 != scan2);

        scan2 = scan1;
        scan2.interfaceName(QStringLiteral("eth2"));
        QVERIFY(scan1 != scan2);

        scan2 = scan1;
        scan2.ipAddress(QStringLiteral("172.16.0.2"));
        QVERIFY(scan1 != scan2);

        scan2 = scan1;
        scan2.prefixLength(16);
        QVERIFY(scan1 != scan2);

        scan2 = scan1;
        scan2.mtu(1300);
        QVERIFY(scan1 != scan2);

        scan2 = scan1;
        scan2.ipAddress6(QStringLiteral("2601::2"));
        QVERIFY(scan1 != scan2);

        scan2 = scan1;
        scan2.gatewayIp6(QStringLiteral("fe80::1"));
        QVERIFY(scan1 != scan2);

        scan2 = scan1;
        scan2.mtu6(1300);
        QVERIFY(scan1 != scan2);
    }

    void testValidity()
    {
        OriginalNetworkScan emptyScan{};
        QVERIFY(emptyScan.ipv4Valid() == false);

        OriginalNetworkScan completeScan{gatewayIp, interfaceName, ipAddress,
                                         prefixLen, 1300, ipAddress6,
                                         gatewayIp6, 1300};
        QVERIFY(completeScan.ipv4Valid() == true);

        // An empty ipAddress6 does not impact Ipv4 validity
        OriginalNetworkScan withoutIPv6{gatewayIp, interfaceName, ipAddress,
                                        1300, prefixLen, "", "", 0};
        QVERIFY(withoutIPv6.ipv4Valid() == true);
    }
};

QTEST_GUILESS_MAIN(tst_originalnetworkscan)
#include TEST_MOC
