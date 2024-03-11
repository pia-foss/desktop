// Copyright (c) 2024 Private Internet Access, Inc.
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
#line SOURCE_FILE("daemonconnection.cpp")

#include "daemonconnection.h"

DaemonConnection::DaemonConnection(QObject* parent)
    : QObject(parent)
    , _ipc(nullptr)
    , _connected(false)
{
    _rpc = new ClientSideInterface(&_methods, this);
    _methods.add({ QStringLiteral("data"), this, &DaemonConnection::RPC_data });
    _connectionTimer.setSingleShot(true);
    connect(&_connectionTimer, &QTimer::timeout, this, [this]() {
        if (!_connected) socketError(QStringLiteral("Timeout waiting for daemon connection"));
    });

    // Redact dedicated IP information.  Notes:
    // - Clients don't have access to DIP tokens, so we don't add redactions
    //   for those.
    // - When adding a DIP region, these redactions aren't in place until the
    //   region is added - the "add" code itself must be careful not to trace
    //   the token.
    // - These don't typically show up in GUI client logs anyway, but they do
    //   show up in CLI client logs (the IP is used in the "friendly region ID")
    connect(&state, &DaemonState::dedicatedIpLocationsChanged, this, [this]()
    {
        for(const auto &dipLoc : state.dedicatedIpLocations())
        {
            const auto &dipIp = dipLoc["dedicatedIp"].toString();
            const auto &dipId = dipLoc["id"].toString();
            const auto &dipCn = dipLoc["dedicatedIpCn"].toString();
            if(!dipIp.isEmpty())
                Logger::addRedaction(dipIp, QStringLiteral("DIP IP %1").arg(dipId));
            if(!dipCn.isEmpty())
                Logger::addRedaction(dipCn, QStringLiteral("DIP CN %1").arg(dipId));
        }
    });
}

DaemonConnection::~DaemonConnection()
{

}

void DaemonConnection::connectToDaemon()
{
    if (_ipc)
        return;

#ifdef Q_OS_WIN
    // On Windows, QLocalSocket::connectToServer() has a hard-coded 5-second
    // blocking wait if the named pipe exists but is busy (qlocalsocket_win.cpp,
    // line 175).
    //
    // Although this wait occurs on a worker thread, we should not abandon the
    // connection before this wait elapses, because the QLocalSocket (and the
    // worker thread it runs on) can't be destroyed until then.  (If we
    // abandoned earlier, we'd either block waiting for it to be destroyed,
    // which hangs up the UI, or we'd start creating more and more threads with
    // QLocalSockets sitting in this blocking wait, which is a pretty unfriendly
    // use of system resources, particularly just after boot when this is likely
    // to occur.)
    //
    // So this timeout should be at least 5 seconds.  It's currently 6 seconds
    // for a bit of wiggle room to minimize UI hangs.
    int abandonTimeout = 6000;
#else
    int abandonTimeout = 1000;
#endif

    _connectionTimer.start(abandonTimeout);

    _ipc = new ThreadedLocalIPCConnection(this);

    connect(_ipc, &IPCConnection::connected, this, &DaemonConnection::socketConnected);
    connect(_ipc, &IPCConnection::disconnected, this, &DaemonConnection::socketDisconnected);
    connect(_ipc, &IPCConnection::error, this, &DaemonConnection::socketError);

    connect(_ipc, &IPCConnection::messageReceived, _rpc, &ClientSideInterface::processMessage);
    connect(_rpc, &ClientSideInterface::messageReady, _ipc, &IPCConnection::sendMessage);
    connect(_ipc, &IPCConnection::messageError, _rpc, &ClientSideInterface::requestSendError);

    _ipc->connectToServer();
}

void DaemonConnection::RPC_data(const QJsonObject &data)
{
    QJsonObject::const_iterator it;
#define AssignObject(name) \
    if ((it = data.find(QStringLiteral(#name))) != data.end() && it.value().isObject()) this->name.assign(it.value().toObject())

    AssignObject(data);
    AssignObject(account);
    AssignObject(settings);
    AssignObject(state);
#undef AssignObject

    if (!_connected && _ipc->isConnected())
    {
        _connectionTimer.stop();
        emit connectedChanged(_connected = true);
    }
}

void DaemonConnection::RPC_error(const QJsonObject& errorObject)
{
    qInfo() << "Received error:" << errorObject;
    Error e(errorObject);
    emit error(e);
}

void DaemonConnection::socketDisconnected()
{
    if (_ipc)
    {
        // Explicitly disconnect signals early
        disconnect(_ipc, nullptr, this, nullptr);
        disconnect(_ipc, nullptr, _rpc, nullptr);
        disconnect(_rpc, nullptr, _ipc, nullptr);
        _ipc->deleteLater();
        _ipc = nullptr;
    }
    // Reject any requests that were sent before the connection was lost
    _rpc->connectionLost();
    if (_connected)
    {
        emit connectedChanged(_connected = false);
    }
    if (!_connectionTimer.isActive())
        connectToDaemon();
}

void DaemonConnection::socketError(const QString &errorString)
{
    emit error(Error(HERE, Error::DaemonConnectionError, { errorString }));
    if (_connected)
    {
        emit connectedChanged(_connected = false);
    }
    socketDisconnected();
}
