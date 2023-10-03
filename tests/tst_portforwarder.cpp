// Copyright (c) 2023 Private Internet Access, Inc.
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

#include "daemon/src/portforwarder.h"
#include <common/src/testshim.h>
#include "src/mocknetwork.h"
#include <QtTest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace
{
    // Response:
    // - status - OK / error
    // - payload - Base64 JSON
    //   - expires_at - ISO date with ms
    //   - port
    // - signature - whatever
    QByteArray buildPfPayload(quint16 port, std::chrono::milliseconds validity)
    {
        QJsonDocument payloadDoc
        {
            QJsonObject
            {
                {QStringLiteral("port"), port},
                {QStringLiteral("expires_at"), QDateTime::currentDateTimeUtc()
                                                .addMSecs(msec(validity))
                                                .toString(Qt::DateFormat::ISODateWithMs)}
            }
        };
        return payloadDoc.toJson().toBase64();
    }

    const QString mockSignature{QStringLiteral("abcdefghijkl")};

    QByteArray buildPfResponse(const QString &status, const QByteArray &payload)
    {
        QJsonDocument responseDoc
        {
            QJsonObject
            {
                {QStringLiteral("status"), status},
                {QStringLiteral("payload"), QString::fromUtf8(payload)},
                {QStringLiteral("signature"), mockSignature}
            }
        };
        return responseDoc.toJson();
    }

    QByteArray buildSuccessResponse(quint16 port)
    {
        return buildPfResponse(QStringLiteral("OK"), buildPfPayload(port, std::chrono::hours{24*30}));
    }

    QByteArray buildBindResponse(const QString &status)
    {
        return QJsonDocument
        {
            QJsonObject
            {
                {QStringLiteral("status"), status},
                {QStringLiteral("message"), QStringLiteral("unit test")}
            }
        }.toJson();
    }

    const auto bindSuccess{buildBindResponse(QStringLiteral("OK"))};
}

// PortForwarder and PortRequester with defaulted parameters, so we don't have
// to repeat this in every test
class TestPortForwarder : public StateModel, public Environment,
    public ApiClient, public DaemonAccount,  public PortForwarder
{
    CLASS_LOGGING_CATEGORY("TestPortForwarder");
public:
    TestPortForwarder()
        : Environment{static_cast<StateModel&>(*this)},
          PortForwarder{*this, *this, *this, *this}
    {
        // A token is needed for the port forward request to proceed
        token(QStringLiteral("abcdef"));
        // A remote address is needed to form the PF request URIs, it can be
        // anything since we mock the network requests
        tunnelDeviceRemoteAddress(QStringLiteral("10.0.0.1"));
    }
};

