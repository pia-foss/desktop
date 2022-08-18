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

#include <kapps_regions/src/servicegroup.h>
#include <kapps_core/src/logger.h>
#include <QtTest>
#include <nlohmann/json.hpp>

class tst_servicegroup : public QObject
{
    Q_OBJECT

private:
    kapps::regions::ServiceGroup parseJson(kapps::core::StringSlice json)
    {
        auto j = nlohmann::json::parse(json);
        return j.get<kapps::regions::ServiceGroup>();
    }

private slots:
    void testValidServices()
    {
        kapps::regions::ServiceGroup g;

        g = parseJson(R"({"services":[{"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false}]})");
        QCOMPARE(g.openVpnUdpPorts(),
                 (std::vector<std::uint16_t>{8080, 853, 123, 53}));
        QCOMPARE(g.openVpnUdpNcp(), false);

        g = parseJson(R"({"services":[{"service":"openvpn_tcp", "ports":[80,443,853,8443], "ncp":true}]})");
        QCOMPARE(g.openVpnTcpPorts(),
                 (std::vector<std::uint16_t>{80, 443, 853, 8443}));
        QCOMPARE(g.openVpnTcpNcp(), true);

        g = parseJson(R"({"services":[{"service":"wireguard", "ports":[1337,2337,3337]}]})");
        QCOMPARE(g.wireGuardPorts(),
                 (std::vector<std::uint16_t>{1337,2337,3337}));

        g = parseJson(R"({"services":[{"service":"ikev2"}]})");
        QCOMPARE(g.ikev2(), true);

        g = parseJson(R"({"services":[{"service":"meta", "ports":[8080,443]}]})");
        QCOMPARE(g.metaPorts(),
                 (std::vector<std::uint16_t>{8080, 443}));
    }

    void testOptionalNcp()
    {
        // The "ncp" field for OpenVPN services is optional and defaults to
        // _true_ (not false)
        kapps::regions::ServiceGroup g;

        // Explicit false
        g = parseJson(R"({"services":[{"service":"openvpn_udp", "ports":[8080], "ncp":false}]})");
        QCOMPARE(g.openVpnUdpNcp(), false);

        // Explicit true
        g = parseJson(R"({"services":[{"service":"openvpn_udp", "ports":[8080], "ncp":true}]})");
        QCOMPARE(g.openVpnUdpNcp(), true);

        // Implicit true
        g = parseJson(R"({"services":[{"service":"openvpn_udp", "ports":[8080]}]})");
        QCOMPARE(g.openVpnUdpNcp(), true);

        // Non-boolean is still invalid
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"services":[{"service":"openvpn_udp", "ports":[8080], "ncp":0}]})"), std::exception);
    }

    void testCombinedServices()
    {
        auto g = parseJson(R"(
            {
              "services":[
                {"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false},
                {"service":"openvpn_tcp", "ports":[80,443,853,8443], "ncp":true},
                {"service":"wireguard", "ports":[1337,2337,3337]},
                {"service":"ikev2"}
              ]
            })"
        );

        QCOMPARE(g.openVpnUdpPorts(),
                 (std::vector<std::uint16_t>{8080, 853, 123, 53}));
        QCOMPARE(g.openVpnUdpNcp(), false);
        QCOMPARE(g.openVpnTcpPorts(),
                 (std::vector<std::uint16_t>{80, 443, 853, 8443}));
        QCOMPARE(g.openVpnTcpNcp(), true);
        QCOMPARE(g.wireGuardPorts(),
                 (std::vector<std::uint16_t>{1337,2337,3337}));
        QCOMPARE(g.ikev2(), true);
    }

    void testTypeErrors()
    {
        // Root type
        QVERIFY_EXCEPTION_THROWN(parseJson(R"([])"), std::exception);
        // Type of services value
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"services":3})"), std::exception);
        // Type of a service name
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"services":{"service":false}})"), std::exception);
        // Type of a service's ports
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"services":{"service":"wireguard","ports":1337}})"), std::exception);
        // Type of an ncp flag
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"services":{"service":"openvpn_udp","ports":[8080],"ncp":1}})"), std::exception);
    }

    // Test tolerance of new fields (should be ignored in any object)
    void testNewFields()
    {
        auto g = parseJson(R"(
            {
              "services":[
                {"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false, "new_ovpn_field":175},
                {"service":"ikev2","new_ikev2_field":"abcdef"}
              ],
              "new_svcgroup_field": true
            })"
        );

        QCOMPARE(g.openVpnUdpPorts(),
                 (std::vector<std::uint16_t>{8080, 853, 123, 53}));
        QCOMPARE(g.openVpnUdpNcp(), false);
        QCOMPARE(g.ikev2(), true);
    }

    // Test tolerance of new services (should be ignored)
    // New services could have any fields, even theoretically like-named fields
    // like 'ports' with different content
    void testNewServices()
    {
        auto g = parseJson(R"(
            {
              "services":[
                {"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false},
                {"service":"mystery_protocol", "ports":{"udp":[53,853],"tcp":[80,443]}, "ncp":9001},
                {"service":"boring_protocol"}
              ]
            })"
        );

        QCOMPARE(g.openVpnUdpPorts(),
                 (std::vector<std::uint16_t>{8080, 853, 123, 53}));
        QCOMPARE(g.openVpnUdpNcp(), false);
    }

    // Test handling of duplicate services
    void testDuplicateServices()
    {
        QVERIFY_EXCEPTION_THROWN(parseJson(R"(
            {
              "services":[
                {"service":"openvpn_udp", "ports":[8080,853,123,53], "ncp":false},
                {"service":"openvpn_udp", "ports":[853,53], "ncp":false}
              ]
            })"), std::exception);

        QVERIFY_EXCEPTION_THROWN(parseJson(R"(
            {
              "services":[
                {"service":"ikev2"},
                {"service":"ikev2"}
              ]
            })"), std::exception);
    }

    // Test all the various errors that are specifically detected for ports
    void testServicePorts()
    {
        // Non-array value for 'ports'
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"services":[{"service":"wireguard","ports":8080}]})"), std::exception);
        // Empty ports
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"services":[{"service":"wireguard","ports":[]}]})"), std::exception);
        // Port 0
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"services":[{"service":"wireguard","ports":[1337,0]}]})"), std::exception);
        // Port >65535
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"services":[{"service":"wireguard","ports":[1337,65536]}]})"), std::exception);
        // Negative
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"services":[{"service":"wireguard","ports":[1337,-10]}]})"), std::exception);
        // Floating-point
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"services":[{"service":"wireguard","ports":[1337,0.15]}]})"), std::exception);
        // Exponent - not valid even if the result is an integer
        QVERIFY_EXCEPTION_THROWN(parseJson(R"({"services":[{"service":"wireguard","ports":[1337,1.2e3]}]})"), std::exception);
    }
};

QTEST_GUILESS_MAIN(tst_servicegroup)
#include TEST_MOC
