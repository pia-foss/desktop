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

#pragma once
#include <kapps_core/src/util.h>
#include <kapps_net/net.h>
#include <unordered_map>
#include <kapps_core/src/posix/pollthread.h>
#include "../firewallparams.h"
#include "../firewall.h"
#include "linux_cgroup.h"
#include "proc_tracker.h"
#include "iptables_firewall.h"
#include "../originalnetworkscan.h"

namespace kapps { namespace net {

class SplitDNSInfo
{
public:
    enum class SplitDNSType
    {
        Bypass,
        VpnOnly
    };

    static std::string existingDNS(const std::vector<uint32_t> &servers);
    static SplitDNSInfo infoFor(const FirewallParams &params, SplitDNSType dnsType, const CGroupIds &cgroup);

public:
    SplitDNSInfo() = default;
    SplitDNSInfo(const std::string &dnsServer, const std::string &cGroupId, const std::string &sourceIp)
        : _dnsServer{dnsServer}, _cGroupId{cGroupId}, _sourceIp{sourceIp}
    {
    }

    bool operator==(const SplitDNSInfo &other) const
    {
        return dnsServer() == other.dnsServer() &&
            cGroupId() == other.cGroupId() &&
            sourceIp() == other.sourceIp();
    }
    bool operator!=(const SplitDNSInfo &other) const {return !(*this == other);}

    const std::string &dnsServer() const { return _dnsServer; }
    const std::string &cGroupId() const { return _cGroupId; }
    const std::string &sourceIp() const { return _sourceIp; }

    bool isValid() const;

private:
    std::string _dnsServer;
    std::string _cGroupId;
    std::string _sourceIp;
};

struct KAPPS_NET_EXPORT UpdateSplitTunnel
{
    FirewallParams params;
};

class KAPPS_NET_EXPORT LinuxFirewall : public PlatformFirewall
{
public:
    LinuxFirewall(FirewallConfig config);
    virtual ~LinuxFirewall() override;

private:
    LinuxFirewall(const LinuxFirewall &) = delete;
    LinuxFirewall &operator=(const LinuxFirewall &) = delete;

public:
    void applyRules(const FirewallParams &params) override;

private:
    void updateRules(const FirewallParams &params);
    std::vector<std::string> getDNSRules(const std::string &vpnAdapterName, const std::vector<std::string>& servers);
    void enableRouteLocalNet();
    void disableRouteLocalNet();
    bool updateVpnTunOnlyAnchor(bool hasConnected, std::string tunnelDeviceName, std::string tunnelDeviceLocalAddress);
    void updateForwardedRoutes(const FirewallParams &params, bool shouldBypassVpn);
    void updateBypassSubnets(IpTablesFirewall::IPVersion ipVersion, const std::set<std::string> &bypassSubnets, std::set<std::string> &oldBypassSubnets);

protected:
    virtual void startSplitTunnel(const FirewallParams& params) override;
    virtual void updateSplitTunnel(const FirewallParams &params) override;
    virtual void stopSplitTunnel() override;

private:
    FirewallConfig _config;
    std::string _executableDir;
    core::nullable_t<IpTablesFirewall> _pFilter;
    std::string _adapterName;
    std::string _ipAddress6;
    std::vector<std::string> _dnsServers;
    std::string _routeLocalNet;
    std::set<std::string> _bypassIpv4Subnets;
    std::set<std::string> _bypassIpv6Subnets;
    core::nullable_t<CGroupIds> _pCgroup;
    SplitDNSInfo _routedDnsInfo;    // Last behavior applied for routed packet DNS
    SplitDNSInfo _appDnsInfo;   // Last behavior applied for app DNS (either bypass or VPN only)

    // Split tunnel process tracker - only created when split tunnel is active.
    // This runs on _pSplitTunnelWorker, so we can only manipulate it by
    // synchronizing with that thread
    core::nullable_t<ProcTracker> _pSplitTunnelTracker;
    // Thread used to run the split tunnel tracker asynchronously while active
    // (handles Netlink events).
    core::nullable_t<core::PollThread> _pSplitTunnelWorker;
};

}}
