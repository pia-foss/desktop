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
#line SOURCE_FILE("socksserver.cpp")

#include "socksserver.h"
#include "brand.h"
#include <QRandomGenerator>
#include <QCryptographicHash>
#include <QNetworkProxy>

// For SO_BINDTODEVICE
#ifdef Q_OS_LINUX
#include <sys/socket.h>
#endif

namespace
{
    // SOCKS protocol constants
    enum : quint8
    {
        SocksVersion = 5,
        // The auth negotiation has its own version
        UsernamePasswordAuthVersion = 1,
    };

    enum Method : quint8
    {
        NoAuth = 0,
        UsernamePassword = 2,
        NotAcceptable = 0xFF
    };

    enum AddressType : quint8
    {
        IPv4 = 1,
        IPv6 = 4,
    };

    enum Command : quint8
    {
        Connect = 1,
    };

    enum Reply : quint8
    {
        Succeeded,
        GeneralFailure,
        NotAllowed,
        NetUnreachable,
        HostUnreachable,
        ConnectionRefused,
        TtlExpired,
        CommandNotSupported,
        AddressTypeNotSupported,
    };

    // SOCKS5 protocol message formats / offsets.
    // Several of these have fields with the same name; anonymous enums in
    // namespaces avoid conflicts (we don't need to declare variables of these
    // types, they're just indexing offsets / sizes).
    namespace AuthMethodHeaderMsg
    {
        enum
        {
            Version,
            NMethods,
            Length
        };
    }
    namespace AuthMethodResponseMsg
    {
        enum
        {
            Version,
            Method,
            Length
        };
    }
    // The auth data is of the form:
    // | version | username-length | <data> | password-length | <data> |
    // This is broken into 4 parts to handle the variable-length data
    namespace AuthUsernameHeaderMsg
    {
        enum
        {
            Version,
            UsernameLength,
            Length
        };
    }
    // Then, username data
    namespace AuthPasswordHeaderMsg
    {
        enum
        {
            // No "version" here since it's really part of one "auth" message.
            PasswordLength,
            Length
        };
    }
    namespace AuthResponseMsg
    {
        enum
        {
            Version,
            Status,
            Length
        };
    }

    namespace ConnectHeaderMsg
    {
        enum
        {
            Version,
            Command,
            Reserved,
            AddrType,
            Length
        };
    }
    // The remaining part of the connect message (treated as a separate message
    // by SocksConnection) depends on the address type specified
    // TODO - May need to support IPv6 too
    /*namespace AddrLength
    {
        enum
        {
            IPv4 = 4,
            IPv6 = 16,
        };
    }*/
    namespace ConnectResponseMsg
    {
        enum
        {
            Version,
            Reply,
            Reserved,
            AddrType,
            // Followed by address and port (length depends on address type)
            Addr,
            Port = Addr + 4,
            Length = Port + 2,
        };
    }

    // Read an unsigned big-endian integer from a QByteArray offset.  This is
    // slightly tricky since QByteArray returns 'chars' as its element type,
    // which may be signed.
    template<class UInt_t>
    UInt_t readUnsignedBE(const QByteArray &data, unsigned offset)
    {
        Q_ASSERT(static_cast<unsigned>(data.size()) >= static_cast<unsigned>(offset) + sizeof(UInt_t)); // Checked by caller

        UInt_t result = 0;
        const char *begin = data.constData() + offset;
        const char *end = begin + sizeof(UInt_t);
        while(begin != end)
        {
            result <<= 8;
            result |= static_cast<unsigned char>(*begin);
            ++begin;
        }
        return result;
    }

    // Check two same-sized QByteArrays in constant time for equality
    bool checkHashEquals(const QByteArray &first, const QByteArray &second)
    {
        Q_ASSERT(first.size() == second.size());    // Ensured by caller

        // Use volatile data pointers and a volatile intermediate, this requires
        // the compiler to emit all memory accesses for these variables, which
        // ensures that the calculation takes place as written.
        volatile const char *pFirst = first.data();
        volatile const char *pSecond = second.data();
        volatile char accumulatedDifference = 0;

        for(int i=0; i<first.size(); ++i)
        {
            accumulatedDifference |= pFirst[i] ^ pSecond[i];
        }

        return !accumulatedDifference;
    }
}

