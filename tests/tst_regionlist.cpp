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

#include <kapps_regions/src/regionlist.h>
#include <kapps_regions/src/metadata.h>
#include <kapps_core/src/logger.h>
#include "src/testresource.h"
#include <QtTest>
#include <nlohmann/json.hpp>

namespace kapps::regions
{

class tst_regionlist : public QObject
{
    Q_OBJECT

private:
    RegionList parseJson(core::StringSlice json)
    {
        return {json, {}, {}, {}};
    }

private slots:
    void testSuccess()
    {
        core::StringSlice json = R"(
            {
              "service_configs": [
                {
                  "name": "traffic1",
                  "services": [
                    {"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false},
                    {"service":"openvpn_tcp", "ports":[80,443,853,8443], "ncp":false},
                    {"service":"wireguard", "ports":[1337]},
                    {"service":"ikev2"}
                  ]
                },
                {
                  "name": "meta",
                  "services": [
                    {"service":"meta", "ports":[443,8080]}
                  ]
                }
              ],
              "regions": [
                {
                  "id": "us_chicago",
                  "auto_region": true,
                  "port_forward": false,
                  "geo": false,
                  "servers": [
                    {"ip":"154.21.23.79", "cn":"chicago412", "service_config":"traffic1"},
                    {"ip":"154.21.114.233", "cn":"chicago421", "service_config":"traffic1"},
                    {"ip":"212.102.59.129", "cn":"chicago403", "service_config":"meta"},
                    {"ip":"154.21.28.4", "cn":"chicago405", "service_config":"meta"}
                  ]
                },
                {
                  "id": "spain",
                  "auto_region": true,
                  "port_forward": true,
                  "geo": false,
                  "servers": [
                    {"ip":"212.102.49.78", "cn":"madrid401", "service_config":"traffic1"},
                    {"ip":"212.102.49.6", "cn":"madrid402", "service_config":"traffic1"},
                    {"ip":"195.181.167.33", "cn":"madrid401", "service_config":"meta"},
                    {"ip":"212.102.49.1", "cn":"madrid402", "service_config":"meta"}
                  ]
                },
                {
                  "id": "aus_perth",
                  "auto_region": true,
                  "port_forward": true,
                  "geo": false,
                  "servers": [
                    {"ip":"179.61.228.124", "cn":"perth404", "service_config":"traffic1"},
                    {"ip":"179.61.228.170", "cn":"perth405", "service_config":"traffic1"},
                    {"ip":"43.250.205.57", "cn":"perth403", "service_config":"meta"},
                    {"ip":"43.250.205.178", "cn":"perth404", "service_config":"meta"}
                  ]
                }
              ]
            }
        )";
        auto r = parseJson(json);

        QCOMPARE(r.regions().size(), 3u);
        QCOMPARE(r.getRegion("us_chicago")->id(), "us_chicago");
        QCOMPARE(r.getRegion("spain")->id(), "spain");
        QCOMPARE(r.getRegion("aus_perth")->id(), "aus_perth");
        const auto &chicago = *r.getRegion("us_chicago");
        QCOMPARE(chicago.autoSafe(), true);
        QCOMPARE(chicago.portForward(), false);
        QCOMPARE(chicago.geoLocated(), false);
        QCOMPARE(chicago.servers().size(), 4u);
        // The servers _do_ need to be reported in the order from the regions
        // list; this order is significant for load balancing.
        const auto &servers = chicago.servers();
        QCOMPARE(servers[0]->address(), (core::Ipv4Address{154, 21, 23, 79}));
        QCOMPARE(servers[0]->commonName(), "chicago412");
        // ServiceGroups have their own tests, just make sure the correct
        // service groups were applied to each server.
        QCOMPARE(servers[0]->hasOpenVpnUdp(), true);
        QCOMPARE(servers[1]->address(), (core::Ipv4Address{154, 21, 114, 233}));
        QCOMPARE(servers[1]->commonName(), "chicago421");
        QCOMPARE(servers[1]->hasOpenVpnUdp(), true);
        QCOMPARE(servers[2]->address(), (core::Ipv4Address{212, 102, 59, 129}));
        QCOMPARE(servers[2]->commonName(), "chicago403");
        QCOMPARE(servers[2]->hasMeta(), true);
        QCOMPARE(servers[3]->address(), (core::Ipv4Address{154, 21, 28, 4}));
        QCOMPARE(servers[3]->commonName(), "chicago405");
        QCOMPARE(servers[3]->hasMeta(), true);
    }

