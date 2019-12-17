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


#include <linux/netlink.h>
#include <linux/cn_proc.h>
#include <linux/connector.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QRegularExpression>
#include <QPointer>
#include <QFile>
#include <QDir>
#include "daemon.h"
#include "path.h"
#include "posix/posix_firewall_iptables.h"
#include "proc_tracker.h"

namespace
{
    RegisterMetaType<QVector<QString>> qStringVector;
    RegisterMetaType<OriginalNetworkScan> qNetScan;
    RegisterMetaType<FirewallParams> qFirewallParams;
}

QSet<pid_t> ProcFs::filterPids(const std::function<bool(pid_t)> &filterFunc)
{
    QDir procDir{"/proc"};
    procDir.setFilter(QDir::Dirs);
    procDir.setNameFilters({"[1-9]*"});

    QSet<pid_t> filteredPids;
    for(const auto &entry : procDir.entryList())
    {
        pid_t pid = entry.toInt();
        if(filterFunc(pid))
            filteredPids.insert(pid);
    }

    return filteredPids;
}

QSet<pid_t> ProcFs::pidsForPath(const QString &path)
{
    return filterPids([&](pid_t pid) { return pathForPid(pid) == path; });
}

QSet<pid_t> ProcFs::childPidsOf(pid_t parentPid)
{
    return filterPids([&](pid_t pid) { return isChildOf(parentPid, pid); });
}

QString ProcFs::pathForPid(pid_t pid)
{
    QString link = QStringLiteral("/proc/%1/exe").arg(pid);
    return QFile::symLinkTarget(link);
}

