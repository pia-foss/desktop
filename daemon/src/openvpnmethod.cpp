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
#line SOURCE_FILE("openvpnmethod.cpp")

#include "openvpnmethod.h"
#include "daemon.h"
#include <common/src/builtin/path.h>
#include "pathmtu.h"
#include <kapps_core/src/ipaddress.h>
#include <QStandardPaths>

#if defined(Q_OS_WIN)
    #include "win/win_daemon.h"
    #include "win/win_interfacemonitor.h"
    #include <common/src/win/win_util.h>
#endif

#if defined(Q_OS_UNIX)
    #include "posix/posix_mtu.h"
#endif

HelperIpcConnection::HelperIpcConnection(QLocalSocket *pConnection)
    : _pConnection{pConnection}
{
    Q_ASSERT(pConnection);  // Ensured by caller

    _pConnection->setParent(nullptr);
    connect(_pConnection.get(), &QLocalSocket::readyRead, this, [this]()
    {
        _buffer.append(_pConnection->readAll());
    });
    connect(&_buffer, &LineBuffer::lineComplete, this, [this](const QByteArray &line)
    {
        QString lineStr{QString::fromUtf8(line)};
        qInfo() << "HELPER" << reinterpret_cast<qintptr>(_pConnection.get())
            << ":" << lineStr;
        emit ipcMessage(lineStr);
    });
}

HelperIpcConnection::~HelperIpcConnection()
{
    Q_ASSERT(_pConnection); // Class invariant

    // If any incomplete message was in the buffer, trace it
    auto incompleteLine = _buffer.reset();
    if(!incompleteLine.isEmpty())
    {
        qWarning() << "HELPER" << reinterpret_cast<qintptr>(_pConnection.get())
            << "- incomplete message:" << QString::fromUtf8(incompleteLine);
    }

    // Close the socket if it isn't already closed, may signal from destructor
    // otherwise.
    _pConnection->close();
}

HelperIpcServer::HelperIpcServer()
{
    _server.setSocketOptions(QLocalServer::SocketOption::UserAccessOption);
    connect(&_server, &QLocalServer::newConnection, this,
            &HelperIpcServer::acceptConnections);
}

HelperIpcServer::~HelperIpcServer()
{
    // Remove client connections and close the server to ensure they don't
    // signal from destructors.
    _clients.clear();
    _server.close();
}

void HelperIpcServer::acceptConnections()
{
    while(QLocalSocket *pNextClient = _server.nextPendingConnection())
    {
        auto emplaceResult = _clients.emplace(pNextClient,
            std::unique_ptr<HelperIpcConnection>{new HelperIpcConnection{pNextClient}});
        Q_ASSERT(emplaceResult.second); // New object, can't have already existed
        auto itNewClient = emplaceResult.first;
        Q_ASSERT(itNewClient != _clients.end());    // Postcondition of emplace()

        connect(pNextClient, &QLocalSocket::disconnected, this, [this, pNextClient]()
        {
            _clients.erase(pNextClient);
        }, Qt::ConnectionType::QueuedConnection);
        connect(itNewClient->second.get(), &HelperIpcConnection::ipcMessage, this,
                &HelperIpcServer::ipcMessage);
    }
}

bool HelperIpcServer::listen()
{
    if(!_server.listen(Path::DaemonHelperIpcSocket))
    {
        qWarning() << "Unable to listen on helper IPC socket:"
            << Path::DaemonHelperIpcSocket << "-" << _server.errorString();
        return false;
    }

    return true;
}

Executor OpenVPNMethod::_executor{CURRENT_CATEGORY};

OpenVPNMethod::OpenVPNMethod(QObject *pParent, const OriginalNetworkScan &netScan)
    : VPNMethod{pParent, netScan}, _openvpn{}
{
}

OpenVPNMethod::~OpenVPNMethod()
{
    if(_openvpn)
        delete _openvpn;
}