    // A service group with an error is ignored; the rest of the list is still
    // loaded.
    void testIgnoredServiceGroup()
    {
        // Error in 'meta' service group, VPN servers still loaded
        core::StringSlice json = R"(
            {
              "service_configs": [
                {
                  "name": "traffic1",
                  "services": [
                    {"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false},
                    {"service":"openvpn_tcp", "ports":[80,443,853,8443], "ncp":false},
                    {"service":"wireguard", "ports":[1337]},
                    {"service":"ikev2"}
                  ]
                },
                {
                  "name": "meta",
                  "services": [
                    {"service":"meta", "ports":"bogus"}
                  ]
                }
              ],
              "regions": [
                {
                  "id": "us_chicago",
                  "auto_region": true,
                  "port_forward": false,
                  "geo": false,
                  "servers": [
                    {"ip":"154.21.23.79", "cn":"chicago412", "service_config":"traffic1"},
                    {"ip":"154.21.114.233", "cn":"chicago421", "service_config":"traffic1"},
                    {"ip":"212.102.59.129", "cn":"chicago403", "service_config":"meta"},
                    {"ip":"154.21.28.4", "cn":"chicago405", "service_config":"meta"}
                  ]
                }
              ]
            }
        )";
        auto r = parseJson(json);

        // Only VPN servers were loaded due to error in 'meta' group
        const auto &chicago = *r.getRegion("us_chicago");
        QCOMPARE(chicago.servers().size(), 2u);
        const auto &servers = chicago.servers();
        QCOMPARE(servers[0]->address(), (core::Ipv4Address{154, 21, 23, 79}));
        QCOMPARE(servers[0]->commonName(), "chicago412");
        QCOMPARE(servers[0]->hasOpenVpnUdp(), true);
        QCOMPARE(servers[1]->address(), (core::Ipv4Address{154, 21, 114, 233}));
        QCOMPARE(servers[1]->commonName(), "chicago421");
        QCOMPARE(servers[1]->hasOpenVpnUdp(), true);
    }

    // Test error in server; other servers in region still loaded
    void testIgnoredServer()
    {
        // Invalid IP for chicago403
        core::StringSlice json = R"(
            {
              "service_configs": [
                {
                  "name": "traffic1",
                  "services": [
                    {"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false},
                    {"service":"openvpn_tcp", "ports":[80,443,853,8443], "ncp":false},
                    {"service":"wireguard", "ports":[1337]},
                    {"service":"ikev2"}
                  ]
                },
                {
                  "name": "meta",
                  "services": [
                    {"service":"meta", "ports":[443,8080]}
                  ]
                }
              ],
              "regions": [
                {
                  "id": "us_chicago",
                  "auto_region": true,
                  "port_forward": false,
                  "geo": false,
                  "servers": [
                    {"ip":"154.21.23.79", "cn":"chicago412", "service_config":"traffic1"},
                    {"ip":"154.21.114.233", "cn":"chicago421", "service_config":"traffic1"},
                    {"ip":"0.0.0.0", "cn":"chicago403", "service_config":"meta"},
                    {"ip":"154.21.28.4", "cn":"chicago405", "service_config":"meta"}
                  ]
                }
              ]
            }
        )";
        auto r = parseJson(json);

        const auto &chicago = *r.getRegion("us_chicago");
        QCOMPARE(chicago.servers().size(), 3u);
        const auto &servers = chicago.servers();
        QCOMPARE(servers[0]->address(), (core::Ipv4Address{154, 21, 23, 79}));
        QCOMPARE(servers[0]->commonName(), "chicago412");
        QCOMPARE(servers[0]->hasOpenVpnUdp(), true);
        QCOMPARE(servers[1]->address(), (core::Ipv4Address{154, 21, 114, 233}));
        QCOMPARE(servers[1]->commonName(), "chicago421");
        QCOMPARE(servers[1]->hasOpenVpnUdp(), true);
        QCOMPARE(servers[2]->address(), (core::Ipv4Address{154, 21, 28, 4}));
        QCOMPARE(servers[2]->commonName(), "chicago405");
        QCOMPARE(servers[2]->hasMeta(), true);
    }

