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

#include "proc_tracker.h"
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <kapps_core/src/newexec.h>
#include "iptables_firewall.h"
#include "linux_routing.h"
#include "linux_proc_fs.h"

namespace kapps { namespace net {

namespace
{
    using IPVersion = IpTablesFirewall::IPVersion;
}

void ProcTracker::initiateConnection(const FirewallParams &params, std::string tunnelDeviceName, std::string tunnelDeviceLocalAddress)
{
    _cnProc.exec = [this](pid_t pid) { addLaunchedApp(pid); };
    _cnProc.exit = [this](pid_t pid) { removeTerminatedApp(pid); };

    // setup cgroups + configure routing rules
    _cgroup.setupNetCls();

    setVpnBlackHole();
    updateFirewall(params);
    updateSplitTunnel(params, tunnelDeviceName, tunnelDeviceLocalAddress);
    setupReversePathFiltering();
}

void ProcTracker::shutdownConnection()
{
    teardownFirewall();
    // Remove cgroup routing rules
    _cgroup.teardownNetCls();
    removeAllApps();
    removeRoutingPolicyForSourceIp(_previousNetScan.ipAddress(), _cgroup.routing().bypassTable());
    removeRoutingPolicyForSourceIp(_previousTunnelDeviceLocalAddress, _cgroup.routing().vpnOnlyTable());
    teardownReversePathFiltering();

    // Clear out our network info
    _previousNetScan = {};

    KAPPS_CORE_INFO() << "Successfully disconnected from Netlink";
}

void ProcTracker::updateSplitTunnel(const FirewallParams &params, std::string tunnelDeviceName,
                                    std::string tunnelDeviceLocalAddress)
{
    // Update network first, then updateApps() can add/remove all excluded apps
    // when we gain/lose a valid network scan
    updateNetwork(params, tunnelDeviceName, tunnelDeviceLocalAddress);
    updateApps(params.excludeApps, params.vpnOnlyApps);
}

void ProcTracker::removeAllApps()
{
    KAPPS_CORE_INFO() << "Removing all apps from cgroups";
    removeApps({}, _exclusionsMap, "bypass");
    removeApps({}, _vpnOnlyMap, "VPN only");

    _exclusionsMap.clear();
    _vpnOnlyMap.clear();
}

void ProcTracker::addApps(const std::vector<std::string> &apps, AppMap &appMap, std::string cGroupPath, core::StringSlice traceName)
{
    KAPPS_CORE_INFO() << "Add" << apps.size() << "apps to" << traceName;
    for(const auto &app : apps)
    {
        // Create the PID set for this app or get the existing one
        auto &appPids = appMap[app];
        // Since this scans all PIDs, it's common for transient processes to
        // cause errors as they exit before we get a chance to check them -
        // silence these errors since this happens a lot.
        for(pid_t pid : ProcFs::pidsForPath(app, true))
        {
            // Both these calls are no-ops if the PID is already excluded
            CGroup::addPidToCgroup(pid, cGroupPath);
            appPids.insert(pid);
        }
    }
}

void ProcTracker::removeApps(const std::vector<std::string> &keepApps, AppMap &appMap, core::StringSlice traceName)
{
    KAPPS_CORE_INFO() << "Remove apps from" << traceName << "- keeping"
        << keepApps.size();
    auto itApp = appMap.begin();
    while(itApp != appMap.end())
    {
        const auto &app = itApp->first;
        const auto itr = std::find(keepApps.begin(), keepApps.end(), app);
        if(itr == keepApps.end())
        {
            for(pid_t pid : itApp->second)
                CGroup::removePidFromCgroup(pid, _defaultFile);

            itApp = appMap.erase(itApp);
        }
        else
            ++itApp;
    }
}

void ProcTracker::updateFirewall(const FirewallParams &params)
{
    // Setup the packet tagging rule (this rule is unaffected by network changes)
    _firewall.setAnchorEnabled(TableEnum::Mangle, IPVersion::Both, ("100.tagBypass"), true);

    // Only create the vpnOnly tagging rule if bypassing is the default
    // (otherwise the packets will go through VPN anyway)
    _firewall.setAnchorEnabled(TableEnum::Mangle, IPVersion::Both, ("100.tagVpnOnly"), params.bypassDefaultApps);

    // Enable the masquerading rule - this gets updated with interface changes via replaceAnchor()
    _firewall.setAnchorEnabled(TableEnum::Nat, IPVersion::Both, ("100.transIp"), true);

    // Setup the packet tagging rule for subnet bypass (tag packets heading to a bypass subnet so they're excluded from VPN)
    _firewall.setAnchorEnabled(TableEnum::Mangle, IPVersion::IPv4, qs::format("90.tagSubnets"), true);
    _firewall.setAnchorEnabled(TableEnum::Mangle, IPVersion::Both, ("200.tagFwdSubnets"), true);
}

void ProcTracker::teardownFirewall()
{
    // Remove subnet bypass tagging rule
    _firewall.setAnchorEnabled(TableEnum::Mangle, IPVersion::IPv4, ("90.tagSubnets"), false);
    _firewall.setAnchorEnabled(TableEnum::Mangle, IPVersion::Both, ("200.tagFwdSubnets"), false);
    // Remove the masquerading rule
    _firewall.setAnchorEnabled(TableEnum::Nat, IPVersion::Both, ("100.transIp"), false);
    // Remove the cgroup marking rule
    _firewall.setAnchorEnabled(TableEnum::Mangle, IPVersion::Both, ("100.tagBypass"), false);
    _firewall.setAnchorEnabled(TableEnum::Mangle, IPVersion::Both, ("100.tagVpnOnly"), false);
}

void ProcTracker::addRoutingPolicyForSourceIp(std::string ipAddress, std::string routingTableName)
{
    if(!ipAddress.empty())
        core::Exec::bash(qs::format("ip rule add from % lookup % pri %", ipAddress, routingTableName, Routing::Priorities::sourceIp));
}

void ProcTracker::removeRoutingPolicyForSourceIp(std::string ipAddress, std::string routingTableName)
{
    if(!ipAddress.empty())
        core::Exec::bash(qs::format("ip rule del from % lookup % pri %", ipAddress, routingTableName, Routing::Priorities::sourceIp));
}

void ProcTracker::removeTerminatedApp(pid_t pid)
{
    // Remove from exclusions
    for(auto &pair : _exclusionsMap)
    {
        auto &set = pair.second;
        set.erase(pid);
    }

    // Remove from vpnOnly
    for(auto &pair : _vpnOnlyMap)
    {
        auto &set = pair.second;
        set.erase(pid);
    }
}

void ProcTracker::addLaunchedApp(pid_t pid)
{
    // Get the launch path associated with the PID.  This also tends to trace
    // errors for transient processes; ignore those.
    std::string appName = ProcFs::pathForPid(pid, true);

    // May be empty if the process was so short-lived it exited before we had a chance to read its name
    // In this case we just early-exit and ignore it
    if(appName.empty())
        return;

    if(_exclusionsMap.count(appName) > 0)
    {
        // Add it if we're currently tracking excluded apps.
        if(_previousNetScan.ipv4Valid())
        {
            _exclusionsMap[appName].insert(pid);
            KAPPS_CORE_INFO() << "Adding" << pid << "to VPN exclusions for app:" << appName;

            // Add the PID to the cgroup so its network traffic goes out the
            // physical uplink
            CGroup::addPidToCgroup(pid, _bypassFile);
        }
    }
    else if(_vpnOnlyMap.count(appName) > 0)
    {
        _vpnOnlyMap[appName].insert(pid);
        KAPPS_CORE_INFO() << "Adding" << pid << "to VPN Only for app:" << appName;

        // Add the PID to the cgroup so its network traffic is forced out the
        // VPN
        CGroup::addPidToCgroup(pid, _vpnOnlyFile);
    }
}

void ProcTracker::updateMasquerade(std::string interfaceName, std::string tunnelDeviceName)
{
    if(interfaceName.empty())
    {
        KAPPS_CORE_INFO() << "Removing masquerade rule, not connected";
        _firewall.replaceAnchor(TableEnum::Nat, IPVersion::Both, qs::format("100.transIp"), {});
    }
    else
    {
        KAPPS_CORE_INFO() << "Updating the masquerade rule for new interface name" << interfaceName;
        _firewall.replaceAnchor(TableEnum::Nat, IPVersion::Both, qs::format("100.transIp"),
            {
                qs::format("-o % -j MASQUERADE", interfaceName),
                qs::format("-o % -j MASQUERADE", tunnelDeviceName)
            });
    }
}

void ProcTracker::updateRoutes(std::string gatewayIp, std::string interfaceName, std::string tunnelDeviceName)
{
    // The bypass route can be left as-is if the configuration is not known,
    // even though the route may be out of date - we don't put any processes in
    // this cgroup when not connected.
    if(gatewayIp.empty() || interfaceName.empty())
    {
        KAPPS_CORE_INFO() << "Not updating bypass route - configuration not known - address:"
            << gatewayIp << "- interface:" << interfaceName;
    }
    else
    {
        core::Exec::bash(qs::format("ip route replace default via % dev % table %",
            gatewayIp, interfaceName, _cgroup.routing().bypassTable()));
    }

    // The VPN-only route can be left as-is if we're not connected, VPN-only
    // processes are expected to lose connectivity in that case.
    if(tunnelDeviceName.empty())
    {
        KAPPS_CORE_WARNING() << "Tunnel configuration not known yet, can't configure VPN-only route yet"
            << "- interface:" << tunnelDeviceName;
    }
    else
    {
        core::Exec::bash(qs::format("ip route replace default dev % table %",
            tunnelDeviceName, _cgroup.routing().vpnOnlyTable()));
    }

    core::Exec::cmd(qs::format("ip"), {"route", "flush", "cache"});
}

void ProcTracker::updateNetwork(const FirewallParams &params, std::string tunnelDeviceName,
                                std::string tunnelDeviceLocalAddress)
{
    KAPPS_CORE_INFO() << "previous gateway IP is" << _previousNetScan.gatewayIp();
    KAPPS_CORE_INFO() << "updated gateway IP is" << params.netScan.gatewayIp();
    KAPPS_CORE_INFO() << "tunnel device is" << tunnelDeviceName;

    if(_previousNetScan.interfaceName() != params.netScan.interfaceName() || _previousTunnelDeviceName != tunnelDeviceName)
        updateMasquerade(params.netScan.interfaceName(), tunnelDeviceName);

    // Ensure that packets with the source IP of the physical interface go out the physical interface
    if(_previousNetScan.ipAddress() != params.netScan.ipAddress())
    {
        // Remove the old one (if it exists) before adding a new one
        removeRoutingPolicyForSourceIp(_previousNetScan.ipAddress(), _cgroup.routing().bypassTable());
        addRoutingPolicyForSourceIp(params.netScan.ipAddress(), _cgroup.routing().bypassTable());
    }

    // Ensure that packets with source IP of the tunnel go out the tunnel interface
    if(_previousTunnelDeviceLocalAddress !=  tunnelDeviceLocalAddress)
    {
        // Remove the old one (if it exists) before adding a new one
        removeRoutingPolicyForSourceIp(_previousTunnelDeviceLocalAddress, _cgroup.routing().vpnOnlyTable());
        addRoutingPolicyForSourceIp(tunnelDeviceLocalAddress, _cgroup.routing().vpnOnlyTable());
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

void ProcTracker::setVpnBlackHole()
{
    // This fall-back route blocks all traffic that hits the vpnOnly routing table
    // The tunnel interface route disappears when the tunnel goes down, exposing this route
    core::Exec::bash(qs::format("ip route replace blackhole default metric 32000 table %", _cgroup.routing().vpnOnlyTable()));
}

void ProcTracker::setupReversePathFiltering()
{
    std::string out = core::Exec::bashWithOutput(qs::format("sysctl -n 'net.ipv4.conf.all.rp_filter'"));

    if(!out.empty())
    {
        if(std::stoul(out) != 2)
        {
            _previousRPFilter = out;
            KAPPS_CORE_INFO() << "Storing old net.ipv4.conf.all.rp_filter value:" << out;
            KAPPS_CORE_INFO() << "Setting rp_filter to loose";
            core::Exec::bash(qs::format("sysctl -w 'net.ipv4.conf.all.rp_filter=2'"));
        }
        else
            KAPPS_CORE_INFO() << "rp_filter already 2 (loose mode); nothing to do!";
    }
    else
    {
        KAPPS_CORE_WARNING() << "Unable to store old net.ipv4.conf.all.rp_filter value";
        _previousRPFilter = "";
        return;
    }
}

void ProcTracker::teardownReversePathFiltering()
{
    if(!_previousRPFilter.empty())
    {
        KAPPS_CORE_INFO() << "Restoring rp_filter to: " << _previousRPFilter;
        core::Exec::bash(qs::format("sysctl -w 'net.ipv4.conf.all.rp_filter=%'", _previousRPFilter));
        _previousRPFilter = "";
    }
}

void ProcTracker::updateApps(std::vector<std::string> excludedApps, std::vector<std::string> vpnOnlyApps)
{
    KAPPS_CORE_INFO() << "ExcludedApps:" << excludedApps << "VPN Only Apps:" << vpnOnlyApps;
    // If we're not tracking excluded apps, remove everything
    if(!_previousNetScan.ipv4Valid())
        excludedApps = {};
    // Update excluded apps
    removeApps(excludedApps, _exclusionsMap, "bypass");
    addApps(excludedApps, _exclusionsMap, _bypassFile, "bypass");

    // Update vpnOnly
    removeApps(vpnOnlyApps, _vpnOnlyMap, "VPN only");
    addApps(vpnOnlyApps, _vpnOnlyMap, _vpnOnlyFile, "VPN only");

    // Indicate that we're done; if any filesystem errors were traced we want
    // to know whether they were associated with this update or something else
    // happening on this thread
    KAPPS_CORE_INFO() << "Finished updating apps";
}

}}
