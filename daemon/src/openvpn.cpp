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
#line SOURCE_FILE("openvpn.cpp")

#include "openvpn.h"
#include <common/src/builtin/path.h>

#include <QNetworkProxy>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

OpenVPNProcess::OpenVPNProcess(QObject *parent)
    : QObject(parent)
    , _state(Created)
    , _process(new QProcess(this))
    , _managementServer(new QTcpServer(this))
    , _managementSocket(nullptr)
    , _remotePort(0)
    , _localPort(0)
    , _managementExitSignaled(false)
    , _exited(false)
{
    _process->setProgram(Path::OpenVPNExecutable);
    _process->setProcessEnvironment({});
    _process->setWorkingDirectory(Path::OpenVPNWorkingDir);
    _process->setStandardInputFile(QProcess::nullDevice());
    connect(_process, &QProcess::readyReadStandardOutput, this, &OpenVPNProcess::stdoutReadyRead);
    connect(_process, &QProcess::readyReadStandardError, this, &OpenVPNProcess::stderrReadyRead);
    connect(_process, &QProcess::errorOccurred, this, &OpenVPNProcess::processError);
    connect(_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &OpenVPNProcess::processFinished);

    _managementServer->setMaxPendingConnections(1);
    _managementServer->setProxy(QNetworkProxy::NoProxy);
    connect(_managementServer, &QTcpServer::newConnection, this, &OpenVPNProcess::managementConnected);
    connect(_managementServer, &QTcpServer::acceptError, this, [this](QAbstractSocket::SocketError error) {
        qCritical() << "Management socket accept error:" << _managementServer->errorString();
        raiseError(Error(HERE, Error::OpenVPNManagementAcceptError));
    });

    connect(this, &OpenVPNProcess::managementLine, this, &OpenVPNProcess::handleManagementLine);
}

void OpenVPNProcess::run(const QStringList& arguments)
{
    if (_state != Created)
    {
        qCritical("Attempted to re-run OpenVPNProcess");
        return;
    }
    _state = Connecting;
    if (!_managementServer->listen(QHostAddress::LocalHost))
    {
        qCritical() << "Management socket listen error:" << _managementServer->errorString();
        emit error(Error(HERE, Error::OpenVPNManagementListenError));
    }
    int port = _managementServer->serverPort();

    QStringList finalArguments = arguments;
    finalArguments += QStringLiteral("--management");
    finalArguments += QStringLiteral("127.0.0.1");
    finalArguments += QString::number(port);
    finalArguments += QStringLiteral("--management-hold");
    finalArguments += QStringLiteral("--management-client");
    finalArguments += QStringLiteral("--management-query-passwords");
    finalArguments += QStringLiteral("--remap-usr1");
    finalArguments += QStringLiteral("SIGTERM");

    _process->setArguments(finalArguments);
    _process->start(QIODevice::ReadOnly);
    _process->closeWriteChannel();

    sendManagementCommand(QLatin1String("state on"));
    sendManagementCommand(QLatin1String("bytecount 5"));
    sendManagementCommand(QLatin1String("hold release"));
}

void OpenVPNProcess::sendManagementCommand(QLatin1String command)
{
    bool empty = _managementWriteBuffer.size() == 0;
    _managementWriteBuffer.append(command.data(), command.size());
    _managementWriteBuffer.append('\n');
    if (empty) managementBytesWritten(0); // Restart write queue if it was exhausted
}

void OpenVPNProcess::shutdown()
{
    if (_state < Exiting)
    {
        QTimer::singleShot(3000, this, [this]()
        {
            if (_process && _process->state() == QProcess::Running) {
                qWarning() << "Sending TERM to openvpn process.";
                _process->terminate();
            }
        });
        sendManagementCommand(QLatin1String("signal SIGTERM"));
    }
}

void OpenVPNProcess::kill()
{

}

void OpenVPNProcess::stdoutReadyRead()
{
    _stdoutBuffer += _process->readAllStandardOutput();
    auto lines = _stdoutBuffer.split('\n');
    _stdoutBuffer = lines.last();
    for (int i = 0; i < lines.size() - 1; i++)
    {
        if (lines[i].endsWith('\r'))
            lines[i].chop(1);
        emit stdoutLine(QString::fromLatin1(lines[i]));
    }
}

void OpenVPNProcess::stderrReadyRead()
{
    _stderrBuffer += _process->readAllStandardError();
    auto lines = _stderrBuffer.split('\n');
    _stderrBuffer = lines.last();
    for (int i = 0; i < lines.size() - 1; i++)
    {
        if (lines[i].endsWith('\r'))
            lines[i].chop(1);
        emit stderrLine(QString::fromLatin1(lines[i]));
    }
}

void OpenVPNProcess::processError(QProcess::ProcessError error)
{
    auto lastErr = SystemError{HERE};
    QString description = QStringLiteral("<nullptr>");
    if(_process)
        description = _process->errorString();
    qWarning() << "Process error:" << traceEnum(error) << description;
    // There may be a GetLastError() / errno code (there usually is on Windows)
    qWarning() << "Possible last error code:" << lastErr;
    switch (error)
    {
    case QProcess::FailedToStart:
        raiseError(Error(HERE, Error::OpenVPNProcessFailedToStart));
        processFinished(0, QProcess::CrashExit);
        break;
    case QProcess::Crashed:
        raiseError(Error(HERE, Error::OpenVPNProcessCrashed));
        processFinished(0, QProcess::CrashExit);
        break;
    default:
        qCritical() << "Process error:" << _process->errorString();
        break;
    }
}

