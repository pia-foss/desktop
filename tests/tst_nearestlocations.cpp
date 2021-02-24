// Copyright (c) 2021 Private Internet Access, Inc.
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
      "id": "austria",
      "name": "Austria",
      "country": "AT",
      "auto_region": true,
      "dns": "us-newyorkcity.privacy.network",
      "port_forward": false,
      "geo": false,
      "servers": {
        "ovpnudp": [{ "ip": "37.235.106.14", "cn": "newyork404" }],
        "ovpntcp": [{ "ip": "37.235.106.10", "cn": "newyork404" }],
        "ikev2": [{ "ip": "37.235.106.21", "cn": "newyork404" }],
        "wg": [{ "ip": "37.235.106.35", "cn": "newyork404" }]
      }
    },
    {
      "id": "czech",
      "name": "Czech Republic",
      "country": "HU",
      "auto_region": true,
      "dns": "hungary.privacy.network",
      "port_forward": false,
      "geo": false,
      "servers": {
        "ovpnudp": [{ "ip": "217.138.192.218", "cn": "budapest401" }],
        "ovpntcp": [{ "ip": "217.138.192.222", "cn": "budapest401" }],
        "ikev2": [{ "ip": "217.138.192.218", "cn": "budapest401" }],
        "wg": [{ "ip": "217.138.192.222", "cn": "budapest401" }]
      }
    }
  ]
}
)").object();

    const auto locationsNoAutoRegions = QJsonDocument::fromJson(R"(
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
      "id": "aus_melbourne",
      "name": "AU Melbourne",
      "country": "US",
      "auto_region": false,
      "dns": "us-newyorkcity.privacy.network",
      "port_forward": false,
      "geo": false,
      "servers": {
        "ovpnudp": [{ "ip": "37.235.106.14", "cn": "newyork404" }],
        "ovpntcp": [{ "ip": "37.235.106.10", "cn": "newyork404" }],
        "ikev2": [{ "ip": "37.235.106.21", "cn": "newyork404" }],
        "wg": [{ "ip": "37.235.106.35", "cn": "newyork404" }]
      }
    },
    {
      "id": "aus_perth",
      "name": "AU Perth",
      "country": "HU",
      "auto_region": false,
      "dns": "hungary.privacy.network",
      "port_forward": false,
      "geo": false,
      "servers": {
        "ovpnudp": [{ "ip": "217.138.192.218", "cn": "budapest401" }],
        "ovpntcp": [{ "ip": "217.138.192.222", "cn": "budapest401" }],
        "ikev2": [{ "ip": "217.138.192.218", "cn": "budapest401" }],
        "wg": [{ "ip": "217.138.192.222", "cn": "budapest401" }]
      }
    }
  ]
}
)").object();

    const auto locations = QJsonDocument::fromJson(R"(
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
      "id": "us2",
      "name": "US New York",
      "country": "US",
      "auto_region": true,
      "dns": "us-newyorkcity.privacy.network",
      "port_forward": false,
      "geo": false,
      "servers": {
        "ovpnudp": [{ "ip": "37.235.106.14", "cn": "newyork404" }],
        "ovpntcp": [{ "ip": "37.235.106.10", "cn": "newyork404" }],
        "ikev2": [{ "ip": "37.235.106.21", "cn": "newyork404" }],
        "wg": [{ "ip": "37.235.106.35", "cn": "newyork404" }]
      }
    },
    {
      "id": "hungary",
      "name": "Hungary",
      "country": "HU",
      "auto_region": false,
      "dns": "hungary.privacy.network",
      "port_forward": false,
      "geo": false,
      "servers": {
        "ovpnudp": [{ "ip": "217.138.192.218", "cn": "budapest401" }],
        "ovpntcp": [{ "ip": "217.138.192.222", "cn": "budapest401" }],
        "ikev2": [{ "ip": "217.138.192.218", "cn": "budapest401" }],
        "wg": [{ "ip": "217.138.192.222", "cn": "budapest401" }]
      }
    },
    {
      "id": "us_california",
      "name": "US California",
      "country": "US",
      "auto_region": true,
      "dns": "us-california.privacy.network",
      "port_forward": false,
      "geo": false,
      "servers": {
        "ovpnudp": [{ "ip": "37.235.107.242", "cn": "losangeles405" }],
        "ovpntcp": [{ "ip": "37.235.107.213", "cn": "losangeles405" }],
        "ikev2": [{ "ip": "37.235.107.214", "cn": "losangeles405" }],
        "wg": [{ "ip": "37.235.107.214", "cn": "losangeles405" }]
      }
    },
    {
      "id": "ro",
      "name": "Romania",
      "country": "RO",
      "auto_region": true,
      "dns": "ro.privacy.network",
      "port_forward": true,
      "geo": false,
      "servers": {
        "ovpnudp": [{ "ip": "143.244.54.160", "cn": "romania407" }],
        "ovpntcp": [{ "ip": "143.244.54.140", "cn": "romania407" }],
        "ikev2": [{ "ip": "143.244.54.131", "cn": "romania407" }],
        "wg": [{ "ip": "143.244.54.140", "cn": "romania407" }]
      }
    },
    {
      "id": "poland",
      "name": "Poland",
      "country": "PL",
      "auto_region": true,
      "dns": "poland.privacy.network",
      "port_forward": true,
      "geo": false,
      "servers": {
        "ovpnudp": [{ "ip": "217.138.209.245", "cn": "warsaw401" }],
        "ovpntcp": [{ "ip": "217.138.209.246", "cn": "warsaw401" }],
        "ikev2": [{ "ip": "217.138.209.246", "cn": "warsaw401" }],
        "wg": [{ "ip": "217.138.209.242", "cn": "warsaw401" }]
      }
    }
  ]
}
)").object();

