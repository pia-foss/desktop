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

#include "daemon/src/win/wg_service_json.h"
#include <QtTest>

class tst_wgservicejson : public QObject
{
    Q_OBJECT

private slots:
    void testParseValidJson()
    {
        // Test basic JSON parsing with valid interface name
        QByteArray json = R"({
            "name": "wgpia0",
            "publicKey": "0000000000000000000000000000000000000000000000000000000000000000",
            "listenPort": 51820,
            "fwmark": 0,
            "peers": []
        })";

        auto pStatus = WgServiceJson::parseWgServiceJson(json);
        QVERIFY(pStatus != nullptr);
        QCOMPARE(QString(pStatus->device().name), QString("wgpia0"));
        QCOMPARE(pStatus->device().listen_port, static_cast<uint16_t>(51820));
        QVERIFY(pStatus->device().flags & WGDEVICE_HAS_PUBLIC_KEY);
        QVERIFY(pStatus->device().flags & WGDEVICE_HAS_LISTEN_PORT);
    }

    void testParseWithPeer()
    {
        // Test JSON parsing with a peer
        QByteArray json = R"({
            "name": "wgpia0",
            "publicKey": "ed141cee184ce745d3b926466ec30d8a95c584b81412f47a96ed42f2dcb2b47f",
            "listenPort": 51820,
            "fwmark": 0,
            "peers": [{
                "publicKey": "a13c0e132bc2f9f2af8d1f0b72a8bd16cf98dfb2c8c5f2dbf7f9b6e2a7f8c9d0",
                "endpoint": "192.168.1.1:51820",
                "persistentKeepalive": 25,
                "txBytes": 1024,
                "rxBytes": 2048,
                "allowedIps": ["0.0.0.0/0"]
            }]
        })";

        auto pStatus = WgServiceJson::parseWgServiceJson(json);
        QVERIFY(pStatus != nullptr);
        QCOMPARE(QString(pStatus->device().name), QString("wgpia0"));

        // Verify peer was parsed
        QVERIFY(pStatus->device().first_peer != nullptr);
        QCOMPARE(pStatus->device().first_peer->persistent_keepalive_interval,
                 static_cast<uint16_t>(25));
        QCOMPARE(pStatus->device().first_peer->tx_bytes, static_cast<uint64_t>(1024));
        QCOMPARE(pStatus->device().first_peer->rx_bytes, static_cast<uint64_t>(2048));
    }

    void testParseMalformedJson()
    {
        // Test that malformed JSON is rejected
        QByteArray json = "{ this is not valid json }";

        auto pStatus = WgServiceJson::parseWgServiceJson(json);
        QVERIFY(pStatus == nullptr);
    }

    void testParseMissingName()
    {
        // Test that JSON without name field is rejected
        QByteArray json = R"({
            "publicKey": "0000000000000000000000000000000000000000000000000000000000000000",
            "listenPort": 51820,
            "peers": []
        })";

        auto pStatus = WgServiceJson::parseWgServiceJson(json);
        QVERIFY(pStatus == nullptr);
    }

    void testParseLongInterfaceName()
    {
        // Test that long interface names are properly truncated
        // wg_device::name is IFNAMSIZ (typically 16 bytes including null terminator)
        QByteArray json = R"({
            "name": "verylonginterfacenamethatshouldbetruncated",
            "publicKey": "0000000000000000000000000000000000000000000000000000000000000000",
            "listenPort": 51820,
            "fwmark": 0,
            "peers": []
        })";

        auto pStatus = WgServiceJson::parseWgServiceJson(json);
        QVERIFY(pStatus != nullptr);

        // Verify name is null-terminated and truncated
        size_t nameLen = strlen(pStatus->device().name);
        QVERIFY(nameLen < IFNAMSIZ);
        QVERIFY(pStatus->device().name[IFNAMSIZ - 1] == '\0');
    }

    void testHexToWgKey()
    {
        // Test hex key conversion
        QString hexKey = "ed141cee184ce745d3b926466ec30d8a95c584b81412f47a96ed42f2dcb2b47f";
        wg_key key;

        bool result = WgServiceJson::hexToWgKey(hexKey, key);
        QVERIFY(result);

        // Verify first few bytes
        QCOMPARE(key[0], static_cast<uint8_t>(0xed));
        QCOMPARE(key[1], static_cast<uint8_t>(0x14));
        QCOMPARE(key[2], static_cast<uint8_t>(0x1c));
    }

    void testHexToWgKeyInvalid()
    {
        // Test invalid hex key (wrong length)
        QString hexKey = "short";
        wg_key key;

        bool result = WgServiceJson::hexToWgKey(hexKey, key);
        QVERIFY(!result);
    }

    void testParseEndpoint()
    {
        // Test IPv4 endpoint parsing
        QString endpoint = "192.168.1.1:51820";
        decltype(wg_peer::endpoint) ep;

        bool result = WgServiceJson::parseEndpoint(endpoint, ep);
        QVERIFY(result);
        QCOMPARE(ep.addr4.sin_family, static_cast<unsigned short>(AF_INET));
    }

    void testParseCIDR()
    {
        // Test CIDR parsing
        QString cidr = "10.0.0.0/8";
        wg_allowedip allowedIp;

        bool result = WgServiceJson::parseCIDR(cidr, allowedIp);
        QVERIFY(result);
        QCOMPARE(allowedIp.family, static_cast<unsigned short>(AF_INET));
        QCOMPARE(allowedIp.cidr, static_cast<uint8_t>(8));
    }
};

QTEST_GUILESS_MAIN(tst_wgservicejson)
#include "tst_wgservicejson.moc"