    // A region with an error is ignored; other regions still loaded
    void testIgnoredRegion()
    {
        // Error in spain region, non-boolean flag
        core::StringSlice json = R"(
            {
              "service_configs": [
                {
                  "name": "traffic1",
                  "services": [
                    {"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false},
                    {"service":"openvpn_tcp", "ports":[80,443,853,8443], "ncp":false},
                    {"service":"wireguard", "ports":[1337]},
                    {"service":"ikev2"}
                  ]
                },
                {
                  "name": "meta",
                  "services": [
                    {"service":"meta", "ports":[443,8080]}
                  ]
                }
              ],
              "regions": [
                {
                  "id": "us_chicago",
                  "auto_region": true,
                  "port_forward": false,
                  "geo": false,
                  "servers": [
                    {"ip":"154.21.23.79", "cn":"chicago412", "service_config":"traffic1"},
                    {"ip":"154.21.114.233", "cn":"chicago421", "service_config":"traffic1"},
                    {"ip":"212.102.59.129", "cn":"chicago403", "service_config":"meta"},
                    {"ip":"154.21.28.4", "cn":"chicago405", "service_config":"meta"}
                  ]
                },
                {
                  "id": "spain",
                  "auto_region": 75,
                  "port_forward": true,
                  "geo": false,
                  "servers": [
                    {"ip":"212.102.49.78", "cn":"madrid401", "service_config":"traffic1"},
                    {"ip":"212.102.49.6", "cn":"madrid402", "service_config":"traffic1"},
                    {"ip":"195.181.167.33", "cn":"madrid401", "service_config":"meta"},
                    {"ip":"212.102.49.1", "cn":"madrid402", "service_config":"meta"}
                  ]
                }
              ]
            }
        )";
        auto r = parseJson(json);

        QCOMPARE(r.regions().size(), 1u);
        QCOMPARE(r.getRegion("spain"), nullptr);
        const auto &chicago = *r.getRegion("us_chicago");
        QCOMPARE(chicago.autoSafe(), true);
        QCOMPARE(chicago.portForward(), false);
        QCOMPARE(chicago.geoLocated(), false);
        QCOMPARE(chicago.servers().size(), 4u);
        const auto &servers = chicago.servers();
        QCOMPARE(servers[0]->address(), (core::Ipv4Address{154, 21, 23, 79}));
        QCOMPARE(servers[0]->commonName(), "chicago412");
        QCOMPARE(servers[0]->hasOpenVpnUdp(), true);
        QCOMPARE(servers[1]->address(), (core::Ipv4Address{154, 21, 114, 233}));
        QCOMPARE(servers[1]->commonName(), "chicago421");
        QCOMPARE(servers[1]->hasOpenVpnUdp(), true);
        QCOMPARE(servers[2]->address(), (core::Ipv4Address{212, 102, 59, 129}));
        QCOMPARE(servers[2]->commonName(), "chicago403");
        QCOMPARE(servers[2]->hasMeta(), true);
        QCOMPARE(servers[3]->address(), (core::Ipv4Address{154, 21, 28, 4}));
        QCOMPARE(servers[3]->commonName(), "chicago405");
        QCOMPARE(servers[3]->hasMeta(), true);
    }