class tst_portforwarder : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        TestShim::installMock<QNetworkAccessManager, MockNetworkManager>();
    }

    // Verify that PortForwarder requests a port when the feature is enabled,
    // then the VPN is connected
    void testEnabledConnect()
    {
        TestPortForwarder forwarder;

        QSignalSpy resultSpy(&forwarder, &PortForwarder::portForwardUpdated);

        enum { MockPort = 59901 };

        auto pReply = MockNetworkManager::enqueueReply(buildSuccessResponse(MockPort));

        forwarder.enablePortForwarding(true);
        QVERIFY(MockNetworkManager::hasNextReply()); // Not requested yet

        forwarder.updateConnectionState(PortForwarder::State::ConnectedSupported);
        // 'Attempting' is emitted synchronously
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), VpnState::PortForwardState::Attempting);
        // The response is consumed asynchronously
        QSignalSpy consumeSpy{&MockNetworkManager::_replyConsumed, &ReplyConsumedSignal::signal};
        QVERIFY(consumeSpy.wait(1000));
        QVERIFY(!MockNetworkManager::hasNextReply());
        auto pBindReply = MockNetworkManager::enqueueReply(bindSuccess);
        consumeSpy.clear();
        pReply->queueFinished();
        QVERIFY(consumeSpy.wait(1000));
        QVERIFY(!MockNetworkManager::hasNextReply());
        pBindReply->queueFinished();
        QVERIFY(resultSpy.wait());
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), MockPort);
    }

    // Verify that if PF is enabled after having connected, PortForwarder does
    // _not_ begin a request.  A reconnect is required to start a PF request.
    void testConnectedEnable()
    {
        TestPortForwarder forwarder;

        QSignalSpy resultSpy(&forwarder, &PortForwarder::portForwardUpdated);

        auto pReply = MockNetworkManager::enqueueReply(buildSuccessResponse(59902));
        auto replyCleanup = raii_sentinel([]{MockNetworkManager::clearQueuedReplies();});

        forwarder.updateConnectionState(PortForwarder::State::ConnectedSupported);
        QVERIFY(MockNetworkManager::hasNextReply()); // Not requested yet

        forwarder.enablePortForwarding(true);
        // No change from this
        QVERIFY(!resultSpy.wait(100));
        QVERIFY(MockNetworkManager::hasNextReply()); // Not consumed
    }

    // Verify that nothing happens when toggling the feature while we're
    // disconnected, or toggling the connection state while the feature is off
    void testNoOps()
    {
        TestPortForwarder forwarder;

        // PortForwarder can emit signals during this test, but they all must
        // indicate that the forwarded port is not set.
        QSignalSpy resultSpy(&forwarder, &PortForwarder::portForwardUpdated);
        // No requests should be made during this test.
        auto pReply = MockNetworkManager::enqueueReply(buildSuccessResponse(59903));
        auto replyCleanup = raii_sentinel([]{MockNetworkManager::clearQueuedReplies();});

        forwarder.enablePortForwarding(true);
        forwarder.enablePortForwarding(false);
        forwarder.updateConnectionState(PortForwarder::State::ConnectedSupported);
        forwarder.updateConnectionState(PortForwarder::State::Disconnected);

        // Let anything that has been queued up clear through the event queue
        // (keep waiting until resultSpy waits without observing any signals)
        while(resultSpy.wait(100));

        // All signals (if any were emitted) should have set the port to Inactive
        for(const auto &emittedSignal : resultSpy)
            QCOMPARE(emittedSignal[0].value<int>(), VpnState::PortForwardState::Inactive);
        // No requests should have occurred (the reply should not have been
        // consumed)
        QCOMPARE(MockNetworkManager::hasNextReply(), true);
    }

    // Losing connection while a port is forwarded should clear it, and
    // reconnecting should forward it again
    void testReconnect()
    {
        TestPortForwarder forwarder;

        QSignalSpy resultSpy(&forwarder, &PortForwarder::portForwardUpdated);

        enum { MockPort = 59904 };
        auto pSuccess1 = MockNetworkManager::enqueueReply(buildSuccessResponse(MockPort));

        forwarder.enablePortForwarding(true);
        forwarder.updateConnectionState(PortForwarder::State::ConnectedSupported);
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), VpnState::PortForwardState::Attempting);

        QSignalSpy consumeSpy{&MockNetworkManager::_replyConsumed, &ReplyConsumedSignal::signal};
        QVERIFY(consumeSpy.wait(1000));
        QVERIFY(!MockNetworkManager::hasNextReply());
        auto pBindReply1 = MockNetworkManager::enqueueReply(bindSuccess);
        consumeSpy.clear();
        pSuccess1->queueFinished();
        QVERIFY(consumeSpy.wait(1000));
        QVERIFY(!MockNetworkManager::hasNextReply());
        pBindReply1->queueFinished();

        QVERIFY(resultSpy.wait());
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), MockPort);

        forwarder.updateConnectionState(PortForwarder::State::Disconnected);
        // This signal occurs synchronously, so there's no wait needed here
        QVERIFY(!resultSpy.empty());
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), VpnState::PortForwardState::Inactive);

        // Only a bind is sent when we reconnect, because the PF payload is
        // still valid
        auto pBindReply2 = MockNetworkManager::enqueueReply(bindSuccess);
        forwarder.updateConnectionState(PortForwarder::State::ConnectedSupported);
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), VpnState::PortForwardState::Attempting);
        pBindReply2->queueFinished();
        QVERIFY(resultSpy.wait());
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), MockPort);
    }

    // Toggling the feature off and on while connected does *not* change the
    // forwarded port or make another request
    void testConnectedToggle()
    {
        TestPortForwarder forwarder;

        QSignalSpy resultSpy(&forwarder, &PortForwarder::portForwardUpdated);

        enum { MockPort = 59905 };

        auto pReply = MockNetworkManager::enqueueReply(buildSuccessResponse(MockPort));

        forwarder.enablePortForwarding(true);
        forwarder.updateConnectionState(PortForwarder::State::ConnectedSupported);
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), VpnState::PortForwardState::Attempting);

        QSignalSpy consumeSpy{&MockNetworkManager::_replyConsumed, &ReplyConsumedSignal::signal};
        QVERIFY(consumeSpy.wait(1000));
        QVERIFY(!MockNetworkManager::hasNextReply());
        auto pBindReply = MockNetworkManager::enqueueReply(bindSuccess);
        consumeSpy.clear();
        pReply->queueFinished();
        QVERIFY(consumeSpy.wait(1000));
        QVERIFY(!MockNetworkManager::hasNextReply());
        pBindReply->queueFinished();
        QVERIFY(resultSpy.wait());
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), MockPort);

        // This reply will *not* be consumed
        auto pExtraReply = MockNetworkManager::enqueueReply(buildSuccessResponse(MockPort));
        auto replyCleanup = raii_sentinel([]{MockNetworkManager::clearQueuedReplies();});

        forwarder.enablePortForwarding(false);
        // No change from this
        QVERIFY(!resultSpy.wait(100));
        QVERIFY(MockNetworkManager::hasNextReply()); // Not consumed

        forwarder.enablePortForwarding(true);
        // Again, no change
        QVERIFY(!resultSpy.wait(100));
        QVERIFY(MockNetworkManager::hasNextReply()); // Not consumed
    }
};

QTEST_GUILESS_MAIN(tst_portforwarder)
#include TEST_MOC
