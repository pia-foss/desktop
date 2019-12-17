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

#include "common.h"
#line SOURCE_FILE("ipc.cpp")

#include "ipc.h"
#include "path.h"

#include <QtEndian>
#include <QByteArray>
#include <QDataStream>
#include <QString>
#include <QUuid>

#if defined(PIA_DAEMON) || defined(UNIT_TEST)

void IPCServer::sendMessageToAllClients(const QByteArray &msg)
{
    for (const auto& connection : _connections)
    {
        if (connection->isConnected())
            connection->sendMessage(msg);
    }
}

#endif // defined(PIA_DAEMON) || defined(UNIT_TEST)



// Implementation using QLocalServer / QLocalSocket ////////////////////////////

#include <QLocalSocket>

#if defined(UNIT_TEST)
// Pick a unique per-process name for unit tests.  This means that two instances
// of the same unit test can run at the same time - connections will only occur
// within the same process.
namespace
{
    const QString &getLocalSocketName()
    {
        // The path is chosen the first time getLocalSocketName() is called.
        class LocalName
        {
        public:
            LocalName()
            {
                auto uuid = QUuid::createUuid();
                _name = QStringLiteral("LocalSocketTest") + uuid.toString(QUuid::StringFormat::Id128).left(10);
                qInfo() << "Using local socket:" << _name;
            }
            QString _name;
        };

        static LocalName localName;
        return localName._name;
    }
}
# define PIA_LOCAL_SOCKET_NAME getLocalSocketName()
#else
# define PIA_LOCAL_SOCKET_NAME static_cast<const QString&>(Path::DaemonLocalSocket)
#endif

static quint32_be PIA_LOCAL_SOCKET_MAGIC { 0xFFACCE55 }; // Note first 0xFF character (always invalid in UTF-8)

// Scan for the start of a (possible) magic value.
static const char* scanForMagic(const char* begin, const char* end)
{
    while (begin != end)
    {
        if (*begin == (char)0xFF) return begin;
        ++begin;
    }
    return nullptr;
}

#if defined(PIA_DAEMON) || defined(UNIT_TEST)

#include <QFile>
#include <QLocalServer>

LocalSocketIPCServer::LocalSocketIPCServer(QObject *parent)
    : IPCServer(parent), _server(nullptr)
{

}

bool LocalSocketIPCServer::listen()
{
    if (_server)
    {
        qWarning() << "Server already listening";
        return false;
    }
    _server = new QLocalServer(this);
    _server->setSocketOptions(QLocalServer::WorldAccessOption);
    connect(_server, &QLocalServer::newConnection, this, [this]() {
        while (_server->hasPendingConnections())
        {
            QLocalSocket* clientSocket = _server->nextPendingConnection();
            LocalSocketIPCConnection* connection = new LocalSocketIPCConnection(clientSocket, this);
            // Set up so we clean out connections from the list, but only later
            // in its own call stack. This is safe as long as the daemon doesn't
            // do anything funny like run synchronous inner event loops...
            connect(clientSocket, &QLocalSocket::disconnected, this, [this, connection]() {
                _connections.remove(connection);
                connection->deleteLater();
            }, Qt::QueuedConnection);
            _connections.insert(connection);
            emit newConnection(connection);
        }
    });
#if !defined(Q_OS_WIN) && !defined(UNIT_TEST)
    (Path::DaemonLocalSocket / "..").mkpath();
#endif
    if (!_server->listen(PIA_LOCAL_SOCKET_NAME))
    {
        qCritical().noquote() << _server->errorString();
        return false;
    }
    return true;
}

void LocalSocketIPCServer::stop()
{
    if (_server)
    {
        _server->close();
        _server->deleteLater();
        _server = nullptr;
    }
}

#endif // defined(PIA_DAEMON) || defined(UNIT_TEST)


LocalSocketIPCConnection::LocalSocketIPCConnection(QLocalSocket *socket, QObject *parent)
    : ClientIPCConnection(parent), _socket(socket), _payloadReceived(0),
      _error(false)
{
    connect(socket, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error), this, [this](QLocalSocket::LocalSocketError e) {
        _error = true;
        _socket->disconnect(this);
        emit error(_socket->errorString());
        _socket->close();
        _socket->deleteLater();
        _socket = nullptr;
    });
    connect(socket, &QLocalSocket::disconnected, this, [this]() {
        _socket->disconnect(this);
        emit disconnected();
        _socket->deleteLater();
        _socket = nullptr;
    });
    connect(socket, &QLocalSocket::readyRead, this, &LocalSocketIPCConnection::onReadReady);
}