void OpenVPNMethod::run(const ConnectionConfig &connectingConfig,
                        const Server &vpnServer,
                        const Transport &transport,
                        const QHostAddress &localAddress,
                        const QHostAddress &shadowsocksServerAddress,
                        quint16 shadowsocksProxyPort)
{
    _connectingConfig = connectingConfig;
    _vpnHost = vpnServer.impl().address();

#if defined(Q_OS_WIN)
    if(!_helperIpcServer.listen())
    {
        // Traced by HelperIpcServer
        raiseError({HERE, Error::Code::OpenVPNHelperListenError});
        return;
    }
    connect(&_helperIpcServer, &HelperIpcServer::ipcMessage, this, [this](const QString &message)
    {
        // Line was traced by HelperIpcServer
        checkForMagicStrings(message);
    });
#endif

    QStringList arguments;
    try
    {
        arguments += QStringLiteral("--verb");
        arguments += QStringLiteral("4");

        _networkAdapter = g_daemon->getNetworkAdapter();
#if defined(Q_OS_WIN)
        // Postcondition of WinDaemon::getNetworkAdapter()
        Q_ASSERT(_networkAdapter);
        Q_ASSERT(!_networkAdapter->devNode().isEmpty());
        arguments += QStringLiteral("--dev-node");
        arguments += _networkAdapter->devNode();
#else
        // Postcondition of PosixDaemon::getNetworkAdapter() (if it did start
        // returning a network adapter, it's ignored here)
        Q_ASSERT(!_networkAdapter);
    #if defined(Q_OS_MAC)
        arguments += QStringLiteral("--dev-node");
        arguments += QStringLiteral("utun");
    #endif
#endif

        arguments += QStringLiteral("--script-security");
        arguments += QStringLiteral("2");

        auto escapeArg = [](QString arg)
        {
            // Escape backslashes and spaces in the command.  Note this is the
            // same even on Windows, because OpenVPN parses this command line
            // with its internal parser, then re-joins and re-quotes it on
            // Windows with Windows quoting conventions.
            arg.replace(QLatin1String(R"(\)"), QLatin1String(R"(\\)"));
            arg.replace(QLatin1String(R"( )"), QLatin1String(R"(\ )"));
            return arg;
        };

        QString updownCmd;

#if defined(Q_OS_WIN)
        updownCmd += escapeArg(QString::fromLocal8Bit(qgetenv("WINDIR")) + "\\system32\\cmd.exe");
        updownCmd += QStringLiteral(" /C call ");
#endif
        updownCmd += escapeArg(Path::OpenVPNUpDownScript);
#if defined(Q_OS_LINUX)
        // OpenVPN invokes our up/down script without PATH (the environment is
        // empty other than the values OpenVPN specifically adds).  Add PATH
        // back in to ensure we can find `ip` if it's somewhere like /usr/sbin
        // (Fedora 33).  We can't apply environment variables with OpenVPN's
        // interface, so the script accepts --path <PATH> on the command line.
        // We also have to break it into <256 char chunks in order to work
        // around OpenVPN's arg limits.
        QString actualPath{QString::fromLocal8Bit(qgetenv("PATH"))};
        enum : int { PathChunk=120 };   // Allows headroom even in extreme cases of quoting
        for(int i=0; i<actualPath.size(); i += PathChunk)
        {
            updownCmd += QStringLiteral(" --path ");
            updownCmd += escapeArg(actualPath.mid(i, PathChunk));
        }
#endif

#ifdef Q_OS_WIN
        // Only the Windows updown script supports logging right now.  Enable it
        // if debug logging is turned on.
        if(g_settings.debugLogging())
        {
            updownCmd += " --log ";
            updownCmd += escapeArg(Path::UpdownLogFile);
        }

        // Pass the configuration method on Windows
        updownCmd += " --method ";
        updownCmd += escapeArg(g_settings.windowsIpMethod());

        // Pass the IPC pipe/socket path to receive tunnel configuration
        updownCmd += " --ipc ";
        updownCmd += escapeArg(_helperIpcServer.fullServerName());
#endif

        QStringList dnsServers;

        if(_connectingConfig.setDefaultDns())
            dnsServers = _connectingConfig.getDnsServers();

        if(!dnsServers.isEmpty())
        {
            updownCmd += " --dns ";
            updownCmd += dnsServers.join(':');
        }

        // Terminate PIA args with '--' (OpenVPN passes several subsequent
        // arguments)
        updownCmd += " --";

        qInfo() << "updownCmd is: " << updownCmd;

#ifdef Q_OS_WIN
        if(g_settings.windowsIpMethod() == QStringLiteral("static"))
        {
            // Static configuration on Windows - use OpenVPN's netsh method,
            // updown script will apply DNS with netsh
            arguments += "--ip-win32";
            arguments += "netsh";
        }
        else
        {
            // DHCP configuration on Windows - use OpenVPN's default ip-win32
            // method, pass DNS servers as DHCP options
            for(const auto &dnsServer : dnsServers)
            {
                arguments += "--dhcp-option";
                arguments += "DNS";
                arguments += dnsServer;
            }
        }
#endif

#ifdef Q_OS_LINUX
        // Pass path to ip command
        QString ipCmd(QStandardPaths::findExecutable("ip"));
        if(!ipCmd.isEmpty())
        {
            arguments += "--iproute";
            arguments += ipCmd;
        }
#endif

        // Use the same script for --up and --down
        arguments += "--up";
        arguments += updownCmd;
        arguments += "--down";
        arguments += updownCmd;

        arguments += QStringLiteral("--config");

        QFile configFile(Path::OpenVPNConfigFile);
        if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text) ||
            !writeOpenVPNConfig(configFile, vpnServer, transport, localAddress,
                                shadowsocksServerAddress,
                                shadowsocksProxyPort))
        {
            throw Error(HERE, Error::OpenVPNConfigFileWriteError);
        }
        configFile.close();

        arguments += Path::OpenVPNConfigFile;
    }
    catch (const Error& ex)
    {
        // Can't build arguments - cannot connect at all.  Stay in Created state
        // (we didn't create _openvpn, in principle the OpenVPNMethod could be
        // started again).  Emit an error - VPNConnection will call shutdown().
        qInfo() << "Can't start OpenVPN, couldn't construct arguments -" << ex;
        raiseError(ex);
        return;
    }

    if (_networkAdapter)
    {
        // Ensure our tunnel has priority over other interfaces. This is especially important for DNS.
        _networkAdapter->setMetricToLowest();
    }

    _openvpn = new OpenVPNProcess(this);
    advanceState(State::Connecting);

    connect(_openvpn, &OpenVPNProcess::stdoutLine, this, &OpenVPNMethod::openvpnStdoutLine);
    connect(_openvpn, &OpenVPNProcess::stderrLine, this, &OpenVPNMethod::openvpnStderrLine);
    connect(_openvpn, &OpenVPNProcess::managementLine, this, &OpenVPNMethod::openvpnManagementLine);
    connect(_openvpn, &OpenVPNProcess::stateChanged, this, &OpenVPNMethod::openvpnStateChanged);
    connect(_openvpn, &OpenVPNProcess::exited, this, &OpenVPNMethod::openvpnExited);
    connect(_openvpn, &OpenVPNProcess::error, this, &OpenVPNMethod::raiseError);

    _connectingTimer.setSingleShot(true);
    _connectingTimer.setInterval(13 * 1000); //slightly longer than
                                             //server-poll-timeout passed
                                             //to openvpn
    connect(&_connectingTimer, &QTimer::timeout, this, [this]() {
                                                         qInfo() << "connecting timeout.";
                                                         raiseError({HERE, Error::Code::OpenVPNError});
                                                       });
    _connectingTimer.start();
    _openvpn->run(arguments);
}