// The username doesn't really matter, brand code is a sane value
const QByteArray SocksConnection::username = QByteArrayLiteral(BRAND_CODE);

SocksServer::SocksServer(QHostAddress bindAddress, QString bindInterface)
    : _bindAddress{std::move(bindAddress)},
      _bindInterface{bindInterface}
{
    Q_ASSERT(_bindAddress.protocol() == QAbstractSocket::NetworkLayerProtocol::IPv4Protocol);

    if(_server.listen(QHostAddress::SpecialAddress::LocalHost))
    {
        Q_ASSERT(_server.serverPort()); // Should have assigned a port if listen succeeded
        qInfo() << "Started API proxy on port" << _server.serverPort();
        connect(&_server, &QTcpServer::newConnection, this, &SocksServer::onNewConnection);

        // Generate a password.  The SocksServer port is reachable by any
        // application, but we only intend to use it from the daemon.
        quint64 passwordData = QRandomGenerator::global()->generate64();
        _password = QByteArray::fromRawData(reinterpret_cast<const char*>(&passwordData), sizeof(passwordData)).toHex();
        // Use the hash of the password for validation; see SocksConnection
        QCryptographicHash hash{QCryptographicHash::Algorithm::Sha256};
        hash.addData(_password);
        _passwordHash = hash.result();
    }
    else
    {
        Q_ASSERT(_server.serverPort() == 0);
        qWarning() << "Failed to start API proxy -" << _server.errorString();
    }
}

void SocksServer::updateBindAddress(QHostAddress bindAddress, QString bindInterface)
{
    // Checked by caller
    Q_ASSERT(bindAddress.protocol() == QAbstractSocket::NetworkLayerProtocol::IPv4Protocol);
    _bindAddress = std::move(bindAddress);
    _bindInterface = bindInterface;
}

void SocksServer::onNewConnection()
{
    while(auto pNewConnection = _server.nextPendingConnection())
    {
        // SocksConnection manages its own lifetime; it becomes parented to the
        // new QTcpSocket.
        new SocksConnection{*pNewConnection, _passwordHash, _bindAddress, _bindInterface};
    }
}

SocksConnection::SocksConnection(QTcpSocket &socksSocket,
                                 QByteArray passwordHash,
                                 const QHostAddress &bindAddress,
                                 QString bindInterface)
    : QObject{&socksSocket}, _socksSocket{socksSocket},
      _passwordHash{std::move(passwordHash)},
      _state{State::ReceiveAuthMethodsHeader}, _nextMessageBytes{2}
{
    // By default QTcpSocket will try to use a system proxy, if configured.
    // This virtually never makes sense for these connections, since we're on
    // the VPN and the proxy is likely not reachable through the VPN.  Worse, on
    // Mac only it can cause Qt to block our thread trying to execute a PAC
    // script; delays have been observed up to 20 seconds on this thread.
    _targetSocket.setProxy({QNetworkProxy::ProxyType::NoProxy});

    connect(&_socksSocket, &QTcpSocket::readyRead, this, &SocksConnection::onSocksReadyRead);
    connect(&_socksSocket, QOverload<QTcpSocket::SocketError>::of(&QTcpSocket::error),
            this, &SocksConnection::onSocksError);
    connect(&_socksSocket, &QTcpSocket::disconnected, this, &SocksConnection::onSocksDisconnected);
    connect(&_targetSocket, &QTcpSocket::connected, this, &SocksConnection::onTargetConnected);
    connect(&_targetSocket, &QTcpSocket::readyRead, this, &SocksConnection::onTargetReadyRead);
    connect(&_targetSocket, QOverload<QTcpSocket::SocketError>::of(&QTcpSocket::error),
            this, &SocksConnection::onTargetError);
    connect(&_targetSocket, &QTcpSocket::disconnected, this, &SocksConnection::onTargetDisconnected);

    _abortTimer.setSingleShot(true);
    _abortTimer.setInterval(msec(std::chrono::seconds(5)));
    _abortTimer.callOnTimeout(this, [this]()
    {
        qWarning() << "Aborting SOCKS connection in state" << traceEnum(_state)
            << "due to timeout";
        abortConnection();
    });

    // Time out if initial negotiation isn't completed
    _abortTimer.start();

    qInfo() << "Target socket:" << _targetSocket.socketDescriptor() << "->" << bindAddress;
    if(!_targetSocket.bind(bindAddress))
    {
        qWarning() << "Bind failed on socket:" << _targetSocket.socketDescriptor()
            << "->" << bindAddress << ":" << traceEnum(_targetSocket.error());
    }
    else
        qInfo() << "Bind succeeded on socket:" << _targetSocket.socketDescriptor()
            << "->" << bindAddress << "==" << _targetSocket.localAddress();

// Also bind the socket to the interface on Linux, as Linux does not support the "strong host model"
// meaning the packets won't be routed through our preferred interface based on source ip alone
#ifdef Q_OS_LINUX
    if(setsockopt(_targetSocket.socketDescriptor(), SOL_SOCKET, SO_BINDTODEVICE, qPrintable(bindInterface), bindInterface.size()))
    {
        qWarning() << QStringLiteral("setsockopt error: %1 (code: %2)").arg(qt_error_string(errno)).arg(errno);
    }
#endif
}

