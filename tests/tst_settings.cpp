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
#include "settings.h"
#include "common/src/locations.h"
#include <QtTest>

namespace sample_docs {

const auto emptyJson = QJsonDocument::fromJson("{}").object();

//JSON doesn't have to have an object at the top level, it could be any JSON
//value
const auto arrayJson = QJsonDocument::fromJson(R"(["foo", 2, false])").object();

const auto twoLocations = QJsonDocument::fromJson(R"(
{
  "us_california": {
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
      "best": "198.8.80.174:500"
    },
    "ips": []
  },
  "us2": {
    "name": "US East",
    "country": "US",
    "dns": "us-east.privateinternetaccess.com",
    "port_forward": false,
    "ping": "209.222.23.59:8888",
    "geo": false,
    "serial": "dummy",
    "openvpn_udp": {
      "best": "209.222.23.59:8080"
    },
    "openvpn_tcp": {
      "best": "209.222.23.59:500"
    },
    "ips": []
  }
}
)").object();

const auto oneLocation = QJsonDocument::fromJson(R"(
{
  "ca": {
    "name": "CA Montreal",
    "country": "CA",
    "dns": "ca.privateinternetaccess.com",
    "port_forward": true,
    "ping": "173.199.65.36:8888",
    "geo": false,
    "serial": "dummy",
    "openvpn_udp": {
      "best": "173.199.65.36:8080"
    },
    "openvpn_tcp": {
      "best": "173.199.65.36:500"
    },
    "ips": []
  }
}
)").object();

//The CA Montreal location again with new IP addresses
const auto oneLocationNewIps = QJsonDocument::fromJson(R"(
{
  "ca": {
    "name": "CA Montreal",
    "country": "CA",
    "dns": "ca.privateinternetaccess.com",
    "port_forward": true,
    "ping": "173.199.65.59:8888",
    "geo": false,
    "serial": "dummy",
    "openvpn_udp": {
      "best": "173.199.65.59:8080"
    },
    "openvpn_tcp": {
      "best": "173.199.65.59:500"
    },
    "ips": []
  }
}
)").object();

