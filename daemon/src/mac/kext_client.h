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
#line HEADER_FILE("mac/kext_client.h")

#ifndef KEXT_CLIENT_H
#define KEXT_CLIENT_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <deque>
#include <QSocketNotifier>
#include <QPointer>
#include <QTimer>
#include "daemon.h"
#include "posix/posix_firewall_pf.h"
#include "processrunner.h"
#include "settings.h"
#include "exec.h"
#include "vpn.h"

// Matches struct in Kernel Extension (see ip_firewall.h)
struct WhitelistPort
{
    uint32_t source_ip;
    uint32_t source_port;
};

// Matches struct in Kernel Extension (see ip_firewall.h)
struct WhitelistSubnet
{
    uint32_t network_ip;
    uint32_t prefix_length;
};

// Given a vector of application paths, this class is responsible for returning
// all the PIDs of processes launched from those paths.
class PidFinder
{
public:
    PidFinder(const QVector<QString> &paths)
        : _paths{paths}
    {
    }

    QSet<pid_t> pids();
    QVector<WhitelistPort> ports(const QSet<pid_t> &pids);

private:
    bool matchesPath(pid_t pid);
    QString pidToPath(pid_t);
    void findPidPorts(pid_t pid, QVector<WhitelistPort> &ports);

public:
    // The maximum number of PIDs and ports we support
    enum { maxPids = 500, maxPorts = 2000 };

private:
    QVector<QString> _paths;
};

class KextClient : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("KextClient")

public:
    // These enums and structs match the corresponding types in the kernel
    // extension
    enum CommandType
    {
        VerifyApp         // check whether the app should be excluded from VPN
    };
    Q_ENUM(CommandType)

    enum RuleType
    {
        BypassVPN,
        OnlyVPN
    };
    Q_ENUM(RuleType)

    struct ProcQuery
    {
        CommandType command;
        char needs_reply;
        char app_path[PATH_MAX];
        int pid;
        int accept;
        enum RuleType rule_type;
        uint32_t id;
        uint32_t source_ip;
        uint32_t source_port;
        uint32_t dest_ip;
        uint32_t dest_port;
        uint32_t bind_ip;

        // SOCK_STREAM or SOCK_DGRAM (tcp or udp)
        int socket_type;
    };

    struct TracedResponse
    {
        int count;
        ProcQuery response;
    };

public:
    KextClient(QObject *pParent)
        : QObject{pParent}
        ,  _state{State::Disconnected}
        ,  _firewallState{}
        ,  _sockFd{-1}
        ,  _hasConnected{false}
    {
    }

    ~KextClient()
    {
        qInfo() << "KextClient destructor called";
        shutdownConnection();
    }

public slots:
    void initiateConnection(const FirewallParams &params, QString tunnelDeviceName,
                            QString tunnelDeviceLocalAddress);
    void shutdownConnection();
    void updateSplitTunnel(const FirewallParams &params, QString tunnelDeviceName,
                           QString tunnelDeviceLocalAddress,
                           QVector<QString> excludedApps, QVector<QString> vpnOnlyApps);

private:
    enum class AddressType
    {
        Source,
        Destination
    };

    void readFromSocket(int socket);
    void showError(QString funcName);
    void verifyApp(const ProcQuery &proc_query, ProcQuery &proc_response);
    void processCommand(const ProcQuery &proc_query,  ProcQuery &proc_response);
    void removeCurrentBoundRoute(const QString &ipAddress, const QString &interfaceName);
    void createBoundRoute(const QString &ipAddress, const QString &interfaceName);
    void updateFirewall(QString ipAddress, bool hasConnected);
    void teardownFirewall();
    void updateIp(QString ipAddress, bool hasConnected);
    void sendExistingPids(const QVector<QString> &excludedApps);
    void sendBypassIpv4Subnets(const QSet<QString> &bypassSubnets);
    void updateKextFirewall(const FirewallParams &params, bool isConnected);
    void updateNetwork(const FirewallParams &params, QString tunnelDeviceName,
                       QString tunnelDeviceLocalAddress);
    void updateApps(QVector<QString> excludedApps, QVector<QString> vpnOnlyApps);

private:
    enum class State
    {
        Connected,
        Disconnected
    };

    struct FirewallState
    {
        // Determines the default policy for the Kext firewall, i.e block or allow
        bool killswitchActive;

        // Permit LAN, broadcast, multicast, link-local, etc
        bool allowLAN;

        // Does the VPN have the default route?
        bool defaultRoute;

        // Is the VPN connected ?
        bool isConnected;
    };

    // Maximum number of ipv4 bypass subnets we allow
    // We need to send these subnets to the kext for whitelisting
    enum { maxBypassSubnets = 500};
private:
    QVector<QString> _excludedApps;
    QVector<QString> _vpnOnlyApps;
    QPointer<QSocketNotifier> _readNotifier;
    OriginalNetworkScan _previousNetScan;
    QSet<QString> _previousBypassIpv4Subnets;
    QString _tunnelDeviceName;
    QString _tunnelDeviceLocalAddress;
    State _state;
    FirewallState _firewallState;
    int _sockFd;
    bool _hasConnected{false};
    // Tracing responses is really important for supportability of this feature
    // (if the daemon is checking PIDs incorrectly, taking too long to respond,
    // etc.), but these responses occur a _lot_, so we can't trace them all.
    // Keep track of the last few processes traced, so we can trace some of
    // these without completely overwhelming the log.
    std::deque<TracedResponse> _tracedResponses;

    static Executor _executor;
};

class KextMonitor : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("KextMonitor")
private:
    static const QStringList _kextLogStreamParams;
    enum
    {
        LogChunkSize = 128*1024,
    };
public:
    using NetExtensionState = DaemonState::NetExtensionState;

public:
    KextMonitor();

private:
    // Run a command (used for kext[util|load|unload]).
    // Logs the result, stdout, and stderr of the command.
    // If pStdErr is non-nullptr, the standard error output will be returned
    // there.
    int runProc(const QString &cmd, const QStringList &args,
                QString *pStdErr = nullptr);
    void updateState(int exitCode);

public:
    void checkState();
    NetExtensionState lastState() const {return _lastState;}

    bool loadKext();
    bool unloadKext();

    QString getKextLog() const;

signals:
    void kextStateChanged(NetExtensionState newState);

private:
    NetExtensionState _lastState;
    bool _loaded;
    // If debug logging is enabled, KextMonitor runs 'log stream' to capture
    // kext logging.  This is crucial for debug reports.  Running 'log show' at
    // report time did not work well, it often times out.
    ProcessRunner _kextLogStream;
    // Kext logs are stored in 128KB chunks.  When we reach 128KB, the current
    // chunk is moved to the old chunk (the old chunk is dropped).  This way, we
    // always have about 128KB - 256KB of logging.
    QByteArray _kextLog;
    QByteArray _oldKextLog;
};

#endif
