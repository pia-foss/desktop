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
#line HEADER_FILE("openvpnmethod.h")

#ifndef OPENVPNMETHOD_H
#define OPENVPNMETHOD_H

#include <common/src/exec.h>
#include "vpnmethod.h"
#include "openvpn.h"
#include "vpn.h"
#include <common/src/linebuffer.h>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <unordered_set>
#include <kapps_core/src/ipaddress.h>

class MtuPinger;


// Owns a QLocalSocket created by the QLocalServer and feeds incoming data
// into a LineBuffer; used by HelperIpcServer
class HelperIpcConnection : public QObject
{
    Q_OBJECT
public:
    // HelperIpcConnection takes ownership of the QLocalSocket (it clears the
    // parent and stores it in _pConnection).
    HelperIpcConnection(QLocalSocket *pConnection);
    ~HelperIpcConnection();
signals:
    void ipcMessage(const QString &line);
private:
    std::unique_ptr<QLocalSocket> _pConnection;
    LineBuffer _buffer;
};

// OpenVPNMethod receives the tunnel configuration from the helper script,
// which captures it from environment variables provided by OpenVPN.
//
// On Mac and Linux, the helpers just write to stderr, which is forwarded to
// the daemon.  On Windows though, stdout/stderr are not forwarded to the
// daemon, so instead the helper writes to a named pipe specified with the --ipc
// parameter (which can be done with a redirection in CMD).
//
// We can't simply redirect to a Unix domain socket in bash, so on Mac/Linux we
// still send these through stderr.
class HelperIpcServer : public QObject
{
    Q_OBJECT

private:

public:
    HelperIpcServer();
    ~HelperIpcServer();

private:
    void acceptConnections();

public:
    // Start listening for IPC connections
    bool listen();

    QString fullServerName() const {return _server.fullServerName();}

signals:
    void ipcMessage(const QString &line);

private:
    QLocalServer _server;
    // Active client connections.  The IpcConnection owns the QLocalSocket, but
    // it's used as the key in the map so we can remove disconnected clients.
    std::unordered_map<QLocalSocket*, std::unique_ptr<HelperIpcConnection>> _clients;
};


// OpenVPNMethod implements VPNMethod using an OpenVPN connection.  It starts an
// OpenVPN process to perform the connection.  Control and state updates are
// handled via the management interface.
class OpenVPNMethod : public VPNMethod
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("openvpnmethod")

public:
    OpenVPNMethod(QObject *pParent, const OriginalNetworkScan &netScan);
    virtual ~OpenVPNMethod() override;

public:
    virtual void run(const ConnectionConfig &connectingConfig,
                     const Server &vpnServer,
                     const Transport &transport,
                     const QHostAddress &localAddress,
                     const QHostAddress &shadowsocksServerAddress,
                     quint16 shadowsocksProxyPort) override;
    virtual void shutdown() override;
    virtual std::shared_ptr<NetworkAdapter> getNetworkAdapter() const override {return _networkAdapter;}

private:
    virtual void networkChanged() override;

private:
    bool writeOpenVPNConfig(QFile& outFile,
                            const Server &vpnServer,
                            const Transport &transport,
                            const QHostAddress &localAddress,
                            const QHostAddress &shadowsocksServerAddress,
                            quint16 shadowsocksProxyPort);

    void openvpnStateChanged();
    void openvpnStdoutLine(const QString& line);
    void checkStdoutErrors(const QString &line);
    void openvpnStderrLine(const QString& line);
    void checkForMagicStrings(const QString& line);
    bool respondToMgmtAuth(const QString &line, const QString &user,
                           const QString &password);
    void openvpnManagementLine(const QString& line);
    void openvpnExited(int exitCode);

    // Calculate the maximum MTU that we could have to the specified VPN server,
    // according to the physical interface MTU and protocol overhead.  (We never
    // apply a larger MTU than this; we might apply a smaller one if probes
    // indicate that the MTU is smaller or if the user requested a smaller MTU.)
    unsigned findMaxMtu(const kapps::core::Ipv4Address &host);

private:
    OpenVPNProcess *_openvpn;
#if defined(Q_OS_WIN)
    // IPC server used to receive info from updown script on Windows
    HelperIpcServer _helperIpcServer;
#endif
    // The network adapter used for the current connection
    std::shared_ptr<NetworkAdapter> _networkAdapter;
    ConnectionConfig _connectingConfig;
    // VPN host we're connecting to - used to find the link MTU to this host
    kapps::core::Ipv4Address _vpnHost;
    QTimer _connectingTimer;
    static Executor _executor;
    std::unique_ptr<MtuPinger> _mtuPinger;
};

#endif