    // Test a service group with no known services - servers using that group
    // are ignored
    void testEmptyServiceGroup()
    {
        // Error in spain region, non-boolean flag
        core::StringSlice json = R"(
            {
              "service_configs": [
                {
                  "name": "traffic1",
                  "services": [
                    {"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false},
                    {"service":"openvpn_tcp", "ports":[80,443,853,8443], "ncp":false},
                    {"service":"wireguard", "ports":[1337]},
                    {"service":"ikev2"}
                  ]
                },
                {
                  "name": "traffic2",
                  "services": [
                    {"service":"fancy_protocol", "ports":{"udp":8080,"tcp":443}}
                  ]
                },
                {
                  "name": "meta",
                  "services": [
                    {"service":"meta", "ports":[443,8080]}
                  ]
                }
              ],
              "regions": [
                {
                  "id": "us_chicago",
                  "auto_region": true,
                  "port_forward": false,
                  "geo": false,
                  "servers": [
                    {"ip":"154.21.23.79", "cn":"chicago412", "service_config":"traffic1"},
                    {"ip":"154.21.114.233", "cn":"chicago421", "service_config":"traffic2"},
                    {"ip":"212.102.59.129", "cn":"chicago403", "service_config":"meta"},
                    {"ip":"154.21.28.4", "cn":"chicago405", "service_config":"meta"}
                  ]
                }
              ]
            }
        )";
        auto r = parseJson(json);

        const auto &chicago = *r.getRegion("us_chicago");
        QCOMPARE(chicago.autoSafe(), true);
        QCOMPARE(chicago.portForward(), false);
        QCOMPARE(chicago.geoLocated(), false);
        QCOMPARE(chicago.servers().size(), 3u);
        const auto &servers = chicago.servers();
        QCOMPARE(servers[0]->address(), (core::Ipv4Address{154, 21, 23, 79}));
        QCOMPARE(servers[0]->commonName(), "chicago412");
        QCOMPARE(servers[0]->hasOpenVpnUdp(), true);
        // chicago421 was ignored
        QCOMPARE(servers[1]->address(), (core::Ipv4Address{212, 102, 59, 129}));
        QCOMPARE(servers[1]->commonName(), "chicago403");
        QCOMPARE(servers[1]->hasMeta(), true);
        QCOMPARE(servers[2]->address(), (core::Ipv4Address{154, 21, 28, 4}));
        QCOMPARE(servers[2]->commonName(), "chicago405");
        QCOMPARE(servers[2]->hasMeta(), true);
    }

    // Test a region with no servers - becomes offline
    void testOfflineRegion()
    {
        // Error in spain region, non-boolean flag
        core::StringSlice json = R"(
            {
              "service_configs": [
                {
                  "name": "traffic1",
                  "services": [
                    {"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false},
                    {"service":"openvpn_tcp", "ports":[80,443,853,8443], "ncp":false},
                    {"service":"wireguard", "ports":[1337]},
                    {"service":"ikev2"}
                  ]
                },
                {
                  "name": "meta",
                  "services": [
                    {"service":"meta", "ports":[443,8080]}
                  ]
                }
              ],
              "regions": [
                {
                  "id": "us_chicago",
                  "auto_region": true,
                  "port_forward": false,
                  "geo": false,
                  "servers": [
                  ]
                }
              ]
            }
        )";
        auto r = parseJson(json);

        const auto &chicago = *r.getRegion("us_chicago");
        QCOMPARE(chicago.autoSafe(), true);
        QCOMPARE(chicago.portForward(), false);
        QCOMPARE(chicago.geoLocated(), false);
        QCOMPARE(chicago.servers().size(), 0u);
        QCOMPARE(chicago.offline(), true);
    }