void SocksConnection::abortConnection()
{
    _socksSocket.abort();
    _targetSocket.abort();  // No effect if not connected
    _state = State::Closed;
    // This can occur during a signal from the QTcpSocket, it may not be safe to
    // delete the socket now.
    _socksSocket.deleteLater();
}

void SocksConnection::respond(const QByteArray &response)
{
    auto result = _socksSocket.write(response);
    if(result != response.size())
    {
        qWarning() << "Failed to write response of" << response.size()
            << "bytes; result was" << result;
        abortConnection();
    }
}

void SocksConnection::rejectConnection(const QByteArray &response)
{
    _nextMessageBytes = 0;
    _state = State::SocksDisconnecting;
    // Abort if the client doesn't disconnect soon.  (If the timer was already
    // running for the negotiation phase, this restarts it.)
    _abortTimer.start();
    respond(response);
    // As long as we didn't abort in respond(), disconnect the socket.  This
    // waits for buffers to clear before disconnecting.
    if(_state == State::SocksDisconnecting)
        _socksSocket.disconnectFromHost();
}

bool SocksConnection::checkMessageVersion(const QByteArray &message,
                                          quint8 version,
                                          const QString &traceName)
{
    Q_ASSERT(message.size() >= 1);  // Checked by caller

    if(message[0] != version)
    {
        // Bail; this does not support any other version.
        qWarning() << "Received unsupported" << traceName << "version"
            << int(message[0]);
        abortConnection();
        return false;
    }

    return true;
}

bool SocksConnection::checkSocksVersion(const QByteArray &message)
{
    return checkMessageVersion(message, SocksVersion, QStringLiteral("SOCKS"));
}

bool SocksConnection::checkUPAuthVersion(const QByteArray &message)
{
    return checkMessageVersion(message, UsernamePasswordAuthVersion,
                               QStringLiteral("U/P auth"));
}

void SocksConnection::forwardData(QTcpSocket &source, QTcpSocket &dest,
                                  const QString &directionTrace)
{
    Q_ASSERT(_state == State::Connected);   // Ensured by caller

    auto data = source.readAll();
    if(!data.isEmpty())
    {
        auto size = dest.write(data);
        if(size != data.size())
        {
            qWarning() << "Failed to forward" << data.size()
                << "bytes of" << directionTrace << "data -" << size;
            abortConnection();
        }
    }
}

