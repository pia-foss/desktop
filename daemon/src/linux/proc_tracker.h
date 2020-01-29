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
#line HEADER_FILE("linux/proc_tracker.h")

#ifndef PROC_TRACKER_H
#define PROC_TRACKER_H

#include <QSocketNotifier>
#include <QPointer>
#include <QDir>
#include "daemon.h"
#include "posix/posix_firewall_pf.h"
#include "vpn.h"
#include "daemon.h"

// Convenience class for working with the Linux /proc VFS
class ProcFs
{
public:
    // Return all pids for the given executable path
    static QSet<pid_t> pidsForPath(const QString &path);

    // Return all (immediate) children pids of parentPid
    static QSet<pid_t> childPidsOf(pid_t parentPid);

    // Iterate and filter over all process PIDs in /proc
    static QSet<pid_t> filterPids(const std::function<bool(pid_t)> &filterFunc);

    // Given a pid, return the launch path for the process
    static QString pathForPid(pid_t pid);

    // Is pid a child of parentPid ?
    static bool isChildOf(pid_t parentPid, pid_t pid);
};

class ProcTracker : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("ProcTracker")

public:
    ProcTracker(QObject *pParent)
        : QObject{pParent},
          _sockFd{-1}
    {
    }

    ~ProcTracker()
    {
        shutdownConnection();
    }

public slots:
    void initiateConnection(const FirewallParams &params, QString tunnelDeviceName,
                            QString tunnelDeviceLocalAddress, QString tunnelDeviceRemoteAddress);
    void readFromSocket(int socket);
    void shutdownConnection();
    void updateSplitTunnel(const FirewallParams &params, QString tunnelDeviceName,
                           QString tunnelDeviceLocalAddress, QString tunnelDeviceRemoteAddress,
                           QVector<QString> excludedApps, QVector<QString> vpnOnlyApps);

private:
    using AppMap = QHash<QString, QSet<pid_t>>;

    void showError(QString funcName);

    int subscribeToProcEvents(int sock, bool enable);
    void addPidToCgroup(pid_t pid, const Path &cGroupPath);
    void removePidFromCgroup(pid_t pid, const Path &cGroupPath);
    void addChildPidsToCgroup(pid_t parentPid, const Path &cGroupPath);
    void removeChildPidsFromCgroup(pid_t parentPid, const Path &cGroupPath);
    // Remove apps that are no longer in this group - removes apps and PIDs from
    // appMap that do not appear in keepApps
    void removeApps(const QVector<QString> &keepApps, AppMap &appMap);
    void addApps(const QVector<QString> &apps, AppMap &appMap, QString cGroupPath);
    void removeAllApps();
    void writePidToCGroup(pid_t pid, const QString &cGroupPath);
    QSet<pid_t> pidsForPath(const QString &path);
    QString pathForPid(pid_t pid);
    void addLaunchedApp(pid_t pid);
    void removeTerminatedApp(pid_t pid);
    void updateMasquerade(QString interfaceName);
    void updateRoutes(QString gatewayIp, QString interfaceName, QString tunnelDeviceName, QString tunnelDeviceRemoteAddress);
    void updateNetwork(const FirewallParams &params, QString tunnelDeviceName,
                       QString tunnelDeviceLocalAddress, QString tunnelDeviceRemoteAddress);
    void addRoutingPolicyForSourceIp(QString ipAddress);
    void removeRoutingPolicyForSourceIp(QString ipAddress);
    void setupFirewall();
    void teardownFirewall();
    void setupReversePathFiltering();
    void teardownReversePathFiltering();
    void updateApps(QVector<QString> excludedApps, QVector<QString> vpnOnlyApps);

private:
    QPointer<QSocketNotifier> _readNotifier;
    OriginalNetworkScan _previousNetScan;
    QString _previousRPFilter;
    AppMap _exclusionsMap;
    AppMap _vpnOnlyMap;
    int _sockFd;
};

#endif
