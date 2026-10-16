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

namespace
{
    QStringList piaDummyDnsServers{QStringLiteral("209.222.18.222"), QStringLiteral("209.222.18.218")};

    const auto locationJson = QJsonDocument::fromJson(R"(
{
  "groups": {
    "ovpntcp": [{ "name": "openvpn_tcp", "ports": [80, 443] }],
    "ovpnudp": [{ "name": "openvpn_udp", "ports": [8080, 853] }]
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
        "ovpntcp": [{ "ip": "43.250.207.85", "cn": "newzealand403" }]
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

class tst_connectionconfig : public QObject
{
    Q_OBJECT

    // Select the given location in the state, as the daemon does when a
    // location is chosen
    void selectLocation(StateModel &state, const QSharedPointer<const Location> &pLocation)
    {
        state.vpnLocations(ServiceLocations{pLocation, pLocation, pLocation});
    }

    // Build the locations with a DIP in "nz", also adding the DIP to the account
    LocationsById buildDipLocations(DaemonAccount &account,
                                    std::vector<quint16> udpPorts = {1197, 1198},
                                    std::vector<quint16> tcpPorts = {501})
    {
        AccountDedicatedIp dip;
        dip.id(QStringLiteral("dip-01234567"));
        dip.dipToken(QStringLiteral("DIP1a2b3c"));
        dip.regionId(QStringLiteral("nz"));
        dip.serviceGroups({QStringLiteral("ovpnudp"), QStringLiteral("ovpntcp")});
        dip.ip(QStringLiteral("191.96.74.2"));
        dip.cn(QStringLiteral("newzealand429"));
        dip.openvpnUdpPorts(std::move(udpPorts));
        dip.openvpnTcpPorts(std::move(tcpPorts));
        account.dedicatedIps({dip});
        return buildModernLocations({}, locationJson, {}, metadataJson,
            account.dedicatedIps(), {}).first;
    }

private slots:

    // Default (PIA dns)
    void testGetDnsServers()
    {
        ConnectionConfig config1;
        QVERIFY(config1.dnsType() == ConnectionConfig::DnsType::Pia);
    }

    // Handshake DNS
    void testGenDnsServersHdns()
    {
        DaemonSettings settings;
        StateModel state;
        DaemonAccount account;

        settings.overrideDNS(QStringLiteral("hdns"));
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.getDnsServers() == QStringList{hDnsAddress()});
        QVERIFY(config.dnsType() == ConnectionConfig::DnsType::HDns);
    }

    // Local DNS
    void testGenDnsServersLocal()
    {
        DaemonSettings settings;
        StateModel state;
        DaemonAccount account;

        settings.overrideDNS(QStringLiteral("local"));
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.getDnsServers() == QStringList{resolverLocalAddress()});
        QVERIFY(config.dnsType() == ConnectionConfig::DnsType::Local);
    }

    // Custom DNS
    void testGetDnsServersCustom()
    {
        DaemonSettings settings;
        StateModel state;
        DaemonAccount account;

        auto customDns = QStringList{"1.1.1.1", "8.8.8.8"};
        settings.overrideDNS(customDns);
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.getDnsServers() == customDns);
        QVERIFY(config.dnsType() == ConnectionConfig::DnsType::Custom);
        // special getter only for customDns
        QVERIFY(config.customDns() == customDns);
    }

    // Existing DNS
    void testGetDnsServersExisting()
    {
        DaemonSettings settings;
        StateModel state;
        DaemonAccount account;

        auto emptyList = QStringList{};
        settings.overrideDNS(""); // Empty string indicates Existing DNS
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.getDnsServers() == emptyList);
        QVERIFY(config.dnsType() == ConnectionConfig::DnsType::Existing);
    }