void OpenVPNMethod::shutdown()
{
    _connectingTimer.stop();
    if(_openvpn)
        _openvpn->shutdown();
    else
    {
        // Nothing to shut down, just go to the Exited state
        advanceState(State::Exited);
    }
}

void OpenVPNMethod::networkChanged()
{
    // OpenVPNMethod doesn't currently do anything when the network state
    // changes.  On some platforms, OpenVPN or our helper will detect the change
    // and kill the connection; on other platforms, we wait for a ping timeout.
    // In the future we might be able to reconnect more quickly by detecting
    // connection loss due to a network change.
}

bool OpenVPNMethod::writeOpenVPNConfig(QFile& outFile,
                                       const Server &vpnServer,
                                       const Transport &transport,
                                       const QHostAddress &localAddress,
                                       const QHostAddress &shadowsocksServerAddress,
                                       quint16 shadowsocksProxyPort)
{
    QTextStream out{&outFile};
    const char endl = '\n';

    auto sanitize = [](const QString& s) {
        QString result;
        result.reserve(s.size());
        for (auto& c : s)
        {
            switch (c.unicode())
            {
            case '\n':
            case '\t':
            case '\r':
            case '\v':
            case '"':
            case '\'':
                break;
            default:
                result.push_back(c);
                break;
            }
        }
        return result;
    };

    if (!_connectingConfig.vpnLocation())
        return false;

    out << "client" << endl;
    out << "dev tun" << endl;

    if (transport.protocol() == QStringLiteral("tcp"))
        out << "proto tcp-client" << endl;
    else
    {
        out << "proto udp" << endl;
        out << "explicit-exit-notify" << endl;
    }

    QString remoteServer = sanitize(vpnServer.ip());
    out << "remote " << remoteServer << ' ' << transport.port() << endl;

    if (vpnServer.commonName().isEmpty())
        return false;
    out << "verify-x509-name " << sanitize(vpnServer.commonName()) << " name" << endl;

    // OpenVPN's default setting is 'ping-restart 120'.  This means it takes up
    // to 2 minutes to notice loss of connection.  (On some OSes/systems it may
    // notice a change in local network connectivity more quickly, but this is
    // not reliable and cannot detect connection loss due to an upstream
    // change.)  This affects both TCP and UDP.
    //
    // 2 minutes is longer than most users seem to wait for the app to
    // reconnect, based on feedback and reports they usually give up and try to
    // reconnect manually before then.
    //
    // We've also seen examples where OpenVPN does not seem to actually exit
    // following connection loss, though we have not been able to reproduce
    // this.  It's possible that this has to do with the default 'ping-restart'
    // vs. 'ping-exit'.
    //
    // Enable ping-exit 25 to attempt to detect connection loss more quickly and
    // ensure OpenVPN exits on connection loss.
    out << "ping 5" << endl;
    out << "ping-exit 25" << endl;

    out << "persist-remote-ip" << endl;
    out << "resolv-retry 0" << endl;
    out << "route-delay 0" << endl;
    out << "reneg-sec 0" << endl;
    out << "server-poll-timeout 10s" << endl;
    out << "tls-client" << endl;
    out << "tls-exit" << endl;
    out << "remote-cert-tls server" << endl;
    out << "auth-user-pass" << endl;
    out << "pull-filter ignore \"auth-token\"" << endl;

    // Increasing sndbuf/rcvbuf can boost throughput, which is what most users
    // prioritize. For now, just copy the values used in the previous client.
    out << "sndbuf 262144" << endl;
    out << "rcvbuf 262144" << endl;

    if (_connectingConfig.setDefaultRoute())
    {
        out << "redirect-gateway def1 bypass-dhcp";
        if (!g_settings.blockIPv6())
            out << " ipv6";
        // Handle the VPN endpoint route ourselves.
        out << " local";
        out << endl;
    }
    else
    {
#ifndef Q_OS_MAC
        // Add a default route with much a worse metric, so traffic can still
        // be routed on the tunnel opt-in by binding to the tunnel interface.
        //
        // On Linux, metrics have been observed as high as 20600 on wireless
        // interfaces.  OpenVPN is still using `route` though, which interprets
        // the metric as 16-bit signed, so 32000 is about as high as we can go.
        //
        // OpenVPN's routing is broken on Mac, it ends up deleting the normal
        // default gateway route instead on shutdown.  KextClient adds a route
        // like this on Mac.  We need this on Windows for OpenVPN's dynamic
        // IPHelper/netsh fallback.  It's fine on Linux too.
        out << "route 0.0.0.0 0.0.0.0 vpn_gateway 32000" << endl;
#endif

        // Ignore pushed settings to add default route
        out << "pull-filter ignore \"redirect-gateway \"" << endl;
    }

    // Route the VPN host and/or proxy remote address via the default gateway,
    // not the VPN.  Do this even if we're not setting the default route - on
    // Mac the split tunnel device may have the default route, and on any
    // platform there could be other routes created by the user.

    // When using a SOCKS proxy, if that proxy is on the Internet, we need
    // to route it back to the default gateway (like we do with the VPN
    // server).
    if(_connectingConfig.proxyType() != ConnectionConfig::ProxyType::None)
    {
        QHostAddress remoteHost;
        switch(_connectingConfig.proxyType())
        {
            default:
            case ConnectionConfig::ProxyType::None:
                Q_ASSERT(false);
                break;
            case ConnectionConfig::ProxyType::Custom:
                remoteHost = _connectingConfig.socksHost();
                break;
            case ConnectionConfig::ProxyType::Shadowsocks:
                remoteHost = shadowsocksServerAddress;
                break;
        }
        // If the remote host address couldn't be parsed, fail now.
        if(remoteHost.isNull())
            throw Error{HERE, Error::Code::OpenVPNProxyResolveError};

        // We do _not_ want this route if the SOCKS proxy is on LAN or
        // localhost.  (It actually mostly works, but on Windows with a LAN
        // proxy it does reroute the traffic through the gateway.)
        //
        // This may be incorrect with nested LANs - a SOCKS proxy on the outer
        // LAN is detected as LAN, but we actually do want this route since it's
        // not on _our_ LAN.  We don't fully support nested LANs right now, and
        // the only fix for that is to actually read the entire routing table.
        // Users can work around this by manually creating this route to the
        // SOCKS proxy.
        kapps::core::Ipv4Address remoteHostIpv4{remoteHost.toIPv4Address()};
        if(remoteHostIpv4.isLoopback() || remoteHostIpv4.isPrivateUse())
        {
            qInfo() << "Not creating route for local proxy" << remoteHost;
        }
        else
        {
            qInfo() << "Creating gateway route for Internet proxy"
                << remoteHost;

            // OpenVPN still creates a route to the remote endpoint when doing
            // this, which might seem unnecessary but is still desirable if the
            // proxy is running on localhost.  (Or even if the proxy is not
            // actually on localhost but ends up communicating over this host,
            // such as a proxy running in a VM on this host.)
            out << "route " << remoteHost.toString()
                << " 255.255.255.255 net_gateway 0" << endl;
        }
    }

    // Route the VPN server back to the default gateway too.  Even if using a
    // proxy, if that proxy communcates via localhost, we need this route.
    // (Otherwise, traffic would be recursively routed back into the proxy.)
    //
    // This is needed for proxies on localhost, on a VM running on this host,
    // etc., and it doesn't hurt anything when it's not needed.  The killswitch
    // still blocks these connections by default though, so this also requires
    // turning killswitch off or manually creating firewall rules to exempt the
    // proxy.
    out << "route " << remoteServer << " 255.255.255.255 net_gateway 0"
        << endl;

    // If we're setting the system default DNS, route the DNS servers through
    // the tunnel.  This is important if split tunnel DNS is disabled (or not
    // available - macOS) and the VPN isn't getting the default route.
    if(_connectingConfig.setDefaultDns())
    {
        // Route DNS servers into the VPN (DNS is always sent through the VPN)
        for(const auto &dnsServer : _connectingConfig.getDnsServers())
        {
            if(!kapps::core::Ipv4Address{dnsServer.toStdString()}.isLocalDNS())
                out << "route " << dnsServer << " 255.255.255.255 vpn_gateway 0" << endl;
        }
    }

    // Set the local address only for alternate transports
    if(!localAddress.isNull())
    {
        out << "local " << localAddress.toString() << endl;
        // We can't use nobind with a specific local address.  We can set lport
        // to 0 to let the network stack pick an ephemeral port though.
        out << "lport " << _connectingConfig.localPort() << endl;
    }
    else if (_connectingConfig.localPort() == 0)
        out << "nobind" << endl;
    else
        out << "lport " << _connectingConfig.localPort() << endl;

    out << "cipher " << sanitize(_connectingConfig.openvpnCipher()) << endl;

    // We support both NCP cipher negotiation and pia-signal-settings right now
    // as we transition from pia-signal-settings to unpatched vanilla OpenVPN.
    // Use whatever method was indicated for this server.
    bool supportsNcp = (transport.protocol() == QStringLiteral("tcp")) ?
        vpnServer.openVpnTcpNcp() : vpnServer.openVpnUdpNcp();
    if(supportsNcp)
    {
        // NCP mode.  Permit only the configured cipher.
        out << "ncp-ciphers " << sanitize(_connectingConfig.openvpnCipher()) << endl;
        // "cipher" was still set above.  This isn't strictly necessary but is
        // good for robustness, OpenVPN's settings have been somewhat fragile in
        // the past, and this ensures we won't fall back to BF-CBC if (say)
        // NCP stops working on the server, etc.
    }
    else
    {
        out << "pia-signal-settings" << endl;
        // Explicitly disable NCP.  The OpenVPN NCP logic still otherwise thinks
        // the server is using BF-CBC (it's not really fully aware of
        // pia-signal-settings) and fails the connection.
        out << "ncp-disable" << endl;
    }

    if(_connectingConfig.proxyType() != ConnectionConfig::ProxyType::None)
    {
        // If the host resolve step failed, _socksRouteAddress is not set, fail.
        if(_connectingConfig.socksHost().isNull())
            throw Error{HERE, Error::Code::OpenVPNProxyResolveError};

        uint port = 0;
        // A Shadowsocks local proxy uses an ephemeral port
        if(_connectingConfig.proxyType() == ConnectionConfig::ProxyType::Shadowsocks)
            port = shadowsocksProxyPort;
        else
            port = _connectingConfig.customProxy().port();

        // Default to 1080 ourselves - OpenVPN doesn't like 0 here, and we can't
        // leave the port blank if we also need to specify "stdin" for auth.
        if(port == 0)
            port = 1080;
        out << "socks-proxy " << _connectingConfig.socksHost().toString() << " " << port;
        // If we have a username / password, ask OpenVPN to prompt over the
        // management interface.
        if(!_connectingConfig.customProxy().username().isEmpty() ||
           !_connectingConfig.customProxy().password().isEmpty())
        {
            out << " stdin";
        }
        out << endl;

        // The default timeout is very long (2 minutes), use a shorter timeout.
        // This applies to both the SOCKS and OpenVPN connections, we might
        // apply this even when not using a proxy in the future.
        out << "connect-timeout 30" << endl;
    }

    // Always ignore pushed DNS servers and use our own settings.
    out << "pull-filter ignore \"dhcp-option DNS \"" << endl;
    out << "pull-filter ignore \"dhcp-option DOMAIN local\"" << endl;

    out << "<ca>" << endl;
    out << g_daemon->environment().getCertificateAuthority(QStringLiteral("RSA-4096")) << endl;
    out << "</ca>" << endl;
    return true;
}

