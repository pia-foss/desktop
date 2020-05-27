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

namespace
{
    QString gatewayIp{"192.168.1.1"};
    QString interfaceName{"eth0"};
    QString ipAddress{"192.168.1.50"};
    QString ipAddress6{"2001:db8::123"};
}

class tst_originalnetworkscan : public QObject
{
    Q_OBJECT

private slots:

    void testEquality()
    {
        OriginalNetworkScan scan1{gatewayIp, interfaceName, ipAddress, ipAddress6};
        OriginalNetworkScan scan2{gatewayIp, interfaceName, ipAddress, ipAddress6};
        QVERIFY(scan1 == scan2);
    }

    void testInequality()
    {
        OriginalNetworkScan scan1{gatewayIp, "eth1", ipAddress, ipAddress6};
        OriginalNetworkScan scan2{gatewayIp, interfaceName, ipAddress, ipAddress6};
        QVERIFY(scan1 != scan2);
    }

    void testValidity()
    {
        OriginalNetworkScan emptyScan{};
        QVERIFY(emptyScan.ipv4Valid() == false);

        OriginalNetworkScan completeScan{gatewayIp, interfaceName, ipAddress, ipAddress6};
        QVERIFY(completeScan.ipv4Valid() == true);

        // An empty ipAddress6 does not impact Ipv4 validity
        OriginalNetworkScan withoutIPv6{gatewayIp, interfaceName, ipAddress, ""};
        QVERIFY(withoutIPv6.ipv4Valid() == true);
    }
};

QTEST_GUILESS_MAIN(tst_originalnetworkscan)
#include TEST_MOC
