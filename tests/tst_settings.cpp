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
#include <common/src/settings/locations.h>
#include <common/src/locations.h>
#include <QtTest>

namespace sample_docs {

const auto emptyJson = QJsonDocument::fromJson("{}").object();

//JSON doesn't have to have an object at the top level, it could be any JSON
//value
const auto arrayJson = QJsonDocument::fromJson(R"(["foo", 2, false])").object();

const auto metadataJson = QJsonDocument::fromJson(R"(
{
  "translations":{},
  "country_groups":{},
  "gps":{}
})").object();

const auto twoLocations = QJsonDocument::fromJson(R"(
{
  "groups": {
    "ovpntcp": [{ "name": "openvpn_tcp", "ports": [80, 443, 853, 8443] }],
    "ovpnudp": [{ "name": "openvpn_udp", "ports": [8080, 853, 123, 53] }],
    "wg": [{ "name": "wireguard", "ports": [1337] }],
    "ikev2": [{ "name": "ikev2", "ports": [500, 4500] }],
    "proxysocks": [{ "name": "socks", "ports": [1080] }],
    "proxyss": [{ "name": "shadowsocks", "ports": [443] }]
  },
  "regions": [
    {
      "id": "al",
      "name": "Albania",
      "country": "AL",
      "auto_region": true,
      "dns": "al.privacy.network",
      "port_forward": true,
      "geo": false,
      "offline": false,
      "servers": {
        "ovpnudp": [{ "ip": "31.171.154.136", "cn": "tirana401" }],
        "ovpntcp": [{ "ip": "31.171.154.138", "cn": "tirana401" }],
        "ikev2": [{ "ip": "31.171.154.135", "cn": "tirana401" }],
        "wg": [{ "ip": "31.171.154.131", "cn": "tirana401" }]
      }
    },
    {
      "id": "ad",
      "name": "Andorra",
      "country": "AD",
      "auto_region": true,
      "dns": "ad.privacy.network",
      "port_forward": true,
      "geo": true,
      "offline": false,
      "servers": {
        "ovpnudp": [{ "ip": "45.139.49.245", "cn": "andorra401" }],
        "ovpntcp": [{ "ip": "45.139.49.249", "cn": "andorra401" }],
        "ikev2": [{ "ip": "45.139.49.247", "cn": "andorra401" }],
        "wg": [{ "ip": "45.139.49.247", "cn": "andorra401" }]
      }
    }
  ]
}
)").object();

//Regions covering each combination of WireGuard server kinds - only vanilla
//servers, only AmneziaWG servers, both, and neither
const auto obfuscationLocations = QJsonDocument::fromJson(R"(
{
  "groups": {
    "ovpnudp": [{ "name": "openvpn_udp", "ports": [8080] }],
    "wg": [{ "name": "wireguard", "ports": [1337] }],
    "awg": [{ "name": "awg", "ports": [1338] }]
  },
  "regions": [
    {
      "id": "plain",
      "name": "Plain",
      "country": "AL",
      "auto_region": true,
      "dns": "plain.privacy.network",
      "port_forward": true,
      "geo": false,
      "offline": false,
      "servers": {
        "wg": [{ "ip": "10.0.0.1", "cn": "plain401" }]
      }
    },
    {
      "id": "obfuscated",
      "name": "Obfuscated",
      "country": "AD",
      "auto_region": true,
      "dns": "obfuscated.privacy.network",
      "port_forward": true,
      "geo": false,
      "offline": false,
      "servers": {
        "awg": [{ "ip": "10.0.1.1", "cn": "obfuscated401" }]
      }
    },
    {
      "id": "both",
      "name": "Both",
      "country": "AE",
      "auto_region": true,
      "dns": "both.privacy.network",
      "port_forward": true,
      "geo": false,
      "offline": false,
      "servers": {
        "wg": [{ "ip": "10.0.2.1", "cn": "both401" }],
        "awg": [{ "ip": "10.0.2.2", "cn": "both402" }]
      }
    },
    {
      "id": "neither",
      "name": "Neither",
      "country": "AF",
      "auto_region": true,
      "dns": "neither.privacy.network",
      "port_forward": true,
      "geo": false,
      "offline": false,
      "servers": {
        "ovpnudp": [{ "ip": "10.0.3.1", "cn": "neither401" }]
      }
    }
  ]
}
)").object();

const auto oneLocation = QJsonDocument::fromJson(R"(
{
  "groups": {
    "ovpntcp": [{ "name": "openvpn_tcp", "ports": [80, 443, 853, 8443] }],
    "ovpnudp": [{ "name": "openvpn_udp", "ports": [8080, 853, 123, 53] }],
    "wg": [{ "name": "wireguard", "ports": [1337] }],
    "ikev2": [{ "name": "ikev2", "ports": [500, 4500] }],
    "proxysocks": [{ "name": "socks", "ports": [1080] }],
    "proxyss": [{ "name": "shadowsocks", "ports": [443] }]
  },
  "regions": [
    {
      "id": "al",
      "name": "Albania",
      "country": "AL",
      "auto_region": true,
      "dns": "al.privacy.network",
      "port_forward": true,
      "geo": false,
      "offline": false,
      "servers": {
        "ovpnudp": [{ "ip": "31.171.154.136", "cn": "tirana401" }],
        "ovpntcp": [{ "ip": "31.171.154.138", "cn": "tirana401" }],
        "ikev2": [{ "ip": "31.171.154.135", "cn": "tirana401" }],
        "wg": [{ "ip": "31.171.154.131", "cn": "tirana401" }]
      }
    }
  ]
}
)").object();

//The Albania location again with new IP addresses
const auto oneLocationNewIps = QJsonDocument::fromJson(R"(
{
  "groups": {
    "ovpntcp": [{ "name": "openvpn_tcp", "ports": [80, 443, 853, 8443] }],
    "ovpnudp": [{ "name": "openvpn_udp", "ports": [8080, 853, 123, 53] }],
    "wg": [{ "name": "wireguard", "ports": [1337] }],
    "ikev2": [{ "name": "ikev2", "ports": [500, 4500] }],
    "proxysocks": [{ "name": "socks", "ports": [1080] }],
    "proxyss": [{ "name": "shadowsocks", "ports": [443] }]
  },
  "regions": [
    {
      "id": "al",
      "name": "Albania",
      "country": "AL",
      "auto_region": true,
      "dns": "al.privacy.network",
      "port_forward": true,
      "geo": false,
      "offline": false,
      "servers": {
        "ovpnudp": [{ "ip": "31.171.154.42", "cn": "tirana401" }],
        "ovpntcp": [{ "ip": "31.171.154.42", "cn": "tirana401" }],
        "ikev2": [{ "ip": "31.171.154.42", "cn": "tirana401" }],
        "wg": [{ "ip": "31.171.154.42", "cn": "tirana401" }]
      }
    }
  ]
}
)").object();

const auto partialInvalid = QJsonDocument::fromJson(R"(
{
  "groups": {
    "ovpntcp": [{ "name": "openvpn_tcp", "ports": [80, 443, 853, 8443] }],
    "ovpnudp": [{ "name": "openvpn_udp", "ports": [8080, 853, 123, 53] }],
    "wg": [{ "name": "wireguard", "ports": [1337] }],
    "ikev2": [{ "name": "ikev2", "ports": [500, 4500] }],
    "proxysocks": [{ "name": "socks", "ports": [1080] }],
    "proxyss": [{ "name": "shadowsocks", "ports": [443] }]
  },
  "regions": [
    {
      "id": "al",
      "name": "Albania",
      "country": "AL",
      "auto_region": true,
      "dns": "al.privacy.network",
      "port_forward": true,
      "geo": false,
      "offline": false,
      "servers": {
        "ovpnudp": [{ "ip": "31.171.154.136", "cn": "tirana401" }],
        "ovpntcp": [{ "ip": "31.171.154.138", "cn": "tirana401" }],
        "ikev2": [{ "ip": "31.171.154.135", "cn": "tirana401" }],
        "wg": [{ "ip": "31.171.154.131", "cn": "tirana401" }]
      }
    },
    {
      "id": "missing_everything"
    }
  ]
}
)").object();

//Each of these locations is missing one field - none of them are valid.
const auto invalidLocations = QJsonDocument::fromJson(R"(
{
  "groups": {
    "ovpntcp": [{ "name": "openvpn_tcp", "ports": [80, 443, 853, 8443] }],
    "ovpnudp": [{ "name": "openvpn_udp", "ports": [8080, 853, 123, 53] }],
    "wg": [{ "name": "wireguard", "ports": [1337] }],
    "ikev2": [{ "name": "ikev2", "ports": [500, 4500] }],
    "proxysocks": [{ "name": "socks", "ports": [1080] }],
    "proxyss": [{ "name": "shadowsocks", "ports": [443] }]
  },
  "missing_name": {
    "country": "US",
    "dns": "us-california.privateinternetaccess.com",
    "port_forward": false,
    "ping": "198.8.80.174:8888",
    "geo": false,
    "offline": false,
    "serial": "dummy",
    "openvpn_udp": {
      "best": "198.8.80.174:8080"
    },
    "openvpn_tcp": {
      "best": "198.8.80.174:500"
    },
    "ips": []
  },
  "missing_country": {
    "name": "US California",
    "dns": "us-california.privateinternetaccess.com",
    "port_forward": false,
    "ping": "198.8.80.174:8888",
    "geo": false,
    "offline": false,
    "serial": "dummy",
    "openvpn_udp": {
      "best": "198.8.80.174:8080"
    },
    "openvpn_tcp": {
      "best": "198.8.80.174:500"
    },
    "ips": []
  },
  "missing_port_forward": {
    "name": "US California",
    "country": "US",
    "dns": "us-california.privateinternetaccess.com",
    "ping": "198.8.80.174:8888",
    "geo": false,
    "offline": false,
    "serial": "dummy",
    "openvpn_udp": {
      "best": "198.8.80.174:8080"
    },
    "openvpn_tcp": {
      "best": "198.8.80.174:500"
    },
    "ips": []
  },
  "missing_ping": {
    "name": "US California",
    "country": "US",
    "dns": "us-california.privateinternetaccess.com",
    "port_forward": false,
    "serial": "dummy",
    "geo": false,
    "offline": false,
    "openvpn_udp": {
      "best": "198.8.80.174:8080"
    },
    "openvpn_tcp": {
      "best": "198.8.80.174:500"
    },
    "ips": []
  },
  "missing_openvpn_udp_best": {
    "name": "US California",
    "country": "US",
    "dns": "us-california.privateinternetaccess.com",
    "port_forward": false,
    "ping": "198.8.80.174:8888",
    "geo": false,
    "offline": false,
    "serial": "dummy",
    "openvpn_udp": {
    },
    "openvpn_tcp": {
      "best": "198.8.80.174:500"
    },
    "ips": []
  },
  "missing_openvpn_udp": {
    "name": "US California",
    "country": "US",
    "dns": "us-california.privateinternetaccess.com",
    "port_forward": false,
    "ping": "198.8.80.174:8888",
    "geo": false,
    "offline": false,
    "serial": "dummy",
    "openvpn_tcp": {
      "best": "198.8.80.174:500"
    },
    "ips": []
  },
  "missing_openvpn_tcp_best": {
    "name": "US California",
    "country": "US",
    "dns": "us-california.privateinternetaccess.com",
    "port_forward": false,
    "ping": "198.8.80.174:8888",
    "geo": false,
    "offline": false,
    "serial": "dummy",
    "openvpn_udp": {
      "best": "198.8.80.174:8080"
    },
    "openvpn_tcp": {
    },
    "ips": []
  },
  "missing_openvpn_tcp": {
    "name": "US California",
    "country": "US",
    "dns": "us-california.privateinternetaccess.com",
    "port_forward": false,
    "ping": "198.8.80.174:8888",
    "geo": false,
    "offline": false,
    "serial": "dummy",
    "openvpn_udp": {
      "best": "198.8.80.174:8080"
    },
    "ips": []
  }
  "missing_geo": {
    "name": "US California",
    "country": "US",
    "dns": "us-california.privateinternetaccess.com",
    "port_forward": false,
    "ping": "198.8.80.174:8888",
    "serial": "dummy",
    "offline": false,
    "openvpn_udp": {
      "best": "198.8.80.174:8080"
    },
    "openvpn_tcp": {
      "best": "198.8.80.174:500"
    },
    "ips": []
  }
}
)").object();
}