void OpenVPNMethod::openvpnStateChanged()
{
    OpenVPNProcess::State openvpnState = _openvpn ? _openvpn->state() : OpenVPNProcess::State::Exited;

    switch(openvpnState)
    {
        case OpenVPNProcess::State::Created:
            // Just sanity check that we never go back to this state, this is a
            // no-op as long as we're still in the Created state
            advanceState(State::Created);
            break;
        case OpenVPNProcess::State::AssignIP:
            advanceState(State::Connecting);
            break;
        case OpenVPNProcess::State::Connecting:
        case OpenVPNProcess::State::Resolve:
        case OpenVPNProcess::State::TCPConnect:
        case OpenVPNProcess::State::Wait:
        case OpenVPNProcess::State::Auth:
        case OpenVPNProcess::State::GetConfig:
        case OpenVPNProcess::State::AddRoutes:
            advanceState(State::Connecting);
            break;
        case OpenVPNProcess::State::Connected:
            _connectingTimer.stop();
            advanceState(State::Connected);
            break;
        case OpenVPNProcess::State::Reconnecting:
            // In some rare cases OpenVPN reconnects on its own (e.g. TAP adapter I/O failure),
            // but we don't want it to try to reconnect on its own; send a SIGTERM instead.
            qWarning() << "OpenVPN trying to reconnect internally, sending SIGTERM";
            shutdown();
            break;
        case OpenVPNProcess::State::Exiting:
            _connectingTimer.stop();
            advanceState(State::Exiting);
            break;
        case OpenVPNProcess::State::Exited:
            _connectingTimer.stop();
            advanceState(State::Exited);
            break;
        default:
            Q_ASSERT(false);    // Handled all states
            break;
    }
}


