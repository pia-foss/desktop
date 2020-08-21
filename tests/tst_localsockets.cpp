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

#include "common.h"
#include <QtTest>
#include <QSignalSpy>

#include "ipc.h"

#include <functional>

using namespace std::placeholders;

class tst_localsockets : public QObject
{
    Q_OBJECT

public:
    using ClientFactoryFunc = ClientIPCConnection*(*)(QObject*);

    tst_localsockets(ClientFactoryFunc pClientFactory)
        : _pClientFactory{pClientFactory}
    {
    }

private:
    ClientFactoryFunc _pClientFactory;
    QPointer<LocalSocketIPCServer> _server = nullptr;
    QPointer<ClientIPCConnection> _connection = nullptr; // client end
    QPointer<IPCConnection> _serverClientConnection = nullptr;  // Server's connection to client

    void setupServer(std::function<void(const QByteArray& msg, IPCConnection* connection)> messageHandler)
    {
        _server = new LocalSocketIPCServer(this);
        connect(_server, &LocalSocketIPCServer::newConnection, this, [this, messageHandler](IPCConnection* connection) {
            _serverClientConnection = connection;
            connect(connection, &IPCConnection::messageReceived, this, std::bind(messageHandler, _1, connection));
        });
    }
    void setupClient(std::function<void(const QByteArray& msg)> messageHandler)
    {
        _connection = _pClientFactory(this);
        connect(_connection, &ClientIPCConnection::messageReceived, this, std::move(messageHandler));
    }
    bool setupServerClientConnection(
            std::function<void(const QByteArray& msg, IPCConnection* connection)> serverMessageHandler,
            std::function<void(const QByteArray& msg)> clientMessageHandler)
    {
        setupServer(std::move(serverMessageHandler));
        setupClient(std::move(clientMessageHandler));
        if (!_server->listen())
            return false;
        _connection->connectToServer();
        return QTest::qWaitFor([this]() { return _server->count() && (_connection->isConnected() || _connection->isError()); });
    }
    bool error()
    {
        return false;
    }

    // Wait for a spy to be signaled, unless it's already signaled.
    // (This works when the spy could be signaled either synchronously or
    // asynchronously, since spy.wait() only returns true if the spy was
    // signaled asynchronously.)
    bool spyWait(QSignalSpy &spy)
    {
        return !spy.isEmpty() || spy.wait();
    }

private slots:
    void cleanup()
    {
        if (_connection)
        {
            _connection->disconnect(this);
            _connection->close();
            _connection->deleteLater();
            _connection = nullptr;
        }
        if (_server)
        {
            _server->disconnect(this);
            _server->stop();
            _server->deleteLater();
            _server = nullptr;
        }
    }

    // Just connect a client to a server and verify the correct signals
    // get sent out.
    void simpleConnection()
    {
        _server = new LocalSocketIPCServer(this);
        QSignalSpy spyServerClientConnected(_server, &LocalSocketIPCServer::newConnection);
        QVERIFY(_server->listen());

        _connection = _pClientFactory(this);
        QSignalSpy spyClientConnected(_connection, &ClientIPCConnection::connected);
        QSignalSpy spyClientError(_connection, &ClientIPCConnection::error);
        _connection->connectToServer();

        // Wait for signals
        QVERIFY2(QTest::qWaitFor([&]() { return spyServerClientConnected.count() && (spyClientConnected.count() || spyClientError.count()); }), "connection timeout");
        QCOMPARE(spyClientConnected.count(), 1);
        QCOMPARE(spyClientError.count(), 0);
        QCOMPARE(spyServerClientConnected.count(), 1);
        QVERIFY(_server->count() == 1);
        QVERIFY(_connection->isConnected());
        QVERIFY(!_connection->isError());
    }

    // Set up a client-server connection and have the client send a
    // sequence of increasingly longer messages to the server, which
    // in turn replies by sending the exact same messages back.
    void pingPong()
    {
        QVector<QByteArray> sentMessages, receivedMessages;

        QVERIFY2(setupServerClientConnection([](const QByteArray& msg, IPCConnection* connection) {
            connection->sendMessage(msg);
        }, [&](const QByteArray& msg) {
            receivedMessages.append(msg);
        }), "failed to setup client-server connection");

        // Raise lag thresholds - for the non-threaded test, the remote side
        // can't send acks until we free up the event loop
        _connection->setLagThreshold(500);
        _serverClientConnection->setLagThreshold(500);

        // Send messages
        for (int i = 0; i < 200; i++)
        {
            QByteArray msg = QString(2 + i * 100, ' ').toUtf8();
            _connection->sendMessage(msg);
            sentMessages.append(msg);
        }

        // Wait for all replies, or an error
        // When testing the threaded client in CI builds, this occasionally
        // takes 5-10 seconds because the machine is loaded down with the
        // ongoing build.  Running the build-windows script on a 1-CPU VM
        // reproduces this, but the same test run outside of a build takes the
        // normal 0.5-1 seconds.  It just looks like poor thread scheduling when
        // the machine is loaded down.
        QVERIFY2(QTest::qWaitFor([&]() { return receivedMessages.count() == sentMessages.count() || _connection->isError(); },
                                 60000),
                 "timed out waiting for responses");
        QVERIFY(!_connection->isError());
        QCOMPARE(receivedMessages, sentMessages);
    }