void SocksConnection::onSocksReadyRead()
{
    while(true)
    {
        auto lastAvailable = _socksSocket.bytesAvailable();

        // If there's nothing to process, we're done.
        if(lastAvailable <= 0)
            break;

        processSocksData();
        auto newAvailable = _socksSocket.bytesAvailable();
        // There can never be _more_ data available now - this would mean that
        // processSocksData() somehow entered an event loop and allowed us to
        // receive more data.  This would interfere with the check to see if it
        // consumed data.
        Q_ASSERT(newAvailable <= lastAvailable);

        // If nothing was consumed, stop processing - we are buffering the data
        // and waiting for something else to happen (we have an incomplete
        // message and need the rest to arrive, or we're waiting for the target
        // to connect).
        if(newAvailable == lastAvailable)
        {
            qInfo() << "Buffering" << newAvailable << "bytes";
            break;
        }
        // Otherwise, some data were consumed; if any data remains, process
        // again.
    }
}

void SocksConnection::processSocksData()
{
    // In Receive* states, wait until we've received the entire message - let
    // QTcpSocket buffer it.  In other states, _nextMessageBytes is 0, so this
    // has no effect.
    QByteArray receivedMsg;
    if(_nextMessageBytes > 0)
    {
        if(_socksSocket.bytesAvailable() < _nextMessageBytes)
        {
            qInfo() << "Wait for complete message of" << _nextMessageBytes
                << "in state" << traceEnum(_state) << "- have"
                << _socksSocket.bytesAvailable() << "bytes";
            return;
        }

        receivedMsg = _socksSocket.read(_nextMessageBytes);
        // This should not fail since we checked bytesAvailable()
        if(receivedMsg.size() != _nextMessageBytes)
        {
            qWarning() << "Failed to read expected message of"
                << _nextMessageBytes << "bytes in state" << traceEnum(_state)
                << "- got" << receivedMsg.size() << "bytes";
            abortConnection();
            // Can't process the message, we're now in the 'Closed' state.
            return;
        }
    }

    switch(_state)
    {
        case State::ReceiveAuthMethodsHeader:
            Q_ASSERT(receivedMsg.size() == AuthMethodHeaderMsg::Length);  // Message size for this state
            if(!checkSocksVersion(receivedMsg))
                break; // Aborted

            _nextMessageBytes = receivedMsg[AuthMethodHeaderMsg::NMethods];
            _state = State::ReceiveAuthMethods;
            break;
        case State::ReceiveAuthMethods:
        {
            // Look for the "username/password" method
            auto isUPAuthMethod = [](quint8 method){return method == Method::UsernamePassword;};
            QByteArray response{AuthMethodResponseMsg::Length, 0};
            response[AuthMethodResponseMsg::Version] = SocksVersion;
            if(std::any_of(receivedMsg.begin(), receivedMsg.end(), isUPAuthMethod))
            {
                // Success, accept this method
                response[AuthMethodResponseMsg::Method] = Method::UsernamePassword;
                _nextMessageBytes = AuthUsernameHeaderMsg::Length;
                _state = State::ReceiveAuthUsernameHeader;
                respond(response);
            }
            else
            {
                // Failure, no acceptable method
                qInfo() << "Rejecting SOCKS connection, no acceptable auth method found in:"
                    << receivedMsg;
                response[AuthMethodResponseMsg::Method] = Method::NotAcceptable;
                rejectConnection(response); // Goes to Rejecting state
            }
            break;
        }
        case State::ReceiveAuthUsernameHeader:
            Q_ASSERT(receivedMsg.size() == AuthUsernameHeaderMsg::Length);
            if(!checkUPAuthVersion(receivedMsg))
                break;

            _nextMessageBytes = receivedMsg[AuthUsernameHeaderMsg::UsernameLength];
            _state = State::ReceiveAuthUsername;
            break;
        case State::ReceiveAuthUsername:
            // This check and response could reveal the username
            // (non-constant-time check, and timing indicates that username
            // failed, not password), but the username is not secret.
            if(receivedMsg != username)
            {
                qInfo() << "Reject connection due to incorrect username";
                QByteArray response{AuthResponseMsg::Length, 0};
                response[AuthResponseMsg::Version] = UsernamePasswordAuthVersion;
                response[AuthResponseMsg::Status] = 1;  // Nonzero = failure
                rejectConnection(response);
            }
            else
            {
                _nextMessageBytes = AuthPasswordHeaderMsg::Length;
                _state = State::ReceiveAuthPasswordHeader;
            }
            break;
        case State::ReceiveAuthPasswordHeader:
            _nextMessageBytes = receivedMsg[AuthPasswordHeaderMsg::PasswordLength];
            _state = State::ReceiveAuthPassword;
            break;
        case State::ReceiveAuthPassword:
        {
            // Check the password by hashing it, then performing a constant-time
            // comparison on the hash.  The hash mainly just ensures that the
            // resulting data are the same length, to simplify the constant-time
            // comparison.
            QCryptographicHash hasher{QCryptographicHash::Algorithm::Sha256};
            hasher.addData(receivedMsg);
            auto hash = hasher.result();

            QByteArray response{AuthResponseMsg::Length, 0};
            response[AuthResponseMsg::Version] = UsernamePasswordAuthVersion;
            response[AuthResponseMsg::Status] = 1;  // Nonzero = failure

            // The hashes should be the same length, but check for sanity, this
            // would prevent all auth from working
            if(hash.size() != _passwordHash.size())
            {
                qWarning() << "Can't compare password hashes of different sizes:"
                    << hash.size() << "-" << _passwordHash.size();
                rejectConnection(response);
            }
            else if(checkHashEquals(hash, _passwordHash))
            {
                _nextMessageBytes = ConnectHeaderMsg::Length;
                _state = State::ReceiveConnectHeader;
                response[AuthResponseMsg::Status] = 0;  // Success
                respond(response);
            }
            else
            {
                qWarning() << "Rejecting connection due to incorrect password";
                rejectConnection(response);
            }
            break;
        }
        case State::ReceiveConnectHeader:
        {
            Q_ASSERT(receivedMsg.size() == ConnectHeaderMsg::Length);  // Message size for this state
            if(!checkSocksVersion(receivedMsg))
                break; // Aborted

            QByteArray response{ConnectResponseMsg::Length, 0};
            response[ConnectResponseMsg::Version] = SocksVersion;
            response[ConnectResponseMsg::AddrType] = AddressType::IPv4;
            // The client will probably try to send an address, but if we don't
            // understand the command or address type, we may not know how long
            // it is.
            // Ignore any subsquent data and send a rejection now.
            if(receivedMsg.at(ConnectHeaderMsg::Command) != Command::Connect)
            {
                qInfo() << "Rejecting SOCKS connection, unexpected command"
                    << int(receivedMsg.at(ConnectHeaderMsg::Command));
                response[ConnectResponseMsg::Reply] = Reply::CommandNotSupported;
                rejectConnection(response);
            }
            else if(receivedMsg.at(ConnectHeaderMsg::AddrType) != AddressType::IPv4)
            {
                qInfo() << "Rejecting SOCKS connection, unexpected address type"
                    << int(receivedMsg.at(ConnectHeaderMsg::AddrType));
                response[ConnectResponseMsg::Reply] = Reply::AddressTypeNotSupported;
                rejectConnection(response);
            }
            else
            {
                // Success, wait for address data
                _nextMessageBytes = 6;  // IPv4 4 bytes + port 2 bytes
                _state = State::ReceiveConnect;
            }
            break;
        }
        case State::ReceiveConnect:
        {
            Q_ASSERT(receivedMsg.size() == 6);  // Message size for this state
            quint32 destAddr = readUnsignedBE<quint32>(receivedMsg, 0);
            quint16 port = readUnsignedBE<quint16>(receivedMsg, 4);
            QHostAddress destHost{destAddr};
            qInfo() << "Connecting to" << destHost << "port" << port;
            _nextMessageBytes = 0;
            _state = State::Connecting;
            // Negotiation completed, now waiting on the connect - stop the
            // abort timeout
            _abortTimer.stop();
            _targetSocket.connectToHost(destHost, port);
            break;
        }
        case State::Connecting:
            // If data is sent in this state, it's supposed to be forwarded
            // after the connection completes.  Don't do anything, let
            // QTcpSocket buffer it.
            break;
        case State::Connected:
            forwardData(_socksSocket, _targetSocket, QStringLiteral("outbound"));
            break;
        default:
        case State::SocksDisconnecting:
        case State::Closed:
            // Ignore any data sent in these states.
            _socksSocket.skip(_socksSocket.bytesAvailable());
            break;
    }

    return;
}