bool ProcFs::isChildOf(pid_t parentPid, pid_t pid)
{
    static const QRegularExpression parentPidRegex{ QStringLiteral("PPid:\\s+([0-9]+)") };

    QFile statusFile{QStringLiteral("/proc/%1/status").arg(pid)};
    if(!statusFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    auto match = parentPidRegex.match(statusFile.readAll());
    if(match.hasMatch())
    {
        auto foundParentPid = match.captured(1).toInt();
        return foundParentPid == parentPid;
    }

    return false;
}

// Explicitly specify struct alignment
typedef struct __attribute__((aligned(NLMSG_ALIGNTO)))
{
    nlmsghdr header;

    // Insert no padding as we want the members contiguous
    struct __attribute__((__packed__))
    {
        cn_msg body;
        proc_cn_mcast_op subscription_type;
    };
} NetlinkRequest;

typedef struct __attribute__((aligned(NLMSG_ALIGNTO)))
{
    nlmsghdr header;

    struct __attribute__((__packed__))
    {
        cn_msg body;
        proc_event event;
    };
} NetlinkResponse;

void ProcTracker::showError(QString funcName)
{
    qWarning() << QStringLiteral("%1 Error (code: %2) %3").arg(funcName).arg(errno).arg(qPrintable(qt_error_string(errno)));
}

void ProcTracker::writePidToCGroup(pid_t pid, const QString &cGroupPath)
{
    QFile cgroupFile{cGroupPath};

    if(!cgroupFile.open(QFile::WriteOnly))
    {
        qWarning() << "Cannot open" << cGroupPath << "for writing!" << cgroupFile.errorString();
        return;
    }

    if(cgroupFile.write(QByteArray::number(pid)) < 0)
        qWarning() << "Could not write to" << cGroupPath << cgroupFile.errorString();
}

void ProcTracker::addPidToExclusions(pid_t pid)
{
    writePidToCGroup(pid, Path::VpnExclusionsFile);
    // Add child processes (NOTE: we also recurse through child processes of child processes)
    addChildPidsToExclusions(pid);
}

void ProcTracker::addChildPidsToExclusions(pid_t parentPid)
{
    for(pid_t pid : ProcFs::childPidsOf(parentPid))
    {
        qInfo() << "Adding child pid" << pid;
        addPidToExclusions(pid);
    }
}

void ProcTracker::removeChildPidsFromExclusions(pid_t parentPid)
{
    for(pid_t pid : ProcFs::childPidsOf(parentPid))
    {
        qInfo() << "Removing child pid" << pid;
        removePidFromExclusions(pid);
    }
}

void ProcTracker::removePidFromExclusions(pid_t pid)
{
    // We remove a PID from a cgroup by adding it to its parent cgroup
    writePidToCGroup(pid, Path::ParentVpnExclusionsFile);
    // Remove child processes (NOTE: we also recurse through child processes of child processes)
    removeChildPidsFromExclusions(pid);
}

void ProcTracker::updateMasquerade(QString interfaceName)
{
    qInfo() << "Updating the masquerade rule for new interface name" << interfaceName;
    IpTablesFirewall::replaceAnchor(
        IpTablesFirewall::Both,
        QStringLiteral("100.transIp"),
        QStringLiteral("-o %1 -j MASQUERADE").arg(interfaceName),
        IpTablesFirewall::kNatTable
    );
}

void ProcTracker::updateRoutes(QString gatewayIp, QString interfaceName)
{
    const QString routingTableName = IpTablesFirewall::kRtableName;

    qInfo() << "Updating the default route in"
        << routingTableName
        << "for"
        << gatewayIp
        << "and"
        << interfaceName;

    auto cmd = QStringLiteral("ip route replace default via %1 dev %2 table %3").arg(gatewayIp, interfaceName, routingTableName);
    qInfo() << "Executing:" << cmd;
    ::shellExecute(cmd);
    ::shellExecute(QStringLiteral("ip route flush cache"));
}

void ProcTracker::updateNetwork(const OriginalNetworkScan &updatedScan, const FirewallParams &params)
{
    qInfo() << "Updating network";

    qInfo() << "_previousNetScan.gatewayIp is" << _previousNetScan.gatewayIp();
    qInfo() << "updatedScan.gatewayIp is" << updatedScan.gatewayIp();

    if(_previousNetScan.interfaceName() != updatedScan.interfaceName())
        updateMasquerade(updatedScan.interfaceName());

    if(_previousNetScan.gatewayIp() != updatedScan.gatewayIp() || _previousNetScan.interfaceName() != updatedScan.interfaceName())
    {
        updateRoutes(updatedScan.gatewayIp(), updatedScan.interfaceName());
        addRoutingPolicyForSourceIp(updatedScan.ipAddress());
    }

    // Update our network info
    _previousNetScan = updatedScan;
}

void ProcTracker::initiateConnection(const OriginalNetworkScan &netScan, const FirewallParams &params)
{
    int sock;
    qInfo() << "Attempting to connect to Netlink";

    if(_sockFd != -1)
    {
        qInfo() << "Existing connection already exists, disconnecting first";
        shutdownConnection();
    }

    // Set SOCK_CLOEXEC to prevent socket being inherited by child processes (such as openvpn)
    sock = ::socket(PF_NETLINK, SOCK_DGRAM|SOCK_CLOEXEC, NETLINK_CONNECTOR);
    if(sock == -1)
    {
        showError("::socket");
        return;
    }

    sockaddr_nl address = {};

    address.nl_pid = getpid();
    address.nl_groups = CN_IDX_PROC;
    address.nl_family = AF_NETLINK;

    if(::bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr_nl)) == -1)
    {
        showError("::bind");
        ::close(sock);
        return;
    }

    if(subscribeToProcEvents(sock, true) == -1)
    {
        qWarning() << "Could not subscribe to proc events";
        ::close(sock);
        return;
    }

    qInfo() << "Successfully connected to Netlink";

    // Save the socket FD to an ivar
    _sockFd = sock;
    setupFirewall();
    updateNetwork(netScan, params);
    _readNotifier = new QSocketNotifier(sock, QSocketNotifier::Read);
    connect(_readNotifier, &QSocketNotifier::activated, this, &ProcTracker::readFromSocket);
}

void ProcTracker::updateExcludedApps(QVector<QString> excludedApps)
{
    QVector<QString> removedApps;

    // Find which apps have been removed from the exclusions
    for(const auto &app : _appMap.keys())
    {
        if(!excludedApps.contains(app))
            removedApps.push_back(app);
    }

    // Add new entries
    for(auto &app : excludedApps)
    {
        _appMap.insert(app, {});
        for(pid_t pid : ProcFs::pidsForPath(app))
        {
            // Both these calls are no-ops if the PID is already excluded
            addPidToExclusions(pid);
            _appMap[app].insert(pid);
        }
    }

    removeApps(removedApps);
}

void ProcTracker::removeAllApps()
{
    qInfo() << "Removing all apps from cgroup";
    removeApps(QVector<QString>::fromList(_appMap.keys()));
    _appMap.clear();
}

