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
#line HEADER_FILE("openvpn.h")

#ifndef OPENVPN_H
#define OPENVPN_H
#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QStringList>

/**
 * @brief The OpenVPNProcess class abstracts the handling of a single OpenVPN
 * process. It hooks up the standard output, standard error and management
 * interface streams to line-based signals. This class is used to manage a
 * single connection attempt; the Connection class uses this class in order
 * to implement an ongoing VPN connection, disabling OpenVPN's built-in
 * reconnect handling in favor of our own.
 */
class OpenVPNProcess : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("openvpn")

public:
    /**
     * @brief The State enum describes the various states the OpenVPN process
     * can be in, from initialization to termination. This closely matches
     * the states reported by the management interface.
     */
    enum State
    {
        Created,
        Connecting,
        Resolve,
        TCPConnect,
        Wait,
        Auth,
        GetConfig,
        AssignIP,
        AddRoutes,
        Connected,
        Reconnecting,
        Exiting,
        Exited,
    };
    Q_ENUM(State)

public:
    explicit OpenVPNProcess(QObject *parent = nullptr);

    void run(const QStringList& arguments);

    State state() const { return _state; }
    QString tunnelIP() const { return _tunnelIP; }
    QString tunnelIPv6() const { return _tunnelIPv6; }
    QString remoteIP() const { return _remoteIP; }
    QString localIP() const { return _localIP; }
    uint remotePort() const { return _remotePort; }
    uint localPort() const { return _localPort; }

signals:
    void stdoutLine(const QString& line);
    void stderrLine(const QString& line);
    void managementLine(const QString& line);
    void stateChanged();
    void exited(int exitCode);
    void error(const Error& error);

public slots:
    void sendManagementCommand(QLatin1String command);
    void shutdown();
    void kill();

private slots:
    void stdoutReadyRead();
    void stderrReadyRead();
    void processError(QProcess::ProcessError error);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void managementConnected();
    void managementReadyRead();
    void managementReadFinished();
    void managementBytesWritten(qint64 bytes);
    void raiseError(const Error& error);

protected:
    void setState(State state);
    void handleManagementLine(const QString& line);

private:
    State _state;
    class QProcess* _process;
    class QTcpServer* _managementServer;
    class QTcpSocket* _managementSocket;

    QByteArray _stdoutBuffer, _stderrBuffer;
    QByteArray _managementReadBuffer, _managementWriteBuffer;

    QString _tunnelIP, _tunnelIPv6;
    QString _remoteIP, _localIP;
    uint _remotePort, _localPort;

    bool _managementExitSignaled;
    bool _exited;
};

#endif // OPENVPN_H