namespace
{
    // The legacy regions list always produces exactly one server for each
    // service.  Get the server for a service if there is exactly one, or
    // nullptr otherwise.
    const Server *getServerForService(const Location &location, Service service)
    {
        const Server *pMatchingServer{nullptr};
        for(const auto &server : location.servers())
        {
            if(server.hasService(service))
            {
                // If we already found a matching server, there is more than
                // one - we're looking for exactly one match
                if(pMatchingServer)
                    return nullptr;
                pMatchingServer = &server;
            }
        }
        // If we found exactly one match, return it, otherwise pMatchingServer
        // is nullptr - no match
        return pMatchingServer;
    }
}
#define COMMA ,

class tst_settings : public QObject
{
    Q_OBJECT

    auto buildRegionsFromJson(const QJsonObject &locationsJson)
    {
        return buildModernLocations({}, locationsJson,
            {}, sample_docs::metadataJson, {}, {}).first;
    }

private slots:
    //Verify that valid locations can be loaded.
    void testTwoValid()
    {
        LocationsById locs{buildRegionsFromJson(sample_docs::twoLocations)};
        QVERIFY(locs.size() == 2);

        //The locations don't have to be in the same order as the original
        //JSON, QJsonDocument might re-order them since they're object
        //attributes.
        QSharedPointer<const Location> pAd, pAl;
        const Server *pServer{nullptr};

        pAl = locs.at("al");
        pAd = locs.at("ad");

        //Both locations must have been found
        QVERIFY(pAd);
        QVERIFY(pAl);

        QVERIFY(pAl);
        QCOMPARE(pAl->portForward(), true);
        pServer = getServerForService(*pAl, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "31.171.154.136");
        QCOMPARE(pServer->openVpnUdpPorts(), std::vector<quint16>{8080 COMMA 853 COMMA 123 COMMA 53});

        pServer = getServerForService(*pAl, Service::OpenVpnTcp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "31.171.154.138");
        qDebug () << pServer->openVpnTcpPorts();
        QCOMPARE(pServer->openVpnTcpPorts(), std::vector<quint16>{80 COMMA 443 COMMA 853 COMMA 8443});

        QVERIFY(pAd);
        QCOMPARE(pAd->portForward(), true);
        pServer = getServerForService(*pAd, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "45.139.49.245");
        QCOMPARE(pServer->openVpnUdpPorts(), std::vector<quint16>{8080 COMMA 853 COMMA 123 COMMA 53});
        pServer = getServerForService(*pAd, Service::OpenVpnTcp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "45.139.49.249");
        QCOMPARE(pServer->openVpnTcpPorts(), std::vector<quint16>{80 COMMA 443 COMMA 853 COMMA 8443});
    }