void SocksConnection::onSocksError(QAbstractSocket::SocketError socketError)
{
    // "RemoteHostClosedError" actually means that the remote host closed the
    // connection normally.  disconnected() will be emitted, ignore the error.
    if(socketError == QAbstractSocket::SocketError::RemoteHostClosedError)
    {
        qInfo() << "SOCKS connection closed in state" << traceEnum(_state);
        return;
    }

    qWarning() << "SOCKS connection error:" << traceEnum(socketError);
    abortConnection();
}

void SocksConnection::onSocksDisconnected()
{
    qInfo() << "SOCKS connection disconnected in state" << traceEnum(_state);
    switch(_state)
    {
        default:
        case State::ReceiveAuthMethodsHeader:
        case State::ReceiveAuthMethods:
        case State::ReceiveConnectHeader:
        case State::ReceiveConnect:
        case State::Connecting:
        case State::TargetDisconnecting:
        case State::Closed:
            // Unexpected, abort.  Can occur in Receive* or Connecting if the
            // SOCKS side disconnects unexpectedly.  Shouldn't occur in
            // TargetDisconnecting/Closed; would indicate that we received
            // more than one disconnect signal.
            qInfo() << "Aborting connection in state" << traceEnum(_state)
                << "due to unexpected SOCKS disconnect";
            abortConnection();
            break;
        case State::Connected:
            // This is normal, SOCKS side has shut down the connection.
            _state = State::TargetDisconnecting;
            _targetSocket.disconnectFromHost(); // Flushes data
            _abortTimer.start();
            break;
        case State::SocksDisconnecting:
            // All done, both sides have disconnected, just shut down
            _state = State::Closed;
            _socksSocket.deleteLater();
            break;
    }
}

