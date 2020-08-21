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

namespace samples
{
    const auto locationsNoPF = QJsonDocument::fromJson(R"(
{
  "austria": {
    "name": "Austria",
    "country": "AT",
    "dns": "austria.privateinternetaccess.com",
    "port_forward": false,
    "ping": "185.216.34.231:8888",
    "geo": false,
    "openvpn_udp": {
      "best": "185.216.34.231:8080"
    },
    "openvpn_tcp": {
      "best": "185.216.34.231:500"
    },
    "serial": "d329be7d4c45f7394f31dd4c24b03d2a"
  },
  "czech": {
    "name": "Czech Republic",
    "country": "CZ",
    "dns": "czech.privateinternetaccess.com",
    "port_forward": false,
    "ping": "89.238.186.229:8888",
    "geo": false,
    "openvpn_udp": {
      "best": "89.238.186.229:8080"
    },
    "openvpn_tcp": {
      "best": "89.238.186.229:500"
    },
    "serial": "2e77cf933e02b1e5f67e942f0c72c956"
  },
  "info": {
    "auto_regions": [
      "austria",
      "czech"
    ]
  }
}
)").object();

    const auto locationsNoAutoRegions = QJsonDocument::fromJson(R"(
{
  "aus_melbourne": {
    "name": "AU Melbourne",
    "country": "AU",
    "dns": "au-melbourne.privateinternetaccess.com",
    "port_forward": false,
    "ping": "168.1.99.210:8888",
    "geo": false,
    "openvpn_udp": {
      "best": "168.1.99.210:8080"
    },
    "openvpn_tcp": {
      "best": "168.1.99.210:500"
    },
    "serial": "cbb82586514d9a229a9afbec18e9a8b5"
  },
  "aus_perth": {
    "name": "AU Perth",
    "country": "AU",
    "dns": "au-perth.privateinternetaccess.com",
    "port_forward": false,
    "ping": "103.231.89.4:8888",
    "geo": false,
    "openvpn_udp": {
      "best": "103.231.89.4:8080"
    },
    "openvpn_tcp": {
      "best": "103.231.89.4:500"
    },
    "serial": "8491c86f848f8933d5f6f1194b2fe6a4"
  },
  "info": {
    "auto_regions": [
    ]
  }
}
)").object();