    //AmneziaWG servers alias their port as a WireGuard port, so a region with
    //only those still reports the WireGuard service - the two capability
    //checks have to tell the kinds apart.
    void testWireGuardObfuscationCapabilities()
    {
        LocationsById locs{buildRegionsFromJson(sample_docs::obfuscationLocations)};
        QVERIFY(locs.size() == 4);

        const auto &pPlain = locs.at("plain");
        QVERIFY(pPlain);
        QCOMPARE(pPlain->hasPlainWireGuard(), true);
        QCOMPARE(pPlain->hasObfuscatedWireGuard(), false);

        const auto &pObfuscated = locs.at("obfuscated");
        QVERIFY(pObfuscated);
        QCOMPARE(pObfuscated->hasPlainWireGuard(), false);
        QCOMPARE(pObfuscated->hasObfuscatedWireGuard(), true);
#if !defined(Q_OS_WIN)
        //The AmneziaWG server is part of the WireGuard pool, which is why the
        //plain check can't just ask for Service::WireGuard.  Windows keeps
        //these servers out of that pool entirely.
        QCOMPARE(pObfuscated->hasService(Service::WireGuard), true);
#endif

        const auto &pBoth = locs.at("both");
        QVERIFY(pBoth);
        QCOMPARE(pBoth->hasPlainWireGuard(), true);
        QCOMPARE(pBoth->hasObfuscatedWireGuard(), true);

        const auto &pNeither = locs.at("neither");
        QVERIFY(pNeither);
        QCOMPARE(pNeither->hasPlainWireGuard(), false);
        QCOMPARE(pNeither->hasObfuscatedWireGuard(), false);
    }