void OpenVPNMethod::openvpnStdoutLine(const QString& line)
{
    FUNCTION_LOGGING_CATEGORY("openvpn.stdout");
    qDebug() << line;

    checkStdoutErrors(line);
}

void OpenVPNMethod::checkStdoutErrors(const QString &line)
{
    // Check for specific errors that we can detect from OpenVPN's output.

    if(line.contains("socks_username_password_auth: server refused the authentication"))
    {
        raiseError({HERE, Error::Code::OpenVPNProxyAuthenticationError});
        return;
    }

    // This error can be logged by socks_handshake and recv_socks_reply, others
    // might be possible too.
    if(line.contains(QRegExp("socks.*: TCP port read failed")))
    {
        raiseError({HERE, Error::Code::OpenVPNProxyError});
        return;
    }

    // This error occurs if OpenVPN fails to open a TCP connection.  If it's
    // reported for the SOCKS proxy, report it as a proxy error, otherwise let
    // it be handled as a general failure.
    QRegExp tcpFailRegex{R"(TCP: connect to \[AF_INET\]([\d\.]+):\d+ failed:)"};
    if(line.contains(tcpFailRegex))
    {
        QHostAddress failedHost;
        if(failedHost.setAddress(tcpFailRegex.cap(1)) &&
            failedHost == _connectingConfig.socksHost())
        {
            raiseError({HERE, Error::Code::OpenVPNProxyError});
            return;
        }
    }
}

