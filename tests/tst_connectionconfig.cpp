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
    QStringList piaDummyDnsServers{QStringLiteral("1.1.1.1"), QStringLiteral("2.2.2.2")};
}

class tst_connectionconfig : public QObject
{
    Q_OBJECT

private slots:

    // Default (PIA dns)
    void testGetDnsServers()
    {
        ConnectionConfig config1;
        QVERIFY(config1.getDnsServers(piaDummyDnsServers) == piaDummyDnsServers);
        QVERIFY(config1.dnsType() == ConnectionConfig::DnsType::Pia);
    }

    // Handshake DNS
    void testGenDnsServersHandshake()
    {
        DaemonSettings settings;
        DaemonState state;
        DaemonAccount account;

        settings.overrideDNS(QStringLiteral("handshake"));
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.getDnsServers(piaDummyDnsServers) == QStringList{resolverLocalAddress});
        QVERIFY(config.dnsType() == ConnectionConfig::DnsType::Handshake);
    }

    // Local DNS
    void testGenDnsServersLocal()
    {
        DaemonSettings settings;
        DaemonState state;
        DaemonAccount account;

        settings.overrideDNS(QStringLiteral("local"));
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.getDnsServers(piaDummyDnsServers) == QStringList{resolverLocalAddress});
        QVERIFY(config.dnsType() == ConnectionConfig::DnsType::Local);
    }

    // Custom DNS
    void testGetDnsServersCustom()
    {
        DaemonSettings settings;
        DaemonState state;
        DaemonAccount account;

        auto customDns = QStringList{"1.1.1.1", "8.8.8.8"};
        settings.overrideDNS(customDns);
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.getDnsServers(piaDummyDnsServers) == customDns);
        QVERIFY(config.dnsType() == ConnectionConfig::DnsType::Custom);
        // special getter only for customDns
        QVERIFY(config.customDns() == customDns);
    }

    // Existing DNS
    void testGetDnsServersExisting()
    {
        DaemonSettings settings;
        DaemonState state;
        DaemonAccount account;

        auto emptyList = QStringList{};
        settings.overrideDNS(""); // Empty string indicates Existing DNS
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.getDnsServers(piaDummyDnsServers) == emptyList);
        QVERIFY(config.dnsType() == ConnectionConfig::DnsType::Existing);
    }

    void testMethodForcedByAuthDefault()
    {
        DaemonSettings settings;
        DaemonState state;
        DaemonAccount account;

        // Default (OpenVPN)
        settings.method(QStringLiteral("openvpn"));
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.method() == ConnectionConfig::Method::OpenVPN);
        QVERIFY(config.methodForcedByAuth() == false);
    }

    void testMethodForcedByAuthWireguard()
    {
        DaemonSettings settings;
        DaemonState state;
        DaemonAccount account;

        settings.method(QStringLiteral("wireguard"));
        // valid token must match this regex: "^[0-9A-Fa-f]+$""
        account.token("BABA");
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.method() == ConnectionConfig::Method::Wireguard);
        QVERIFY(config.methodForcedByAuth() == false);
    }

    void testMethodForcedByAuthWireguardNoToken()
    {
        DaemonSettings settings;
        DaemonState state;
        DaemonAccount account;

        // Wanted Wireguard but forced to OpenVPN as no valid token
        settings.method(QStringLiteral("wireguard"));
        account.token(""); // no token
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.method() == ConnectionConfig::Method::OpenVPN);
        QVERIFY(config.methodForcedByAuth() == true);
    }

    void testparseIpv4Host()
    {
        // Valid Ipv4 IP
        QHostAddress address1 = ConnectionConfig::parseIpv4Host(QStringLiteral("1.1.1.1"));
        QVERIFY(!address1.isNull());

        // Hostnames are not accepted
        QTest::ignoreMessage(QtInfoMsg, "Invalid SOCKS proxy address: \"privateinternetaccess.com\"");
        QHostAddress address2 = ConnectionConfig::parseIpv4Host(QStringLiteral("privateinternetaccess.com"));
        QVERIFY(address2.isNull());

        // Ipv6 IP are not accepted
        QTest::ignoreMessage(QtWarningMsg, "Invalid SOCKS proxy network protocol QAbstractSocket::IPv6Protocol for address QHostAddress(\"fe80::1\") - parsed from \"fe80::1\"");
        QHostAddress address3 = ConnectionConfig::parseIpv4Host(QStringLiteral("fe80::1"));
        QVERIFY(address3.isNull());
    }
};

QTEST_GUILESS_MAIN(tst_connectionconfig)
#include TEST_MOC