    //Loading JSON data with no valid locations should fail without changing
    //the existing data.
    void testInvalidLoad()
    {
        LocationsById locs;

        //Attempt to load empty JSON
        QVERIFY_EXCEPTION_THROWN(buildRegionsFromJson(sample_docs::emptyJson), std::exception);
        //Attempt to load non-object JSON
        QVERIFY_EXCEPTION_THROWN(buildRegionsFromJson(sample_docs::arrayJson), std::exception);
        QCOMPARE(locs.empty(), true);
    }

    //Load one valid location
    void testOneValid()
    {
        LocationsById locs{buildRegionsFromJson(sample_docs::oneLocation)};
        QVERIFY(locs.size() == 1);

        const auto &pAl = locs.at("al");
        const Server *pServer{nullptr};

        QVERIFY(pAl);
        QCOMPARE(pAl->portForward(), true);
        pServer = getServerForService(*pAl, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "31.171.154.136");
        QCOMPARE(pServer->openVpnUdpPorts(), std::vector<quint16>{8080 COMMA 853 COMMA 123 COMMA 53});

        pServer = getServerForService(*pAl, Service::OpenVpnTcp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "31.171.154.138");
        qDebug () << pServer->openVpnTcpPorts();
        QCOMPARE(pServer->openVpnTcpPorts(), std::vector<quint16>{80 COMMA 443 COMMA 853 COMMA 8443});
    }