LocalSocketIPCConnection::LocalSocketIPCConnection(QObject *parent)
    : LocalSocketIPCConnection(new QLocalSocket(), parent)
{
    _socket->setParent(this);
}

void LocalSocketIPCConnection::connectToServer()
{
    connect(_socket, &QLocalSocket::connected, this, [this]()
        {
            emit connected(_socket->socketDescriptor());
        });
    _socket->connectToServer(PIA_LOCAL_SOCKET_NAME);
}

void LocalSocketIPCConnection::connectToSocketFd(qintptr socketFd)
{
    if(_socket->setSocketDescriptor(socketFd))
        emit connected(socketFd);
    else
    {
        _error = true;
        _socket->disconnect(this);
        emit error(QStringLiteral("Failed to connect to socket descriptor %1").arg(socketFd));
        _socket->close();
        _socket->deleteLater();
        _socket = nullptr;
    }
}

bool LocalSocketIPCConnection::isConnected()
{
    return _socket && _socket->state() == QLocalSocket::ConnectedState;
}

bool LocalSocketIPCConnection::isError()
{
    return !_socket || _error;
}

void LocalSocketIPCConnection::writeMessage(const QByteArray &data, QDataStream& stream)
{
    auto byteOrder = stream.byteOrder();
    stream.setByteOrder(QDataStream::BigEndian);
    stream << PIA_LOCAL_SOCKET_MAGIC;
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << data;
    stream.setByteOrder(byteOrder);
}

#ifdef UNIT_TEST
void LocalSocketIPCConnection::sendRawMessage(const QByteArray& msg)
{
    if (!isConnected())
        return;
    auto written = _socket->write(msg);
    if (written != msg.size())
    {
        qCritical() << "Warning: didn't write entire message!";
    }
    _socket->flush();
}
#endif

void LocalSocketIPCConnection::sendMessage(const QByteArray &data)
{
    if (!isConnected())
    {
        emit messageError({HERE, Error::Code::IPCNotConnected}, data);
        return;
    }

    {
        QDataStream stream(_socket);
        writeMessage(data, stream);
    }
    _socket->flush();
}

void LocalSocketIPCConnection::onReadReady()
{
    while (isConnected())
    {
        if (_payload.size() == 0)
        {
            // We are not currently receiving a message; look for the next
            // start of a packet, identified by its magic tag.

            struct { quint32_be tag; quint32_le size; } header;
            Q_STATIC_ASSERT(sizeof(header) == 8);

            if (_socket->bytesAvailable() < (qint64)sizeof(header) || _socket->peek(reinterpret_cast<char*>(&header), sizeof(header)) != (qint64)sizeof(header))
            {
                // Not enough data available yet; wait for next readyRead.
                return;
            }

            if (header.tag != PIA_LOCAL_SOCKET_MAGIC)
            {
                qWarning() << "Invalid message: missing or incorrect magic tag";
                // Keep going below
            }
            else if (header.size < 2)
            {
                qWarning() << "Invalid message: payload too small";
                // Keep going below
            }
            else if (header.size > 1024 * 1024)
            {
                qWarning() << "Invalid message: payload too large";
                // Keep going below
            }
            else
            {
                // Reserve buffer for payload.
                _payload.resize((int)header.size);
                _payloadReceived = 0;
                _socket->skip(sizeof(header));

                // Continue loop; will start reading payload next.
                continue;
            }

            // Invalid message; scan ahead for valid tag
            {
                // Skip one character so we don't find the current (bad) message
                _socket->skip(1);

                char tmp[1000];
                auto len = _socket->peek(tmp, sizeof(tmp));
                if (len < 4)
                {
                    // Not enough data avilable yet; wait for next readyRead.
                    return;
                }

                // Skip forward until next tag (if found) or until next earliest
                // possible tag (if not found). On the next iteration of the loop,
                // the new location will be checked (if possible).
                auto magic = scanForMagic(tmp, tmp + len);
                _socket->skip(magic ? (magic - tmp) : len);
            }
        }
        else if (_payloadReceived < _payload.size())
        {
            // We are currently receiving the payload of a packet.

            auto read = _socket->peek(_payload.data() + _payloadReceived, _payload.size() - _payloadReceived);
            if (read < 0)
            {
                qCritical() << "Local socket read error";
                return;
            }
            if (read == 0)
            {
                // Not enough data avilable yet; wait for next readyRead.
                return;
            }
            // Check for start of magic tag, indicating a truncated message
            auto magic = scanForMagic(_payload.data() + _payloadReceived, _payload.data() + _payloadReceived + read);
            if (magic)
            {
                qWarning() << "Invalid message: truncated message";
                _socket->skip(magic - _payload.data() - _payloadReceived);
                _payload.resize(0);
                _payloadReceived = 0;
            }
            else
            {
                _socket->skip(read);
                _payloadReceived += read;
            }
        }
        else
        {
            // We have finished reading a packet.
            emit messageReceived(_payload);
            _payload.resize(0);
            _payloadReceived = 0;
        }
    }
}

