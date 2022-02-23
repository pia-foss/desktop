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
#include "exec.h"
#include "linux_cgroup.h"
#include "linux_routing.h"
#include "posix/posix_firewall_iptables.h"
#include "proc_tracker.h"
#include "linux_proc_fs.h"

namespace
{
    RegisterMetaType<QVector<QString>> qStringVector;
    RegisterMetaType<OriginalNetworkScan> qNetScan;
    RegisterMetaType<FirewallParams> qFirewallParams;
}

Executor ProcTracker::_executor{CURRENT_CATEGORY};

void ProcTracker::updateMasquerade(QString interfaceName, QString tunnelDeviceName)
{
    if(interfaceName.isEmpty())
    {
        qInfo() << "Removing masquerade rule, not connected";
        IpTablesFirewall::replaceAnchor(IpTablesFirewall::Both, QStringLiteral("100.transIp"), {}, IpTablesFirewall::kNatTable);
    }
    else
    {
        qInfo() << "Updating the masquerade rule for new interface name" << interfaceName;
        IpTablesFirewall::replaceAnchor(
            IpTablesFirewall::Both,
            QStringLiteral("100.transIp"),
            {
                QStringLiteral("-o %1 -j MASQUERADE").arg(interfaceName),
                QStringLiteral("-o %1 -j MASQUERADE").arg(tunnelDeviceName)
            },
            IpTablesFirewall::kNatTable
        );
    }
}

void ProcTracker::updateRoutes(QString gatewayIp, QString interfaceName, QString tunnelDeviceName)
{
    // The bypass route can be left as-is if the configuration is not known,
    // even though the route may be out of date - we don't put any processes in
    // this cgroup when not connected.
    if(gatewayIp.isEmpty() || interfaceName.isEmpty())
    {
        qInfo() << "Not updating bypass route - configuration not known - address:"
            << gatewayIp << "- interface:" << interfaceName;
    }
    else
    {
        auto cmd = QStringLiteral("ip route replace default via %1 dev %2 table %3").arg(gatewayIp, interfaceName, Routing::bypassTable);
        qInfo() << "Executing:" << cmd;
        _executor.bash(cmd);
    }

    // The VPN-only route can be left as-is if we're not connected, VPN-only
    // processes are expected to lose connectivity in that case.
    if(tunnelDeviceName.isEmpty())
    {
        qWarning() << "Tunnel configuration not known yet, can't configure VPN-only route yet"
            << "- interface:" << tunnelDeviceName;
    }
    else
    {
        auto cmd = QStringLiteral("ip route replace default dev %1 table %2").arg(tunnelDeviceName, Routing::vpnOnlyTable);
        qInfo() << "Executing:" << cmd;
        _executor.bash(cmd);
    }

    _executor.cmd(QStringLiteral("ip"), {"route", "flush", "cache"});
}

void ProcTracker::updateNetwork(const FirewallParams &params, QString tunnelDeviceName,
                                QString tunnelDeviceLocalAddress)
{
    qInfo() << "previous gateway IP is" << _previousNetScan.gatewayIp();
    qInfo() << "updated gateway IP is" << params.netScan.gatewayIp();
    qInfo() << "tunnel device is" << tunnelDeviceName;

    if(_previousNetScan.interfaceName() != params.netScan.interfaceName() || _previousTunnelDeviceName != tunnelDeviceName)
        updateMasquerade(params.netScan.interfaceName(), tunnelDeviceName);

    // Ensure that packets with the source IP of the physical interface go out the physical interface
    if(_previousNetScan.ipAddress() != params.netScan.ipAddress())
    {
        // Remove the old one (if it exists) before adding a new one
        removeRoutingPolicyForSourceIp(_previousNetScan.ipAddress(), Routing::bypassTable);
        addRoutingPolicyForSourceIp(params.netScan.ipAddress(), Routing::bypassTable);
    }

    // Ensure that packets with source IP of the tunnel go out the tunnel interface
    if(_previousTunnelDeviceLocalAddress !=  tunnelDeviceLocalAddress)
    {
        // Remove the old one (if it exists) before adding a new one
        removeRoutingPolicyForSourceIp(_previousTunnelDeviceLocalAddress, Routing::vpnOnlyTable);
        addRoutingPolicyForSourceIp(tunnelDeviceLocalAddress, Routing::vpnOnlyTable);
    }

    // always update the routes - as we use 'route replace' so we don't have to worry about adding the same route multiple times
    updateRoutes(params.netScan.gatewayIp(), params.netScan.interfaceName(), tunnelDeviceName);

    updateFirewall(params);

    // If we just got a valid network scan (we're connecting) or we lost it
    // (we're disconnected), the subsequent call to updateApps() will add/remove
    // all excluded apps (which are only tracked when we have a network scan).
    _previousNetScan = params.netScan;
    _previousTunnelDeviceLocalAddress = tunnelDeviceLocalAddress;
    _previousTunnelDeviceName = tunnelDeviceName;
}