    // //Invalid locations should be ignored when loaded, but any valid locations
    // //should still be returned
    void testPartialInvalid()
    {
        //Load one valid location.  The invalid location should be ignored.
        LocationsById locs{buildRegionsFromJson(sample_docs::partialInvalid)};
        QVERIFY(locs.size() == 1);

        const auto &pAl = locs.at("al");
        const Server *pServer{nullptr};

        QVERIFY(pAl);
        QCOMPARE(pAl->portForward(), true);
        pServer = getServerForService(*pAl, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "31.171.154.136");
        QCOMPARE(pServer->openVpnUdpPorts(), std::vector<quint16>{8080 COMMA 853 COMMA 123 COMMA 53});

        pServer = getServerForService(*pAl, Service::OpenVpnTcp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "31.171.154.138");
        qDebug () << pServer->openVpnTcpPorts();
        QCOMPARE(pServer->openVpnTcpPorts(), std::vector<quint16>{80 COMMA 443 COMMA 853 COMMA 8443});
    }

    // // Building locations with a set of latency measurements should apply those
    // // measurements
    void preserveLatency()
    {
        double alLatency{121.0};
        LatencyMap latencies;
        latencies["al"] = alLatency;

        LocationsById updatedLocs{buildModernLocations(latencies,
            sample_docs::oneLocationNewIps, {}, sample_docs::metadataJson, {},
            {}).first};
        QVERIFY(updatedLocs.size() == 1);

        const auto &pAlUpd = updatedLocs.at("al");
        QVERIFY(pAlUpd);
        QCOMPARE(pAlUpd->latency().get(), alLatency);
    }

    // DIP OpenVPN ports are persisted with the DIP.  A DIP saved by an older
    // version, without the port fields, loads with empty port lists.
    void testDedicatedIpPortsJson()
    {
        AccountDedicatedIp dip;
        dip.id(QStringLiteral("dip-01234567"));
        dip.openvpnUdpPorts({1197, 1198});
        dip.openvpnTcpPorts({501});

        AccountDedicatedIp loaded;
        QVERIFY(loaded.readJsonObject(dip.toJsonObject()));
        QVERIFY(loaded == dip);
        QCOMPARE(loaded.openvpnUdpPorts(), std::vector<quint16>{1197 COMMA 1198});
        QCOMPARE(loaded.openvpnTcpPorts(), std::vector<quint16>{501});

        QJsonObject oldJson = dip.toJsonObject();
        oldJson.remove(QStringLiteral("openvpnUdpPorts"));
        oldJson.remove(QStringLiteral("openvpnTcpPorts"));
        AccountDedicatedIp oldLoaded;
        QVERIFY(oldLoaded.readJsonObject(oldJson));
        QCOMPARE(oldLoaded.openvpnUdpPorts(), std::vector<quint16>{});
        QCOMPARE(oldLoaded.openvpnTcpPorts(), std::vector<quint16>{});
    }