void OpenVPNProcess::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!std::exchange(_exited, true))
    {
        emit exited(exitStatus == QProcess::NormalExit ? exitCode : -1);
    }
}

void OpenVPNProcess::managementConnected()
{
    _managementSocket = _managementServer->nextPendingConnection();
    _managementSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    _managementServer->close();
    connect(_managementSocket, &QTcpSocket::readyRead, this, &OpenVPNProcess::managementReadyRead);
    connect(_managementSocket, &QTcpSocket::readChannelFinished, this, &OpenVPNProcess::managementReadFinished);
    connect(_managementSocket, &QTcpSocket::bytesWritten, this, &OpenVPNProcess::managementBytesWritten);
    if (_managementSocket->isReadable()) managementReadyRead();
    if (_managementSocket->isWritable()) managementBytesWritten(0);
}

void OpenVPNProcess::managementReadyRead()
{
    if (_managementSocket->bytesAvailable() > 0)
    {
        _managementReadBuffer += _managementSocket->readAll();
        auto lines = _managementReadBuffer.split('\n');
        _managementReadBuffer = lines.last();
        for (int i = 0; i < lines.size() - 1; i++)
        {
            if (lines[i].endsWith('\r'))
                lines[i].chop(1);
            emit managementLine(QString::fromLatin1(lines[i]));
        }
    }
}

void OpenVPNProcess::managementReadFinished()
{
    managementReadyRead();
    if (_managementReadBuffer.size() > 0)
    {
        emit managementLine(QString::fromLatin1(_managementReadBuffer));
        _managementReadBuffer.clear();
    }
}

void OpenVPNProcess::managementBytesWritten(qint64 bytes)
{
    if (bytes > 0)
    {
        if (bytes == _managementWriteBuffer.size())
            _managementWriteBuffer.resize(0);
        else
            _managementWriteBuffer.remove(0, (int)bytes);
    }
    if (_managementWriteBuffer.size() > 0 && _managementSocket)
    {
        if (_managementSocket->write(_managementWriteBuffer) < 0)
            raiseError(Error(HERE, Error::OpenVPNManagementWriteError));
    }
}

void OpenVPNProcess::raiseError(const Error& err)
{
    emit error(err);
}

void OpenVPNProcess::setState(State state)
{
    if (state != _state)
    {
        _state = state;
        emit stateChanged();
    }
}

void OpenVPNProcess::handleManagementLine(const QString& line)
{
    if (line.isEmpty() || line[0] != '>')
        return;
    if (line.startsWith(QLatin1String(">STATE:")))
    {
        auto params = line.midRef(7).split(',');
        if (params.size() < 2)
        {
            qWarning() << "Unrecognized OpenVPN state:" << line.midRef(7);
            return;
        }
        QStringRef description, tunnelIP, remoteIP, remotePort, localIP, localPort, tunnelIPv6;
        switch (params.size())
        {
        default:
        case 9: tunnelIPv6 = params[8];
        case 8: localPort = params[7];
        case 7: localIP = params[6];
        case 6: remotePort = params[5];
        case 5: remoteIP = params[4];
        case 4: tunnelIP = params[3];
        case 3: description = params[2];
        case 2: case 1: case 0: break;
        }

        auto assignUInt = [](uint& var, const QStringRef& str) { bool ok; uint value = str.toUInt(&ok); if (ok) var = value; };

        if (!tunnelIP.isEmpty())
            _tunnelIP = tunnelIP.toString();
        if (!tunnelIPv6.isEmpty())
            _tunnelIPv6 = tunnelIPv6.toString();
        if (!remoteIP.isEmpty())
            _remoteIP = remoteIP.toString();
        if (!localIP.isEmpty())
            _localIP = localIP.toString();
        if (!remotePort.isEmpty())
            assignUInt(_remotePort, remotePort);
        if (!localPort.isEmpty())
            assignUInt(_localPort, localPort);

        if (params[1] == QLatin1String("CONNECTING"))
            setState(Connecting);
        else if (params[1] == QLatin1String("RESOLVE"))
            setState(Resolve);
        else if (params[1] == QLatin1String("TCP_CONNECT"))
            setState(TCPConnect);
        else if (params[1] == QLatin1String("WAIT"))
            setState(Wait);
        else if (params[1] == QLatin1String("AUTH"))
            setState(Auth);
        else if (params[1] == QLatin1String("GET_CONFIG"))
            setState(GetConfig);
        else if (params[1] == QLatin1String("ASSIGN_IP"))
            setState(AssignIP);
        else if (params[1] == QLatin1String("ADD_ROUTES"))
            setState(AddRoutes);
        else if (params[1] == QLatin1String("CONNECTED"))
            setState(Connected);
        else if (params[1] == QLatin1String("RECONNECTING"))
            setState(Reconnecting);
        else if (params[1] == QLatin1String("EXITING"))
        {
            if (description == QLatin1String("tls-error"))
                raiseError(Error(HERE, Error::OpenVPNTLSHandshakeError));
            setState(Exiting);
        }
        else
            qWarning() << "Unrecognized OpenVPN state:" << line.midRef(7);
    }
    else if (line.startsWith(QLatin1String(">HOLD:")))
        sendManagementCommand(QLatin1String("hold release"));
}