void SocksConnection::onTargetConnected()
{
    switch(_state)
    {
        default:
        case State::ReceiveAuthMethodsHeader:
        case State::ReceiveAuthMethods:
        case State::ReceiveConnectHeader:
        case State::ReceiveConnect:
        case State::Connected:
        case State::SocksDisconnecting:
        case State::TargetDisconnecting:
        case State::Closed:
            // Unexpected, abort
            qInfo() << "Aborting connection in state" << traceEnum(_state)
                << "due to unexpected target connect";
            abortConnection();
            break;
        case State::Connecting:
        {
            _state = State::Connected;
            // Send the success reply to the SOCKS connection
            QByteArray response{ConnectResponseMsg::Length, 0};
            response[ConnectResponseMsg::Version] = SocksVersion;
            response[ConnectResponseMsg::AddrType] = AddressType::IPv4;
            response[ConnectResponseMsg::Reply] = Reply::Succeeded;
            QHostAddress localHostAddr = _targetSocket.localAddress();
            quint32 localAddr = localHostAddr.toIPv4Address();
            response[ConnectResponseMsg::Addr] = static_cast<quint8>(localAddr >> 24);
            response[ConnectResponseMsg::Addr+1] = static_cast<quint8>(localAddr >> 16);
            response[ConnectResponseMsg::Addr+2] = static_cast<quint8>(localAddr >> 8);
            response[ConnectResponseMsg::Addr+2] = static_cast<quint8>(localAddr);
            quint16 localPort = _targetSocket.localPort();
            response[ConnectResponseMsg::Port] = static_cast<quint8>(localPort >> 8);
            response[ConnectResponseMsg::Port+1] = static_cast<quint8>(localPort);
            qInfo() << "Connected" << localHostAddr << ":" << localPort << "->"
                << _targetSocket.peerAddress() << ":"
                << _targetSocket.peerPort();
            respond(response);

            // Forward any data that had already arrived from either end,
            // unless we aborted in respond()
            if(_state == State::Connected)
                forwardData(_socksSocket, _targetSocket, QStringLiteral("outbound"));
            // Could also have aborted in the first forwardData() call
            if(_state == State::Connected)
                forwardData(_targetSocket, _socksSocket, QStringLiteral("inbound"));

            break;
        }
    }
}

