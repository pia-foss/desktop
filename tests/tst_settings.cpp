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
    "openvpn_udp": {
      "best": "198.8.80.174:8080"
    },
    "openvpn_tcp": {
      "best": "198.8.80.174:500"
    },
    "ips": []
  },
  "missing_dns": {
    "name": "US California",
    "country": "US",
    "port_forward": false,
    "ping": "198.8.80.174:8888",
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
    "openvpn_udp": {
      "best": "198.8.80.174:8080"
    },
    "ips": []
  }
}
)").object();

}

namespace
{
    ServerLocations emptyLocs{};
}

class tst_settings : public QObject
{
    Q_OBJECT

private slots:
    //Verify that valid locations can be loaded.
    void testTwoValid()
    {
        ServerLocations locs{updateServerLocations(emptyLocs, sample_docs::twoLocations)};
        QVERIFY(locs.size() == 2);

        //The locations don't have to be in the same order as the original
        //JSON, QJsonDocument might re-order them since they're object
        //attributes.
        QSharedPointer<ServerLocation> pUsCal, pUs2;

        pUsCal = locs.value(QStringLiteral("us_california"));
        pUs2 = locs.value(QStringLiteral("us2"));

        //Both locations must have been found
        QVERIFY(pUsCal);
        QVERIFY(pUs2);

        QVERIFY(pUsCal);
        QCOMPARE(pUsCal->name(), "US California");
        QCOMPARE(pUsCal->country(), "US");
        QCOMPARE(pUsCal->dns(), "us-california.privateinternetaccess.com");
        QCOMPARE(pUsCal->portForward(), false);
        QCOMPARE(pUsCal->ping(), "198.8.80.174:8888");
        QCOMPARE(pUsCal->openvpnUDP(), "198.8.80.174:8080");
        QCOMPARE(pUsCal->openvpnTCP(), "198.8.80.174:500");

        QVERIFY(pUs2);
        QCOMPARE(pUs2->name(), "US East");
        QCOMPARE(pUs2->country(), "US");
        QCOMPARE(pUs2->dns(), "us-east.privateinternetaccess.com");
        QCOMPARE(pUs2->portForward(), false);
        QCOMPARE(pUs2->ping(), "209.222.23.59:8888");
        QCOMPARE(pUs2->openvpnUDP(), "209.222.23.59:8080");
        QCOMPARE(pUs2->openvpnTCP(), "209.222.23.59:500");
    }

    //Loading JSON data with no valid locations should fail without changing
    //the existing data.
    void testInvalidLoad()
    {
        ServerLocations locs;

        //Attempt to load empty JSON
        locs = updateServerLocations(emptyLocs, sample_docs::emptyJson);
        QCOMPARE(locs.empty(), true);
        //Attempt to load non-object JSON
        locs = updateServerLocations(emptyLocs, sample_docs::arrayJson);
        QCOMPARE(locs.empty(), true);
        //Attempt to load JSON with all invalid locations
        locs = updateServerLocations(emptyLocs, sample_docs::invalidLocations);
        QCOMPARE(locs.empty(), true);
    }

    //Load one valid location
    void testOneValid()
    {
        ServerLocations locs{updateServerLocations(emptyLocs, sample_docs::oneLocation)};
        QVERIFY(locs.size() == 1);

        const auto &pMontreal = locs.value(QStringLiteral("ca"));
        QVERIFY(pMontreal);
        QCOMPARE(pMontreal->id(), "ca");
        QCOMPARE(pMontreal->name(), "CA Montreal");
        QCOMPARE(pMontreal->country(), "CA");
        QCOMPARE(pMontreal->dns(), "ca.privateinternetaccess.com");
        QCOMPARE(pMontreal->portForward(), true);
        QCOMPARE(pMontreal->ping(), "173.199.65.36:8888");
        QCOMPARE(pMontreal->openvpnUDP(), "173.199.65.36:8080");
        QCOMPARE(pMontreal->openvpnTCP(), "173.199.65.36:500");
    }

    //Invalid locations should be ignored when loaded, but any valid locations
    //should still be returned
    void testPartialInvalid()
    {
        //Load one valid location.  The invalid location should be ignored.
        ServerLocations locs{updateServerLocations(emptyLocs, sample_docs::partialInvalid)};
        QVERIFY(locs.size() == 1);

        const auto &pNz = locs.value(QStringLiteral("nz"));
        QVERIFY(pNz);
        QCOMPARE(pNz->id(), "nz");
        QCOMPARE(pNz->name(), "New Zealand");
        QCOMPARE(pNz->country(), "NZ");
        QCOMPARE(pNz->dns(), "nz.privateinternetaccess.com");
        QCOMPARE(pNz->portForward(), false);
        QCOMPARE(pNz->ping(), "103.231.91.35:8888");
        QCOMPARE(pNz->openvpnUDP(), "103.231.91.35:8080");
        QCOMPARE(pNz->openvpnTCP(), "103.231.91.35:500");
    }

    //Refreshing the locations should preserve latency measurements, even if the
    //location's IPs are updated
    void preserveLatency()
    {
        ServerLocations origLocs{updateServerLocations(emptyLocs, sample_docs::oneLocation)};
        QVERIFY(origLocs.size() == 1);

        quint64 montrealLatency{121};

        const auto &pMontreal = origLocs.value(QStringLiteral("ca"));
        QVERIFY(pMontreal);
        pMontreal->latency(montrealLatency);

        ServerLocations updatedLocs{updateServerLocations(origLocs, sample_docs::oneLocationNewIps)};
        QVERIFY(updatedLocs.size() == 1);

        const auto &pMontrealUpd = updatedLocs.value(QStringLiteral("ca"));
        QVERIFY(pMontrealUpd);
        QCOMPARE(pMontrealUpd->latency().get(), montrealLatency);
    }
};

QTEST_GUILESS_MAIN(tst_settings)
#include TEST_MOC
