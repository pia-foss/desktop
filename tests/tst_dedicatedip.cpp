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
#include <common/src/settings/dedicatedip.h>
#include <QtTest>
#include <QJsonDocument>

namespace
{
    // One active token from the /v5/dedicated_ip response
    QJsonObject sampleTokenData()
    {
        return QJsonDocument::fromJson(R"(
            {
              "id": "us_atlanta",
              "dip_token": "DIP1a2b3c",
              "dip_expire": 1767225600,
              "ip": "191.96.74.2",
              "cn": "atlanta429",
              "status": "active",
              "groups": ["ovpntcp", "ovpnudp", "wg", "ikev2"],
              "van": false,
              "ports": {
                "ovpnudp": [1197],
                "ovpntcp": [501]
              }
            }
        )").object();
    }

    // A stored DIP whose fields all differ from the sample token data
    AccountDedicatedIp storedDip()
    {
        AccountDedicatedIp dip;
        dip.id(QStringLiteral("dip-01234567"));
        dip.dipToken(QStringLiteral("DIP1a2b3c"));
        dip.expire(1000);
        dip.regionId(QStringLiteral("us_chicago"));
        dip.serviceGroups({QStringLiteral("wg")});
        dip.ip(QStringLiteral("191.96.74.9"));
        dip.cn(QStringLiteral("chicago400"));
        dip.openvpnUdpPorts({2000});
        dip.openvpnTcpPorts({3000});
        return dip;
    }

    // The sample token data with "ports" replaced
    QJsonObject tokenDataWithPorts(const QJsonValue &ports)
    {
        QJsonObject tokenData = sampleTokenData();
        tokenData.insert(QStringLiteral("ports"), ports);
        return tokenData;
    }

    // The sample token data with one entry of "ports" replaced
    QJsonObject tokenDataWithPortEntry(const QString &entry, const QJsonValue &value)
    {
        QJsonObject ports = sampleTokenData()[QStringLiteral("ports")].toObject();
        ports.insert(entry, value);
        return tokenDataWithPorts(ports);
    }

    // The sample token data without one entry of "ports"
    QJsonObject tokenDataWithoutPortEntry(const QString &entry)
    {
        QJsonObject ports = sampleTokenData()[QStringLiteral("ports")].toObject();
        ports.remove(entry);
        return tokenDataWithPorts(ports);
    }
}

class tst_dedicatedip : public QObject
{
    Q_OBJECT

private slots:
    // All fields are read from the API response, including the OpenVPN ports
    void testApplySample()
    {
        AccountDedicatedIp dip;
        applyDedicatedIpJson(sampleTokenData(), dip);

        QCOMPARE(dip.regionId(), QStringLiteral("us_atlanta"));
        QCOMPARE(dip.expire(), quint64{1767225600000});
        QCOMPARE(dip.ip(), QStringLiteral("191.96.74.2"));
        QCOMPARE(dip.cn(), QStringLiteral("atlanta429"));
        QCOMPARE(dip.serviceGroups(), (std::vector<QString>{QStringLiteral("ovpntcp"),
            QStringLiteral("ovpnudp"), QStringLiteral("wg"), QStringLiteral("ikev2")}));
        QCOMPARE(dip.openvpnUdpPorts(), std::vector<quint16>{1197});
        QCOMPARE(dip.openvpnTcpPorts(), std::vector<quint16>{501});
        // The token and region ID are managed by the daemon, not the response
        QCOMPARE(dip.dipToken(), QString{});
        QCOMPARE(dip.id(), QString{});
        // A new DIP has no IP change
        QCOMPARE(dip.lastIpChange(), quint64{0});
    }

    // Multiple ports are kept in API order, including ports that aren't
    // sorted and the boundary values of a port number
    void testApplyMultiplePorts()
    {
        AccountDedicatedIp dip;
        applyDedicatedIpJson(tokenDataWithPorts(QJsonObject{
            {QStringLiteral("ovpnudp"), QJsonArray{1198, 1197, 65535}},
            {QStringLiteral("ovpntcp"), QJsonArray{502, 1, 501}}
        }), dip);
        QCOMPARE(dip.openvpnUdpPorts(), (std::vector<quint16>{1198, 1197, 65535}));
        QCOMPARE(dip.openvpnTcpPorts(), (std::vector<quint16>{502, 1, 501}));
    }

    // An empty port list is accepted; the DIP just doesn't offer that OpenVPN
    // service
    void testApplyEmptyPorts()
    {
        AccountDedicatedIp dip;
        applyDedicatedIpJson(tokenDataWithPorts(QJsonObject{
            {QStringLiteral("ovpnudp"), QJsonArray{}},
            {QStringLiteral("ovpntcp"), QJsonArray{501}}
        }), dip);
        QCOMPARE(dip.openvpnUdpPorts(), std::vector<quint16>{});
        QCOMPARE(dip.openvpnTcpPorts(), std::vector<quint16>{501});
    }

