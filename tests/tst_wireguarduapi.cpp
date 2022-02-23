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

#include "daemon/src/wireguarduapi.h"
#include <QtTest>
#include <limits>

#if defined(Q_OS_WIN)
#include <WinSock2.h> // ntohl(), etc.
#else
#include <arpa/inet.h>  // ntohl(), etc.
#endif

namespace
{
    // Dummy key name for messages
    const QLatin1String dummyKey{"dummy"};
}


class tst_wireguarduapi : public QObject
{
    Q_OBJECT

private slots:
    void testParseLongLong()
    {
        // Valid tests
        QCOMPARE(Uapi::parseLonglong<long long>(QLatin1String{"0"}), 0ll);
        QCOMPARE(Uapi::parseLonglong<long long>(QLatin1String{"-1"}), -1ll);
        QCOMPARE(Uapi::parseLonglong<long long>(QLatin1String{"6000000000"}), 6000000000ll);  // >32 bits
        QCOMPARE(Uapi::parseLonglong<long long>(QLatin1String{"-6000000000"}), -6000000000ll);    // >32 bits
        QCOMPARE(Uapi::parseLonglong<long long>(QLatin1String{"9223372036854775807"}), std::numeric_limits<long long>::max()); // max
        QCOMPARE(Uapi::parseLonglong<long long>(QLatin1String{"-9223372036854775808"}), std::numeric_limits<long long>::min()); // min
        QCOMPARE(Uapi::parseLonglong<unsigned long long>(QLatin1String{"0"}), 0ull);
        QCOMPARE(Uapi::parseLonglong<unsigned long long>(QLatin1String{"6000000000"}), 6000000000ull); // >32 bits
        QCOMPARE(Uapi::parseLonglong<unsigned long long>(QLatin1String{"18446744073709551615"}), std::numeric_limits<unsigned long long>::max());  // max

        // Invalid tests
        QVERIFY_EXCEPTION_THROWN(Uapi::parseLonglong<long long>(QLatin1String{"invalid"}), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseLonglong<long long>(QLatin1String{" 0 "}), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseLonglong<long long>(QLatin1String{"0invalid"}), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseLonglong<long long>(QLatin1String{"0."}), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseLonglong<long long>(QLatin1String{"1e7"}), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseLonglong<unsigned long long>(QLatin1String{"invalid"}), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseLonglong<unsigned long long>(QLatin1String{" 0 "}), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseLonglong<unsigned long long>(QLatin1String{"0invalid"}), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseLonglong<unsigned long long>(QLatin1String{"0."}), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseLonglong<unsigned long long>(QLatin1String{"1e7"}), Error);
    }

    void testParseInt()
    {
        // Integers shorter than 64 bits are limited to the correct range
        QCOMPARE(Uapi::parseInt<short>(QLatin1String{"32767"}), 32767);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseInt<short>(QLatin1String{"32768"}), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseInt<short>(QLatin1String{"-32769"}), Error);