    // Send a mix of valid and invalid messages and verify that all the valid
    // messages are properly received.
    void garbageRecovery()
    {
        QVector<int> receivedMessages, validMessages;

        QVERIFY2(setupServerClientConnection([&](const QByteArray& msg, IPCConnection* connection) {
            receivedMessages.append(msg[0]);
        }, [&](const QByteArray& msg) {
            // empty
        }), "failed to setup client-server connection");

        // Since we send garbage sequence numbers for this test, set the lag
        // threshold very high to effectively disable lag detection.
        _connection->setLagThreshold(0x20000);
        _serverClientConnection->setLagThreshold(0x20000);

        // Send 9 messages in valid-invalid-valid order, checking that
        // all the valid ones get through intact. Send a valid message
        // at the end so we notice errors more quickly.
        for (int i = 0; i < 9; i++)
        {
            QByteArray msg;
            {
                QDataStream stream(&msg, QIODevice::WriteOnly);
                LocalSocketIPCConnection::writeFrame(0xFFFFu, QByteArray(4, i), stream);
            }
            switch (i % 8)
            {
            case 1: // corrupt magic tag
                QTest::ignoreMessage(QtWarningMsg, "Invalid message: missing or incorrect magic tag: \"ffacce66\"");
                msg[3] = 0x66;
                break;
            case 3: // truncate message
                QTest::ignoreMessage(QtWarningMsg, "Invalid message: truncated message");
                msg.chop(1);
                break;
            case 5: // corrupt payload with 0xFF (truncates message and makes the rest of message look like garbage)
                QTest::ignoreMessage(QtWarningMsg, "Invalid message: truncated message");
                QTest::ignoreMessage(QtWarningMsg, "Invalid message: missing or incorrect magic tag: \"ff0505ff\"");
                msg[13] = 0xFF;
                break;
            case 7: // corrupt size
                QTest::ignoreMessage(QtWarningMsg, QRegularExpression{R"(Invalid message: payload too large: \d+)"});
                msg[10] = 100;
                break;
            default:
                validMessages.append(i);
                break;
            }
            _connection->sendRawMessage(msg);
        }

        QVERIFY2(QTest::qWaitFor([&]() { return receivedMessages.count() == validMessages.count() || _connection->isError(); }), "timed out waiting for responses");
        QVERIFY(!_connection->isError());
        QTest::qWait(100); // Wait a little extra and re-check in case server receives more messages
        QCOMPARE(receivedMessages.count(), validMessages.count());
        QCOMPARE(receivedMessages, validMessages);
    }

    // Verify that sending a message when not connected emits a message error
    void notConnectedError()
    {
        _connection = _pClientFactory(this);

        QSignalSpy spyClientMessageError{_connection, &ClientIPCConnection::messageError};
        QSignalSpy spyClientError{_connection, &ClientIPCConnection::error};

        _connection->sendMessage(QByteArrayLiteral("not-connected-msg"));

        QVERIFY(spyWait(spyClientMessageError));
        // No general errors are emitted
        QVERIFY(spyClientError.isEmpty());
    }

    // Verify that sending a message after disconnecting emits a message error
    void afterDisconnectedError()
    {
        setupServerClientConnection({}, {});
        QSignalSpy spyClientError{_connection, &ClientIPCConnection::error};
        QSignalSpy spyClientMessageError{_connection, &ClientIPCConnection::messageError};

        // Disconnect the client
        QSignalSpy spyClientDisconnected{_connection, &ClientIPCConnection::disconnected};
        _connection->close();
        QVERIFY(spyWait(spyClientDisconnected));

        // Try to send a message and verify that it generates a message error
        _connection->sendMessage(QByteArrayLiteral("after-disconnect-msg"));
        QVERIFY(spyWait(spyClientMessageError));
        // No general errors
        QVERIFY(spyClientError.isEmpty());
    }

    // Verify that sending a message after the connection is lost emits a message error
    void afterConnLostError()
    {
        setupServerClientConnection({}, {});
        QSignalSpy spyClientError{_connection, &ClientIPCConnection::error};
        QSignalSpy spyClientDisconnected{_connection, &ClientIPCConnection::disconnected};
        QSignalSpy spyClientMessageError{_connection, &ClientIPCConnection::messageError};

        // Close the connection from the server
        QVERIFY(_serverClientConnection);
        _serverClientConnection->close();
        // On Mac/Linux this results in an error, on Windows it emits disconnected
        QVERIFY(spyWait(spyClientDisconnected) || spyWait(spyClientError));

        // Disconnection generates an error on Mac/Linux only, just verify that
        // the message doesn't generate any additional errors
        auto disconnectErrCount = spyClientError.size();

        // Try to send a message and verify that it generates a message error
        _connection->sendMessage(QByteArrayLiteral("after-conn-lost-msg"));
        QVERIFY(spyWait(spyClientMessageError));
        // No additional general errors
        QVERIFY(spyClientError.size() == disconnectErrCount);
    }
};

template<class Connection_t>
ClientIPCConnection *connectionFactory(QObject *pParent)
{
    return new Connection_t{pParent};
}

int main(int argc, char *argv[])
{
    QCoreApplication app{argc, argv};

    int failures = 0;

    {
        tst_localsockets syncTest{&connectionFactory<LocalSocketIPCConnection>};
        failures += QTest::qExec(&syncTest, argc, argv);
    }
    {
        tst_localsockets threadedTest{&connectionFactory<ThreadedLocalIPCConnection>};
        failures += QTest::qExec(&threadedTest, argc, argv);
    }

    return failures;
}

#include TEST_MOC
