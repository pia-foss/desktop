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
#include <kapps_net/net.h>
#include "../firewallparams.h"
#include "../routemanager.h"
#include "../subnetbypass.h"
#include "../firewall.h"
#include "pf_firewall.h"
#include "../originalnetworkscan.h"
#include "mac_splittunnel.h"
#include "transparent_proxy.h"
#include <kapps_core/src/posix/pollthread.h>
#include <kapps_core/src/util.h>
#include <kapps_core/src/processrunner.h>
#include <string>

namespace kapps { namespace net {

class BoundRouteUpdater
{
public:
    void update(const FirewallParams &params);

private:
    void createBoundRoute(const std::string &ipAddress, const std::string &interfaceName);
    void removeBoundRoute(const std::string &ipAddress, const std::string &interfaceName);

private:
    OriginalNetworkScan _netScan;
};

// Run Unbound on an auxiliary loopback address configured to return
// NXDOMAIN for everything.  Used to block DNS in a way that still allows
// mDNSResponder to bring up the physical interface.
class StubUnbound
{
public:
    StubUnbound(FirewallConfig config);
    ~StubUnbound();

public:
    bool enable(bool enabled);

private:
    core::ProcessRunner _unboundRunner;
    FirewallConfig _config;
};

class MacFirewall : public PlatformFirewall
{
public:
    MacFirewall(FirewallConfig config);
    virtual ~MacFirewall() override;

private:
    struct DNSLeakProtectionInfo
    {
        bool macBlockDNS{false};
        bool macStubDNS{false};
        std::vector<std::string> localDnsServers;
        std::vector<std::string> tunnelDnsServers;
    };

public:
    void applyRules(const FirewallParams &params) override;
    void aboutToConnectToVpn() override;

    void updateBoundRoute(const FirewallParams &params);
    std::vector<std::string> subnetsToBypass(const FirewallParams &params);
    DNSLeakProtectionInfo getDnsLeakProtectionInfo(const FirewallParams &params);

    // TODO: should this go here?
    void dnsCacheFlush() const;

protected:
    virtual void startSplitTunnel(const FirewallParams& params) override;
    virtual void updateSplitTunnel(const FirewallParams &params) override;
    virtual void stopSplitTunnel() override;

private:
    FirewallConfig _config;
    StubUnbound _stubUnbound;
    kapps::core::nullable_t<PFFirewall> _pFilter;
    SubnetBypass _subnetBypass;
    std::string _executableDir;
    // The split tunnel implementation - keeps track of the TCP/UDP flows routed
    // to either the VPN or physical interface, detects and handles new flows
    // from the split tunnel interface, etc.  This runs on _pSplitTunnelWorker,
    // so we can only manipulate it by synchronizing with that thread.
    core::nullable_t<MacSplitTunnel> _pSplitTunnel;
    // Thread used to run the split tunnel implementation asynchronously.
    core::nullable_t<core::PollThread> _pSplitTunnelWorker;
    BoundRouteUpdater _boundRouteUpdater;
    // Split tunnel implementation based on Apple's transparent proxy APIs
    core::nullable_t<TransparentProxy> _pTransparentProxy;
};

}}