void ProcTracker::removeApps(const QVector<QString> &removedApps)
{
    // Remove existing entries
    for(const auto &app : removedApps)
    {
        qInfo() << "Removing all pids for" << app << "from exclusions";
        // Remove all PIDs for the app from the cgroup
        for(pid_t pid : _appMap[app])
        {
            qInfo() << "Removing pid" << pid;
            removePidFromExclusions(pid);
        }

        // Remove the app from our model
        _appMap.remove(app);
    }
}

int ProcTracker::subscribeToProcEvents(int sock, bool enabled)
{
    NetlinkRequest message = {};

    message.subscription_type = enabled ? PROC_CN_MCAST_LISTEN : PROC_CN_MCAST_IGNORE;

    message.header.nlmsg_len = sizeof(message);
    message.header.nlmsg_pid = getpid();
    message.header.nlmsg_type = NLMSG_DONE;

    message.body.len = sizeof(proc_cn_mcast_op);
    message.body.id.val = CN_VAL_PROC;
    message.body.id.idx = CN_IDX_PROC;

    if(::send(sock, &message, sizeof(message), 0) == -1)
    {
        showError("::send");
        return -1;
    }

    return 0;
}

void ProcTracker::setupFirewall()
{
    // Setup the packet tagging rule (this rule is unaffected by network changes)
    // This rule also has callbacks that sets up the cgroup and the routing policy
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("100.tagPkts"), true, IpTablesFirewall::kMangleTable);

    // Enable the masquerading rule - this gets updated with interface changes via replaceAnchor()
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("100.transIp"), true, IpTablesFirewall::kNatTable);
}

void ProcTracker::teardownFirewall()
{
    // Remove the masquerading rule
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("100.transIp"), false, IpTablesFirewall::kNatTable);
    // Remove the cgroup marking rule
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("100.tagPkts"), false, IpTablesFirewall::kMangleTable);
}

void ProcTracker::addRoutingPolicyForSourceIp(QString ipAddress)
{
    ::shellExecute(QStringLiteral("ip rule add from %1 lookup %3 pri 101")
        .arg(ipAddress, IpTablesFirewall::kRtableName));
}

void ProcTracker::removeRoutingPolicyForSourceIp(QString ipAddress)
{
    ::shellExecute(QStringLiteral("ip rule del from %1 lookup %3 pri 101")
        .arg(ipAddress, IpTablesFirewall::kRtableName));
}

void ProcTracker::shutdownConnection()
{
    qInfo() << "Attempting to disconnect from Netlink";
    if(_readNotifier)
    {
        _readNotifier->setEnabled(false);
        delete _readNotifier;
    }

    if(_sockFd != -1)
    {
        // Unsubscribe from proc events
        subscribeToProcEvents(_sockFd, false);
        if(::close(_sockFd) != 0)
            showError("::close");
    }

    teardownFirewall();
    removeAllApps();
    removeRoutingPolicyForSourceIp(_previousNetScan.ipAddress());

    // Clear out our network info
    _previousNetScan = {};
    _sockFd = -1;

    qInfo() << "Successfully disconnected from Netlink";
}

void ProcTracker::readFromSocket(int sock)
{
    NetlinkResponse message = {};

    ::recv(sock, &message, sizeof(message), 0);

    // shortcut
    const auto &eventData = message.event.event_data;
    pid_t pid;
    QString appName;

    switch(message.event.what)
    {
    case proc_event::PROC_EVENT_NONE:
        qInfo() << "Listening to process events";
        break;
    case proc_event::PROC_EVENT_EXEC:
        pid = eventData.exec.process_pid;
        // Get the launch path associated with the PID
        appName = ProcFs::pathForPid(pid);
        // If the path is an "excluded app" then exclude it
        if(_appMap.contains(appName))
        {
            _appMap[appName].insert(pid);
            qInfo() << "Adding" << pid << "to VPN exclusions for excluded app:" << appName;

            // Add the PID to the cgroup so its network traffic goes out the
            // physical uplink
            addPidToExclusions(pid);
        }

        break;
    case proc_event::PROC_EVENT_EXIT:
        pid = eventData.exit.process_pid;

        // Update our internal model to remove the pid from the associated app entry
        // We do not need to explicitly remove the PID from the cgroup as
        // an exiting process is removed automatically
        for(AppMap::iterator i = _appMap.begin(); i != _appMap.end(); ++i)
        {
            auto &set = i.value();
            set.remove(pid);
        }

        break;
    default:
        // We're not interested in any other events
        break;
    }
    _readNotifier->setEnabled(true);
}