    // The DIP location uses the DIP OpenVPN ports, while its corresponding
    // region keeps the service group ports.
    void testDedicatedIpLocationPorts()
    {
        AccountDedicatedIp dip;
        dip.id(QStringLiteral("dip-01234567"));
        dip.regionId(QStringLiteral("al"));
        dip.serviceGroups({QStringLiteral("ovpnudp"), QStringLiteral("ovpntcp"),
                           QStringLiteral("wg")});
        dip.ip(QStringLiteral("191.96.74.2"));
        dip.cn(QStringLiteral("tirana429"));
        dip.openvpnUdpPorts({1197});
        dip.openvpnTcpPorts({501});

        LocationsById locs{buildModernLocations({}, sample_docs::twoLocations,
            {}, sample_docs::metadataJson, {dip}, {}).first};

        const auto &pDip = locs.at("dip-01234567");
        QVERIFY(pDip);
        QVERIFY(pDip->isDedicatedIp());
        const Server *pServer = getServerForService(*pDip, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->openVpnUdpPorts(), std::vector<quint16>{1197});
        pServer = getServerForService(*pDip, Service::OpenVpnTcp);
        QVERIFY(pServer);
        QCOMPARE(pServer->openVpnTcpPorts(), std::vector<quint16>{501});
        pServer = getServerForService(*pDip, Service::WireGuard);
        QVERIFY(pServer);
        QCOMPARE(pServer->wireGuardPorts(), std::vector<quint16>{1337});

        const auto &pAl = locs.at("al");
        QVERIFY(pAl);
        pServer = getServerForService(*pAl, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->openVpnUdpPorts(), std::vector<quint16>{8080 COMMA 853 COMMA 123 COMMA 53});
    }

    // DIP ports are only offered as DIP port choices, in API order, and the
    // regular port choices only contain the regular region ports
    void testOpenVpnPortChoices()
    {
        AccountDedicatedIp dip;
        dip.id(QStringLiteral("dip-01234567"));
        dip.regionId(QStringLiteral("al"));
        dip.serviceGroups({QStringLiteral("ovpnudp"), QStringLiteral("ovpntcp")});
        dip.ip(QStringLiteral("191.96.74.2"));
        dip.cn(QStringLiteral("tirana429"));
        dip.openvpnUdpPorts({1198, 1197});
        dip.openvpnTcpPorts({501});

        LocationsById locs{buildModernLocations({}, sample_docs::twoLocations,
            {}, sample_docs::metadataJson, {dip}, {}).first};

        DescendingPortSet udpPorts, tcpPorts;
        std::vector<quint16> dipUdpPorts, dipTcpPorts;
        buildOpenVpnPortChoices(locs, udpPorts, tcpPorts, dipUdpPorts, dipTcpPorts);
        QVERIFY(udpPorts == (DescendingPortSet{8080, 853, 123, 53}));
        QVERIFY(tcpPorts == (DescendingPortSet{80, 443, 853, 8443}));
        QCOMPARE(dipUdpPorts, std::vector<quint16>{1198 COMMA 1197});
        QCOMPARE(dipTcpPorts, std::vector<quint16>{501});

        // Without DIPs there are no DIP port choices
        locs = buildRegionsFromJson(sample_docs::twoLocations);
        buildOpenVpnPortChoices(locs, udpPorts, tcpPorts, dipUdpPorts, dipTcpPorts);
        QVERIFY(udpPorts == (DescendingPortSet{8080, 853, 123, 53}));
        QVERIFY(dipUdpPorts.empty());
        QVERIFY(dipTcpPorts.empty());
    }

