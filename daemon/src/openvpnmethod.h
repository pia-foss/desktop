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
#line HEADER_FILE("openvpnmethod.h")

#ifndef OPENVPNMETHOD_H
#define OPENVPNMETHOD_H

#include "vpnmethod.h"
#include "openvpn.h"
#include "vpn.h"

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
                     const Transport &transport,
                     const QHostAddress &localAddress,
                     quint16 shadowsocksProxyPort) override;
    virtual void shutdown() override;
    virtual std::shared_ptr<NetworkAdapter> getNetworkAdapter() const override {return _networkAdapter;}

private:
    bool writeOpenVPNConfig(QFile& outFile,
                            const Transport &transport,
                            const QHostAddress &localAddress,
                            const QStringList &dnsServers,
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

private:
    OpenVPNProcess *_openvpn;
    // The network adapter used for the current connection
    std::shared_ptr<NetworkAdapter> _networkAdapter;
    ConnectionConfig _connectingConfig;
};

#endif
