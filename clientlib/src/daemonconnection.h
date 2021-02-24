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
#line HEADER_FILE("daemonconnection.h")

#ifndef DAEMONCONNECTION_H
#define DAEMONCONNECTION_H

#include "clientlib.h"
#include "ipc.h"
#include "jsonrpc.h"
#include "settings.h"
#include <QObject>
#include <QTimer>

// Handle the native connection to the daemon, as well as storing its state.
class CLIENTLIB_EXPORT DaemonConnection : public QObject
{
    Q_OBJECT
public:
    explicit DaemonConnection(QObject* parent = nullptr);
    ~DaemonConnection();

    void connectToDaemon();
    bool isConnected() const { return _connected; }

// Information gathered from the daemon to display in the client
public:
    // List of server locations and certificate info
    DaemonData data;
    // Username, expiration date, etc for logged-in user
    DaemonAccount account;
    // Settings set by the user from the Settings GUI
    DaemonSettings settings;
    // Connection state including reconnects and errors, forwarded port,
    // bytes sent and received, IP addresses, transport, port,
    // closest server, and handshake. Also includes the WFP callout state
    // on Windows, or the kext state on macOS
    DaemonState state;

public slots:
    // Generic call mechanism for any RPC function from native code.
    // This returns an Async<QJsonValue> which is suitable for use by native
    // code.
    Async<QJsonValue> call(const QString &method, const QJsonArray &params)
    {
        return _rpc->callWithParams(method, params);
    }

    // Generic post mechanism for any RPC function
    void post(const QString& method, const QJsonArray& params)
    {
        _rpc->postWithParams(method, params);
    }

    // Any daemon RPC function which needs to be accessed from native code
    // can be added as a shorthand here.

protected slots:
    void RPC_data(const QJsonObject& data);
    void RPC_error(const QJsonObject& errorObject);

protected slots:
    void socketDisconnected();
    void socketError(const QString& errorString);

signals:
    // A new daemon socket connection has been established.  This doesn't mean
    // that we are in the "connected" state yet (we wait until we've synced data
    // for that), but the Linux/X11 shutdown needs to know the socket descriptor
    // in case Xlib tries to terminate the process.
    void socketConnected(qintptr socketFd);
    void connectedChanged(bool isConnected);
    void error(const Error& error);

private:
    LocalMethodRegistry _methods;
    ClientIPCConnection* _ipc;
    ClientSideInterface* _rpc;
    QTimer _connectionTimer;
    bool _connected;
};

#endif
