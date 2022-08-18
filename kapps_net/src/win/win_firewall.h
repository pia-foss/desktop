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

#pragma once
#include <kapps_core/src/util.h>
#include <kapps_net/net.h>
#include <kapps_core/core.h>
#include <kapps_core/logger.h>
#include <unordered_map>
#include "../firewallparams.h"
#include "../routemanager.h"
#include "../subnetbypass.h"
#include "../firewall.h"
#include "wfp_firewall.h"
#include "../originalnetworkscan.h"
#include <string>
#include <vector>
#include <map>

namespace kapps { namespace net {

        struct FirewallFilters
        {
            std::vector<WfpFilterObject> permitProduct;
            WfpFilterObject permitAdapter[2];
            WfpFilterObject permitLocalhost[2];
            WfpFilterObject permitDHCP[2];
            WfpFilterObject permitLAN[10];
            WfpFilterObject blockDNS[2];
            WfpFilterObject permitInjectedDns;
            WfpFilterObject ipInbound;
            WfpFilterObject ipOutbound;
            WfpFilterObject permitDNS[2];
            WfpFilterObject blockAll[2];
            std::vector<WfpFilterObject> permitResolvers;
            std::vector<WfpFilterObject> blockResolvers;

            // This is not strictly a filter, but it can in nearly all respects be treated the same way
            // so we store it here for simplicity and so we can re-use the filter-related code
            WfpCalloutObject splitCalloutBind;
            WfpCalloutObject splitCalloutConnect;
            WfpCalloutObject splitCalloutFlowEstablished;
            WfpCalloutObject splitCalloutConnectAuth;
            WfpCalloutObject splitCalloutIpInbound;
            WfpCalloutObject splitCalloutIpOutbound;

            WfpProviderContextObject providerContextKey;
            WfpProviderContextObject vpnOnlyProviderContextKey;

        };

    struct SplitAppFilters
    {
        // Filter to permit any IPv4 traffic from app.  Used for "bypass" apps
        // only; only-VPN app traffic is permitted by normal firewall rules
        // (they should only communicate over the tunnel, which is already
        // permitted).
        WfpFilterObject permitApp;
        // Filter to block IPv4 traffic from app, used for only-VPN apps when
        // disconnected
        WfpFilterObject blockAppIpv4;
        // Filter to block IPv6 traffic from app, used for only-VPN apps in all
        // connection states
        WfpFilterObject blockAppIpv6;
        // Filter to invoke callout for app in bind layer.  Used to force UDP
        // sockets to particular interfaces.
        WfpFilterObject splitAppBind;
        // Filter to invoke callout for app in connect layer.  Used to force
        // TCP sockets to particular interfaces, and to capture DNS flow
        // information (DNS packets are rewritten in the IP packet layers, which
        // are not in an application context.)
        WfpFilterObject splitAppConnect;
        // Filter to permit any DNS address for this app - applied if DNS
        // rewriting is active for this type of app, since we will rewrite these
        // DNS queries to the correct address, which also provides leak
        // protection.
        WfpFilterObject appPermitAnyDns;
        // Filter to invoke callout for app in flow established layer.  This is
        // used to inspect DNS flows, which has to be done in this layer in
        // order to attach a flow context and cause WFP to notify us when the
        // flow is removed.
        WfpFilterObject splitAppFlowEstablished;
    };

    struct SplitTunnelFirewallParams : public kapps::core::OStreamInsertable<SplitTunnelFirewallParams>
    {
        // Local IP and prefix length of the non-VPN network interface
        // TODO - Use Ipv4Address instead of string
        std::string _physicalIp;
        unsigned _physicalNetPrefix;
        // Local IP of the VPN tunnel device
        std::string _tunnelIp;
        // Whether we are connected to the VPN right now.
        bool _isConnected;
        // Whether we have connected since enabling the VPN (FirewallParams::hasConnected)
        // NOTE: _not_ whether we are currently connected right now
        bool _hasConnected;
        // Whether the VPN gets the default route (affects which DNS rewriting
        // rules become active)
        bool _vpnDefaultRoute;
        // Whether DNS leak protection is active
        bool _blockDNS;
        // Whether we need to force "VPN only" apps to use PIA's configured DNS.
        bool _forceVpnOnlyDns;
        // Whether we need to force "bypass" apps to use the existing DNS.
        bool _forceBypassDns;
        // Existing DNS servers on the physical interface
        std::vector<core::Ipv4Address> _existingDnsServers;
        // Effective DNS servers configured in PIA (empty = use existing)
        std::vector<core::Ipv4Address> _effectiveDnsServers;