void SocksConnection::onTargetError(QAbstractSocket::SocketError socketError)
{
    // "RemoteHostClosedError" actually means that the remote host closed the
    // connection normally.  disconnected() will be emitted, ignore the error.
    if(socketError == QAbstractSocket::SocketError::RemoteHostClosedError)
    {
        qInfo() << "Target closed the connection in state" << traceEnum(_state);
        return;
    }

    switch(_state)
    {
        default:
        case State::ReceiveAuthMethodsHeader:
        case State::ReceiveAuthMethods:
        case State::ReceiveConnectHeader:
        case State::ReceiveConnect:
        case State::Connected:
        case State::SocksDisconnecting:
        case State::TargetDisconnecting:
        case State::Closed:
            // Unexpected, abort
            qInfo() << "Aborting connection in state" << traceEnum(_state)
                << "due to target error" << traceEnum(socketError);
            abortConnection();
            break;
        case State::Connecting:
        {
            _state = State::SocksDisconnecting;
            // Send the failure reply to the SOCKS connection
            QByteArray response{ConnectResponseMsg::Length, 0};
            response[ConnectResponseMsg::Version] = SocksVersion;
            response[ConnectResponseMsg::AddrType] = AddressType::IPv4;

            Reply result;
            switch(socketError)
            {
                default:
                    result = Reply::GeneralFailure;
                    break;
                case QAbstractSocket::SocketError::NetworkError:
                    result = Reply::NetUnreachable;
                    break;
                case QAbstractSocket::SocketError::HostNotFoundError:
                    result = Reply::HostUnreachable;
                    break;
                case QAbstractSocket::SocketError::ConnectionRefusedError:
                    result = Reply::ConnectionRefused;
                    break;
            }
            response[ConnectResponseMsg::Reply] = result;

            qInfo() << "SOCKS connection to" << _targetSocket.peerAddress()
                << ":" << _targetSocket.peerPort() << "failed with error"
                << traceEnum(socketError) << "- respond with code" << result;
            rejectConnection(response);

            break;
        }
    }
}

void SocksConnection::onTargetReadyRead()
{
    switch(_state)
    {
        default:
        case State::ReceiveAuthMethodsHeader:
        case State::ReceiveAuthMethods:
        case State::ReceiveConnectHeader:
        case State::ReceiveConnect:
        case State::Connecting:
            // Not ready to forward data, let the QTcpSocket buffer it
            qInfo() << "Buffering data from target in state" << traceEnum(_state)
                << "- have" << _targetSocket.bytesAvailable() << "bytes";
            break;
        case State::Connected:
            forwardData(_targetSocket, _socksSocket, QStringLiteral("inbound"));
            break;
        case State::SocksDisconnecting:
        case State::TargetDisconnecting:
        case State::Closed:
            qWarning() << "Discarding" << _targetSocket.bytesAvailable()
                << "from target in state" << traceEnum(_state);
            // No data expected in these states, discard it
            _targetSocket.skip(_targetSocket.bytesAvailable());
            break;
    }
}

void SocksConnection::onTargetDisconnected()
{
    qInfo() << "Target socket disconnected in state" << traceEnum(_state);
    switch(_state)
    {
        default:
        case State::ReceiveAuthMethodsHeader:
        case State::ReceiveAuthMethods:
        case State::ReceiveConnectHeader:
        case State::ReceiveConnect:
        case State::Connecting:
        case State::SocksDisconnecting:
        case State::Closed:
            // Unexpected, abort.  Can occur in Receive* or Connecting if the
            // SOCKS side disconnects unexpectedly.  Shouldn't occur in
            // TargetDisconnecting/Closed; would indicate that we received
            // more than one disconnect signal.
            qInfo() << "Aborting connection in state" << traceEnum(_state)
                << "due to unexpected SOCKS disconnect";
            abortConnection();
            break;
        case State::Connected:
            // This is normal, target side has shut down the connection.
            _state = State::SocksDisconnecting;
            _socksSocket.disconnectFromHost(); // Flushes data
            _abortTimer.start();
            break;
        case State::TargetDisconnecting:
            // All done, both sides have disconnected, just shut down
            _state = State::Closed;
            _socksSocket.deleteLater();
            break;
    }
}