void ProcTracker::initiateConnection(const FirewallParams &params,
                                     QString tunnelDeviceName, QString tunnelDeviceLocalAddress)
{
    if(_pCnProc)
    {
        qInfo() << "Existing connection already exists, disconnecting first";
        shutdownConnection();
    }

    _pCnProc.emplace();

    connect(_pCnProc.ptr(), &CnProc::exec, this,
            &ProcTracker::addLaunchedApp);
    connect(_pCnProc.ptr(), &CnProc::exit, this,
            &ProcTracker::removeTerminatedApp);

    // setup cgroups + configure routing rules
    CGroup::setupNetCls();

    setVpnBlackHole();
    updateFirewall(params);
    updateSplitTunnel(params, tunnelDeviceName, tunnelDeviceLocalAddress);
    setupReversePathFiltering();
}

void ProcTracker::setVpnBlackHole()
{
    // This fall-back route blocks all traffic that hits the vpnOnly routing table
    // The tunnel interface route disappears when the tunnel goes down, exposing this route
    _executor.bash(QStringLiteral("ip route replace blackhole default metric 32000 table %1").arg(Routing::vpnOnlyTable));
}

void ProcTracker::setupReversePathFiltering()
{
    QString out = _executor.bashWithOutput(QStringLiteral("sysctl -n 'net.ipv4.conf.all.rp_filter'"));

    if(!out.isEmpty())
    {
        if(out.toInt() != 2)
        {
            _previousRPFilter = out;
            qInfo() << "Storing old net.ipv4.conf.all.rp_filter value:" << out;
            qInfo() << "Setting rp_filter to loose";
            _executor.bash(QStringLiteral("sysctl -w 'net.ipv4.conf.all.rp_filter=2'"));
        }
        else
            qInfo() << "rp_filter already 2 (loose mode); nothing to do!";
    }
    else
    {
        qWarning() << "Unable to store old net.ipv4.conf.all.rp_filter value";
        _previousRPFilter = "";
        return;
    }
}

void ProcTracker::teardownReversePathFiltering()
{
    if(!_previousRPFilter.isEmpty())
    {
        qInfo() << "Restoring rp_filter to: " << _previousRPFilter;
        _executor.bash(QStringLiteral("sysctl -w 'net.ipv4.conf.all.rp_filter=%1'").arg(_previousRPFilter));
        _previousRPFilter = "";
    }
}

void ProcTracker::updateApps(QVector<QString> excludedApps, QVector<QString> vpnOnlyApps)
{
    qInfo() << "ExcludedApps:" << excludedApps << "VPN Only Apps:" << vpnOnlyApps;
    // If we're not tracking excluded apps, remove everything
    if(!_previousNetScan.ipv4Valid())
        excludedApps = {};
    // Update excluded apps
    removeApps(excludedApps, _exclusionsMap);
    addApps(excludedApps, _exclusionsMap, Path::VpnExclusionsFile);

    // Update vpnOnly
    removeApps(vpnOnlyApps, _vpnOnlyMap);
    addApps(vpnOnlyApps, _vpnOnlyMap, Path::VpnOnlyFile);
}

void ProcTracker::removeAllApps()
{
    qInfo() << "Removing all apps from cgroups";
    removeApps({}, _exclusionsMap);
    removeApps({}, _vpnOnlyMap);

    _exclusionsMap.clear();
    _vpnOnlyMap.clear();
}

void ProcTracker::addApps(const QVector<QString> &apps, AppMap &appMap, QString cGroupPath)
{
    for(auto &app : apps)
    {
        appMap.insert(app, {});
        for(pid_t pid : ProcFs::pidsForPath(app))
        {
            // Both these calls are no-ops if the PID is already excluded
            CGroup::addPidToCgroup(pid, cGroupPath);
            appMap[app].insert(pid);
        }
    }
}

void ProcTracker::removeApps(const QVector<QString> &keepApps, AppMap &appMap)
{
    for(const auto &app : appMap.keys())
    {
        if(!keepApps.contains(app))
        {
            for(pid_t pid : appMap[app])
            {
                CGroup::removePidFromCgroup(pid, Path::ParentVpnExclusionsFile);
            }

            appMap.remove(app);
        }
    }
}