    const auto locations = QJsonDocument::fromJson(R"(
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
  },
  "poland": {
    "name": "Poland",
    "country": "PL",
    "dns": "poland.privateinternetaccess.com",
    "port_forward": true,
    "ping": "185.244.214.14:8888",
    "geo": false,
    "openvpn_udp": {
      "best": "185.244.214.14:8080"
    },
    "openvpn_tcp": {
      "best": "185.244.214.14:500"
    },
    "serial": "6d1e672185ccc54d184b8c212c0ecf2a"
  },
  "ro": {
    "name": "Romania",
    "country": "RO",
    "dns": "ro.privateinternetaccess.com",
    "port_forward": true,
    "ping": "89.33.8.42:8888",
    "geo": false,
    "openvpn_udp": {
      "best": "89.33.8.42:8080"
    },
    "openvpn_tcp": {
      "best": "89.33.8.42:500"
    },
    "serial": "7f714703f7f4fae7391271ecf4bebce8"
   },
  "hungary": {
    "name": "Hungary",
    "country": "HU",
    "dns": "hungary.privateinternetaccess.com",
    "port_forward": false,
    "ping": "185.128.26.18:8888",
    "geo": false,
    "openvpn_udp": {
      "best": "185.128.26.18:8080"
    },
    "openvpn_tcp": {
      "best": "185.128.26.18:500"
    },
    "serial": "150f4e24540e431092227d119a13d396"
  },
  "info": {
    "auto_regions": [
      "us_california",
      "us2",
      "poland",
      "ro"
    ]
  }
}
)").object();

    const auto locationsGeo = QJsonDocument::fromJson(R"(
{
  "pf_auto_ng": {
    "name": "has PF, auto safe, not geo",
    "country": "us",
    "ping": "0.0.0.0:1",
    "openvpn_udp":{"best":"0.0.0.0:1"},
    "openvpn_tcp":{"best":"0.0.0.0:1"},
    "serial":"na",
    "port_forward": true,
    "geo": false
  },
  "pf_auto_g": {
    "name": "has PF, auto safe, is geo",
    "country": "us",
    "ping": "0.0.0.0:1",
    "openvpn_udp":{"best":"0.0.0.0:1"},
    "openvpn_tcp":{"best":"0.0.0.0:1"},
    "serial":"na",
    "port_forward": true,
    "geo": true
  },
  "pf_nauto_ng": {
    "name": "has PF, not auto safe, not geo",
    "country": "us",
    "ping": "0.0.0.0:1",
    "openvpn_udp":{"best":"0.0.0.0:1"},
    "openvpn_tcp":{"best":"0.0.0.0:1"},
    "serial":"na",
    "port_forward": true,
    "geo": false
  },
  "pf_nauto_g": {
    "name": "has PF, not auto safe, is geo",
    "country": "us",
    "ping": "0.0.0.0:1",
    "openvpn_udp":{"best":"0.0.0.0:1"},
    "openvpn_tcp":{"best":"0.0.0.0:1"},
    "serial":"na",
    "port_forward": true,
    "geo": true
  },
  "npf_auto_ng": {
    "name": "no PF, auto safe, not geo",
    "country": "us",
    "ping": "0.0.0.0:1",
    "openvpn_udp":{"best":"0.0.0.0:1"},
    "openvpn_tcp":{"best":"0.0.0.0:1"},
    "serial":"na",
    "port_forward": false,
    "geo": false
  },
  "npf_auto_g": {
    "name": "no PF, auto safe, is geo",
    "country": "us",
    "ping": "0.0.0.0:1",
    "openvpn_udp":{"best":"0.0.0.0:1"},
    "openvpn_tcp":{"best":"0.0.0.0:1"},
    "serial":"na",
    "port_forward": false,
    "geo": true
  },
  "npf_nauto_ng": {
    "name": "no PF, not auto safe, not geo",
    "country": "us",
    "ping": "0.0.0.0:1",
    "openvpn_udp":{"best":"0.0.0.0:1"},
    "openvpn_tcp":{"best":"0.0.0.0:1"},
    "serial":"na",
    "port_forward": false,
    "geo": false
  },
  "npf_nauto_g": {
    "name": "no PF, not auto safe, is geo",
    "country": "us",
    "ping": "0.0.0.0:1",
    "openvpn_udp":{"best":"0.0.0.0:1"},
    "openvpn_tcp":{"best":"0.0.0.0:1"},
    "serial":"na",
    "port_forward": false,
    "geo": true
  },
  "info": {
    "auto_regions": [
      "pf_auto_ng",
      "pf_auto_g",
      "npf_auto_ng",
      "npf_auto_g"
    ]
  }
}
)").object();

    const auto emptyShadowsocks = QJsonDocument::fromJson("{}").object();
}

class tst_nearestlocations : public QObject
{
    Q_OBJECT

    // Region latencies:
    // hungary       500 (lowest latency but not part of auto_regions so should be ignored by getNearestSafeVpnLocation)
    // us2           600
    // us_california 700
    // romania       800  (has port forwarding)
    // poland        900  (has port forwarding)
    void setLatencies()
    {
        auto &hungary = *locs.at("hungary");
        auto &usEast = *locs.at("us2");
        auto &usCalif = *locs.at("us_california");
        auto &romania = *locs.at("ro");
        auto &poland = *locs.at("poland");

        hungary.latency(500);
        usEast.latency(600);
        usCalif.latency(700);
        romania.latency(800);
        poland.latency(900);
    }

    LocationsById locs;
    LocationsById locsNoPF;
    LocationsById locsNoAutoRegions;
    LocationsById locsGeo;

public:
    tst_nearestlocations(QObject *parent = NULL) : QObject(parent)
      , locs{buildLegacyLocations({}, samples::locations, samples::emptyShadowsocks)}
      , locsNoPF{buildLegacyLocations({}, samples::locationsNoPF, samples::emptyShadowsocks)}
      , locsNoAutoRegions{buildLegacyLocations({}, samples::locationsNoAutoRegions, samples::emptyShadowsocks)}
      , locsGeo{buildLegacyLocations({}, samples::locationsGeo, samples::emptyShadowsocks)} {

    }

private slots:

    void testGetNearestSafeVpnLocation()
    {
        setLatencies();
        NearestLocations nearestLocations{locs};

        // Don't prioritze port forwarding
        auto &nearest = *nearestLocations.getNearestSafeVpnLocation(false);
        // Note: hungary is ignored even though it has lowest latency
        QVERIFY(nearest.id() == "us2");
    }

