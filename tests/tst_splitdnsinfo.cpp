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
#include <QtTest>

#include "daemon/src/posix/posix_firewall_iptables.h"
#include "linux/linux_cgroup.h"

namespace
{
    const QString dnsServer1{"1.1.1.1"};
    const QString dnsServer2{"8.8.8.8"};
    const QString localDnsServer{"127.0.0.53"};
    const QString sourceIp1{"192.168.1.2"};
    const QString sourceIp2{"192.168.1.3"};
    const QString localSourceIp{"127.0.0.1"};
    OriginalNetworkScan netScan{"192.168.1.1", "eth0", "192.168.1.43", 24, 1500,
                                "2001:db8::123", "2001::1", 1500};
}

class tst_splitdnsinfo : public QObject
{
    Q_OBJECT

private slots:

    void testValidity()
    {
        // Empty
        QVERIFY(SplitDNSInfo{}.isValid() == false);
        // Missing a field
        SplitDNSInfo info1{dnsServer1, CGroup::bypassId, ""};
        QVERIFY(info1.isValid() == false);
        // All fields present
        SplitDNSInfo info2{dnsServer1, CGroup::bypassId, sourceIp1};
        QVERIFY(info2.isValid() == true);
    }

    void testEquality()
    {
        SplitDNSInfo info1{dnsServer1, CGroup::bypassId, sourceIp1};
        SplitDNSInfo info2{dnsServer2, CGroup::bypassId, sourceIp1};
        SplitDNSInfo info3{dnsServer1, CGroup::bypassId, sourceIp1};

        QVERIFY(info1 != info2);
        QVERIFY(info1 == info3);
    }

    void testInfoForRemoteDnsWithBypass()
    {
        FirewallParams params{};
        params.netScan = netScan;

        DaemonState state;
        state.existingDNSServers({QHostAddress{dnsServer1}.toIPv4Address()});

        // Bypass apps
        auto info1 = SplitDNSInfo::infoFor(params, state, SplitDNSInfo::SplitDNSType::Bypass);

        QVERIFY(info1.dnsServer() == dnsServer1);
        QVERIFY(info1.cGroupId() == CGroup::bypassId);
        QVERIFY(info1.sourceIp() == netScan.ipAddress());
    }

    void testInfoForLocalDnsWithBypass()
    {
        FirewallParams params{};
        params.netScan = netScan;

        DaemonState state;
        state.existingDNSServers({QHostAddress{localDnsServer}.toIPv4Address()});

        auto info1 = SplitDNSInfo::infoFor(params, state, SplitDNSInfo::SplitDNSType::Bypass);

        QVERIFY(info1.dnsServer() == localDnsServer);
        QVERIFY(info1.cGroupId() == CGroup::bypassId);
        // Use of a localhost DNS (127/8) forces a localhost ip
        // We need a localhost source to send to a localhost dest (localDnsServer1)
        QVERIFY(info1.sourceIp() == "127.0.0.1");
    }

    void testInfoForRemoteDnsWithVpnOnly()
    {
        DaemonSettings settings{};
        auto customDNS = QStringList{dnsServer1, dnsServer2};
        settings.overrideDNS(customDNS);

        DaemonState state;
        state.tunnelDeviceLocalAddress(sourceIp1);

        FirewallParams params{};
        DaemonAccount account{};
        params._connectionSettings.emplace(settings, state, account);

        auto info1 = SplitDNSInfo::infoFor(params, state, SplitDNSInfo::SplitDNSType::VpnOnly);

        QVERIFY(info1.dnsServer() == dnsServer1);
        QVERIFY(info1.cGroupId() == CGroup::vpnOnlyId);
        QVERIFY(info1.sourceIp() == state.tunnelDeviceLocalAddress());
    }

    void testInfoForLocalDnsWithVpnOnly()
    {
        DaemonSettings settings{};
        settings.overrideDNS(QStringLiteral("local"));

        DaemonState state;
        state.tunnelDeviceLocalAddress(sourceIp1);

        FirewallParams params{};
        DaemonAccount account{};
        params._connectionSettings.emplace(settings, state, account);

        auto info1 = SplitDNSInfo::infoFor(params, state, SplitDNSInfo::SplitDNSType::VpnOnly);

        // the ip of our local resolve dns server
        QVERIFY(info1.dnsServer() == resolverLocalAddress());
        QVERIFY(info1.cGroupId() == CGroup::vpnOnlyId);
        // local source ip as the dns is local (127/8)
        QVERIFY(info1.sourceIp() == localSourceIp);
    }
};

QTEST_GUILESS_MAIN(tst_splitdnsinfo)
#include TEST_MOC