        QCOMPARE(Uapi::parseInt<unsigned short>(QLatin1String{"65535"}), 65535);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseInt<unsigned short>(QLatin1String{"65536"}), Error);
        // Negative values do not throw for unsigned, due to strtoull()
        // permitting '-' (takes 2's complement of the unsigned value)
    }

    void testParseWgKey()
    {
        wg_key expected{0x30, 0xf0, 0xa2, 0xfb, 0x80, 0x71, 0x8b, 0x6c, 0x42,
                        0x66, 0x88, 0xd1, 0x42, 0x9f, 0xe6, 0x12, 0xa9, 0xcb,
                        0x62, 0xe4, 0x2d, 0x21, 0xc0, 0xdc, 0x81, 0x89, 0x05,
                        0x8a, 0xb2, 0xcb, 0x21, 0x7f};
        wg_key key{};
        Uapi::parseWireguardKey(QLatin1String{"30f0a2fb80718b6c426688d1429fe612a9cb62e42d21c0dc8189058ab2cb217f"},
                                key);
        QCOMPARE(QByteArray(reinterpret_cast<const char *>(&key[0]), sizeof(key)),
                 QByteArray(reinterpret_cast<const char *>(&expected[0]), sizeof(expected)));

        // Correct length, invalid chars
        QVERIFY_EXCEPTION_THROWN(Uapi::parseWireguardKey(QLatin1String{"30f0a2fb80718b6c426688d1429fe612a9cb62e42d21c0dc8189058ab2cbXXXX"},
                                                         key), Error);
        // key is unmodified
        QCOMPARE(QByteArray(reinterpret_cast<const char *>(&key[0]), sizeof(key)),
                 QByteArray(reinterpret_cast<const char *>(&expected[0]), sizeof(expected)));
        // Incorrect length
        QVERIFY_EXCEPTION_THROWN(Uapi::parseWireguardKey(QLatin1String{"30f0a2fb80718b6c426688d1429fe612a9cb62e42d21c0dc8189058ab2cb217f0000"},
                                                         key), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseWireguardKey(QLatin1String{"30f0a2fb80718b6c426688d1429fe612a9cb62e42d21c0dc8189058ab2cb"},
                                                         key), Error);
    }

    void testParsePeerEndpoint()
    {
        wg_peer peer{};
        Uapi::parsePeerEndpoint(QLatin1String{"[8000::0090]:23456"}, peer.endpoint);
        QCOMPARE(peer.endpoint.addr6.sin6_family, AF_INET6);
        QCOMPARE(peer.endpoint.addr6.sin6_port, htons(23456));
        in6_addr expected6{{{0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90}}};
        QCOMPARE((QByteArray{reinterpret_cast<const char*>(expected6.s6_addr), 16}),
                 (QByteArray{reinterpret_cast<const char*>(peer.endpoint.addr6.sin6_addr.s6_addr), 16}));

        Uapi::parsePeerEndpoint(QLatin1String{"101.102.201.202:12345"}, peer.endpoint);
        QCOMPARE(peer.endpoint.addr4.sin_family, AF_INET);
        QCOMPARE(peer.endpoint.addr4.sin_port, htons(12345));
        QCOMPARE(peer.endpoint.addr4.sin_addr.s_addr, htonl(0x6566C9CAu));

        QVERIFY_EXCEPTION_THROWN(Uapi::parsePeerEndpoint(QLatin1String{"123456"}, peer.endpoint), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parsePeerEndpoint(QLatin1String{"100.200.100.200"}, peer.endpoint), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parsePeerEndpoint(QLatin1String{"100.200.100.300:345"}, peer.endpoint), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parsePeerEndpoint(QLatin1String{"[123:123"}, peer.endpoint), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parsePeerEndpoint(QLatin1String{"[123]"}, peer.endpoint), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parsePeerEndpoint(QLatin1String{"[123]123"}, peer.endpoint), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parsePeerEndpoint(QLatin1String{"[123]:123"}, peer.endpoint), Error);
        // Address too long for internal buffer
        QVERIFY_EXCEPTION_THROWN(Uapi::parsePeerEndpoint(QLatin1String{"[00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:000]:123"}, peer.endpoint), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parsePeerEndpoint(QLatin1String{"000000000000000000000000000000000100.200.100.200:123"}, peer.endpoint), Error);
        // Unchanged
        QCOMPARE(peer.endpoint.addr4.sin_family, AF_INET);
        QCOMPARE(peer.endpoint.addr4.sin_port, htons(12345));
        QCOMPARE(peer.endpoint.addr4.sin_addr.s_addr, htonl(0x6566C9CAu));
    }

    void testParseAllowedIp()
    {
        wg_allowedip ip{};
        Uapi::parseAllowedIp(QLatin1String{"8000::0090/96"}, ip);
        QCOMPARE(ip.family, AF_INET6);
        QCOMPARE(ip.cidr, 96);
        in6_addr expected6{{{0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90}}};
        QCOMPARE((QByteArray{reinterpret_cast<const char*>(expected6.s6_addr), 16}),
                 (QByteArray{reinterpret_cast<const char*>(ip.ip6.s6_addr), 16}));

        Uapi::parseAllowedIp(QLatin1String{"101.102.201.202/16"}, ip);
        QCOMPARE(ip.family, AF_INET);
        QCOMPARE(ip.cidr, 16);
        QCOMPARE(ip.ip4.s_addr, htonl(0x6566C9CAu));

        QVERIFY_EXCEPTION_THROWN(Uapi::parseAllowedIp(QLatin1String{"123456"}, ip), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseAllowedIp(QLatin1String{"100.200.100.200"}, ip), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseAllowedIp(QLatin1String{"100.200.100.300/12"}, ip), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseAllowedIp(QLatin1String{"123/32"}, ip), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseAllowedIp(QLatin1String{"123"}, ip), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseAllowedIp(QLatin1String{"123/64"}, ip), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseAllowedIp(QLatin1String{"123/64"}, ip), Error);
        // Address too long for internal buffer
        QVERIFY_EXCEPTION_THROWN(Uapi::parseAllowedIp(QLatin1String{"00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:000/64"}, ip), Error);
        QVERIFY_EXCEPTION_THROWN(Uapi::parseAllowedIp(QLatin1String{"000000000000000000000000000000000100.200.100.200/22"}, ip), Error);
        // Unchanged
        QCOMPARE(ip.family, AF_INET);
        QCOMPARE(ip.cidr, 16);
        QCOMPARE(ip.ip4.s_addr, htonl(0x6566C9CAu));
    }

    void testMsgKey()
    {
        wg_key key{0x30, 0xf0, 0xa2, 0xfb, 0x80, 0x71, 0x8b, 0x6c, 0x42,
                   0x66, 0x88, 0xd1, 0x42, 0x9f, 0xe6, 0x12, 0xa9, 0xcb,
                   0x62, 0xe4, 0x2d, 0x21, 0xc0, 0xdc, 0x81, 0x89, 0x05,
                   0x8a, 0xb2, 0xcb, 0x21, 0x7f};
        QByteArray msg;
        Uapi::appendRequest(msg, dummyKey, key);
        QCOMPARE(msg, QByteArrayLiteral("dummy=30f0a2fb80718b6c426688d1429fe612a9cb62e42d21c0dc8189058ab2cb217f\n"));
    }

    void testMsgSockaddr4()
    {
        sockaddr_in addr4{};
        addr4.sin_family = AF_INET;
        addr4.sin_addr.s_addr = htonl(0x7F000001u);
        addr4.sin_port = htons(2345);
        QByteArray msg;
        Uapi::appendRequest(msg, dummyKey, addr4);
        QCOMPARE(msg, QByteArrayLiteral("dummy=127.0.0.1:2345\n"));
    }

    void testMsgSockaddr6()
    {
        sockaddr_in6 addr6{};
        addr6.sin6_family = AF_INET;
        addr6.sin6_addr.s6_addr[0] = 0x77;
        addr6.sin6_addr.s6_addr[15] = 0x33;
        addr6.sin6_port = htons(3456);
        QByteArray msg;
        Uapi::appendRequest(msg, dummyKey, addr6);
        QCOMPARE(msg, QByteArrayLiteral("dummy=[7700::33]:3456\n"));
    }

    void testMsgAllowedIp4()
    {
        wg_allowedip ip4{};
        ip4.family = AF_INET;
        ip4.ip4.s_addr = htonl(0xC0102004u);
        ip4.cidr = 16;
        QByteArray msg;
        Uapi::appendRequest(msg, dummyKey, ip4);
        QCOMPARE(msg, QByteArrayLiteral("dummy=192.16.32.4/16\n"));
    }

    void testMsgAllowedIp6()
    {
        wg_allowedip ip6{};
        ip6.family = AF_INET6;
        ip6.ip6.s6_addr[0] = 0x28;
        ip6.ip6.s6_addr[15] = 0x56;
        ip6.cidr = 64;
        QByteArray msg;
        Uapi::appendRequest(msg, dummyKey, ip6);
        QCOMPARE(msg, QByteArrayLiteral("dummy=2800::56/64\n"));
    }
};

QTEST_GUILESS_MAIN(tst_wireguarduapi)
#include TEST_MOC