void OpenVPNMethod::openvpnStderrLine(const QString& line)
{
    FUNCTION_LOGGING_CATEGORY("openvpn.stderr");
    qDebug() << line;

    checkForMagicStrings(line);
}

unsigned OpenVPNMethod::findMaxMtu(const kapps::core::Ipv4Address &host)
{
    int mtu = 0;

    // Find an MTU if none was specified.
    // Find the MTU for the interface used to reach the remote host.
#if defined(Q_OS_WIN)
    mtu = originalNetwork().mtu();
#else
    mtu = PosixMtu::findHostMtu(qs::toQString(host.toString()));
#endif
    // Default to 1500 if no MTU was found.  Never assume the MTU to the
    // Internet is larger than 1500, even if the physical interface claims to
    // have a larger link MTU.
    if(!mtu || mtu > 1500)
        mtu = 1500;
    // Subtract overhead for encapsulation (OpenVPN with AES GCM; overhead
    // varies by transport due to additional framing needed in TCP)
    if(_connectingConfig.openvpnProtocol() == ConnectionConfig::Protocol::TCP)
        mtu -= 74;
    else
        mtu -= 52;

    return mtu;
}

void OpenVPNMethod::checkForMagicStrings(const QString& line)
{
    QRegExp tunDeviceNameRegex{R"(Using device:([^ ]+) local_address:([^ ]+) remote_address:([^ ]+))"};
    if(line.contains(tunDeviceNameRegex))
    {
        if(!_networkAdapter)
            _networkAdapter.reset(new NetworkAdapter{tunDeviceNameRegex.cap(1)});

        int maxMtu = findMaxMtu(_vpnHost);
        qInfo() << "MTU config:" << _connectingConfig.mtu()
            << " - calculated tunnel MTU to VPN host" << _vpnHost << ":"
            << maxMtu;

        _mtuPinger.reset(new MtuPinger(_networkAdapter, maxMtu, _connectingConfig.mtu()));

        emitTunnelConfiguration(tunDeviceNameRegex.cap(1), tunDeviceNameRegex.cap(2),
                                tunDeviceNameRegex.cap(3));
    }

    // TODO: extract this out into a more general error mechanism, where the "!!!" prefix
    // indicates an error condition followed by the code.
    if (line.startsWith("!!!updown.sh!!!dnsConfigFailure")) {
        raiseError(Error(HERE, Error::OpenVPNDNSConfigError));
    }
}

