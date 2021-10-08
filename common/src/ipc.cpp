// Copyright (c) 2021 Private Internet Access, Inc.
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
#include <QFile>
#include <QLocalServer>

// The IPC layer provides basic message framing for UTF-8-encoded payloads.
// (The JSON-RPC implementation is connected to this IPC layer to transport its
// messages.)
//
// The framing protocol is designed to be able to resynchronize in the event of
// a partially-transmitted message.  Since 2.3.0, it also provides
// acknowledgement of received messages.
//
// '0xFF' indicates the beginning of a frame and must not appear anywhere else
// in a frame (this byte never appears in UTF-8).  The sequence value is encoded
// in a way that prevents 0xFF from appearing in the sequence.
//
// It's possible for '0xFF' to occur in the payload length, which might be
// misidentified as the start of a frame if a prior desynchronization had
// occurred, but the client will still resynchronize correctly as long as the
// length is not 0xFFACCE56 exactly, which is not an allowed length anyway.
//
// Frames are formatted as:
// | Offset | Length | Value
// | 0      | 4      | FF AC CE 56  (note - formerly 0xFFACCE55, bumped due to framing change)
// | 4      | 2      | Sequence low byte << 4 (to avoid 0xFF in sequence; LE)
// | 6      | 2      | Sequence high byte << 4 (to avoid 0xFF in sequence; LE)
// | 8      | 4      | Payload length (excludes header; LE)
// | 12     | ...    | Payload
//
// For messages, the payload length is in the range [2, 0x100000].
// Acknowledgements are indicated with a length field of 0.
//
// Messages are assigned a 16-bit sequence value by the sender.  Upon receiving
// a message, the receiver sends an acknowledgement with the sender's latest
// sequence that was received.  The sender can thus determine the number of
// unacknowledged messages that are outstanding at any time, and the protocol
// can resync even if a message is interrupted.
//
// Notes:
// - Message interruption is rare but can occur on Linux.  On Linux, if the X
//   connection is lost, Qt will abort(), which the client then overrides with
//   an exec() to send a clean-exit message (this is interpreted as a clean exit
//   because many desktop env logouts cause the X connection to be lost before
//   SIGINT is sent).  It's rare but possible that the client was in the middle
//   of sending a message when the exec() occurred.
//
// - Acknowledgements are necessary because on Windows, the client process may
//   be suspended by the OS when the system is idling to save battery.  The
//   system would allow daemon updates to queue up in client memory
//   indefinitely, and the client would have to process them all upon waking.
//   Instead, the daemon kills the connection if the client starts to lag, and
//   the client can reconnect and re-sync upon waking.

namespace
{
    enum : int
    {
        // Threshold where IPC starts emitting the remoteLagging() signal
        DefaultLagThreshold = 10,
    };
}

void IPCServer::sendMessageToAllClients(const QByteArray &msg)
{
    for (const auto& connection : _connections)
    {
        if (connection->isConnected())
            connection->sendMessage(msg);
    }
}

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

static quint32_be PIA_LOCAL_SOCKET_MAGIC { 0xFFACCE56 }; // Note first 0xFF character (always invalid in UTF-8)

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

LocalSocketIPCConnection::LocalSocketIPCConnection(QLocalSocket *socket, QObject *parent)
    : ClientIPCConnection{parent}, _socket{socket}, _payloadReceived{0},
      _lagThreshold{DefaultLagThreshold},
      _payloadSequence{0},
      _lastSendSequence{0xFFF0},    // Start from a high value so wraparound is easily verified
      _acknowledgedSequence{_lastSendSequence},
      _error{false}
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

void LocalSocketIPCConnection::setLagThreshold(int threshold)
{
    _lagThreshold = threshold;
}