const auto partialInvalid = QJsonDocument::fromJson(R"(
{
  "nz": {
    "name": "New Zealand",
    "country": "NZ",
    "dns": "nz.privateinternetaccess.com",
    "port_forward": false,
    "ping": "103.231.91.35:8888",
    "geo": false,
    "serial": "dummy",
    "openvpn_udp": {
      "best": "103.231.91.35:8080"
    },
    "openvpn_tcp": {
      "best": "103.231.91.35:500"
    },
    "ips": []
  },
  "missing_everything": {
  }
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

class tst_settings : public QObject
{
    Q_OBJECT

private slots:
    //Verify that valid locations can be loaded.
    void testTwoValid()
    {
        LocationsById locs{buildLegacyLocations({}, sample_docs::twoLocations,
                           sample_docs::emptyJson)};
        QVERIFY(locs.size() == 2);

        //The locations don't have to be in the same order as the original
        //JSON, QJsonDocument might re-order them since they're object
        //attributes.
        QSharedPointer<Location> pUsCal, pUs2;
        const Server *pServer{nullptr};

        pUsCal = locs.at(QStringLiteral("us_california"));
        pUs2 = locs.at(QStringLiteral("us2"));

        //Both locations must have been found
        QVERIFY(pUsCal);
        QVERIFY(pUs2);

        QVERIFY(pUsCal);
        QCOMPARE(pUsCal->name(), "US California");
        QCOMPARE(pUsCal->country(), "US");
        QCOMPARE(pUsCal->portForward(), false);
        pServer = getServerForService(*pUsCal, Service::Latency);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "198.8.80.174");
        QCOMPARE(pServer->latencyPorts(), std::vector<quint16>{8888});
        pServer = getServerForService(*pUsCal, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "198.8.80.174");
        QCOMPARE(pServer->openvpnUdpPorts(), std::vector<quint16>{8080});
        pServer = getServerForService(*pUsCal, Service::OpenVpnTcp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "198.8.80.174");
        QCOMPARE(pServer->openvpnTcpPorts(), std::vector<quint16>{500});

        QVERIFY(pUs2);
        QCOMPARE(pUs2->name(), "US East");
        QCOMPARE(pUs2->country(), "US");
        QCOMPARE(pUs2->portForward(), false);
        pServer = getServerForService(*pUs2, Service::Latency);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "209.222.23.59");
        QCOMPARE(pServer->latencyPorts(), std::vector<quint16>{8888});
        pServer = getServerForService(*pUs2, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "209.222.23.59");
        QCOMPARE(pServer->openvpnUdpPorts(), std::vector<quint16>{8080});
        pServer = getServerForService(*pUs2, Service::OpenVpnTcp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "209.222.23.59");
        QCOMPARE(pServer->openvpnTcpPorts(), std::vector<quint16>{500});
    }

    //Loading JSON data with no valid locations should fail without changing
    //the existing data.
    void testInvalidLoad()
    {
        LocationsById locs;

        //Attempt to load empty JSON
        locs = buildLegacyLocations({}, sample_docs::emptyJson, sample_docs::emptyJson);
        QCOMPARE(locs.empty(), true);
        //Attempt to load non-object JSON
        locs = buildLegacyLocations({}, sample_docs::arrayJson, sample_docs::emptyJson);
        QCOMPARE(locs.empty(), true);
        //Attempt to load JSON with all invalid locations
        locs = buildLegacyLocations({}, sample_docs::invalidLocations, sample_docs::emptyJson);
        QCOMPARE(locs.empty(), true);
    }

    //Load one valid location
    void testOneValid()
    {
        LocationsById locs{buildLegacyLocations({}, sample_docs::oneLocation, sample_docs::emptyJson)};
        QVERIFY(locs.size() == 1);

        const auto &pMontreal = locs.at(QStringLiteral("ca"));
        const Server *pServer{nullptr};
        QVERIFY(pMontreal);
        QCOMPARE(pMontreal->id(), "ca");
        QCOMPARE(pMontreal->name(), "CA Montreal");
        QCOMPARE(pMontreal->country(), "CA");
        QCOMPARE(pMontreal->portForward(), true);
        pServer = getServerForService(*pMontreal, Service::Latency);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "173.199.65.36");
        QCOMPARE(pServer->latencyPorts(), std::vector<quint16>{8888});
        pServer = getServerForService(*pMontreal, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "173.199.65.36");
        QCOMPARE(pServer->openvpnUdpPorts(), std::vector<quint16>{8080});
        pServer = getServerForService(*pMontreal, Service::OpenVpnTcp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "173.199.65.36");
        QCOMPARE(pServer->openvpnTcpPorts(), std::vector<quint16>{500});
    }

    //Invalid locations should be ignored when loaded, but any valid locations
    //should still be returned
    void testPartialInvalid()
    {
        //Load one valid location.  The invalid location should be ignored.
        LocationsById locs{buildLegacyLocations({}, sample_docs::partialInvalid, sample_docs::emptyJson)};
        QVERIFY(locs.size() == 1);

        const auto &pNz = locs.at(QStringLiteral("nz"));
        const Server *pServer;
        QVERIFY(pNz);
        QCOMPARE(pNz->id(), "nz");
        QCOMPARE(pNz->name(), "New Zealand");
        QCOMPARE(pNz->country(), "NZ");
        QCOMPARE(pNz->portForward(), false);
        pServer = getServerForService(*pNz, Service::Latency);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "103.231.91.35");
        QCOMPARE(pServer->latencyPorts(), std::vector<quint16>{8888});
        pServer = getServerForService(*pNz, Service::OpenVpnUdp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "103.231.91.35");
        QCOMPARE(pServer->openvpnUdpPorts(), std::vector<quint16>{8080});
        pServer = getServerForService(*pNz, Service::OpenVpnTcp);
        QVERIFY(pServer);
        QCOMPARE(pServer->ip(), "103.231.91.35");
        QCOMPARE(pServer->openvpnTcpPorts(), std::vector<quint16>{500});
    }

    // Building locations with a set of latency measurements should apply those
    // measurements
    void preserveLatency()
    {
        double montrealLatency{121.0};
        LatencyMap latencies;
        latencies["ca"] = montrealLatency;

        LocationsById updatedLocs{buildLegacyLocations(latencies, sample_docs::oneLocationNewIps, sample_docs::emptyJson)};
        QVERIFY(updatedLocs.size() == 1);

        const auto &pMontrealUpd = updatedLocs.at(QStringLiteral("ca"));
        QVERIFY(pMontrealUpd);
        QCOMPARE(pMontrealUpd->latency().get(), montrealLatency);
    }
};

QTEST_GUILESS_MAIN(tst_settings)
#include TEST_MOC