    public:
        bool operator==(const SplitTunnelFirewallParams &other)
        {
            return _physicalIp == other._physicalIp &&
                _physicalNetPrefix == other._physicalNetPrefix &&
                _tunnelIp == other._tunnelIp &&
                _isConnected == other._isConnected &&
                _hasConnected == other._hasConnected &&
                _vpnDefaultRoute == other._vpnDefaultRoute &&
                _blockDNS == other._blockDNS &&
                _forceVpnOnlyDns == other._forceVpnOnlyDns &&
                _forceBypassDns == other._forceBypassDns &&
                _existingDnsServers == other._existingDnsServers &&
                _effectiveDnsServers == other._effectiveDnsServers;
        }

        void trace(std::ostream &os) const
        {
            os << "- physical IP known: " << !_physicalIp.empty()
                << " - physical net prefix: " << _physicalNetPrefix
                << " - tunnel IP known: " << !_tunnelIp.empty()
                << " - have connected: " << _hasConnected
                << " - VPN default route: " << _vpnDefaultRoute
                << " - block DNS: " << _blockDNS
                << " - force VPN-only DNS: " << _forceVpnOnlyDns
                << " - force bypass DNS: " << _forceBypassDns
                << " - existing DNS server count: " << _existingDnsServers.size()
                << " - effective DNS server count: " << _effectiveDnsServers.size();
        }
    };


        class WinFirewall : public PlatformFirewall
        {
        private:
            using SplitAppFilterMap = std::map<std::shared_ptr<const AppIdKey>, SplitAppFilters, core::PtrValueLess>;

        public:
            WinFirewall(FirewallConfig config);
            virtual ~WinFirewall() override;

        public:
            void applyRules(const FirewallParams &params) override;

        protected:
            // Not used in win
            virtual void startSplitTunnel(const FirewallParams &params) override {}
            virtual void updateSplitTunnel(const FirewallParams &params) override {}
            virtual void stopSplitTunnel() override {}

        private:
            bool areAppsUnchanged(const AppIdSet &newApps,
                                  const SplitAppFilterMap &oldAppMap)
            {
                // Compare these element-wise; valid because both containers are sorted
                // lexically.
                return std::equal(oldAppMap.begin(), oldAppMap.end(),
                newApps.begin(), newApps.end(),
                [](const auto &existing, const auto &pNewApp)
                {
                    if(!existing.first && !pNewApp)
                        return true;
                    return existing.first && pNewApp && *existing.first == *pNewApp;
                });
            }

            void updateAllBypassSubnetFilters(const FirewallParams &params);
            void updateBypassSubnetFilters(const std::set<std::string> &subnets, std::set<std::string> &oldSubnets,
                                   std::vector<WfpFilterObject> &subnetBypassFilters, FWP_IP_VERSION ipVersion);
            void removeSplitTunnelAppFilters(SplitAppFilterMap &apps,
                                     const core::StringSlice &traceType);
            void createBypassAppFilters(SplitAppFilterMap &apps,
                                const WfpProviderContextObject &context,
                                const std::shared_ptr<const AppIdKey> &pAppId, bool rewriteDns);
            void createOnlyVPNAppFilters(SplitAppFilterMap &apps,
                                 const WfpProviderContextObject &context,
                                 const std::shared_ptr<const AppIdKey> &pAppId, bool rewriteDns);
            void createBlockAppFilters(SplitAppFilterMap &apps,
                               const std::shared_ptr<const AppIdKey> &pAppId);
            void reapplySplitTunnelFirewall(const SplitTunnelFirewallParams &params,
                                    const AppIdSet &newExcludedApps,
                                    const AppIdSet &newVpnOnlyApps,
                                    const AppIdSet &newVpnOnlyResolvers);

        private:
            const FirewallConfig _config{};
            kapps::net::SubnetBypass _subnetBypass;
            std::unique_ptr<FirewallEngine> _firewall{};

            // App IDs for resolver executables, needed to bind these programs
            // to the VPN when the default behavior is to bypass
            AppIdSet _resolverAppIds;

            // Inputs to reapplySplitTunnelFirewall() - the last set of inputs used is
            // stored so we know when to recreate the firewall rules.
            SplitTunnelFirewallParams _lastSplitParams;

            UINT64 _filterAdapterLuid{0}; // LUID of the TAP adapter used in some rules
            std::string _dnsServers[2]; // Last DNS servers that we applied
            FirewallFilters _filters;

            SplitAppFilterMap excludedApps;
            SplitAppFilterMap vpnOnlyApps;
            SplitAppFilterMap vpnOnlyResolvers;

            std::set<std::string> _bypassIpv4Subnets;
            std::set<std::string> _bypassIpv6Subnets;

            std::vector<WfpFilterObject> _subnetBypassFilters4;
            std::vector<WfpFilterObject> _subnetBypassFilters6;
        };
    }}