    // A DIP without OpenVPN ports has no DIP port choices, and the regular
    // choices are unchanged
    void testOpenVpnPortChoicesDipWithoutPorts()
    {
        AccountDedicatedIp dip;
        dip.id(QStringLiteral("dip-01234567"));
        dip.regionId(QStringLiteral("al"));
        dip.serviceGroups({QStringLiteral("ovpnudp"), QStringLiteral("ovpntcp")});
        dip.ip(QStringLiteral("191.96.74.2"));
        dip.cn(QStringLiteral("tirana429"));

        LocationsById locs{buildModernLocations({}, sample_docs::twoLocations,
            {}, sample_docs::metadataJson, {dip}, {}).first};

        DescendingPortSet udpPorts, tcpPorts;
        std::vector<quint16> dipUdpPorts, dipTcpPorts;
        buildOpenVpnPortChoices(locs, udpPorts, tcpPorts, dipUdpPorts, dipTcpPorts);
        QVERIFY(udpPorts == (DescendingPortSet{8080, 853, 123, 53}));
        QVERIFY(tcpPorts == (DescendingPortSet{80, 443, 853, 8443}));
        QCOMPARE(dipUdpPorts, std::vector<quint16>{});
        QCOMPARE(dipTcpPorts, std::vector<quint16>{});
    }

    // DIP port choices don't repeat a port offered by more than one DIP server
    // or by more than one DIP, and the choices are cleared on each rebuild
    void testOpenVpnPortChoicesNoDuplicates()
    {
        AccountDedicatedIp first;
        first.id(QStringLiteral("dip-00000001"));
        first.regionId(QStringLiteral("al"));
        // The same group twice gives two DIP servers with the same ports
        first.serviceGroups({QStringLiteral("ovpnudp"), QStringLiteral("ovpnudp")});
        first.ip(QStringLiteral("191.96.74.2"));
        first.cn(QStringLiteral("tirana429"));
        first.openvpnUdpPorts({1198, 1197});
        AccountDedicatedIp second{first};
        second.id(QStringLiteral("dip-00000002"));
        second.ip(QStringLiteral("191.96.74.3"));
        second.openvpnUdpPorts({1197, 1200});

        LocationsById locs{buildModernLocations({}, sample_docs::twoLocations,
            {}, sample_docs::metadataJson, {first, second}, {}).first};

        DescendingPortSet udpPorts, tcpPorts;
        // Start with stale values to check that they are cleared
        std::vector<quint16> dipUdpPorts{9999}, dipTcpPorts{9999};
        buildOpenVpnPortChoices(locs, udpPorts, tcpPorts, dipUdpPorts, dipTcpPorts);
        // The order between different DIPs depends on the locations map, so
        // only check the content
        std::vector<quint16> sortedUdp{dipUdpPorts};
        std::sort(sortedUdp.begin(), sortedUdp.end());
        QCOMPARE(sortedUdp, (std::vector<quint16>{1197, 1198, 1200}));
        QCOMPARE(dipTcpPorts, std::vector<quint16>{});
    }

    // The DIP port used for the connection: the selected port if the DIP
    // offers it, otherwise the first DIP port
    void testDedicatedIpPortToUse()
    {
        struct Case
        {
            const char *name;
            uint selectedPort;
            std::vector<quint16> dipPorts;
            quint16 expected;
        };
        const Case cases[]
        {
            {"single port, nothing selected", 0, {123}, 123},
            {"single port, other port selected", 456, {123}, 123},
            {"two ports, nothing selected", 0, {123, 456}, 123},
            {"two ports, first selected", 123, {123, 456}, 123},
            {"two ports, second selected", 456, {123, 456}, 456},
            {"selected port still offered with a new port", 456, {123, 456, 789}, 456},
            {"selected port no longer offered", 456, {123, 789}, 123},
            {"API order, not sorted", 0, {789, 123}, 789},
            {"no ports, nothing selected", 0, {}, 0},
            {"no ports, port selected", 456, {}, 0},
            // A setting value above 65535 must not match a port by truncation
            {"selected value out of port range", 65536 + 123, {123, 456}, 123},
        };
        for(const auto &c : cases)
        {
            QVERIFY2(dedicatedIpPortToUse(c.selectedPort, c.dipPorts) == c.expected, c.name);
        }
    }
};
#undef COMMA
QTEST_GUILESS_MAIN(tst_settings)
#include TEST_MOC