void ProcTracker::updateFirewall(const FirewallParams &params)
{
    // Setup the packet tagging rule (this rule is unaffected by network changes)
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("100.tagBypass"), true, IpTablesFirewall::kMangleTable);

    // Only create the vpnOnly tagging rule if bypassing is the default
    // (otherwise the packets will go through VPN anyway)
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("100.tagVpnOnly"), params.bypassDefaultApps, IpTablesFirewall::kMangleTable);

    // Enable the masquerading rule - this gets updated with interface changes via replaceAnchor()
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("100.transIp"), true, IpTablesFirewall::kNatTable);

    // Setup the packet tagging rule for subnet bypass (tag packets heading to a bypass subnet so they're excluded from VPN)
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::IPv4, QStringLiteral("90.tagSubnets"), true, IpTablesFirewall::kMangleTable);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("200.tagFwdSubnets"), true, IpTablesFirewall::kMangleTable);
}

void ProcTracker::teardownFirewall()
{
    // Remove subnet bypass tagging rule
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::IPv4, QStringLiteral("90.tagSubnets"), false, IpTablesFirewall::kMangleTable);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("200.tagFwdSubnets"), false, IpTablesFirewall::kMangleTable);
    // Remove the masquerading rule
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("100.transIp"), false, IpTablesFirewall::kNatTable);
    // Remove the cgroup marking rule
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("100.tagBypass"), false, IpTablesFirewall::kMangleTable);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("100.tagVpnOnly"), false, IpTablesFirewall::kMangleTable);
}

void ProcTracker::addRoutingPolicyForSourceIp(QString ipAddress, QString routingTableName)
{
    if(!ipAddress.isEmpty())
        _executor.bash(QStringLiteral("ip rule add from %1 lookup %2 pri %3")
            .arg(ipAddress, routingTableName).arg(Routing::Priorities::sourceIp));
}

void ProcTracker::removeRoutingPolicyForSourceIp(QString ipAddress, QString routingTableName)
{
    if(!ipAddress.isEmpty())
        _executor.bash(QStringLiteral("ip rule del from %1 lookup %2 pri %3")
            .arg(ipAddress, routingTableName).arg(Routing::Priorities::sourceIp));
}

void ProcTracker::shutdownConnection()
{
    _pCnProc.clear();

    teardownFirewall();
    // Remove cgroup routing rules
    CGroup::teardownNetCls();
    removeAllApps();
    removeRoutingPolicyForSourceIp(_previousNetScan.ipAddress(), Routing::bypassTable);
    removeRoutingPolicyForSourceIp(_previousTunnelDeviceLocalAddress, Routing::vpnOnlyTable);
    teardownReversePathFiltering();

    // Clear out our network info
    _previousNetScan = {};

    qInfo() << "Successfully disconnected from Netlink";
}

void ProcTracker::updateSplitTunnel(const FirewallParams &params, QString tunnelDeviceName,
                                    QString tunnelDeviceLocalAddress)
{
    // Update network first, then updateApps() can add/remove all excluded apps
    // when we gain/lose a valid network scan
    updateNetwork(params, tunnelDeviceName, tunnelDeviceLocalAddress);
    updateApps(params.excludeApps, params.vpnOnlyApps);
}

void ProcTracker::removeTerminatedApp(pid_t pid)
{
    // Remove from exclusions
    for(AppMap::iterator i = _exclusionsMap.begin(); i != _exclusionsMap.end(); ++i)
    {
        auto &set = i.value();
        set.remove(pid);
    }

    // Remove from vpnOnly
    for(AppMap::iterator i = _vpnOnlyMap.begin(); i != _vpnOnlyMap.end(); ++i)
    {
        auto &set = i.value();
        set.remove(pid);
    }
}

void ProcTracker::addLaunchedApp(pid_t pid)
{
    // Get the launch path associated with the PID
    QString appName = ProcFs::pathForPid(pid);

    // May be empty if the process was so short-lived it exited before we had a chance to read its name
    // In this case we just early-exit and ignore it
    if(appName.isEmpty())
        return;

    if(_exclusionsMap.contains(appName))
    {
        // Add it if we're currently tracking excluded apps.
        if(_previousNetScan.ipv4Valid())
        {
            _exclusionsMap[appName].insert(pid);
            qInfo() << "Adding" << pid << "to VPN exclusions for app:" << appName;

            // Add the PID to the cgroup so its network traffic goes out the
            // physical uplink
            CGroup::addPidToCgroup(pid, Path::VpnExclusionsFile);
        }
    }
    else if(_vpnOnlyMap.contains(appName))
    {
        _vpnOnlyMap[appName].insert(pid);
        qInfo() << "Adding" << pid << "to VPN Only for app:" << appName;

        // Add the PID to the cgroup so its network traffic is forced out the
        // VPN
        CGroup::addPidToCgroup(pid, Path::VpnOnlyFile);
    }
}
