// Copyright (c) 2019 London Trust Media Incorporated
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
#include "testshim.h"
#include "src/mocknetwork.h"
#include <QtTest>
#include <QNetworkReply>

namespace Responses
{

const int successPort = 55947;
const QByteArray success{R"(
{
    "port": 55947
}
)"};

// Success again, different port
const int successPort2 = 53511;
const QByteArray success2{R"(
{
    "port": 53511
}
)"};

const QByteArray error{R"(
{
    "error": "Mock unit test error"
}
)"};

// Completely invalid JSON
const QByteArray invalidJson{R"(fhqwgads)"};
// No port attribute
const QByteArray missingPort{R"({})"};
// Port is 0
const QByteArray zeroPort{R"(
{
    "port": 0
}
)"};
// Port is a string, not an integer
const QByteArray invalidPort{R"(
{
    "port": "harbor"
}
)"};
}

// PortForwarder and PortRequester with defaulted parameters, so we don't have
// to repeat this in every test
class TestPortForwarder : public PortForwarder
{
public:
    TestPortForwarder()
        : PortForwarder{nullptr, QStringLiteral("00000000000000000000000000000000000000000000000000")}
    {}
};

class TestPortRequester : public PortRequester
{
public:
    TestPortRequester()
        : PortRequester{QStringLiteral("http://209.222.18.222:2000/?client_id=00000000000000000000000000000000000000000000000000")}
    {}
};

class tst_portforwarder : public QObject
{
    Q_OBJECT

private:
    using IdBits = ClientId::IdBits;

    void checkId(const QString &name, const IdBits &idNum, const QString &expected)
    {
        ClientId id{idNum};
        qInfo() << name << "-" << id.id();
        QCOMPARE(id.id(), expected);
    }

    // Test that a particular type of error fails and retries as expected
    void testErrorRetry(const QByteArray &errorJson)
    {
        auto pErrReply = MockNetworkManager::enqueueReply(errorJson);
        TestPortRequester requester;
        QCOMPARE(MockNetworkManager::hasNextReply(), false);

        auto pSuccessReply = MockNetworkManager::enqueueReply(Responses::success);

        QSignalSpy resultSpy{&requester, &PortRequester::portForwardComplete};

        pErrReply->queueFinished();
        // This wait *should* time out, we do not expect a response yet
        QVERIFY(!resultSpy.wait(1000));

        // The error reply should have been destroyed
        QCOMPARE(pErrReply, nullptr);
        // It should have taken the success reply by now
        QCOMPARE(MockNetworkManager::hasNextReply(), false);

        pSuccessReply->queueFinished();
        QVERIFY(resultSpy.wait());

        QVERIFY(resultSpy.size() == 1);
        QCOMPARE(resultSpy[0][0].value<int>(), Responses::successPort);
    }

private slots:
    void init()
    {
        TestShim::installMock<QNetworkAccessManager, MockNetworkManager>();
    }

    // Test a few ClientId values to ensure that the encoding works correctly
    // Nontrivial expected results were computed with Wolfram Alpha
    void testClientIds()
    {
        checkId("Zero", {},
                "00000000000000000000000000000000000000000000000000");

        checkId("Ten", {{0, 0, 0, 0, 0, 0, 0, 10}},
                "0000000000000000000000000000000000000000000000000a");

        checkId("36", {{0, 0, 0, 0, 0, 0, 0, 36}},
                "00000000000000000000000000000000000000000000000010");

        checkId("Carry-1", {{0, 0, 0, 0, 0, 0, 1, 0}},
                "00000000000000000000000000000000000000000001z141z4");

        // Arbitrary value, SHA256("pia")
        checkId("Arbitrary", {{0xFA9918F9, 0x94901C08, 0x7ECA14CA, 0x42E8A54C,
                               0xFF9B4D8E, 0x04DC0593, 0xAEE58A9F, 0x6833F772}},
                "68uo21fd3panj4on3hjs8zxy6opt52uvte1z9d5bdab05x8sj6");

        checkId("Max-256", {{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                             0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}},
                "6dp5qcb22im238nr3wvp0ic7q99w035jmy2iw7i6n43d37jtof");
    }

    // Test a successful port forward with PortRequester.
    void testSuccessfulForward()
    {
        auto pReply = MockNetworkManager::enqueueReply(Responses::success);
        TestPortRequester requester;
        QCOMPARE(MockNetworkManager::hasNextReply(), false);

        QSignalSpy resultSpy{&requester, &PortRequester::portForwardComplete};

        pReply->queueFinished();
        QVERIFY(resultSpy.wait());

        QVERIFY(resultSpy.size() == 1);
        QCOMPARE(resultSpy[0][0].value<int>(), Responses::successPort);
    }

    // Test an error returned in the reply JSON
    void testReplyError()
    {
        QTest::ignoreMessage(QtWarningMsg, "Port could not be forwarded due to error: QJsonValue(string, \"Mock unit test error\")");
        testErrorRetry(Responses::error);
    }