void LocalSocketIPCConnection::close()
{
    if(_socket)
        _socket->disconnectFromServer();
}

namespace
{
    RegisterMetaType<qintptr> qintptrMetaType{"qintptr"};
}

ThreadedLocalIPCConnection::ThreadedLocalIPCConnection(QObject *pParent)
    : ClientIPCConnection{pParent}, _connected{false}, _error{false}
{
    _socketThread.invokeOnThread([&]()
    {
        _pConnection = new LocalSocketIPCConnection{&_socketThread.objectOwner()};
    });

    connect(_pConnection, &ClientIPCConnection::connected, this,
            &ThreadedLocalIPCConnection::onConnected);
    connect(_pConnection, &ClientIPCConnection::messageReceived, this,
            &ThreadedLocalIPCConnection::onMessageReceived);
    connect(_pConnection, &ClientIPCConnection::disconnected, this,
            &ThreadedLocalIPCConnection::onDisconnected);
    connect(_pConnection, &ClientIPCConnection::error, this,
            &ThreadedLocalIPCConnection::onError);
    connect(_pConnection, &ClientIPCConnection::messageError, this,
            &ThreadedLocalIPCConnection::messageError);
}

void ThreadedLocalIPCConnection::onConnected(qintptr socketFd)
{
    // LocalSocketIPCConnection does not emit this signal when already
    // connected.
    Q_ASSERT(!_connected);
    // LocalSocketIPCConnection doesn't continue to try to connect after
    // encountering an error.
    Q_ASSERT(!_error);

    _connected = true;
    emit connected(socketFd);
}

void ThreadedLocalIPCConnection::onMessageReceived(const QByteArray &msg)
{
    emit messageReceived(msg);
}

void ThreadedLocalIPCConnection::onDisconnected()
{
    // Must have been connected
    Q_ASSERT(_connected);

    _connected = false;
    emit disconnected();
    // After being disconnected, LocalSocketIPCConnection returns true for
    // isError() (it clears out QLocalSocket pointer)
    _error = true;
}

void ThreadedLocalIPCConnection::onError(const QString &errorString)
{
    _connected = false;
    _error = true;
    emit error(errorString);
}

void ThreadedLocalIPCConnection::connectToServer()
{
    QMetaObject::invokeMethod(_pConnection,
                              &ClientIPCConnection::connectToServer);
}

void ThreadedLocalIPCConnection::connectToSocketFd(qintptr socketFd)
{
    QMetaObject::invokeMethod(_pConnection,
        // Capture _pConnection's value, not the this pointer
        [pConnection = _pConnection, socketFd]()
        {
            pConnection->connectToSocketFd(socketFd);
        });
}

bool ThreadedLocalIPCConnection::isConnected()
{
    return _connected;
}

bool ThreadedLocalIPCConnection::isError()
{
    return _error;
}

void ThreadedLocalIPCConnection::sendMessage(const QByteArray &msg)
{
    // Capture _pConnection by value (not via this) since that is the state
    // referenced by the queued method invocation.
    auto pConnLocal = _pConnection;
    QMetaObject::invokeMethod(pConnLocal,
        [pConnLocal, msg]()
        {
            pConnLocal->sendMessage(msg);
        });
}

void ThreadedLocalIPCConnection::close()
{
    QMetaObject::invokeMethod(_pConnection, &ClientIPCConnection::close);
}

#ifdef UNIT_TEST
void ThreadedLocalIPCConnection::sendRawMessage(const QByteArray &msg)
{
    // As in sendMessage(), capture _pConnection by value
    auto pConnLocal = _pConnection;
    QMetaObject::invokeMethod(pConnLocal,
        [pConnLocal, msg]()
        {
            pConnLocal->sendRawMessage(msg);
        });
}
#endif