bool OpenVPNMethod::respondToMgmtAuth(const QString &line, const QString &user,
                                      const QString &password)
{
    // Extract the auth type from the prompt - such as `Auth` or `SOCKS Proxy`.
    // The line looks like:
    // >PASSWORD:Need 'Auth' username/password
    auto quotes = line.midRef(15).split('\'');
    if (quotes.size() >= 3)
    {
        auto id = quotes[1].toString();
        auto cmd = QStringLiteral("username \"%1\" \"%2\"\npassword \"%1\" \"%3\"")
                .arg(id, user, password);
        _openvpn->sendManagementCommand(QLatin1String(cmd.toLatin1()));
        return true;
    }

    qError() << "Invalid password request";
    return false;
}

void OpenVPNMethod::openvpnManagementLine(const QString& line)
{
    FUNCTION_LOGGING_CATEGORY("openvpn.mgmt");
    qDebug() << line;

    if (!line.isEmpty() && line[0] == '>')
    {
        if (line.startsWith(QLatin1String(">PASSWORD:")))
        {
            // SOCKS proxy auth
            if (line.startsWith(QLatin1String(">PASSWORD:Need 'SOCKS Proxy'")))
            {
                if(respondToMgmtAuth(line, _connectingConfig.customProxy().username(),
                                     _connectingConfig.customProxy().password()))
                {
                    return;
                }
            }
            // Normal password authentication
            // The type is usually 'Auth', but use these creds by default to
            // preserve existing behavior.
            else if (line.startsWith(QLatin1String(">PASSWORD:Need ")))
            {
                if(respondToMgmtAuth(line, _connectingConfig.vpnUsername(),
                                     _connectingConfig.vpnPassword()))
                    return;
            }
            else if (line.startsWith(QLatin1String(">PASSWORD:Auth-Token:")))
            {
                // TODO: PIA servers aren't set up to use this properly yet
                return;
            }
            else if (line.startsWith(QLatin1String(">PASSWORD:Verification Failed: ")))
            {
                raiseError(Error(HERE, Error::OpenVPNAuthenticationError));
                return;
            }
            // All unhandled cases
            raiseError(Error(HERE, Error::OpenVPNAuthenticationError));
        }
        else if (line.startsWith(QLatin1String(">BYTECOUNT:")))
        {
            auto params = line.midRef(11).split(',');
            if (params.size() >= 2)
            {
                bool ok = false;
                quint64 up, down;
                if (((down = params[0].toULongLong(&ok)), ok) && ((up = params[1].toULongLong(&ok)), ok))
                {
                    emitBytecounts(down, up);
                }
            }
        }
    }
}

void OpenVPNMethod::openvpnExited(int exitCode)
{
    if (_networkAdapter)
    {
        // Ensure we return our tunnel metric to how it was before we lowered it.
        _networkAdapter->restoreOriginalMetric();
        _networkAdapter.reset();
    }
    _openvpn->deleteLater();
    _openvpn = nullptr;
    // This advances to 'exited' since we've wiped out _openvpn
    openvpnStateChanged();
}
