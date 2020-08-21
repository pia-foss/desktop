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
class TestPortForwarder : public DaemonState, public Environment,
    public ApiClient, public DaemonAccount,  public PortForwarder
{
public:
    TestPortForwarder()
        : Environment{static_cast<DaemonState&>(*this)},
          PortForwarder{*this, *this, *this, *this}
    {
        clientId(QStringLiteral("00000000000000000000000000000000000000000000000000"));
    }
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
        // The response is consumed asynchronously
        QSignalSpy consumeSpy{&MockNetworkManager::_replyConsumed, &ReplyConsumedSignal::signal};
        QVERIFY(consumeSpy.wait(1000));
        QVERIFY(!MockNetworkManager::hasNextReply());
        pReply->queueFinished();
        QVERIFY(resultSpy.wait());
        QCOMPARE(resultSpy.takeFirst()[0].value<int>(), Responses::successPort);
    }

    // Verify that if PF is enabled after having connected, PortForwarder does
    // _not_ begin a request.  A reconnect is required to start a PF request.
    void testConnectedEnable()
    {
        TestPortForwarder forwarder;

        QSignalSpy resultSpy(&forwarder, &PortForwarder::portForwardUpdated);

        auto pReply = MockNetworkManager::enqueueReply(Responses::success);

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
