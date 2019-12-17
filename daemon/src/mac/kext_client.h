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
#line HEADER_FILE("mac/kext_client.h")

#ifndef KEXT_CLIENT_H
#define KEXT_CLIENT_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <QSocketNotifier>
#include <QPointer>
#include <QTimer>
#include "daemon.h"
#include "posix/posix_firewall_pf.h"
#include "settings.h"
#include "vpn.h"

struct ProcQuery;

struct WhitelistPort
{
    uint32_t source_ip;
    uint32_t source_port;
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
    KextClient(QObject *pParent)
        : QObject{pParent},
          _state{State::Disconnected},
          _firewallState{},
          _sockFd{-1}
    {
    }

    ~KextClient()
    {
        qInfo() << "KextClient destructor called";
        shutdownConnection();
    }

public slots:
    void initiateConnection(const OriginalNetworkScan &netScan, const FirewallParams &params);
    void updateExcludedApps(QVector<QString> excludedApps);
    void updateNetwork(const OriginalNetworkScan &netScan, const FirewallParams &params);
    void shutdownConnection();

private:
    void readFromSocket(int socket);
    void showError(QString funcName);
    void verifyApp(const ProcQuery &proc_query, ProcQuery &proc_response);
    void processCommand(const ProcQuery &proc_query,  ProcQuery &proc_response);
    void removeApps(const QVector<QString> removedApps);
    int connectToKext(const OriginalNetworkScan &netScan, const FirewallParams &params);
    void removeCurrentBoundRoute();
    void createBoundRoute(const OriginalNetworkScan &netScan);
    void updateFirewall(QString ipAddress);
    void teardownFirewall();
    void updateIp(QString ipAddress);
    void sendExistingPids(const QVector<QString> &excludedApps);
    void updateKextFirewall(const FirewallParams &params);
    void manageWebKitExclusions(QVector<QString> &excludedApps);

public:
    enum CommandType
    {
        VerifyApp         // check whether the app should be excluded from VPN
    };
    Q_ENUM(CommandType)

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
    };

private:
    QVector<QString> _excludedApps;
    QPointer<QSocketNotifier> _readNotifier;
    OriginalNetworkScan _previousNetScan;
    State _state;
    FirewallState _firewallState;
    int _sockFd;
};

class KextMonitor : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("KextMonitor")
public:
    using NetExtensionState = DaemonState::NetExtensionState;

public:
    KextMonitor();

private:
    int runProc(const QString &cmd, const QStringList &args);
    void updateState(int exitCode);

public:
    void checkState();
    NetExtensionState lastState() const {return _lastState;}

    bool loadKext();
    bool unloadKext();

signals:
    void kextStateChanged(NetExtensionState newState);

private:
    NetExtensionState _lastState;
};

#endif
