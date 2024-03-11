// Copyright (c) 2024 Private Internet Access, Inc.
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

#pragma once
#include "linux_cn_proc.h"
#include "iptables_firewall.h"
#include <kapps_core/core.h>
#include <kapps_net/net.h>
#include <kapps_core/src/util.h>
#include <kapps_core/src/newexec.h>
#include "../firewallparams.h"
#include <unordered_map>
#include "linux_cgroup.h"
#include "linux_proc_fs.h"

namespace kapps { namespace net {

class ProcTracker
{
public:
    ProcTracker(FirewallParams params, IpTablesFirewall &firewall, CGroupIds cgroup, const std::string &bypassFile, const std::string &vpnOnlyFile, const std::string &defaultFile)
    : _bypassFile{bypassFile}
    , _vpnOnlyFile{vpnOnlyFile}
    , _defaultFile{defaultFile}
    , _cgroup{std::move(cgroup)}
    , _firewall{firewall}
    {
        // TODO: only need to pass params
        initiateConnection(params, params.tunnelDeviceName, params.tunnelDeviceLocalAddress);

        // Get the mount namespace of the daemon - we use this to validate the paths
        // we get when deciding to split tunnel an app. We only want to consider paths
        // that belong to the same mount namespace - otherwise malicious apps in a different namespace
        // could bypass the VPN by setting their path to a bypassed app.
        _defaultMountNamespaceId = ProcFs::mountNamespaceId(getpid());
    }

    ~ProcTracker()
    {
        shutdownConnection();
    }

private:
    // TODO - Fold initiateConnection() and shutdownConnection() into ctors
    void initiateConnection(const FirewallParams &params, std::string tunnelDeviceName,
                            std::string tunnelDeviceLocalAddress);
    void shutdownConnection();

public:
    void updateSplitTunnel(const FirewallParams &params, std::string tunnelDeviceName,
                           std::string tunnelDeviceLocalAddress);

private:
    using AppMap = std::unordered_map<std::string, std::set<pid_t>>;

    void removeAllApps();
    void addApps(const std::vector<std::string> &apps, AppMap &appMap,
                 std::string cGroupPath, core::StringSlice traceName);
    // Remove apps that are no longer in this group - removes apps and PIDs from
    // appMap that do not appear in keepApps
    void removeApps(const std::vector<std::string> &keepApps, AppMap &appMap,
                    core::StringSlice traceName);
    void updateFirewall(const FirewallParams &params);
    void teardownFirewall();
    void addRoutingPolicyForSourceIp(std::string ipAddress, std::string routingTableName);
    void removeRoutingPolicyForSourceIp(std::string ipAddress, std::string routingTableName);
    void removeTerminatedApp(pid_t pid);
    void addLaunchedApp(pid_t pid);
    void updateMasquerade(std::string interfaceName, std::string tunnelDeviceName);
    void updateRoutes(std::string gatewayIp, std::string interfaceName, std::string tunnelDeviceName);
    void updateNetwork(const FirewallParams &params, std::string tunnelDeviceName,
                       std::string tunnelDeviceLocalAddres);
    void setVpnBlackHole();
    void setupReversePathFiltering();
    void teardownReversePathFiltering();
    void updateApps(std::vector<std::string> excludedApps, std::vector<std::string> vpnOnlyApps);

    // Whether a given process belongs to an allowed mount namespace
    // Currently the only mount namespace we allow is the one that pia-daemon
    // runs in.
    bool isProcessInAllowedMountNamespace(pid_t pid) const { return ProcFs::mountNamespaceId(pid) == _defaultMountNamespaceId; }

    void showInvalidMountNamespaceWarning(const std::string &appName, pid_t pid) const;

private:
    CnProc _cnProc;
    OriginalNetworkScan _previousNetScan;
    std::string _previousRPFilter;
    std::string _bypassFile;
    std::string _vpnOnlyFile;
    std::string _defaultFile;
    AppMap _exclusionsMap;
    AppMap _vpnOnlyMap;
    std::string _previousTunnelDeviceLocalAddress;
    std::string _previousTunnelDeviceName;
    CGroupIds _cgroup;
    std::string _defaultMountNamespaceId;

    // IpTablesFirewall provided by LinuxFirewall - used to update firewall
    // rules.
    //
    // This is somewhat fragile since it is used from both the main thread and
    // the worker thread.  On the worker thread, we can _only_ use this during
    // updateSplitTunnel(), as the main thread invokes that synchronously.  We
    // _can't_ use it from any app events.
    //
    // We could almost have the caller always pass in the IpTablesFirewall
    // during updateSplitTunnel(), _except_ that the destructor also uses it to
    // shut down.
    //
    // Instead, this should really be a different IpTablesFirewall.  The product
    // shouldn't be touching the same firewall rules as ProcTracker, so we
    // should be able to use two separate IpTablesFirewall objects to manage our
    // own firewall rules.
    IpTablesFirewall &_firewall;
};

}}
