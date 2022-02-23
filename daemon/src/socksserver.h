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

#include "common.h"
#line HEADER_FILE("socksserver.h")

#ifndef SOCKSSERVER_H
#define SOCKSSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

// SocksServer runs a minimal TCP SOCKS5 server that forwards connections
// through the VPN interface.  This is used to route QNetworkAccessManager-based
// requests through the VPN even when it is not used as the default gateway.
class SocksServer : public QObject
{
    Q_OBJECT

public:
    // Create SocksServer with the VPN IP address that it will bind to for
    // outgoing connections.  This must be a valid IPv4 address. Also provide he interface the socket will bind to.
    SocksServer(QHostAddress bindAddress, QString bindInterface);

public:
    // Get the port that the server is listening on.  If this returns 0, the
    // server failed to start.
    quint16 port() const {return _server.serverPort();}
    // Get the password to the proxy.  SocksServer generates a password to
    // ensure that only the daemon can connect to it.  The user name is always
    // SocksConnection::username.
    QByteArray password() const {return _password;}

    // Update the bind address - the new address must be a valid IPv4 address.
    void updateBindAddress(QHostAddress bindAddress, QString bindInterface);

private:
    void onNewConnection();

private:
    QHostAddress _bindAddress;
    QString _bindInterface;
    QTcpServer _server;
    QByteArray _password;
    QByteArray _passwordHash;
};

// SocksConnection handles a single connection established to the SocksServer.
class SocksConnection : public QObject
{
    Q_OBJECT

public:
    static const QByteArray username;

public:
    // Lifecycle of a SOCKS5 connection
    enum class State
    {
        // Receive the VER and NMETHODS bytes of the client greeting.
        // RFC1928 documents this and the methods themselves as one greeting
        // message, but we treat them separately since this part specifies the
        // length of the next part.
        ReceiveAuthMethodsHeader,
        // Receive the variable-length method list.  The response is sent when
        // this completes.
        ReceiveAuthMethods,
        // Receive the auth username header (version and name length)
        ReceiveAuthUsernameHeader,
        // Recieve the auth username
        ReceiveAuthUsername,
        // Receive the auth password header (password length)
        ReceiveAuthPasswordHeader,
        // Receive the auth password
        ReceiveAuthPassword,
        // Receive the TCP connect request header - everything up to the address
        // type, which determines the length of the rest of the message.
        ReceiveConnectHeader,
        // Receive the rest of the connect request, for either IPv4 or IPv6
        ReceiveConnectIPv4,
        ReceiveConnectIPv6,
        // We're connecting the outgoing socket; response is sent when this
        // completes.
        Connecting,
        // We are connected, relay data from both sides
        Connected,
        // Waiting for the SOCKS connection to disconnect.  Occurs if the target
        // disconnects after successfully connecting, or if we send a failure to
        // the SOCKS side without having connected.
        SocksDisconnecting,
        // Waiting for the target connection to disconnect.  Occurs if the SOCKS
        // connection disconnects after successfully connecting.
        TargetDisconnecting,
        // Once the connection closes, we queue deletion of the parent
        // QTcpSocket, which in turn destroys SocksConnection.
        Closed,
    };
    Q_ENUM(State);

public:
    // Create SocksConnection with a QTcpSocket accepted from the QTcpServer.
    //
    // SocksConnection is self owning - it ensures that it is destroyed if the
    // connection or server are closed.
    //
    // SocksConnection uses the QTcpSocket as its own parent, so it will be
    // destroyed if the SocksServer is destroyed (the QTcpSocket is parented to
    // the SocksServer's QTcpServer).
    //
    // SocksConnection also destroys the QTcpSocket (and consequently, itself)
    // if the connection is closed.
    SocksConnection(QTcpSocket &socksSocket, QByteArray passwordHash,
                    QHostAddress bindAddress, QString bindInterface);

private:
    // Close the TCP connection(s) immediately without sending any failure
    // response.  Used for protocol errors.  Goes to the Closed state and queues
    // deletion of the QTcpSocket parent (and this SocksConnection).
    void abortConnection();

    // Send a response.  If this fails, aborts the socket.
    void respond(const QByteArray &response);

    // Reject the SOCKS connection by sending a failure response, then
    // close the socket after a timeout.  (Goes to the SocksDisconnecting state, or
    // aborts if the response can't be sent.)
    void rejectConnection(const QByteArray &response);

    // Check the protocol version in a message - aborts if it is invalid
    // (returns false in that case).
    bool checkMessageVersion(const QByteArray &message, quint8 version,
                             const QString &traceName);
    bool checkSocksVersion(const QByteArray &message);
    bool checkUPAuthVersion(const QByteArray &message);

    // Forward all available data in both directions.  If any write fails, this
    // aborts the connection.
    void forwardData(QTcpSocket &source, QTcpSocket &dest,
                     const QString &directionTrace);
    // Process incoming data on the SOCKS connection (protocol messages or
    // application data).  Used by onSocksReadyRead().
    void processSocksData();

    void onSocksReadyRead();
    void onSocksError(QTcpSocket::SocketError socketError);
    void onSocksDisconnected();

    void onTargetConnected();
    void onTargetError(QTcpSocket::SocketError socketError);
    void onTargetReadyRead();
    void onTargetDisconnected();

private:
    // The incoming QTcpSocket is held by reference - since SocksConnection is
    // parented to it, this reference remains valid as long as SocksConnection
    // exists.
    QTcpSocket &_socksSocket;
    QByteArray _passwordHash;
    QHostAddress _bindAddress;
    QString _bindInterface;
    State _state;
    // In states other than Connecting and Connected, we set a 5-second timer
    // that will abort the connection.  This means that:
    // - The SOCKS client must complete the initial negotiation within 5 seconds
    // - If the connection fails, the client has 5 seconds to receive the
    //   response
    // - If either side disconnects, the other side has 5 seconds to recieve any
    //   remaining data and disconnect
    QTimer _abortTimer;
    // In Receive* states, the number of bytes in the next message.  In other
    // states, 0.
    qint64 _nextMessageBytes;
    QTcpSocket _targetSocket;
};

#endif