    // Test invalid JSON
    void testInvalidJson()
    {
        QTest::ignoreMessage(QtWarningMsg, "Couldn't read port forward reply due to error: 5 at position 1");
        QTest::ignoreMessage(QtWarningMsg, "Retrieved JSON: \"fhqwgads\"");
        testErrorRetry(Responses::invalidJson);
    }

    // Test a missing port value in the reply
    void testMissingPort()
    {
        QTest::ignoreMessage(QtCriticalMsg, "Invalid port value from port forward request: 0");
        QTest::ignoreMessage(QtCriticalMsg, "Received JSON: \"{}\"");
        testErrorRetry(Responses::missingPort);
    }

    // Test a zero port value in the reply
    void testZeroPort()
    {
        QTest::ignoreMessage(QtCriticalMsg, "Invalid port value from port forward request: 0");
        QTest::ignoreMessage(QtCriticalMsg, "Received JSON: \"\\n{\\n    \\\"port\\\": 0\\n}\\n\"");
        testErrorRetry(Responses::zeroPort);
    }

    // Test an invalid value for port
    void testInvalidPort()
    {
        QTest::ignoreMessage(QtCriticalMsg, "Invalid port value from port forward request: 0");
        QTest::ignoreMessage(QtCriticalMsg, "Received JSON: \"\\n{\\n    \\\"port\\\": \\\"harbor\\\"\\n}\\n\"");
        testErrorRetry(Responses::invalidPort);
    }

    // Test repeated failures to exhaust all retries
    void testFailAllRetries()
    {
        QTest::ignoreMessage(QtWarningMsg, "Port could not be forwarded due to error: QJsonValue(string, \"Mock unit test error\")");
        QTest::ignoreMessage(QtWarningMsg, "Port could not be forwarded due to error: QJsonValue(string, \"Mock unit test error\")");
        QTest::ignoreMessage(QtWarningMsg, "Port could not be forwarded due to error: QJsonValue(string, \"Mock unit test error\")");
        QTest::ignoreMessage(QtWarningMsg, "Port could not be forwarded due to error: QJsonValue(string, \"Mock unit test error\")");
        QTest::ignoreMessage(QtWarningMsg, "Port forward request failed 4 attempts, giving up");
        auto pErr1 = MockNetworkManager::enqueueReply(Responses::error);
        TestPortRequester requester;

        QSignalSpy resultSpy{&requester, &PortRequester::portForwardComplete};

        auto pErr2 = MockNetworkManager::enqueueReply(Responses::error);
        pErr1->finished();
        auto pErr3 = MockNetworkManager::enqueueReply(Responses::error);
        pErr2->finished();
        auto pErr4 = MockNetworkManager::enqueueReply(Responses::error);
        pErr3->finished();
        // At this point there should not have been any signals yet, we're still
        // retrying
        QVERIFY(!resultSpy.wait(1000));
        QVERIFY(resultSpy.empty());

        pErr4->queueFinished();
        QVERIFY(resultSpy.wait());
        QVERIFY(resultSpy.size() == 1);
        QCOMPARE(resultSpy[0][0].value<int>(), 0);
    }

