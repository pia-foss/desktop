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

#include "common.h"
#line HEADER_FILE("ipc.h")

#ifndef IPC_H
#define IPC_H
#pragma once

#include "thread.h"
#include <QByteArray>
#include <QSet>
#include <QString>

class COMMON_EXPORT IPCConnection;

/**
 * @brief The IPCServer class handles the server side of a UTF-8 message
 * based IPC protocol.
 */
class COMMON_EXPORT IPCServer : public QObject
{
    Q_OBJECT
public:
    IPCServer(QObject* parent) : QObject(parent) {}

    virtual bool listen() = 0;
    virtual void stop() = 0;

    int count() { return _connections.count(); }

public slots:
    void sendMessageToAllClients(const QByteArray &msg);

signals:
    void newConnection(IPCConnection *connection);

protected:
    QSet<IPCConnection*> _connections;
};

/**
 * @brief The IPCConnection class handles a connection of a UTF-8 message
 * based IPC protocol.
 */
class COMMON_EXPORT IPCConnection : public QObject
{
    Q_OBJECT
public:
    IPCConnection(QObject *parent) : QObject(parent) {}

    virtual bool isConnected() = 0;
    virtual bool isError() { return false; }
    // Set the threshold used to trigger the remoteLagging() signal.  Tracing
    // starts at half this threshold.  (Used in unit tests.)
    virtual void setLagThreshold(int threshold) = 0;
public slots:
    virtual void sendMessage(const QByteArray &msg) = 0;
    virtual void close() = 0;

signals:
    void connected(qintptr socketFd);
    void messageReceived(const QByteArray &msg);
    void disconnected();
    void error(const QString& errorString);
    // A message queued for sending with sendMessage() could not be sent.
    void messageError(const Error &error, const QByteArray &msg);
    // The remote party is not acknowledging messages (emitted when a message is
    // sent if there are 9 prior unacknowledged messages)
    void remoteLagging();
};

// IPC connection for use in clients.  In addition to IPCConnection, has
// connectToServer() to establish the connection.
class COMMON_EXPORT ClientIPCConnection : public IPCConnection
{
    Q_OBJECT
public:
    using IPCConnection::IPCConnection;

    // Connect to the server endpoint.
    virtual void connectToServer() = 0;
    // Connect using a socket that was previously opened.
    virtual void connectToSocketFd(qintptr socketFd) = 0;

#ifdef UNIT_TEST
    virtual void sendRawMessage(const QByteArray& msg) = 0;
#endif
};


// Implementation using QLocalServer / QLocalSocket ////////////////////////////

class COMMON_EXPORT LocalSocketIPCServer : public IPCServer
{
    Q_OBJECT
public:
    LocalSocketIPCServer(QObject* parent = nullptr);

    virtual bool listen() override;
    virtual void stop() override;

private:
    class QLocalServer* _server;

    friend class LocalSocketIPCConnection;
};

class COMMON_EXPORT LocalSocketIPCConnection : public ClientIPCConnection
{
    Q_OBJECT

public:
    // Just serialize a frame into a raw buffer.  This can be a message frame
    // (non-empty data) or an acknowledgement frame (empty data).
    static void writeFrame(quint16 sequence, const QByteArray &data,
                           QDataStream &stream);

private:
    // Wrap around an existing socket
    LocalSocketIPCConnection(class QLocalSocket* socket, QObject *parent = nullptr);
public:
    // Create a connection wrapper around an unconnected socket
    LocalSocketIPCConnection(QObject *parent = nullptr);

    // Connect to the server
    virtual void connectToServer() override;
    virtual void connectToSocketFd(qintptr socketFd) override;

    virtual bool isConnected() override;
    virtual bool isError() override;
    virtual void setLagThreshold(int threshold) override;

#ifdef UNIT_TEST
    virtual void sendRawMessage(const QByteArray& msg) override;
#endif

protected slots:
    void onReadReady();

private:
    int getUnackedCount() const;
    void sendFrame(quint16 sequence, const QByteArray &payload);

public slots:
    virtual void sendMessage(const QByteArray &msg) override;
    virtual void close() override;

private:
    class QLocalSocket* _socket;
    // Payload currently being recieved.  This is sized to the expected size
    // when a header is completed.  If it has size 0, we're receiving the header
    // (which is left in the socket buffer until it is complete).
    QByteArray _payload;
    // Bytes received so far in _payload
    int _payloadReceived;
    int _lagThreshold;
    // The sequence of the payload currently being received
    quint16 _payloadSequence;
    // Sequence that was last sent - incremented when we send a message
    quint16 _lastSendSequence;
    // The last sequence that was acknowledged from the remote side
    quint16 _acknowledgedSequence;
    bool _error;

    friend class LocalSocketIPCServer;
};

Q_DECLARE_METATYPE(qintptr);

// ThreadedLocalIPCConnection is an IPC client connection using a local socket
// on a worker thread, which avoids blocking the main thread.
// This is particularly important on Windows, where QLocalSocket::connect()
// blocks if the named pipe does not yet exist.
//
// This object is also a ClientIPCConnection, it decorates a
// LocalSocketIPCConnection.
class COMMON_EXPORT ThreadedLocalIPCConnection : public ClientIPCConnection
{
    Q_OBJECT

public:
    ThreadedLocalIPCConnection(QObject *pParent);

private:
    void onConnected(qintptr socketFd);
    void onMessageReceived(const QByteArray &msg);
    void onDisconnected();
    void onError(const QString &errorString);

public:
    virtual void connectToServer() override;
    virtual void connectToSocketFd(qintptr socketFd) override;
    virtual bool isConnected() override;
    virtual bool isError() override;
    // The lag threshold is applied asynchronously, but it is serialized with
    // calls to sendMessage().
    virtual void setLagThreshold(int threshold) override;
    virtual void sendMessage(const QByteArray &msg) override;
    virtual void close() override;

#ifdef UNIT_TEST
    virtual void sendRawMessage(const QByteArray& msg) override;
#endif

public:
    // The actual LocalSocketIPCConnection runs on this thread.
    RunningWorkerThread _socketThread;
    // This is the actual connection implementation.  It's always valid.
    // The object is parented to _socketThread's object owner so it will be
    // destroyed on the worker thread correctly.  Since RunningWorkerThread's
    // destructor destroys the thread and waits for it to terminate, this will
    // be destroyed when ThreadedLocalIPCConnection is destroyed.
    ClientIPCConnection *_pConnection;
    // The connection and error states are stored on the main thread to avoid
    // blocking calls over to the worker thread to check them.
    bool _connected;
    bool _error;
};

#endif // IPC_H