    // Unknown entries in "ports" are ignored
    void testApplyIgnoresOtherPorts()
    {
        AccountDedicatedIp dip;
        applyDedicatedIpJson(tokenDataWithPortEntry(QStringLiteral("newservice"),
                                                    QStringLiteral("not a port list")),
                             dip);
        QCOMPARE(dip.openvpnUdpPorts(), std::vector<quint16>{1197});
        QCOMPARE(dip.openvpnTcpPorts(), std::vector<quint16>{501});
    }

    // A missing or null "ports" field, or a missing or null entry, means no
    // ports for that OpenVPN protocol.  The DIP is still added (WireGuard can
    // be used) and the other fields are applied.
    void testApplyMissingPorts_data()
    {
        QTest::addColumn<QJsonObject>("tokenData");
        QTest::addColumn<QString>("warning");
        QTest::addColumn<bool>("hasUdp");
        QTest::addColumn<bool>("hasTcp");

        const QString noPortsWarning{QStringLiteral("Dedicated IP \"dip-01234567\" has no OpenVPN ports")};
        const QString noUdpWarning{QStringLiteral("Dedicated IP \"dip-01234567\" has no ovpnudp ports")};
        const QString noTcpWarning{QStringLiteral("Dedicated IP \"dip-01234567\" has no ovpntcp ports")};

        QJsonObject noPorts = sampleTokenData();
        noPorts.remove(QStringLiteral("ports"));
        QTest::newRow("ports missing") << noPorts << noPortsWarning << false << false;
        QTest::newRow("ports null") << tokenDataWithPorts(QJsonValue::Null) << noPortsWarning << false << false;
        QTest::newRow("ovpnudp missing") << tokenDataWithoutPortEntry(QStringLiteral("ovpnudp")) << noUdpWarning << false << true;
        QTest::newRow("ovpnudp null") << tokenDataWithPortEntry(QStringLiteral("ovpnudp"), QJsonValue::Null) << noUdpWarning << false << true;
        QTest::newRow("ovpntcp missing") << tokenDataWithoutPortEntry(QStringLiteral("ovpntcp")) << noTcpWarning << true << false;
    }
    void testApplyMissingPorts()
    {
        QFETCH(QJsonObject, tokenData);
        QFETCH(QString, warning);
        QFETCH(bool, hasUdp);
        QFETCH(bool, hasTcp);

        AccountDedicatedIp dip;
        dip.id(QStringLiteral("dip-01234567"));
        QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));
        applyDedicatedIpJson(tokenData, dip);
        QCOMPARE(dip.openvpnUdpPorts(), hasUdp ? std::vector<quint16>{1197} : std::vector<quint16>{});
        QCOMPARE(dip.openvpnTcpPorts(), hasTcp ? std::vector<quint16>{501} : std::vector<quint16>{});
        QCOMPARE(dip.regionId(), QStringLiteral("us_atlanta"));
        QCOMPARE(dip.ip(), QStringLiteral("191.96.74.2"));
        QCOMPARE(dip.cn(), QStringLiteral("atlanta429"));
    }

    // A malformed "ports" field makes the response invalid, like the other
    // required fields, and the DIP is not changed
    void testApplyInvalidPorts_data()
    {
        QTest::addColumn<QJsonObject>("tokenData");

        QTest::newRow("ports not an object") << tokenDataWithPorts(QJsonArray{1197});
        QTest::newRow("ovpnudp not an array") << tokenDataWithPortEntry(QStringLiteral("ovpnudp"), 1197);
        QTest::newRow("ovpntcp not an array") << tokenDataWithPortEntry(QStringLiteral("ovpntcp"), 501);
        QTest::newRow("port is a string") << tokenDataWithPortEntry(QStringLiteral("ovpnudp"), QJsonArray{QStringLiteral("1197")});
        QTest::newRow("port is fractional") << tokenDataWithPortEntry(QStringLiteral("ovpnudp"), QJsonArray{1197.5});
        QTest::newRow("port is negative") << tokenDataWithPortEntry(QStringLiteral("ovpnudp"), QJsonArray{-1});
        QTest::newRow("port is too large") << tokenDataWithPortEntry(QStringLiteral("ovpntcp"), QJsonArray{65536});
        QTest::newRow("port is null") << tokenDataWithPortEntry(QStringLiteral("ovpntcp"), QJsonArray{QJsonValue::Null});
    }
    void testApplyInvalidPorts()
    {
        QFETCH(QJsonObject, tokenData);
        const AccountDedicatedIp before{storedDip()};
        AccountDedicatedIp dip{before};
        QVERIFY_EXCEPTION_THROWN(applyDedicatedIpJson(tokenData, dip), Error);
        QVERIFY(dip == before);
    }

    // A malformed response during a refresh leaves the stored DIP unchanged,
    // so a new IP in that response isn't stored without recording the IP
    // change.  The next valid response records it.
    void testRefreshFailureKeepsDip()
    {
        AccountDedicatedIp dip;
        applyDedicatedIpJson(sampleTokenData(), dip);
        const AccountDedicatedIp before{dip};

        QJsonObject changed = tokenDataWithPortEntry(QStringLiteral("ovpnudp"), QStringLiteral("1197"));
        changed.insert(QStringLiteral("ip"), QStringLiteral("191.96.74.3"));
        QVERIFY_EXCEPTION_THROWN(applyDedicatedIpJson(changed, dip), Error);
        QVERIFY(dip == before);
        QCOMPARE(dip.lastIpChange(), quint64{0});

        changed = sampleTokenData();
        changed.insert(QStringLiteral("ip"), QStringLiteral("191.96.74.3"));
        const quint64 beforeChange = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
        applyDedicatedIpJson(changed, dip);
        QCOMPARE(dip.ip(), QStringLiteral("191.96.74.3"));
        QVERIFY(dip.lastIpChange() >= beforeChange);
    }

    // A refresh replaces the ports entirely, it doesn't merge them with the
    // previous ports
    void testRefreshReplacesPorts()
    {
        AccountDedicatedIp dip;
        applyDedicatedIpJson(tokenDataWithPorts(QJsonObject{
            {QStringLiteral("ovpnudp"), QJsonArray{123, 456}},
            {QStringLiteral("ovpntcp"), QJsonArray{501}}
        }), dip);

        applyDedicatedIpJson(tokenDataWithPorts(QJsonObject{
            {QStringLiteral("ovpnudp"), QJsonArray{123, 789}},
            {QStringLiteral("ovpntcp"), QJsonArray{}}
        }), dip);
        QCOMPARE(dip.openvpnUdpPorts(), (std::vector<quint16>{123, 789}));
        QCOMPARE(dip.openvpnTcpPorts(), std::vector<quint16>{});
    }

    // A refresh fills in the ports of a DIP saved by an older version, without
    // the port fields
    void testRefreshAddsPortsToOldDip()
    {
        AccountDedicatedIp dip;
        dip.id(QStringLiteral("dip-01234567"));
        dip.dipToken(QStringLiteral("DIP1a2b3c"));
        dip.ip(QStringLiteral("191.96.74.2"));
        QCOMPARE(dip.openvpnUdpPorts(), std::vector<quint16>{});

        applyDedicatedIpJson(sampleTokenData(), dip);
        QCOMPARE(dip.openvpnUdpPorts(), std::vector<quint16>{1197});
        QCOMPARE(dip.openvpnTcpPorts(), std::vector<quint16>{501});
        // The client-side fields are kept
        QCOMPARE(dip.id(), QStringLiteral("dip-01234567"));
        QCOMPARE(dip.dipToken(), QStringLiteral("DIP1a2b3c"));
        // Same IP, so no IP change
        QCOMPARE(dip.lastIpChange(), quint64{0});
    }

    // A changed IP on refresh records the change time; the ports are still
    // applied
    void testRefreshIpChange()
    {
        AccountDedicatedIp dip;
        applyDedicatedIpJson(sampleTokenData(), dip);

        QJsonObject changed = tokenDataWithPortEntry(QStringLiteral("ovpnudp"), QJsonArray{1198});
        changed.insert(QStringLiteral("ip"), QStringLiteral("191.96.74.3"));
        const quint64 before = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
        applyDedicatedIpJson(changed, dip);

        QCOMPARE(dip.ip(), QStringLiteral("191.96.74.3"));
        QVERIFY(dip.lastIpChange() >= before);
        QCOMPARE(dip.openvpnUdpPorts(), std::vector<quint16>{1198});
    }

    // An expiration of 0 is never stored, it would make the region look like
    // a non-DIP region
    void testZeroExpire()
    {
        QJsonObject tokenData = sampleTokenData();
        tokenData.insert(QStringLiteral("dip_expire"), 0);
        AccountDedicatedIp dip;
        QTest::ignoreMessage(QtWarningMsg, "Dedicated IP \"\" returned invalid expiration 0");
        applyDedicatedIpJson(tokenData, dip);
        QCOMPARE(dip.expire(), quint64{1000});
    }

    // Copying and comparing include the ports, so a port change from the API
    // is seen as a change to the DIP
    void testCopyAndCompare()
    {
        AccountDedicatedIp dip;
        applyDedicatedIpJson(sampleTokenData(), dip);

        AccountDedicatedIp copy{dip};
        QVERIFY(copy == dip);
        QCOMPARE(copy.openvpnUdpPorts(), std::vector<quint16>{1197});
        QCOMPARE(copy.openvpnTcpPorts(), std::vector<quint16>{501});

        AccountDedicatedIp assigned;
        assigned = dip;
        QVERIFY(assigned == dip);

        copy.openvpnUdpPorts({1198});
        QVERIFY(copy != dip);
        copy = dip;
        copy.openvpnTcpPorts({502});
        QVERIFY(copy != dip);
        // Same ports in a different order is a change, the order matters
        copy = dip;
        dip.openvpnUdpPorts({1197, 1198});
        copy.openvpnUdpPorts({1198, 1197});
        QVERIFY(copy != dip);
    }
};

QTEST_GUILESS_MAIN(tst_dedicatedip)
#include TEST_MOC