    // Test a region with only ignored servers (becomes offline)
    void testAllIgnoredServers()
    {
        // Error in spain region, non-boolean flag
        core::StringSlice json = R"(
            {
              "service_configs": [
                {
                  "name": "traffic1",
                  "services": [
                    {"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false},
                    {"service":"openvpn_tcp", "ports":[80,443,853,8443], "ncp":false},
                    {"service":"wireguard", "ports":[1337]},
                    {"service":"ikev2"}
                  ]
                },
                {
                  "name": "traffic2",
                  "services": [
                    {"service":"fancy_protocol", "ports":{"udp":8080,"tcp":443}}
                  ]
                },
                {
                  "name": "meta",
                  "services": [
                    {"service":"meta", "ports":[443,8080]}
                  ]
                },
                {
                  "name": "meta2",
                  "services": [
                    {"service":"meta2", "ports":[8443,18080]}
                  ]
                }
              ],
              "regions": [
                {
                  "id": "us_chicago",
                  "auto_region": true,
                  "port_forward": false,
                  "geo": false,
                  "servers": [
                    {"ip":"154.21.23.79", "cn":"chicago412", "service_config":"traffic2"},
                    {"ip":"154.21.114.233", "cn":"chicago421", "service_config":"traffic2"},
                    {"ip":"212.102.59.129", "cn":"chicago403", "service_config":"meta2"},
                    {"ip":"154.21.28.4", "cn":"chicago405", "service_config":"meta2"}
                  ]
                }
              ]
            }
        )";
        auto r = parseJson(json);

        const auto &chicago = *r.getRegion("us_chicago");
        QCOMPARE(chicago.autoSafe(), true);
        QCOMPARE(chicago.portForward(), false);
        QCOMPARE(chicago.geoLocated(), false);
        QCOMPARE(chicago.servers().size(), 0u);
        QCOMPARE(chicago.offline(), true);
    }

    // Server FQDNs can optionally be provided; these are needed only for some
    // platform-specific IKEv2 implementations.
    void testServerFqdns()
    {
        // The two VPN servers provide FQDNs; the two meta servers do not (one
        // is present but empty, the other is omitted)
        core::StringSlice json = R"(
            {
              "service_configs": [
                {
                  "name": "traffic1",
                  "services": [
                    {"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false},
                    {"service":"openvpn_tcp", "ports":[80,443,853,8443], "ncp":false},
                    {"service":"wireguard", "ports":[1337]},
                    {"service":"ikev2"}
                  ]
                },
                {
                  "name": "meta",
                  "services": [
                    {"service":"meta", "ports":[443,8080]}
                  ]
                }
              ],
              "regions": [
                {
                  "id": "us_chicago",
                  "auto_region": true,
                  "port_forward": false,
                  "geo": false,
                  "servers": [
                    {"ip":"154.21.23.79", "cn":"chicago412",
                        "fqdn":"blade9.chicago-rack412.nodes.gen4.ninja",
                        "service_config":"traffic1"},
                    {"ip":"154.21.114.233", "cn":"chicago421",
                        "fqdn":"blade7.chicago-rack421.nodes.gen4.ninja",
                        "service_config":"traffic1"},
                    {"ip":"212.102.59.129", "cn":"chicago403", "fqdn":"",
                        "service_config":"meta"},
                    {"ip":"154.21.28.4", "cn":"chicago405", "service_config":"meta"}
                  ]
                }
              ]
            }
        )";
        auto r = parseJson(json);

        const auto &chicago = *r.getRegion("us_chicago");
        QCOMPARE(chicago.servers().size(), 4u);
        const auto &servers = chicago.servers();
        QCOMPARE(servers[0]->address(), (core::Ipv4Address{154, 21, 23, 79}));
        QCOMPARE(servers[0]->commonName(), "chicago412");
        QCOMPARE(servers[0]->fqdn(), "blade9.chicago-rack412.nodes.gen4.ninja");
        QCOMPARE(servers[0]->hasOpenVpnUdp(), true);
        QCOMPARE(servers[1]->address(), (core::Ipv4Address{154, 21, 114, 233}));
        QCOMPARE(servers[1]->commonName(), "chicago421");
        QCOMPARE(servers[1]->fqdn(), "blade7.chicago-rack421.nodes.gen4.ninja");
        QCOMPARE(servers[1]->hasOpenVpnUdp(), true);
        QCOMPARE(servers[2]->address(), (core::Ipv4Address{212, 102, 59, 129}));
        QCOMPARE(servers[2]->commonName(), "chicago403");
        QCOMPARE(servers[2]->fqdn(), "");    // Was explicitly empty
        QCOMPARE(servers[2]->hasMeta(), true);
        QCOMPARE(servers[3]->address(), (core::Ipv4Address{154, 21, 28, 4}));
        QCOMPARE(servers[3]->commonName(), "chicago405");
        QCOMPARE(servers[3]->fqdn(), "");    // Was omitted
        QCOMPARE(servers[3]->hasMeta(), true);
    }

