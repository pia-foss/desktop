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

#include <common/src/common.h>
#include <QtTest>

#include <kapps_core/src/ipaddress.h>
#include <kapps_net/src/linux/linux_firewall.h>
#include <kapps_net/src/linux/linux_cgroup.h>
#include "daemon/src/vpn.h"
#include "brand.h"
#include <common/src/builtin/path.h>
#include <common/src/settings/daemonaccount.h>

namespace
{
    using SplitDNSInfo = kapps::net::SplitDNSInfo;
    using FirewallParams = kapps::net::FirewallParams;

    const std::string dnsServer1{"1.1.1.1"};
    const std::string dnsServer2{"8.8.8.8"};
    const std::string localDnsServer{"127.0.0.53"};
    const std::string sourceIp1{"192.168.1.2"};
    const std::string sourceIp2{"192.168.1.3"};
    const std::string localSourceIp{"127.0.0.1"};
    OriginalNetworkScan netScan{"192.168.1.1", "eth0", "192.168.1.43", 24, 1500,
        "2001:db8::123", "2001::1", 1500};

    kapps::net::FirewallConfig firewallConfig()
    {
        kapps::net::FirewallConfig c{};
        c.brandInfo.code = BRAND_CODE;
        c.brandInfo.cgroupBase = BRAND_LINUX_CGROUP_BASE;
        c.brandInfo.fwmarkBase = BRAND_LINUX_FWMARK_BASE;
        c.bypassFile = Path::VpnExclusionsFile;
        c.vpnOnlyFile = Path::VpnOnlyFile;
        c.defaultFile = Path::ParentVpnExclusionsFile;
        return c;
    }

    kapps::net::CGroupIds &cgroup()
    {
        static kapps::net::CGroupIds cgroup{firewallConfig()};
        return cgroup;
    }
}

class tst_splitdnsinfo : public QObject
{
    Q_OBJECT

private slots:

    void init()
    {
        Path::initializePreApp();
        Path::initializePostApp();
    }

    void testValidity()
    {
        // Empty
        QVERIFY(SplitDNSInfo{}.isValid() == false);
        // Missing a field
        SplitDNSInfo info1{dnsServer1, cgroup().bypassId(), ""};
        QVERIFY(info1.isValid() == false);
        // All fields present
        SplitDNSInfo info2{dnsServer1, cgroup().bypassId(), sourceIp1};
        QVERIFY(info2.isValid() == true);
    }

    void testEquality()
    {
        SplitDNSInfo info1{dnsServer1, cgroup().bypassId(), sourceIp1};
        SplitDNSInfo info2{dnsServer2, cgroup().bypassId(), sourceIp1};
        SplitDNSInfo info3{dnsServer1, cgroup().bypassId(), sourceIp1};

        QVERIFY(info1 != info2);
        QVERIFY(info1 == info3);
    }

    void testInfoForRemoteDnsWithBypass()
    {
        FirewallParams params{};
        params.netScan = netScan;
        params.existingDNSServers = {kapps::core::Ipv4Address{dnsServer1}.address()};

        // Bypass apps
        auto info1 = SplitDNSInfo::infoFor(params, SplitDNSInfo::SplitDNSType::Bypass, cgroup());

        QVERIFY(info1.dnsServer() == dnsServer1);
        QVERIFY(info1.cGroupId() == cgroup().bypassId());
        QVERIFY(info1.sourceIp() == netScan.ipAddress());
    }

    void testInfoForLocalDnsWithBypass()
    {
        FirewallParams params{};
        params.netScan = netScan;
        params.existingDNSServers = {kapps::core::Ipv4Address{localDnsServer}.address()};

        auto info1 = SplitDNSInfo::infoFor(params, SplitDNSInfo::SplitDNSType::Bypass, cgroup());

        QVERIFY(info1.dnsServer() == localDnsServer);
        QVERIFY(info1.cGroupId() == cgroup().bypassId());
        // Use of a localhost DNS (127/8) forces a localhost ip
        // We need a localhost source to send to a localhost dest (localDnsServer1)
        QVERIFY(info1.sourceIp() == "127.0.0.1");
    }

    void testInfoForRemoteDnsWithVpnOnly()
    {
        FirewallParams params{};
        params.tunnelDeviceLocalAddress = sourceIp1;
        params.effectiveDnsServers = { dnsServer1, dnsServer2 };

        auto info1 = SplitDNSInfo::infoFor(params, SplitDNSInfo::SplitDNSType::VpnOnly, cgroup());

        QVERIFY(info1.dnsServer() == dnsServer1);
        QVERIFY(info1.cGroupId() == cgroup().vpnOnlyId());
        QVERIFY(info1.sourceIp() == params.tunnelDeviceLocalAddress);
    }

    void testInfoForLocalDnsWithVpnOnly()
    {
        FirewallParams params{};
        params.effectiveDnsServers = { resolverLocalAddress().toStdString() };
        params.tunnelDeviceLocalAddress = sourceIp1;
        DaemonAccount account{};

        auto info1 = SplitDNSInfo::infoFor(params, SplitDNSInfo::SplitDNSType::VpnOnly, cgroup());

        // the ip of our local resolve dns server
        QVERIFY(info1.dnsServer() == resolverLocalAddress().toStdString());
        QVERIFY(info1.cGroupId() == cgroup().vpnOnlyId());
        // local source ip as the dns is local (127/8)
        QVERIFY(info1.sourceIp() == localSourceIp);
    }
};

QTEST_GUILESS_MAIN(tst_splitdnsinfo)
#include TEST_MOC