void LocalSocketIPCConnection::writeFrame(quint16 sequence,
                                          const QByteArray &data,
                                          QDataStream& stream)
{
    auto byteOrder = stream.byteOrder();
    stream.setByteOrder(QDataStream::BigEndian);
    stream << PIA_LOCAL_SOCKET_MAGIC;
    stream.setByteOrder(QDataStream::LittleEndian);
    // The sequence bytes are split up and each straddle two message bytes to
    // ensure that this doesn't result in an 0xFF byte.
    quint16 sequenceLowShifted = (sequence & 0x00FF) << 4;
    quint16 sequenceHighShifted = (sequence & 0xFF00) >> 4;
    stream << sequenceLowShifted;
    stream << sequenceHighShifted;
    // This writes a 32-bit length and the payload data
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

int LocalSocketIPCConnection::getUnackedCount() const
{
    int lastSend{_lastSendSequence};
    int acked{_acknowledgedSequence};
    // Check for wraparound
    if(lastSend < acked)
        lastSend += 0x10000;
    return lastSend - acked;
}

void LocalSocketIPCConnection::sendFrame(quint16 sequence, const QByteArray &payload)
{
    Q_ASSERT(isConnected());     // Checked by caller

    {
        QDataStream stream{_socket};
        writeFrame(sequence, payload, stream);
    }
    _socket->flush();
}

void LocalSocketIPCConnection::sendMessage(const QByteArray &data)
{
    if (!isConnected())
    {
        emit messageError({HERE, Error::Code::IPCNotConnected}, data);
        return;
    }

    ++_lastSendSequence;
    sendFrame(_lastSendSequence, data);

    int sequenceUnacked = getUnackedCount();
    // Check if the remote end is falling behind
    if(sequenceUnacked >= _lagThreshold / 2)
    {
        // Warn starting at a depth of 5
        qWarning() << "Sent message; have" << sequenceUnacked
            << "unacknowledged messages";
        // Emit the lagging signal at 10 messages - the client/server may decide
        // to kill the connection
        if(sequenceUnacked >= _lagThreshold)
            emit remoteLagging();
    }
}

void LocalSocketIPCConnection::onReadReady()
{
    while (isConnected())
    {
        if (_payload.size() == 0)
        {
            // We are not currently receiving a message; look for the next
            // start of a packet, identified by its magic tag.

            struct
            {
                quint32_be tag;
                quint16_le sequenceLow;
                quint16_le sequenceHigh;
                quint32_le size;
            } header;
            Q_STATIC_ASSERT(sizeof(header) == 12);

            if (_socket->bytesAvailable() < (qint64)sizeof(header) || _socket->peek(reinterpret_cast<char*>(&header), sizeof(header)) != (qint64)sizeof(header))
            {
                // Not enough data available yet; wait for next readyRead.
                return;
            }

            // Reconstruct the sequence being received or acknowledged
            _payloadSequence = (quint16{header.sequenceLow} >> 4) |
                                (quint16{header.sequenceHigh} << 4);

            if (header.tag != PIA_LOCAL_SOCKET_MAGIC)
            {
                qWarning() << "Invalid message: missing or incorrect magic tag:"
                    << QString::number(header.tag, 16);
                // Keep going below
            }
            else if (header.size == 0)
            {
                // Acknowledgement - update _acknowledgedSequence
                int priorUnacked = getUnackedCount();
                _acknowledgedSequence = _payloadSequence;
                int newUnacked = getUnackedCount();

                // Trace the new count if we were previously warning based on
                // the old count
                if(priorUnacked >= _lagThreshold / 2)
                {
                    qInfo() << "Received acknowledgement, now have"
                        << newUnacked << "unacknowledged messages (down from"
                        << priorUnacked << ")";
                }

                // Skip over the ack and continue reading in case more data are
                // available
                _socket->skip(sizeof(header));
                continue;
            }
            else if (header.size < 2)
            {
                qWarning() << "Invalid message: payload too small:" << header.size;
                // Keep going below
            }
            else if (header.size > 1024 * 1024)
            {
                qWarning() << "Invalid message: payload too large:" << header.size;
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
            // We have finished reading a message.
            // Send an acknowledgement frame
            if(isConnected())
            {
                // QByteArray{} and QByteArray{nullptr} actually create a "null"
                // byte array that serializes as length -1.  Send an empty byte
                // array with length 0 using
                // QByteArray{0, Qt::Initialization::Uninitialized}.
                sendFrame(_payloadSequence, {0, Qt::Initialization::Uninitialized});
            }
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
    connect(_pConnection, &ClientIPCConnection::remoteLagging, this,
            &ThreadedLocalIPCConnection::remoteLagging);
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

void ThreadedLocalIPCConnection::setLagThreshold(int threshold)
{
    auto pConnLocal = _pConnection;
    QMetaObject::invokeMethod(pConnLocal,
        [pConnLocal, threshold]()
        {
            pConnLocal->setLagThreshold(threshold);
        });
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