const auto locationsGeo = QJsonDocument::fromJson(R"(
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
      "id": "pf_auto_ng",
      "name": "has PF, auto safe, not geo",
      "country": "US",
      "auto_region": true,
      "dns": "us-newyorkcity.privacy.network",
      "port_forward": true,
      "geo": false,
      "servers": {
        "ovpnudp": [{ "ip": "37.235.106.14", "cn": "newyork404" }],
        "ovpntcp": [{ "ip": "37.235.106.10", "cn": "newyork404" }],
        "ikev2": [{ "ip": "37.235.106.21", "cn": "newyork404" }],
        "wg": [{ "ip": "37.235.106.35", "cn": "newyork404" }]
      }
    },
    {
      "id": "pf_auto_g",
      "name": "has PF, auto safe, is geo",
      "country": "US",
      "auto_region": true,
      "dns": "us-newyorkcity.privacy.network",
      "port_forward": true,
      "geo": true,
      "servers": {
        "ovpnudp": [{ "ip": "37.235.106.14", "cn": "newyork404" }],
        "ovpntcp": [{ "ip": "37.235.106.10", "cn": "newyork404" }],
        "ikev2": [{ "ip": "37.235.106.21", "cn": "newyork404" }],
        "wg": [{ "ip": "37.235.106.35", "cn": "newyork404" }]
      }
    },
    {
      "id": "pf_nauto_ng",
      "name": "has PF, not auto safe, not geo",
      "country": "US",
      "auto_region": false,
      "dns": "us-newyorkcity.privacy.network",
      "port_forward": true,
      "geo": false,
      "servers": {
        "ovpnudp": [{ "ip": "37.235.106.14", "cn": "newyork404" }],
        "ovpntcp": [{ "ip": "37.235.106.10", "cn": "newyork404" }],
        "ikev2": [{ "ip": "37.235.106.21", "cn": "newyork404" }],
        "wg": [{ "ip": "37.235.106.35", "cn": "newyork404" }]
      }
    },
    {
      "id": "pf_nauto_g",
      "name": "has PF, not auto safe, is geo",
      "country": "US",
      "auto_region": false,
      "dns": "us-newyorkcity.privacy.network",
      "port_forward": true,
      "geo": true,
      "servers": {
        "ovpnudp": [{ "ip": "37.235.106.14", "cn": "newyork404" }],
        "ovpntcp": [{ "ip": "37.235.106.10", "cn": "newyork404" }],
        "ikev2": [{ "ip": "37.235.106.21", "cn": "newyork404" }],
        "wg": [{ "ip": "37.235.106.35", "cn": "newyork404" }]
      }
    },
    {
      "id": "npf_auto_ng",
      "name": "no PF, auto safe, not geo",
      "country": "US",
      "auto_region": true,
      "dns": "us-newyorkcity.privacy.network",
      "port_forward": false,
      "geo": false,
      "servers": {
        "ovpnudp": [{ "ip": "37.235.106.14", "cn": "newyork404" }],
        "ovpntcp": [{ "ip": "37.235.106.10", "cn": "newyork404" }],
        "ikev2": [{ "ip": "37.235.106.21", "cn": "newyork404" }],
        "wg": [{ "ip": "37.235.106.35", "cn": "newyork404" }]
      }
    },
    {
      "id": "npf_auto_g",
      "name": "no PF, auto safe, is geo",
      "country": "US",
      "auto_region": true,
      "dns": "us-newyorkcity.privacy.network",
      "port_forward": false,
      "geo": true,
      "servers": {
        "ovpnudp": [{ "ip": "37.235.106.14", "cn": "newyork404" }],
        "ovpntcp": [{ "ip": "37.235.106.10", "cn": "newyork404" }],
        "ikev2": [{ "ip": "37.235.106.21", "cn": "newyork404" }],
        "wg": [{ "ip": "37.235.106.35", "cn": "newyork404" }]
      }
    },
    {
      "id": "npf_nauto_ng",
      "name": "no PF, not auto safe, not geo",
      "country": "US",
      "auto_region": false,
      "dns": "us-newyorkcity.privacy.network",
      "port_forward": false,
      "geo": false,
      "servers": {
        "ovpnudp": [{ "ip": "37.235.106.14", "cn": "newyork404" }],
        "ovpntcp": [{ "ip": "37.235.106.10", "cn": "newyork404" }],
        "ikev2": [{ "ip": "37.235.106.21", "cn": "newyork404" }],
        "wg": [{ "ip": "37.235.106.35", "cn": "newyork404" }]
      }
    },
    {
      "id": "npf_nauto_g",
      "name": "no PF, not auto safe, is geo",
      "country": "US",
      "auto_region": false,
      "dns": "us-newyorkcity.privacy.network",
      "port_forward": false,
      "geo": true,
      "servers": {
        "ovpnudp": [{ "ip": "37.235.106.14", "cn": "newyork404" }],
        "ovpntcp": [{ "ip": "37.235.106.10", "cn": "newyork404" }],
        "ikev2": [{ "ip": "37.235.106.21", "cn": "newyork404" }],
        "wg": [{ "ip": "37.235.106.35", "cn": "newyork404" }]
      }
    }
  ]
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
      , locs{buildModernLocations({}, samples::locations, samples::emptyShadowsocks, {})}
      , locsNoPF{buildModernLocations({}, samples::locationsNoPF, samples::emptyShadowsocks, {})}
      , locsNoAutoRegions{buildModernLocations({}, samples::locationsNoAutoRegions, samples::emptyShadowsocks, {})}
      , locsGeo{buildModernLocations({}, samples::locationsGeo, samples::emptyShadowsocks, {})} {

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

    // // Further constrain the output with a predicate function
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

    // // Test all combinations of preferences with geo, auto, and port forwarding.
    // //
    void testGeoPreferences()
    {
        LocationsById testLocations{locsGeo};

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