    // Test public DNS servers (optional field)
    void testPubDns()
    {
        // RegionList itself accepts a list with no regions (though the daemon
        // ignores the resulting empty regions list), so we can test this with
        // just a 'pubdns' field
        RegionList r;
        r = parseJson(R"(
            {
              "service_configs": [
              ],
              "pubdns": ["1.2.3.4","5.6.7.8"],
              "regions": [
              ]
            }
        )");
        QCOMPARE(r.publicDnsServers(),
            (std::vector<core::Ipv4Address>{{1, 2, 3, 4}, {5, 6, 7, 8}}));

        // pubdns is optional; empty array if omitted
        r = parseJson(R"({"service_configs":[],"regions":[]})");
        QCOMPARE(r.publicDnsServers(), std::vector<core::Ipv4Address>{});

        // pubdns can be empty
        r = parseJson(R"({"service_configs":[],"pubdns":[],"regions":[]})");
        QCOMPARE(r.publicDnsServers(), std::vector<core::Ipv4Address>{});

        // If specified, must be an array with IPv4 address elements
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"service_configs":[],"regions":[],
            "pubdns":"1.2.3.4"})"), std::exception);
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"service_configs":[],"regions":[],
            "pubdns":["1.2.3.4",9001]})"), std::exception);
    }

    // Test legacy PIAv6 regions support
    void testPiav6()
    {
        QByteArray regionsv6 = TestResource::load(QStringLiteral(":/regions-v6.json"));
        RegionList r{RegionList::PIAv6, regionsv6.data(), {}, {}, {}};
        KAPPS_CORE_INFO() << "v6:" << r.regions().size() << "regions";

        QCOMPARE(r.regions().size(), 107u);
        const auto &chicago = *r.getRegion("us_chicago");
        QCOMPARE(chicago.autoSafe(), true);
        QCOMPARE(chicago.portForward(), false);
        QCOMPARE(chicago.geoLocated(), false);
        QCOMPARE(chicago.servers().size(), 25u);
        // The v6 format only orders servers within a service group, so the fact
        // that IKEv2 servers are first is not significant.  The order within
        // each group is significant, though.
        const auto &firstIkev2 = *chicago.servers()[0];
        const auto &lastIkev2 = *chicago.servers()[4];
        QCOMPARE(firstIkev2.address(), (core::Ipv4Address{154, 21, 114, 182}));
        QCOMPARE(firstIkev2.commonName(), "chicago419");
        QCOMPARE(firstIkev2.hasIkev2(), true);
        QCOMPARE(firstIkev2.hasOpenVpnUdp(), false);
        QCOMPARE(lastIkev2.address(), (core::Ipv4Address{154, 21, 23, 182}));
        QCOMPARE(lastIkev2.commonName(), "chicago414");
        QCOMPARE(lastIkev2.hasIkev2(), true);
        QCOMPARE(lastIkev2.hasOpenVpnUdp(), false);
        // The next groups are 'meta' and then 'ovpntcp', check the OpenVPN
        // servers specifically
        const auto &firstOVTcp = *chicago.servers()[10];
        const auto &lastOVTcp = *chicago.servers()[14];
        QCOMPARE(firstOVTcp.address(), (core::Ipv4Address{154, 21, 114, 179}));
        QCOMPARE(firstOVTcp.commonName(), "chicago419");
        QCOMPARE(firstOVTcp.hasOpenVpnTcp(), true);
        QCOMPARE(firstOVTcp.openVpnTcpNcp(), false);
        QCOMPARE(firstOVTcp.openVpnTcpPorts(), (std::vector<std::uint16_t>{80,443,853,8443}));
        QCOMPARE(lastOVTcp.address(), (core::Ipv4Address{154, 21, 23, 189}));
        QCOMPARE(lastOVTcp.commonName(), "chicago414");
        QCOMPARE(lastOVTcp.hasOpenVpnTcp(), true);
        QCOMPARE(lastOVTcp.openVpnTcpNcp(), false);
        QCOMPARE(lastOVTcp.openVpnTcpPorts(), (std::vector<std::uint16_t>{80,443,853,8443}));
    }

    // Test legacy PIAv6 metadata support
    void testPiaMetadatav2()
    {
        QByteArray regionsv6 = TestResource::load(QStringLiteral(":/regions-v6.json"));
        QByteArray metadatav2 = TestResource::load(QStringLiteral(":/metadata-v2.json"));
        Metadata m{regionsv6.data(), metadatav2.data(), {}, {}};
        KAPPS_CORE_INFO() << "metadata v2:";
        KAPPS_CORE_INFO() << " -" << m.countryDisplays().size() << "countries";
        KAPPS_CORE_INFO() << " -" << m.regionDisplays().size() << "regions";

        // Dump all the country displays to see prefixes
        for(const auto &pCountry : m.countryDisplays())
        {
            // Only do the full dump if there's at least one non-empty string
            bool havePrefix{false};
            for(const auto &[lang, text] : pCountry->prefix().texts())
            {
                if(!text.empty())
                {
                    havePrefix = true;
                    break;
                }
            }

            if(havePrefix)
            {
                KAPPS_CORE_INFO() << "country" << pCountry->code();
                for(const auto &[lang, text] : pCountry->prefix().texts())
                {
                    KAPPS_CORE_INFO().nospace() << " - " << lang << " - ["
                        << text << "]";
                }
            }
            else
            {
                KAPPS_CORE_INFO() << "country" << pCountry->code() << "- empty,"
                    << pCountry->prefix().texts().size() << "empty prefixes";
            }
        }

        QCOMPARE(m.dynamicGroups().size(), 0u);
        QCOMPARE(m.countryDisplays().size(), 78u);
        QCOMPARE(m.regionDisplays().size(), 107u);

        auto pUs = m.getCountryDisplay("US");
        QCOMPARE(pUs->code(), "US");
        QCOMPARE(pUs->name().getLanguageText({"ar"}), "الولايات المتحدة");
        QCOMPARE(pUs->name().getLanguageText({"da"}), "USA");
        QCOMPARE(pUs->name().getLanguageText({"de"}), "USA");
        QCOMPARE(pUs->name().getLanguageText({"en-US"}), "United States");
        QCOMPARE(pUs->name().getLanguageText({"es-MX"}), "Estados Unidos");
        QCOMPARE(pUs->name().getLanguageText({"fr"}), "États-Unis");
        QCOMPARE(pUs->name().getLanguageText({"it"}), "Stati Uniti");
        QCOMPARE(pUs->name().getLanguageText({"ja"}), "アメリカ");
        QCOMPARE(pUs->name().getLanguageText({"ko"}), "미국");
        QCOMPARE(pUs->name().getLanguageText({"nb"}), "USA");
        QCOMPARE(pUs->name().getLanguageText({"nl"}), "Verenigde Staten");
        QCOMPARE(pUs->name().getLanguageText({"pl"}), "Stany Zjednoczone");
        QCOMPARE(pUs->name().getLanguageText({"pt-BR"}), "Estados Unidos");
        QCOMPARE(pUs->name().getLanguageText({"ru"}), "Соединенные Штаты");
        QCOMPARE(pUs->name().getLanguageText({"sv"}), "USA");
        QCOMPARE(pUs->name().getLanguageText({"th"}), "สหรัฐอเมริกา");
        QCOMPARE(pUs->name().getLanguageText({"tr"}), "Birleşik Devletler");
        QCOMPARE(pUs->name().getLanguageText({"zh-Hans"}), "美国");
        QCOMPARE(pUs->name().getLanguageText({"zh-Hant"}), "美國");
        QCOMPARE(pUs->prefix().getLanguageText({"ar"}), "الولايات المتحدة، ");
        QCOMPARE(pUs->prefix().getLanguageText({"da"}), "USA ");
        QCOMPARE(pUs->prefix().getLanguageText({"de"}), "USA ");
        QCOMPARE(pUs->prefix().getLanguageText({"en-US"}), "US ");
        QCOMPARE(pUs->prefix().getLanguageText({"es-MX"}), "EE. UU. ");
        QCOMPARE(pUs->prefix().getLanguageText({"fr"}), "US ");
        QCOMPARE(pUs->prefix().getLanguageText({"it"}), "US ");
        QCOMPARE(pUs->prefix().getLanguageText({"ja"}), "米国 ");
        QCOMPARE(pUs->prefix().getLanguageText({"ko"}), "미국 ");
        QCOMPARE(pUs->prefix().getLanguageText({"nb"}), "US ");
        QCOMPARE(pUs->prefix().getLanguageText({"nl"}), "VS ");
        QCOMPARE(pUs->prefix().getLanguageText({"pl"}), "USA ");
        QCOMPARE(pUs->prefix().getLanguageText({"pt-BR"}), "US ");
        QCOMPARE(pUs->prefix().getLanguageText({"ru"}), "США - ");
        QCOMPARE(pUs->prefix().getLanguageText({"sv"}), "US ");
        QCOMPARE(pUs->prefix().getLanguageText({"th"}), "สหรัฐอเมริกา ");
        QCOMPARE(pUs->prefix().getLanguageText({"tr"}), "ABD ");
        QCOMPARE(pUs->prefix().getLanguageText({"zh-Hans"}), "美国");
        QCOMPARE(pUs->prefix().getLanguageText({"zh-Hant"}), "美國");

        auto pUsChicago = m.getRegionDisplay("us_chicago");
        QCOMPARE(pUsChicago->id(), "us_chicago");
        QCOMPARE(pUsChicago->country(), "US");
        QCOMPARE(pUsChicago->geoLatitude(), 41.883229);
        QCOMPARE(pUsChicago->geoLongitude(), -87.632398);
        QCOMPARE(pUsChicago->name().getLanguageText({"ar"}), "شيكاغو");
        QCOMPARE(pUsChicago->name().getLanguageText({"da"}), "Chicago");
        QCOMPARE(pUsChicago->name().getLanguageText({"de"}), "Chicago");
        QCOMPARE(pUsChicago->name().getLanguageText({"en-US"}), "Chicago");
        QCOMPARE(pUsChicago->name().getLanguageText({"es-MX"}), "Chicago");
        QCOMPARE(pUsChicago->name().getLanguageText({"fr"}), "Chicago");
        QCOMPARE(pUsChicago->name().getLanguageText({"it"}), "Chicago");
        QCOMPARE(pUsChicago->name().getLanguageText({"ja"}), "シカゴ");
        QCOMPARE(pUsChicago->name().getLanguageText({"ko"}), "시카고");
        QCOMPARE(pUsChicago->name().getLanguageText({"nb"}), "Chicago");
        QCOMPARE(pUsChicago->name().getLanguageText({"nl"}), "Chicago");
        QCOMPARE(pUsChicago->name().getLanguageText({"pl"}), "Chicago");
        QCOMPARE(pUsChicago->name().getLanguageText({"pt-BR"}), "Chicago");
        QCOMPARE(pUsChicago->name().getLanguageText({"ru"}), "Чикаго");
        QCOMPARE(pUsChicago->name().getLanguageText({"sv"}), "Chicago");
        QCOMPARE(pUsChicago->name().getLanguageText({"th"}), "ชิคาโก");
        QCOMPARE(pUsChicago->name().getLanguageText({"tr"}), "Şikago");
        QCOMPARE(pUsChicago->name().getLanguageText({"zh-Hans"}), "芝加哥");
        QCOMPARE(pUsChicago->name().getLanguageText({"zh-Hant"}), "芝加哥");
    }
};

}

QTEST_GUILESS_MAIN(kapps::regions::tst_regionlist)
#include TEST_MOC