    void testMethodForcedByAuthDefault()
    {
        DaemonSettings settings;
        StateModel state;
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
        StateModel state;
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
        StateModel state;
        DaemonAccount account;

        // Wanted Wireguard but forced to OpenVPN as no valid token
        settings.method(QStringLiteral("wireguard"));
        account.token(""); // no token
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.method() == ConnectionConfig::Method::OpenVPN);
        QVERIFY(config.methodForcedByAuth() == true);
    }

    // Obfuscation is off by default, and only captured for WireGuard
    void testWireguardObfuscationDefault()
    {
        DaemonSettings settings;
        StateModel state;
        DaemonAccount account;

        settings.method(QStringLiteral("wireguard"));
        account.token("BABA");
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.wireguardObfuscation() == false);
    }

    // Enabling the setting is captured, except on Windows where AmneziaWG is
    // not supported at all
    void testWireguardObfuscationEnabled()
    {
        DaemonSettings settings;
        StateModel state;
        DaemonAccount account;

        settings.method(QStringLiteral("wireguard"));
        settings.wireguardObfuscation(true);
        account.token("BABA");
        ConnectionConfig config{settings, state, account};
#if defined(Q_OS_WIN)
        QVERIFY(config.wireguardObfuscation() == false);
#else
        QVERIFY(config.wireguardObfuscation() == true);
#endif
    }

    // Toggling obfuscation has to reconnect to apply
    void testWireguardObfuscationRequiresReconnect()
    {
        DaemonSettings settings;
        StateModel state;
        DaemonAccount account;

        settings.method(QStringLiteral("wireguard"));
        account.token("BABA");
        ConnectionConfig configOff{settings, state, account};

        settings.wireguardObfuscation(true);
        ConnectionConfig configOn{settings, state, account};

#if defined(Q_OS_WIN)
        // The setting is ignored on Windows, so nothing changes
        QVERIFY(!configOff.hasChanged(configOn));
#else
        QVERIFY(configOff.hasChanged(configOn));
#endif
    }

    // A DIP uses the DIP remote port settings, other regions use the regular
    // remote port settings
    void testDedicatedIpRemotePort()
    {
        DaemonSettings settings;
        StateModel state;
        DaemonAccount account;
        LocationsById locs{buildDipLocations(account)};

        settings.method(QStringLiteral("openvpn"));
        settings.remotePortUDP(853);
        settings.remotePortTCP(443);
        settings.dipRemotePortUDP(1198);
        settings.dipRemotePortTCP(501);

        selectLocation(state, locs.at("dip-01234567"));
        settings.protocol(QStringLiteral("udp"));
        QCOMPARE(ConnectionConfig(settings, state, account).openvpnRemotePort(), quint16{1198});
        settings.protocol(QStringLiteral("tcp"));
        QCOMPARE(ConnectionConfig(settings, state, account).openvpnRemotePort(), quint16{501});

        selectLocation(state, locs.at("nz"));
        settings.protocol(QStringLiteral("udp"));
        QCOMPARE(ConnectionConfig(settings, state, account).openvpnRemotePort(), quint16{853});
        settings.protocol(QStringLiteral("tcp"));
        QCOMPARE(ConnectionConfig(settings, state, account).openvpnRemotePort(), quint16{443});
    }

    // Changing the DIP port requires a reconnect only when connecting to a DIP
    void testDedicatedIpRemotePortRequiresReconnect()
    {
        DaemonSettings settings;
        StateModel state;
        DaemonAccount account;
        LocationsById locs{buildDipLocations(account)};

        settings.method(QStringLiteral("openvpn"));
        settings.protocol(QStringLiteral("udp"));
        settings.dipRemotePortUDP(1197);

        selectLocation(state, locs.at("dip-01234567"));
        ConnectionConfig dipBefore{settings, state, account};
        selectLocation(state, locs.at("nz"));
        ConnectionConfig regularBefore{settings, state, account};

        settings.dipRemotePortUDP(1198);

        selectLocation(state, locs.at("dip-01234567"));
        QVERIFY(dipBefore.hasChanged(ConnectionConfig{settings, state, account}));
        selectLocation(state, locs.at("nz"));
        QVERIFY(!regularBefore.hasChanged(ConnectionConfig{settings, state, account}));
    }

    // The DIP port settings default to 0, so a DIP connection uses the first
    // DIP port until the user picks one
    void testDedicatedIpRemotePortDefault()
    {
        DaemonSettings settings;
        StateModel state;
        DaemonAccount account;
        LocationsById locs{buildDipLocations(account)};

        QCOMPARE(settings.dipRemotePortUDP(), 0u);
        QCOMPARE(settings.dipRemotePortTCP(), 0u);

        settings.method(QStringLiteral("openvpn"));
        selectLocation(state, locs.at("dip-01234567"));
        settings.protocol(QStringLiteral("udp"));
        QCOMPARE(ConnectionConfig(settings, state, account).openvpnRemotePort(), quint16{0});
        settings.protocol(QStringLiteral("tcp"));
        QCOMPARE(ConnectionConfig(settings, state, account).openvpnRemotePort(), quint16{0});
    }

    // The Shadowsocks proxy forces TCP, so a DIP uses the DIP TCP port even if
    // the protocol setting is UDP
    void testDedicatedIpRemotePortShadowsocks()
    {
        DaemonSettings settings;
        StateModel state;
        DaemonAccount account;
        LocationsById locs{buildDipLocations(account)};

        settings.method(QStringLiteral("openvpn"));
        settings.protocol(QStringLiteral("udp"));
        settings.proxyEnabled(true);
        settings.proxyType(QStringLiteral("shadowsocks"));
        settings.remotePortTCP(443);
        settings.dipRemotePortUDP(1198);
        settings.dipRemotePortTCP(501);

        selectLocation(state, locs.at("dip-01234567"));
        ConnectionConfig config{settings, state, account};
        QVERIFY(config.openvpnProtocol() == ConnectionConfig::Protocol::TCP);
        QCOMPARE(config.openvpnRemotePort(), quint16{501});
    }

    // Changing the regular remote port doesn't change a DIP connection
    void testDedicatedIpIgnoresRegularRemotePort()
    {
        DaemonSettings settings;
        StateModel state;
        DaemonAccount account;
        LocationsById locs{buildDipLocations(account)};

        settings.method(QStringLiteral("openvpn"));
        settings.protocol(QStringLiteral("udp"));
        settings.dipRemotePortUDP(1197);
        selectLocation(state, locs.at("dip-01234567"));
        ConnectionConfig before{settings, state, account};

        settings.remotePortUDP(853);
        ConnectionConfig after{settings, state, account};
        QCOMPARE(after.openvpnRemotePort(), quint16{1197});
        QVERIFY(!before.hasChanged(after));
    }

    // With WireGuard the DIP OpenVPN port isn't used, so changing it doesn't
    // require a reconnect
    void testDedicatedIpRemotePortWireguard()
    {
        DaemonSettings settings;
        StateModel state;
        DaemonAccount account;
        LocationsById locs{buildDipLocations(account)};

        settings.method(QStringLiteral("wireguard"));
        settings.dipRemotePortUDP(1197);
        selectLocation(state, locs.at("dip-01234567"));
        ConnectionConfig before{settings, state, account};
        QVERIFY(before.method() == ConnectionConfig::Method::Wireguard);
        QCOMPARE(before.openvpnRemotePort(), quint16{0});

        settings.dipRemotePortUDP(1198);
        QVERIFY(!before.hasChanged(ConnectionConfig{settings, state, account}));
    }

    // OpenVPN to a DIP is blocked only when the DIP has no ports for the
    // OpenVPN protocol that would be used; WireGuard and regular regions are
    // never blocked
    void testDedicatedIpLacksOpenVpn()
    {
        struct Case
        {
            const char *name;
            const char *method;
            const char *protocol;
            bool shadowsocks;
            std::vector<quint16> udpPorts;
            std::vector<quint16> tcpPorts;
            bool expected;
        };
        const Case cases[]
        {
            {"UDP with UDP ports", "openvpn", "udp", false, {1197}, {501}, false},
            {"TCP with TCP ports", "openvpn", "tcp", false, {1197}, {501}, false},
            {"UDP without UDP ports", "openvpn", "udp", false, {}, {501}, true},
            {"TCP without TCP ports", "openvpn", "tcp", false, {1197}, {}, true},
            {"no ports at all", "openvpn", "udp", false, {}, {}, true},
            // Shadowsocks forces TCP, so the UDP ports don't matter
            {"Shadowsocks without TCP ports", "openvpn", "udp", true, {1197}, {}, true},
            {"Shadowsocks with TCP ports", "openvpn", "udp", true, {}, {501}, false},
            // WireGuard doesn't use the DIP ports
            {"WireGuard without ports", "wireguard", "udp", false, {}, {}, false},
        };
        for(const auto &c : cases)
        {
            DaemonSettings settings;
            DaemonAccount account;
            LocationsById locs{buildDipLocations(account, c.udpPorts, c.tcpPorts)};
            settings.method(QString::fromLatin1(c.method));
            settings.protocol(QString::fromLatin1(c.protocol));
            if(c.shadowsocks)
            {
                settings.proxyEnabled(true);
                settings.proxyType(QStringLiteral("shadowsocks"));
            }
            QVERIFY2(dedicatedIpLacksOpenVpn(settings, *locs.at("dip-01234567")) == c.expected,
                     c.name);
            // The regular region is never blocked by this check
            QVERIFY2(!dedicatedIpLacksOpenVpn(settings, *locs.at("nz")), c.name);
        }
    }

    // A custom proxy doesn't force TCP, so it doesn't block a DIP with UDP ports
    void testDedicatedIpLacksOpenVpnCustomProxy()
    {
        DaemonSettings settings;
        DaemonAccount account;
        LocationsById locs{buildDipLocations(account, {1197}, {})};
        settings.method(QStringLiteral("openvpn"));
        settings.protocol(QStringLiteral("udp"));
        settings.proxyEnabled(true);
        settings.proxyType(QStringLiteral("custom"));
        QVERIFY(!dedicatedIpLacksOpenVpn(settings, *locs.at("dip-01234567")));
    }

    // The check uses the same OpenVPN protocol as the connection
    void testDedicatedIpLacksOpenVpnMatchesConnectionProtocol()
    {
        DaemonAccount account;
        // Only TCP ports, so the DIP is blocked exactly when UDP would be used
        LocationsById locs{buildDipLocations(account, {}, {501})};
        StateModel state;
        selectLocation(state, locs.at("dip-01234567"));

        for(const char *protocol : {"udp", "tcp"})
        {
            for(const char *proxyType : {"", "custom", "shadowsocks"})
            {
                DaemonSettings settings;
                settings.method(QStringLiteral("openvpn"));
                settings.protocol(QString::fromLatin1(protocol));
                if(*proxyType)
                {
                    settings.proxyEnabled(true);
                    settings.proxyType(QString::fromLatin1(proxyType));
                }
                ConnectionConfig config{settings, state, account};
                const bool usesUdp = config.openvpnProtocol() == ConnectionConfig::Protocol::UDP;
                QVERIFY2(dedicatedIpLacksOpenVpn(settings, *locs.at("dip-01234567")) == usesUdp,
                         QStringLiteral("%1 / %2").arg(protocol, proxyType).toLatin1());
            }
        }
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
