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

#include <common/src/common.h>
#include <QtTest>

#include <common/src/async.h>
#include <common/src/jsonrpc.h>

#include <QJsonObject>


class tst_jsonrpc : public QObject
{
    Q_OBJECT

    QString concat(QString a, QString b) { return a + b; }
    double multiply(double a, double b) { return a * b; }
    int increment(int a) { return a + 1; }
    int count() { return ++_count; }
    QString getTestName() { return QString::fromLatin1(QTest::currentTestFunction()); }

    int _count;

private slots:
    void init()
    {
        _count = 0;
    }

    void clientToServerNotification()
    {
        bool called = false;
        LocalMethodRegistry registry {
            { QStringLiteral("test"), [&]() { called = true; } },
        };
        LocalNotificationInterface server(&registry);
        RemoteNotificationInterface client;
        // Simulate a connection channel by connecting the client and server objects directly
        connect(&client, &RemoteNotificationInterface::messageReady, &server, &LocalNotificationInterface::processMessage);
        client.post(QStringLiteral("test"));
        QVERIFY(called);
    }
    void clientToServerCall()
    {
        bool called = false, responded = false;
        LocalMethodRegistry registry {
            { QStringLiteral("test"), [&](int param) { called = true; return param + 34; } },
        };
        LocalCallInterface server(&registry);
        RemoteCallInterface client;
        // Simulate a connection channel by connecting the client and server objects directly
        connect(&client, &RemoteCallInterface::messageReady, &server, &LocalCallInterface::processMessage);
        connect(&server, &LocalCallInterface::messageReady, &client, &RemoteCallInterface::processMessage);
        auto call = client.call(QStringLiteral("test"), 12);
        call->notify([&](const Error&, const QJsonValue&) { responded = true; });
        QTRY_VERIFY(responded);
        QVERIFY(called);
        QVERIFY(call->isResolved());
        QVERIFY(!call->isRejected());
        QCOMPARE(call->result(), 12 + 34);
    }

    // Test that a call() while disconnected is rejected
    void disconnectedCall()
    {
        RemoteCallInterface client;
        // Fail to send anything that the client tries to send
        connect(&client, &RemoteCallInterface::messageReady,
                this, [&](const QByteArray &msg){
            client.requestSendError({HERE, Error::Code::IPCNotConnected}, msg);
        });

        auto call = client.call(QStringLiteral("test"));
        QVERIFY(call->isRejected());
        QVERIFY(call->error().code() == Error::Code::IPCNotConnected);
    }

    // Test that losing the connection after attempting a call causes it to
    // reject
    void connLostCall()
    {
        RemoteCallInterface client;
        // Ignore anything that the client tries to send (as if it's being
        // processed until we lose the connection).  We still have to connect
        // something or RemoteCallInterface short-circuits.
        connect(&client, &RemoteCallInterface::messageReady,
                this, [](){});

        auto call = client.call(QStringLiteral("test"));
        QVERIFY(call->isPending());

        client.connectionLost();
        QVERIFY(call->isRejected());
        QVERIFY(call->error().code() == Error::Code::JsonRPCConnectionLost);
    }
};

QTEST_GUILESS_MAIN(tst_jsonrpc)
#include TEST_MOC