    // Only consider regions with port forwarding
    void testGetNearestSafeVpnLocationWithPF()
    {
        setLatencies();
        NearestLocations nearestLocations{locs};

        auto &nearest = *nearestLocations.getNearestSafeVpnLocation(true);
        QVERIFY(nearest.id() == "ro");
    }

    // If we request PF regions but no PF regions available then fallback to fasted non-PF region
    void testGetNearestSafeVpnLocationWithPFButNoPFRegions()
    {
        locsNoPF.at("austria")->latency(100);
        locsNoPF.at("czech")->latency(200);
        NearestLocations nearestLocations{locsNoPF};

        // requesting regions with PF but none are available
        auto &nearest = *nearestLocations.getNearestSafeVpnLocation(true);

        QVERIFY(nearest.id() == "austria");
    }

    // If no regions are part of auto_regions then ALL regions are treated
    // as if they're part of auto regions (the alternative is that no
    // regions can be connected to on auto, which is not acceptable)
    void testGetNearestSafeVpnLocationsWithNoAutoRegions()
    {
        locsNoAutoRegions.at("aus_melbourne")->latency(100);
        locsNoAutoRegions.at("aus_perth")->latency(200);
        NearestLocations nearestLocations{locsNoAutoRegions};

        auto &nearest = *nearestLocations.getNearestSafeVpnLocation(false);
        QVERIFY(nearest.id() == "aus_melbourne");
    }

    // Further constrain the output with a predicate function
    void testGetNearestSafeServiceLocation()
    {
        setLatencies();
        NearestLocations nearestLocations{locs};

        // Nearest region after constraining output to region ids that begin with "u"
        // i.e "us2" and "us_california" in this case
        auto &nearest1 = *nearestLocations.getBestMatchingLocation(
            [](auto loc){ return loc.id().startsWith(QStringLiteral("u")); });

        QVERIFY(nearest1.id() == "us2");

        // Only regions that begin with "zzz"
        // There are none, so return nothing
        auto nearest2 = nearestLocations.getBestMatchingLocation(
            [](auto loc){ return loc.id().startsWith(QStringLiteral("zzz")); });

        QVERIFY(!nearest2);
    }

    // Test all combinations of preferences with geo, auto, and port forwarding.
    //
    void testGeoPreferences()
    {
        LocationsById testLocations{buildLegacyLocations({}, samples::locationsGeo, samples::emptyShadowsocks)};

        // Set latencies in reverse precedence order.  This follows the table
        // in NearestLocations (port forwarding is the "context" requirement
        // here).
        testLocations["pf_auto_ng"]->latency(1000);
        testLocations["pf_auto_g"]->latency(900);
        testLocations["pf_nauto_ng"]->latency(800);
        testLocations["pf_nauto_g"]->latency(700);
        testLocations["npf_auto_ng"]->latency(600);
        testLocations["npf_auto_g"]->latency(500);
        testLocations["npf_nauto_ng"]->latency(400);
        testLocations["npf_nauto_g"]->latency(300);

        auto getBestId = [&]() -> QString
        {
            auto pBest = NearestLocations{testLocations}.getNearestSafeVpnLocation(true);
            return pBest ? pBest->id() : QString{};
        };

        // First preference is PF, auto, not geo
        QCOMPARE(getBestId(), "pf_auto_ng");

        // Remove that - next preference is PF+auto, even if it's a geo location
        testLocations.erase("pf_auto_ng");
        QCOMPARE(getBestId(), "pf_auto_g");

        // Next preference is PF and non-geo, even if it's not auto safe
        testLocations.erase("pf_auto_g");
        QCOMPARE(getBestId(), "pf_nauto_ng");

        // Next preference is the only remaining PF location (not auto and geo)
        testLocations.erase("pf_nauto_ng");
        QCOMPARE(getBestId(), "pf_nauto_g");

        // After that, there are no PF locations, it'll try to match the other
        // criteria.  Next is auto and not geo.
        testLocations.erase("pf_nauto_g");
        QCOMPARE(getBestId(), "npf_auto_ng");

        // Next is auto, even if it's geo.
        testLocations.erase("npf_auto_ng");
        QCOMPARE(getBestId(), "npf_auto_g");

        // Next is geo, even if it's not auto safe.
        testLocations.erase("npf_auto_g");
        QCOMPARE(getBestId(), "npf_nauto_ng");

        // Finally, anything.
        testLocations.erase("npf_nauto_ng");
        QCOMPARE(getBestId(), "npf_nauto_g");
    }
};

QTEST_GUILESS_MAIN(tst_nearestlocations)
#include TEST_MOC