    // Test that timeouts back off correctly for each request
    void testTimeoutBackoff()
    {
        // Enums are traced differently in Qt 5.11 / 5.12, so use a regex for
        // those traces.
        QTest::ignoreMessage(QtWarningMsg, "Request 1 for port forward timed out after 5 seconds");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression{"Couldn't request port forward due to error: .*OperationCanceledError.*"});
        QTest::ignoreMessage(QtWarningMsg, "Request 2 for port forward timed out after 10 seconds");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression{"Couldn't request port forward due to error: .*OperationCanceledError.*"});
        QTest::ignoreMessage(QtWarningMsg, "Request 3 for port forward timed out after 15 seconds");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression{"Couldn't request port forward due to error: .*OperationCanceledError.*"});
        QTest::ignoreMessage(QtWarningMsg, "Request 4 for port forward timed out after 20 seconds");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression{"Couldn't request port forward due to error: .*OperationCanceledError.*"});
        QTest::ignoreMessage(QtWarningMsg, "Port forward request failed 4 attempts, giving up");
        auto pErr1 = MockNetworkManager::enqueueReply();
        TestPortRequester requester;

        QSignalSpy resultSpy{&requester, &PortRequester::portForwardComplete};
        QSignalSpy consumeSpy{&MockNetworkManager::_replyConsumed, &ReplyConsumedSignal::signal};

        // Note that due to the way QVERIFY is implemented, a failed QVERIFY
        // inside this lambda will only abort the lambda, not the entire test
        // case.
        auto checkTimeout = [&](int timeoutMsec,
                                const auto &pPriorReply)
        {
            consumeSpy.clear();
            // Not consumed yet
            QVERIFY(!consumeSpy.wait(timeoutMsec-1000));
            // The next reply should be consumed when the timeout elapses
            QVERIFY(consumeSpy.wait(2000));
            // The reply should be destroyed after that.
            // This happens with deleteLater(), so it occurs after the signal
            // is emitted
            QSignalSpy destroySpy{pPriorReply, &QObject::destroyed};
            QVERIFY(destroySpy.wait(1));
            // No signal yet
            QVERIFY(resultSpy.empty());
        };

        // The first timeout should be 5 seconds.
        auto pErr2 = MockNetworkManager::enqueueReply();
        checkTimeout(5000, pErr1);

        // The next timeout should be 10 seconds.
        auto pErr3 = MockNetworkManager::enqueueReply();
        checkTimeout(10000, pErr2);

        // Then, 15 seconds
        auto pErr4 = MockNetworkManager::enqueueReply();
        checkTimeout(15000, pErr3);

        // Then, 20 seconds, but in this case the signal should be emitted since
        // this is the last attempt, and we don't queue up a new reply.
        QSignalSpy lastDestroy{pErr4, &QObject::destroyed};
        QVERIFY(!lastDestroy.wait(19000));
        QVERIFY(resultSpy.empty()); // No signals yet
        // Next, the result should be signaled after ~1000 ms
        QVERIFY(resultSpy.wait(2000));
        // The last result should be destroyed
        QVERIFY(lastDestroy.wait());

        QCOMPARE(resultSpy[0][0].value<int>(), 0);
    }

    // Verify that PortForwarder requests a port when the feature is enabled,
    // then the VPN is connected
    void testEnabledConnect()
    {
        TestPortForwarder forwarder;

        QSignalSpy resultSpy(&forwarder, &PortForwarder::portForwardUpdated);

        auto pReply = MockNetworkManager::enqueueReply(Responses::success);

        forwarder.enablePortForwarding(true);
        QVERIFY(MockNetworkManager::hasNextReply()); // Not requested yet

        forwarder.updateConnectionState(PortForwarder::State::ConnectedSupported);
        // 'Attempting' is emitted synchronously
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), DaemonState::PortForwardState::Attempting);
        QVERIFY(!MockNetworkManager::hasNextReply());
        pReply->queueFinished();
        QVERIFY(resultSpy.wait());
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), Responses::successPort);
    }

    // Verify that PortForwarder requests a port when the connection is
    // established, then the feature is enabled but no port request takes place
    // Instead, we signal port forwarding is Inative and a reconnection is required to request a port
    void testConnectedEnable()
    {
        TestPortForwarder forwarder;

        QSignalSpy resultSpy(&forwarder, &PortForwarder::portForwardUpdated);

        auto pReply = MockNetworkManager::enqueueReply(Responses::success);

        forwarder.updateConnectionState(PortForwarder::State::ConnectedSupported);
        QVERIFY(MockNetworkManager::hasNextReply()); // Not requested yet

        forwarder.enablePortForwarding(true);
        auto arguments { resultSpy.takeFirst() };
        QCOMPARE(arguments[0].value<int>(), DaemonState::PortForwardState::Inactive);
        QCOMPARE(arguments[1].value<bool>(), true);  // needsReconnect is set to true
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
        auto pReply = MockNetworkManager::enqueueReply(Responses::success);
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
            QCOMPARE(emittedSignal[0].value<int>(), DaemonState::PortForwardState::Inactive);
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
        auto pSuccess1 = MockNetworkManager::enqueueReply(Responses::success);

        forwarder.enablePortForwarding(true);
        forwarder.updateConnectionState(PortForwarder::State::ConnectedSupported);
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), DaemonState::PortForwardState::Attempting);
        pSuccess1->queueFinished();
        QVERIFY(resultSpy.wait());
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), Responses::successPort);

        forwarder.updateConnectionState(PortForwarder::State::Disconnected);
        // This signal occurs synchronously, so there's no wait needed here
        QVERIFY(!resultSpy.empty());
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), DaemonState::PortForwardState::Inactive);

        auto pSuccess2 = MockNetworkManager::enqueueReply(Responses::success2);
        forwarder.updateConnectionState(PortForwarder::State::ConnectedSupported);
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), DaemonState::PortForwardState::Attempting);
        pSuccess2->queueFinished();
        QVERIFY(resultSpy.wait());
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), Responses::successPort2);
    }

    // Toggling the feature off and on while connected does *not* change the
    // forwarded port or make another request
    void testConnectedToggle()
    {
        TestPortForwarder forwarder;

        QSignalSpy resultSpy(&forwarder, &PortForwarder::portForwardUpdated);
        auto pReply = MockNetworkManager::enqueueReply(Responses::success);

        forwarder.enablePortForwarding(true);
        forwarder.updateConnectionState(PortForwarder::State::ConnectedSupported);
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), DaemonState::PortForwardState::Attempting);
        pReply->queueFinished();
        QVERIFY(resultSpy.wait());
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), Responses::successPort);

        // This reply will *not* be consumed
        auto pExtraReply = MockNetworkManager::enqueueReply(Responses::success2);
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
