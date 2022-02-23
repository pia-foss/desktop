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
#include "settings/locations.h"
#include "common/src/locations.h"
#include <QtTest>

namespace sample_docs {

const auto emptyJson = QJsonDocument::fromJson("{}").object();

//JSON doesn't have to have an object at the top level, it could be any JSON
//value
const auto arrayJson = QJsonDocument::fromJson(R"(["foo", 2, false])").object();

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
  "missing_name": {
    "country": "US",
    "dns": "us-california.privateinternetaccess.com",
    "port_forward": false,
    "ping": "198.8.80.174:8888",
    "geo": false,
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
        // Location::allServersForService() returns a temporary vector, avoid
        // a dangling reference by finding the server in the servers() vector,
        // which is held in Location
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

private slots:
    //Verify that valid locations can be loaded.
    void testTwoValid()
    {
        LocationsById locs{buildModernLocations({}, sample_docs::twoLocations,
                           {}, {}, {})};
        QVERIFY(locs.size() == 2);

        //The locations don't have to be in the same order as the original
        //JSON, QJsonDocument might re-order them since they're object
        //attributes.
        QSharedPointer<Location> pAd, pAl;
        const Server *pServer{nullptr};

        pAl = locs.at(QStringLiteral("al"));
        pAd = locs.at(QStringLiteral("ad"));

        //Both locations must have been found
        QVERIFY(pAd);
        QVERIFY(pAl);

        QVERIFY(pAl);
        QCOMPARE(pAl->name(), "Albania");
        QCOMPARE(pAl->country(), "AL");
        QCOMPARE(pAl->portForward(), true);
        pServer = getServerForService(*pAl, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "31.171.154.136");
        QCOMPARE(pServer->openvpnUdpPorts(), std::vector<quint16>{8080 COMMA 853 COMMA 123 COMMA 53});

        pServer = getServerForService(*pAl, Service::OpenVpnTcp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "31.171.154.138");
        qDebug () << pServer->openvpnTcpPorts();
        QCOMPARE(pServer->openvpnTcpPorts(), std::vector<quint16>{80 COMMA 443 COMMA 853 COMMA 8443});

        QVERIFY(pAd);
        QCOMPARE(pAd->name(), "Andorra");
        QCOMPARE(pAd->country(), "AD");
        QCOMPARE(pAd->portForward(), true);
        pServer = getServerForService(*pAd, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "45.139.49.245");
        QCOMPARE(pServer->openvpnUdpPorts(), std::vector<quint16>{8080 COMMA 853 COMMA 123 COMMA 53});
        pServer = getServerForService(*pAd, Service::OpenVpnTcp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "45.139.49.249");
        QCOMPARE(pServer->openvpnTcpPorts(), std::vector<quint16>{80 COMMA 443 COMMA 853 COMMA 8443});
    }

    //Loading JSON data with no valid locations should fail without changing
    //the existing data.
    void testInvalidLoad()
    {
        LocationsById locs;

        //Attempt to load empty JSON
        locs = buildModernLocations({}, sample_docs::emptyJson, {}, {}, {});
        QCOMPARE(locs.empty(), true);
        //Attempt to load non-object JSON
        locs = buildModernLocations({}, sample_docs::arrayJson, {}, {}, {});
        QCOMPARE(locs.empty(), true);
    }

    //Load one valid location
    void testOneValid()
    {
        LocationsById locs{buildModernLocations({}, sample_docs::oneLocation, {}, {}, {})};
        QVERIFY(locs.size() == 1);

        const auto &pAl = locs.at(QStringLiteral("al"));
        const Server *pServer{nullptr};

        QVERIFY(pAl);
        QCOMPARE(pAl->name(), "Albania");
        QCOMPARE(pAl->country(), "AL");
        QCOMPARE(pAl->portForward(), true);
        pServer = getServerForService(*pAl, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "31.171.154.136");
        QCOMPARE(pServer->openvpnUdpPorts(), std::vector<quint16>{8080 COMMA 853 COMMA 123 COMMA 53});

        pServer = getServerForService(*pAl, Service::OpenVpnTcp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "31.171.154.138");
        qDebug () << pServer->openvpnTcpPorts();
        QCOMPARE(pServer->openvpnTcpPorts(), std::vector<quint16>{80 COMMA 443 COMMA 853 COMMA 8443});
    }

    // //Invalid locations should be ignored when loaded, but any valid locations
    // //should still be returned
    void testPartialInvalid()
    {
        //Load one valid location.  The invalid location should be ignored.
        LocationsById locs{buildModernLocations({}, sample_docs::partialInvalid, {}, {}, {})};
        QVERIFY(locs.size() == 1);

        const auto &pAl = locs.at(QStringLiteral("al"));
        const Server *pServer{nullptr};

        QVERIFY(pAl);
        QCOMPARE(pAl->name(), "Albania");
        QCOMPARE(pAl->country(), "AL");
        QCOMPARE(pAl->portForward(), true);
        pServer = getServerForService(*pAl, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "31.171.154.136");
        QCOMPARE(pServer->openvpnUdpPorts(), std::vector<quint16>{8080 COMMA 853 COMMA 123 COMMA 53});

        pServer = getServerForService(*pAl, Service::OpenVpnTcp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "31.171.154.138");
        qDebug () << pServer->openvpnTcpPorts();
        QCOMPARE(pServer->openvpnTcpPorts(), std::vector<quint16>{80 COMMA 443 COMMA 853 COMMA 8443});
    }

    // // Building locations with a set of latency measurements should apply those
    // // measurements
    void preserveLatency()
    {
        double alLatency{121.0};
        LatencyMap latencies;
        latencies["al"] = alLatency;

        LocationsById updatedLocs{buildModernLocations(latencies, sample_docs::oneLocationNewIps, {}, {}, {})};
        QVERIFY(updatedLocs.size() == 1);

        const auto &pAlUpd = updatedLocs.at(QStringLiteral("al"));
        QVERIFY(pAlUpd);
        QCOMPARE(pAlUpd->latency().get(), alLatency);
    }
};
#undef COMMA
QTEST_GUILESS_MAIN(tst_settings)
#include TEST_MOC
