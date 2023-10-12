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

#include "win_firewall.h"
#include "wfp_firewall.h"
#include "win_routemanager.h"
#include <kapps_core/src/win/win_error.h>
#include <thread>
#include <tuple>
#include <kapps_core/src/logger.h>
#include <kapps_core/src/ipaddress.h>
#include <kapps_core/src/newexec.h>
#include <WinDNS.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "Ws2_32.lib")

namespace kapps { namespace net {

namespace
{
    void logFilter(const char* filterName, int currentState, bool enableCondition, bool invalidateCondition = false)
    {
        if (enableCondition ? currentState != 1 || invalidateCondition : currentState != 0)
        {
            KAPPS_CORE_INFO().nospace() << filterName << ": "
                << (currentState == 1 ? "ON" : currentState == 0 ? "OFF" : "MIXED")
                << " -> " << (enableCondition ? "ON" : "OFF");
        }
        else
        {
            KAPPS_CORE_INFO().nospace() << filterName << ": "
                << (enableCondition ? "ON" : "OFF");
        }
    }

    void logFilter(const char* filterName, const GUID& filterVariable, bool enableCondition, bool invalidateCondition = false)
    {
        logFilter(filterName, filterVariable == zeroGuid ? 0 : 1, enableCondition, invalidateCondition);
    }

    template<class FilterObjType, class FilterVarIterT>
    void logFilter(const char *filterName, FilterVarIterT itFiltersBegin,
        FilterVarIterT itFiltersEnd, bool enableCondition,
        bool invalidateCondition = false)
    {
        if(itFiltersBegin == itFiltersEnd)
        {
            // No filter variables - assume it was inactive
            logFilter(filterName, 0, enableCondition, invalidateCondition);
            return;
        }

        // Get the state of the first filter
        int state = (*itFiltersBegin == zeroGuid) ? 0 : 1;
        ++itFiltersBegin;
        // Check for the "mixed" state if any filter is in the opposite state
        while(itFiltersBegin != itFiltersEnd)
        {
            int s = (*itFiltersBegin == zeroGuid) ? 0 : 1;
            if (s != state)
            {
                state = 2;
                break;
            }
            ++itFiltersBegin;
        }
        logFilter(filterName, state, enableCondition, invalidateCondition);
    }

    template<class FilterObjType, size_t N>
    void logFilter(const char* filterName, const FilterObjType (&filterVariables)[N], bool enableCondition, bool invalidateCondition = false)
    {
        // MSVC has trouble deducing the iterator type in this call for some
        // reason, tell it explicitly
        using FilterVarIterT = decltype(std::begin(filterVariables));
        logFilter<FilterObjType, FilterVarIterT>(filterName,
            std::begin(filterVariables), std::end(filterVariables),
            enableCondition, invalidateCondition);
    }
    template<class FilterObjType>
    void logFilter(const char* filterName,
        const std::vector<FilterObjType> &filterVariables, bool enableCondition,
        bool invalidateCondition = false)
    {
        // MSVC has trouble deducing the iterator type in this call for some
        // reason, tell it explicitly
        using FilterVarIterT = decltype(filterVariables.begin());
        logFilter<FilterObjType, FilterVarIterT>(filterName,
            filterVariables.begin(), filterVariables.end(),
            enableCondition, invalidateCondition);
    }

    std::vector<core::Ipv4Address> findExistingDNS(const std::vector<core::Ipv4Address> &piaDNSServers)
    {
        std::vector<core::Ipv4Address> newDNSServers;

        // What we'd really like to do here is get the DNS servers for the primary
        // network interface.  Unfortunately, there is no API to do that, and even
        // parsing `netsh interface ip show dnsservers` won't work since that relies
        // on the DNSCache service (which PIA must stop to implement split tunnel
        // DNS).
        //
        // The best we can do is to get all DNS servers, which may include PIA's,
        // but the preexisting servers will still be there.  Then filter out PIA's
        // servers.
        //
        // If no DNS servers remain, then assume that the existing DNS servers were
        // the same as PIA's - use PIA's DNS servers for bypass apps too.
        //
        // There are a few ways that this can be incorrect if there were no existing
        // DNS servers, or the existing DNS servers were a subset of PIA's, etc.
        // Given the assumption that alternate DNS servers configured on the same
        // adapter are equivalent, it will behave reasonably, generally just adding
        // or removing some equivalent DNS servers.
        //
        // The only way this can really significantly fail is if the user has
        // multiple physical adapters, with different DNS servers configured on
        // each, and where the primary adapter's DNS servers are the same as PIA's.
        // In this case, we would incorrectly treat the secondary adapter's servers
        // as the preexisting DNS, and likely use them on the primary adapter.

        std::aligned_storage_t<1024, alignof(IP4_ARRAY)> dnsAddrBuf;
        DWORD dnsBufLen = sizeof(dnsAddrBuf);
        DNS_STATUS status = DnsQueryConfig(DnsConfigDnsServerList, 0,
                                        NULL, NULL, &dnsAddrBuf, &dnsBufLen);
        if(status == 0) // Success
        {
            const IP4_ARRAY &dnsServers = *reinterpret_cast<const IP4_ARRAY*>(&dnsAddrBuf);
            newDNSServers.reserve(dnsServers.AddrCount);
            KAPPS_CORE_INFO() << "Got" << dnsServers.AddrCount
                << "existing DNS servers";
            for(DWORD i=0; i<dnsServers.AddrCount; ++i)
            {
                core::Ipv4Address dnsServerAddr{ntohl(dnsServers.AddrArray[i])};
                // Check if this was one of ours.  We only apply up to 2 DNS
                // servers, so a linear search through the vector is fine.
                bool setByPia = std::find(piaDNSServers.begin(), piaDNSServers.end(),
                                        dnsServerAddr) != piaDNSServers.end();
                KAPPS_CORE_INFO() << " -" << i << "-" << dnsServerAddr
                    << (setByPia ? "(ours)" : "");
                if(!setByPia)
                    newDNSServers.push_back(dnsServerAddr);
            }

            // If no DNS servers remain, then the preexisting DNS servers were
            // likely the same as PIA's - assume they were.
            if(newDNSServers.empty())
            {
                KAPPS_CORE_INFO() << "All DNS servers appear to be ours, assuming preexisting servers were the same";
                newDNSServers = piaDNSServers;
            }
        }
        else
        {
            KAPPS_CORE_WARNING() << "Could not get existing DNS servers - error" << status;
        }

        return newDNSServers;
    }

}

WinFirewall::WinFirewall(FirewallConfig config)
    : _config{std::move(config)},
      _subnetBypass{std::make_unique<WinRouteManager>()},
      _firewall{new FirewallEngine{_config.brandInfo}},
      _filterAdapterLuid{0},
      // Ensure our filters are zero-initialized - since
      // it's an aggregate with simple members this wont happen by default.
      _filters{}
{
    // Product and resolver executables can't be changed after the firewall is
    // created, size our GUID vectors appropriately now
    _filters.permitProduct.resize(_config.productExecutables.size());
    _filters.permitResolvers.resize(_config.resolverExecutables.size());
    _filters.blockResolvers.resize(_config.resolverExecutables.size());

    for(const auto &exe : _config.resolverExecutables)
    {
        std::shared_ptr<const AppIdKey> pResolverId{new AppIdKey{exe}};
        if(*pResolverId)
            _resolverAppIds.insert(std::move(pResolverId));
        else
        {
            KAPPS_CORE_WARNING() << "Failed to find app ID for resolver"
                << exe;
        }
    }

    if(!_firewall->open() || !_firewall->installProvider())
    {
        KAPPS_CORE_ERROR() << "Unable to initialize WFP firewall";
        _firewall.reset();
    }
    else
    {
        _firewall->removeAll();
    }
}

WinFirewall::~WinFirewall()
{
#define deactivateFilter(filterVariable, removeCondition) \
    do { \
        /* Remove existing rule if necessary */ \
        if ((removeCondition) && filterVariable != zeroGuid) \
        { \
            if (!_firewall->remove(filterVariable)) { \
                KAPPS_CORE_WARNING() << "Failed to remove WFP filter" << #filterVariable; \
            } \
            filterVariable = {zeroGuid}; \
        } \
    } \
    while(false)
#define activateFilter(filterVariable, addCondition, ...) \
    do { \
        /* Add new rule if necessary */ \
        if ((addCondition) && filterVariable == zeroGuid) \
        { \
            if ((filterVariable = _firewall->add(__VA_ARGS__)) == zeroGuid) { \
                /* TODO: report error to product */ \
                KAPPS_CORE_WARNING() << "Firewall rule failed:" << #filterVariable; \
                /*reportError(Error(HERE, Error::FirewallRuleFailed, { std::stringLiteral(#filterVariable) }));*/ \
            } \
        } \
    } \
    while(false)
#define updateFilter(filterVariable, removeCondition, addCondition, ...) \
    do { \
        deactivateFilter(_filters.filterVariable, removeCondition); \
        activateFilter(_filters.filterVariable, addCondition, __VA_ARGS__); \
    } while(false)
#define updateBooleanFilter(filterVariable, enableCondition, ...) \
    do { \
        const bool enable = (enableCondition); \
        updateFilter(filterVariable, !enable, enable, __VA_ARGS__); \
    } while(false)
#define updateBooleanInvalidateFilter(filterVariable, enableCondition, invalidateCondition, ...) \
    do { \
        const bool enable = (enableCondition); \
        const bool disable = !enable || (invalidateCondition); \
        updateFilter(filterVariable, disable, enable, __VA_ARGS__); \
    } while(false)
#define filterActive(filterVariable) (_filters.filterVariable != zeroGuid)

    if (_firewall)
    {
        KAPPS_CORE_INFO() << "Cleaning up WFP objects";

        if (_filters.ipInbound != zeroGuid)
        {
            KAPPS_CORE_INFO() << "deactivate IpInbound object";
            deactivateFilter(_filters.ipInbound, true);
        }

        if (_filters.splitCalloutIpInbound != zeroGuid)
        {
            KAPPS_CORE_INFO() << "deactivate IpInbound callout object";
            deactivateFilter(_filters.splitCalloutIpInbound, true);
        }

        if (_filters.ipOutbound != zeroGuid)
        {
            KAPPS_CORE_INFO() << "deactivate IpOutbound object";
            deactivateFilter(_filters.ipOutbound, true);
        }

        if (_filters.splitCalloutIpOutbound != zeroGuid)
        {
            KAPPS_CORE_INFO() << "deactivate IpOutbound callout object";
            deactivateFilter(_filters.splitCalloutIpOutbound, true);
        }

        _firewall->removeAll();
        _firewall->uninstallProvider();
        _firewall->checkLeakedObjects();
    }
    else
        KAPPS_CORE_INFO() << "Firewall was not initialized, nothing to clean up";

    KAPPS_CORE_INFO() << "Windows firewall shutdown complete";
}

void WinFirewall::applyRules(const FirewallParams &params)
{
    if(!_firewall)
        return;

    FirewallTransaction tx(_firewall.get());

#define deactivateFilter(filterVariable, removeCondition) \
    do { \
        /* Remove existing rule if necessary */ \
        if ((removeCondition) && filterVariable != zeroGuid) \
        { \
            if (!_firewall->remove(filterVariable)) { \
                KAPPS_CORE_WARNING() << "Failed to remove WFP filter" << #filterVariable; \
            } \
            filterVariable = {zeroGuid}; \
        } \
    } \
    while(false)
#define activateFilter(filterVariable, addCondition, ...) \
    do { \
        /* Add new rule if necessary */ \
        if ((addCondition) && filterVariable == zeroGuid) \
        { \
            if ((filterVariable = _firewall->add(__VA_ARGS__)) == zeroGuid) { \
                /* TODO: report error to product */ \
                KAPPS_CORE_WARNING() << "Firewall rule failed:" << #filterVariable; \
                /*reportError(Error(HERE, Error::FirewallRuleFailed, { std::string(#filterVariable) }));*/ \
            } \
        } \
    } \
    while(false)
#define updateFilter(filterVariable, removeCondition, addCondition, ...) \
    do { \
        deactivateFilter(_filters.filterVariable, removeCondition); \
        activateFilter(_filters.filterVariable, addCondition, __VA_ARGS__); \
    } while(false)
#define updateBooleanFilter(filterVariable, enableCondition, ...) \
    do { \
        const bool enable = (enableCondition); \
        updateFilter(filterVariable, !enable, enable, __VA_ARGS__); \
    } while(false)
#define updateBooleanInvalidateFilter(filterVariable, enableCondition, invalidateCondition, ...) \
    do { \
        const bool enable = (enableCondition); \
        const bool disable = !enable || (invalidateCondition); \
        updateFilter(filterVariable, disable, enable, __VA_ARGS__); \
    } while(false)
#define filterActive(filterVariable) (_filters.filterVariable != zeroGuid)

    // Firewall rules, listed in order of ascending priority (as if the last
    // matching rule applies, but note that it is the priority argument that
    // actually determines precedence).

    // As a bit of an exception to the normal firewall rule logic, the WFP
    // rules handle the blockIPv6 rule by changing the priority of the IPv6
    // part of the killswitch rule instead of having a dedicated IPv6 block.

    // Block all other traffic when killswitch is enabled. If blockIPv6 is
    // true, block IPv6 regardless of killswitch state.
    logFilter("blockAll(IPv4)", _filters.blockAll[0], params.blockAll);
    updateBooleanFilter(blockAll[0], params.blockAll,                     EverythingFilter<FWP_ACTION_BLOCK, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(0));
    logFilter("blockAll(IPv6)", _filters.blockAll[1], params.blockAll || params.blockIPv6);
    updateBooleanFilter(blockAll[1], params.blockAll || params.blockIPv6, EverythingFilter<FWP_ACTION_BLOCK, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(params.blockIPv6 ? 4 : 0));

    // Exempt traffic going over the VPN adapter.  This is the TAP adapter for
    // OpenVPN, or the WinTUN adapter for Wireguard.

    UINT64 luid{};
    if(!params.tunnelDeviceName.empty())
    {
        try
        {
            luid = std::stoull(params.tunnelDeviceName);
        }
        catch(const std::exception &ex)
        {
            KAPPS_CORE_WARNING() << "Unable to parse tunnel device name"
                << params.tunnelDeviceName << "-" << ex.what();
            // Leave luid == 0, handled in filter logic below
        }
    }

    logFilter("allowVPN", _filters.permitAdapter, luid && params.allowVPN, luid != _filterAdapterLuid);
    updateBooleanInvalidateFilter(permitAdapter[0], luid && params.allowVPN, luid != _filterAdapterLuid, InterfaceFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(luid, 2));
    updateBooleanInvalidateFilter(permitAdapter[1], luid && params.allowVPN, luid != _filterAdapterLuid, InterfaceFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(luid, 2));
    _filterAdapterLuid = luid;
    // Note: This is where the IPv6 block rule is ordered if blockIPv6 is true.

    // Exempt DHCP traffic.
    logFilter("allowDHCP", _filters.permitDHCP, params.allowDHCP);
    updateBooleanFilter(permitDHCP[0], params.allowDHCP, DHCPFilter<FWP_ACTION_PERMIT, FWP_IP_VERSION_V4>(6));
    updateBooleanFilter(permitDHCP[1], params.allowDHCP, DHCPFilter<FWP_ACTION_PERMIT, FWP_IP_VERSION_V6>(6));

    // Permit LAN traffic depending on settings
    logFilter("allowLAN", _filters.permitLAN, params.allowLAN);
    updateBooleanFilter(permitLAN[0], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(core::Ipv4Address{192,168,0,0}, 16, 8));
    updateBooleanFilter(permitLAN[1], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(core::Ipv4Address{172,16,0,0}, 12, 8));
    updateBooleanFilter(permitLAN[2], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(core::Ipv4Address{10,0,0,0}, 8, 8));
    updateBooleanFilter(permitLAN[3], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(core::Ipv4Address{224,0,0,0}, 4, 8));
    updateBooleanFilter(permitLAN[4], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(core::Ipv4Address{169,254,0,0}, 16, 8));
    updateBooleanFilter(permitLAN[5], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(core::Ipv4Address{255,255,255,255}, 32, 8));
    updateBooleanFilter(permitLAN[6], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(core::Ipv6Address{0xfc00}, 7, 8));
    updateBooleanFilter(permitLAN[7], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(core::Ipv6Address{0xfe80}, 10, 8));
    updateBooleanFilter(permitLAN[8], params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(core::Ipv6Address{0xff00}, 8, 8));
    // Permit the IPv6 global Network Prefix - this allows on-link IPv6 hosts to communicate using their global IPs
    // which is more common in practice than link-local
    updateBooleanFilter(permitLAN[9], params.netScan.hasIpv6() && params.allowLAN, IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(
            // First 64 bits of a global IPv6 IP is the Network Prefix.
            core::Ipv6Address{params.netScan.ipAddress6()}, 64, 8));

    // Poke holes in firewall for the bypass subnets for Ipv4 and Ipv6
    updateAllBypassSubnetFilters(params);

    // Add rules to block non-PIA DNS servers if connected and DNS leak protection is enabled
    logFilter("blockDNS", _filters.blockDNS, params.blockDNS);
    updateBooleanFilter(blockDNS[0], params.blockDNS, DNSFilter<FWP_ACTION_BLOCK, FWP_IP_VERSION_V4>(10));
    updateBooleanFilter(blockDNS[1], params.blockDNS, DNSFilter<FWP_ACTION_BLOCK, FWP_IP_VERSION_V6>(10));

    std::string dnsServers[2];
    if(params.effectiveDnsServers.size() >= 1)
        dnsServers[0] = params.effectiveDnsServers[0];
    if(params.effectiveDnsServers.size() >= 2)
        dnsServers[1] = params.effectiveDnsServers[1];
    logFilter("allowDNS(1)", _filters.permitDNS[0], params.blockDNS && !dnsServers[0].empty(), _dnsServers[0] != dnsServers[0]);
    updateBooleanInvalidateFilter(permitDNS[0], params.blockDNS && !dnsServers[0].empty(), _dnsServers[0] != dnsServers[0], IPAddressFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(core::Ipv4Address{dnsServers[0]}, 14));
    _dnsServers[0] = dnsServers[0];
    logFilter("allowDNS(2)", _filters.permitDNS[1], params.blockDNS && !dnsServers[1].empty(), _dnsServers[1] != dnsServers[1]);
    updateBooleanInvalidateFilter(permitDNS[1], params.blockDNS && !dnsServers[1].empty(), _dnsServers[1] != dnsServers[1], IPAddressFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(core::Ipv4Address{dnsServers[1]}, 14));
    _dnsServers[1] = dnsServers[1];

    // Always permit traffic from product executables.  This allows us to fetch
    // metadata, download updates, submit debug reports, etc., even when the
    // kill switch is active.
    logFilter("allowPIA", _filters.permitProduct, params.allowPIA);
    // Class invariant - product executables can't be changed.
    // Note that it's also important that the _content_ of productExecutables is
    // immutable as well as the length; that's not checked here.
    assert(_filters.permitProduct.size() == _config.productExecutables.size());
    for(std::size_t i=0; i<_config.productExecutables.size(); ++i)
    {
        const std::wstring &exe{_config.productExecutables[i]};
        updateBooleanFilter(permitProduct[i], params.allowPIA && !exe.empty(),
            ApplicationFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND,
                FWP_IP_VERSION_V4>{exe, 15});
    }

    // Local resolver related filters
    logFilter("allowResolver (block everything)", _filters.blockResolvers, luid && params.allowResolver, luid != _filterAdapterLuid);
    logFilter("allowResolver (tunnel traffic)", _filters.permitResolvers, luid && params.allowResolver, luid != _filterAdapterLuid);
    // Class invariant - resolver executables can't be changed.
    // Just like the product executables, it's also relevant that the actual
    // resolver paths can't be changed too.
    assert(_filters.blockResolvers.size() == _config.resolverExecutables.size());
    assert(_filters.permitResolvers.size() == _config.resolverExecutables.size());
    for(std::size_t i=0; i<_config.resolverExecutables.size(); ++i)
    {
        const std::wstring &exe{_config.resolverExecutables[i]};
        // (1) First we block everything coming from the resolver processes
        updateBooleanInvalidateFilter(blockResolvers[i],
            luid && params.allowResolver && !exe.empty(),
            luid != _filterAdapterLuid,
            ApplicationFilter<FWP_ACTION_BLOCK, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>{exe, 14});
        // (2) Next we poke a hole in this block but only allow data that goes across the tunnel
        updateBooleanInvalidateFilter(permitResolvers[i],
            luid && params.allowResolver && !exe.empty(),
            luid != _filterAdapterLuid,
            ApplicationFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>{exe, 15,
                Condition<FWP_UINT64>{FWPM_CONDITION_IP_LOCAL_INTERFACE, FWP_MATCH_EQUAL, &luid},
                Condition<FWP_UINT16>{FWPM_CONDITION_IP_REMOTE_PORT, FWP_MATCH_EQUAL, 53},
        // OR'ing of conditions is done automatically when you have 2 or more
        // consecutive conditions of the same fieldId. 13038 is the Handshake
        // control port
                Condition<FWP_UINT16>{FWPM_CONDITION_IP_REMOTE_PORT, FWP_MATCH_EQUAL, 13038}
            });
    }

    // Always permit loopback traffic, including IPv6.
    logFilter("allowLoopback", _filters.permitLocalhost, params.allowLoopback);
    updateBooleanFilter(permitLocalhost[0], params.allowLoopback, LocalhostFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(15));
    updateBooleanFilter(permitLocalhost[1], params.allowLoopback, LocalhostFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(15));

    // Get the current set of excluded app IDs.  If they've changed we recreate
    // all app rules, but if they stay the same we don't recreate them.
    AppIdSet emptyApps;
    const AppIdSet &newExcludedApps{params.enableSplitTunnel ? params.excludeApps : emptyApps};
    const AppIdSet &newVpnOnlyApps{params.enableSplitTunnel ? params.vpnOnlyApps : emptyApps};
    const AppIdSet &newVpnOnlyResolvers{params.enableSplitTunnel && params.bypassDefaultApps ? _resolverAppIds : emptyApps};
    std::string splitTunnelPhysIp;
    unsigned splitTunnelPhysNetPrefix{0};
    if(params.enableSplitTunnel)
    {
        splitTunnelPhysIp = params.netScan.ipAddress();
        splitTunnelPhysNetPrefix = params.netScan.prefixLength();
    }

    KAPPS_CORE_INFO() << "Number of excluded apps" << newExcludedApps.size();
    KAPPS_CORE_INFO() << "Number of vpnOnly apps" << newVpnOnlyApps.size();
    KAPPS_CORE_INFO() << "Number of vpnOnly resolvers" << newVpnOnlyResolvers.size();

    SplitTunnelFirewallParams newSplitParams{};
    newSplitParams._physicalIp = splitTunnelPhysIp;
    newSplitParams._physicalNetPrefix = splitTunnelPhysNetPrefix;
    newSplitParams._tunnelIp = params.tunnelDeviceLocalAddress;
    newSplitParams._isConnected = params.isConnected;
    newSplitParams._hasConnected = params.hasConnected;
    newSplitParams._vpnDefaultRoute = !params.bypassDefaultApps;
    newSplitParams._blockDNS = params.blockDNS;
    newSplitParams._forceVpnOnlyDns = newSplitParams._forceBypassDns = false;
    if(params.splitTunnelDnsEnabled)
    {
        if(_config.brandInfo.enableDnscache)
        {
            newSplitParams._forceVpnOnlyDns = params.bypassDefaultApps;
            newSplitParams._forceBypassDns = !params.bypassDefaultApps;
        }
        else
        {
            KAPPS_CORE_WARNING() << "_config.brandInfo.enableDnscache must be provided to enable split tunnel DNS; ignoring splitTunnelDnsEnabled";
        }
    }
    newSplitParams._effectiveDnsServers.reserve(params.effectiveDnsServers.size());
    for(const auto &effectiveDnsServer : params.effectiveDnsServers)
    {
        core::Ipv4Address serverIp{effectiveDnsServer};
        if(serverIp != core::Ipv4Address{})
            newSplitParams._effectiveDnsServers.push_back(serverIp.address());
    }
    newSplitParams._existingDnsServers = findExistingDNS(newSplitParams._effectiveDnsServers);

    reapplySplitTunnelFirewall(newSplitParams, newExcludedApps, newVpnOnlyApps,
                               newVpnOnlyResolvers);

    // Update subnet bypass routes
    _subnetBypass.updateRoutes(params);

    tx.commit();
}

void WinFirewall::updateAllBypassSubnetFilters(const FirewallParams &params)
{
    if(params.enableSplitTunnel)
    {
        if(params.bypassIpv4Subnets != _bypassIpv4Subnets)
            updateBypassSubnetFilters(params.bypassIpv4Subnets, _bypassIpv4Subnets, _subnetBypassFilters4, FWP_IP_VERSION_V4);

        if(params.bypassIpv6Subnets != _bypassIpv6Subnets)
            updateBypassSubnetFilters(params.bypassIpv6Subnets, _bypassIpv6Subnets, _subnetBypassFilters6, FWP_IP_VERSION_V6);
    }
    else
    {
        if(!_bypassIpv4Subnets.empty())
            updateBypassSubnetFilters({}, _bypassIpv4Subnets, _subnetBypassFilters4, FWP_IP_VERSION_V4);

        if(!_bypassIpv6Subnets.empty())
            updateBypassSubnetFilters({}, _bypassIpv6Subnets, _subnetBypassFilters6, FWP_IP_VERSION_V6);
    }
}

void WinFirewall::updateBypassSubnetFilters(const std::set<std::string> &subnets, std::set<std::string> &oldSubnets, std::vector<WfpFilterObject> &subnetBypassFilters, FWP_IP_VERSION ipVersion)
{
    for (auto &filter : subnetBypassFilters)
        deactivateFilter(filter, true);

    // If we have any IPv6 subnets we need to also whitelist IPv6 link-local and broadcast ranges
    // required by IPv6 Neighbor Discovery
    auto adjustedSubnets = subnets;
    if(ipVersion == FWP_IP_VERSION_V6 && !subnets.empty())
    {
        adjustedSubnets.emplace("fe80::/10");
        adjustedSubnets.emplace("ff00::/8");
    }

    subnetBypassFilters.resize(adjustedSubnets.size());

    int index{0};
    for(auto it = adjustedSubnets.begin(); it != adjustedSubnets.end(); ++it, ++index)
    {
        if(ipVersion == FWP_IP_VERSION_V6)
        {
            KAPPS_CORE_INFO() << "Creating Subnet ipv6 rule" << *it;
            activateFilter(subnetBypassFilters[index], true,
                IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>(*it, 10));
        }
        else
        {
            KAPPS_CORE_INFO() << "Creating Subnet ipv4 rule" << *it;
            activateFilter(subnetBypassFilters[index], true,
                IPSubnetFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>(*it, 10));
        }
    }

    // Update the bypass subnets
    oldSubnets = subnets;
}

void WinFirewall::removeSplitTunnelAppFilters(SplitAppFilterMap &apps,
                                            const core::StringSlice &traceType)
{
    for(auto &oldApp : apps)
    {
        KAPPS_CORE_INFO() << "remove" << traceType << "app filters:"
            << core::tracePointer(oldApp.first);
        deactivateFilter(oldApp.second.splitAppBind, true);
        deactivateFilter(oldApp.second.splitAppConnect, true);
        deactivateFilter(oldApp.second.appPermitAnyDns, true);
        deactivateFilter(oldApp.second.splitAppFlowEstablished, true);
        deactivateFilter(oldApp.second.permitApp, true);
        deactivateFilter(oldApp.second.blockAppIpv4, true);
        deactivateFilter(oldApp.second.blockAppIpv6, true);
    }
    apps.clear();
}

void WinFirewall::createBypassAppFilters(SplitAppFilterMap &apps,
                                         const WfpProviderContextObject &context,
                                         const std::shared_ptr<const AppIdKey> &pAppId, bool rewriteDns)
{
    KAPPS_CORE_INFO() << "add bypass app filters:" << core::tracePointer(pAppId);
    // If, for some reason, we get a nullptr pAppId here (meaning the app ID
    // couldn't be loaded), still create a nullptr entry so we don't reset the
    // filters every time we reapply, but don't create any filters.
    auto empResult = apps.emplace(pAppId, SplitAppFilters{});
    if(empResult.second && pAppId)
    {
        auto &appFilters = empResult.first->second;
        activateFilter(appFilters.permitApp, true, AppIdFilter<FWP_IP_VERSION_V4>{*pAppId, 15});
        activateFilter(appFilters.splitAppBind, true,
                        SplitFilter<FWP_IP_VERSION_V4>{*pAppId,
                            _config.brandInfo.wfpCalloutBindV4,
                            FWPM_LAYER_ALE_BIND_REDIRECT_V4,
                            context,
                            FWP_ACTION_CALLOUT_TERMINATING,
                            15});
        activateFilter(appFilters.splitAppConnect, true,
                        SplitFilter<FWP_IP_VERSION_V4>{*pAppId,
                            _config.brandInfo.wfpCalloutConnectV4,
                            FWPM_LAYER_ALE_CONNECT_REDIRECT_V4,
                            context,
                            FWP_ACTION_CALLOUT_TERMINATING,
                            15});
        if(rewriteDns)
        {
            // Permit sending to any IPv4 DNS address, then rewrite to the
            // intended address
            activateFilter(appFilters.appPermitAnyDns, true,
                            AppDNSFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>{
                                *pAppId, 15});
            activateFilter(appFilters.splitAppFlowEstablished, true,
                            SplitFilter<FWP_IP_VERSION_V4>{*pAppId,
                                _config.brandInfo.wfpCalloutFlowEstablishedV4,
                                FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4,
                                context,
                                FWP_ACTION_CALLOUT_INSPECTION,
                                15});
        }
    }
}

void WinFirewall::createOnlyVPNAppFilters(SplitAppFilterMap &apps,
                                        const WfpProviderContextObject &context,
                                        const std::shared_ptr<const AppIdKey> &pAppId, bool rewriteDns)
{
    KAPPS_CORE_INFO() << "add only-VPN app filters:" << core::tracePointer(pAppId);
    auto empResult = apps.emplace(pAppId, SplitAppFilters{});
    if(empResult.second && pAppId)
    {
        auto &appFilters = empResult.first->second;
        // While connected, the normal IPv6 firewall rule should still take care
        // of this, but keep this per-app rule around for robustness.
        activateFilter(appFilters.blockAppIpv6, true,
                       AppIdFilter<FWP_IP_VERSION_V6, FWP_ACTION_BLOCK>{*pAppId, 14});
        activateFilter(appFilters.splitAppBind, true,
                        SplitFilter<FWP_IP_VERSION_V4>{*pAppId,
                            _config.brandInfo.wfpCalloutBindV4,
                            FWPM_LAYER_ALE_BIND_REDIRECT_V4,
                            context,
                            FWP_ACTION_CALLOUT_TERMINATING,
                            15});
        activateFilter(appFilters.splitAppConnect, true,
                        SplitFilter<FWP_IP_VERSION_V4>{*pAppId,
                            _config.brandInfo.wfpCalloutConnectV4,
                            FWPM_LAYER_ALE_CONNECT_REDIRECT_V4,
                            context,
                            FWP_ACTION_CALLOUT_TERMINATING,
                            15});
        if(rewriteDns)
        {
            // Permit sending to any IPv4 DNS address, then rewrite to the
            // intended address
            activateFilter(appFilters.appPermitAnyDns, true,
                            AppDNSFilter<FWP_ACTION_PERMIT, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>{
                                *pAppId, 15});
            activateFilter(appFilters.splitAppFlowEstablished, true,
                            SplitFilter<FWP_IP_VERSION_V4>{*pAppId,
                                _config.brandInfo.wfpCalloutFlowEstablishedV4,
                                FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4,
                                context,
                                FWP_ACTION_CALLOUT_INSPECTION,
                                15});
        }
    }
}

void WinFirewall::createBlockAppFilters(SplitAppFilterMap &apps,
                                        const std::shared_ptr<const AppIdKey> &pAppId)
{
    KAPPS_CORE_INFO() << "add block app filters:" << core::tracePointer(pAppId);
    auto empResult = apps.emplace(pAppId, SplitAppFilters{});
    if(empResult.second && pAppId)
    {
        auto &appFilters = empResult.first->second;
        // Block IPv4, because we can't bind this app to the tunnel (the VPN is
        // not connected).
        activateFilter(appFilters.blockAppIpv4, true,
                       AppIdFilter<FWP_IP_VERSION_V4, FWP_ACTION_BLOCK>{*pAppId, 14});
        // Block IPv6, because the normal IPv6 firewall rule is not active when
        // disconnected (unless the killswitch is set to Always).
        activateFilter(appFilters.blockAppIpv6, true,
                       AppIdFilter<FWP_IP_VERSION_V6, FWP_ACTION_BLOCK>{*pAppId, 14});
    }
}

void WinFirewall::reapplySplitTunnelFirewall(const SplitTunnelFirewallParams &params,
                                           const AppIdSet &newExcludedApps,
                                           const AppIdSet &newVpnOnlyApps,
                                           const AppIdSet &newVpnOnlyResolvers)
{
    bool sameExcludedApps = areAppsUnchanged(newExcludedApps, excludedApps);
    bool sameVpnOnlyApps = areAppsUnchanged(newVpnOnlyApps, vpnOnlyApps);
    bool sameVpnOnlyResolvers = areAppsUnchanged(newVpnOnlyResolvers, vpnOnlyResolvers);

    // If anything changes, we have to delete all filters and recreate
    // everything.  WFP has been known to throw spurious errors if we try to
    // reuse callout or context objects, so we delete everything in order to
    // tear those down and recreate them.
    if(sameExcludedApps && sameVpnOnlyApps && sameVpnOnlyResolvers &&
        _lastSplitParams == params)
    {
        KAPPS_CORE_INFO() << "Split tunnel rules have not changed - excluded:"
            << excludedApps.size() << "- VPN-only:" << vpnOnlyApps.size()
            << "- resolvers:" << vpnOnlyResolvers.size() << _lastSplitParams;
        return;
    }

    if(_filters.ipInbound != zeroGuid)
    {
        KAPPS_CORE_INFO() << "deactivate IpInbound object";
        deactivateFilter(_filters.ipInbound, true);
    }

    if(_filters.splitCalloutIpInbound != zeroGuid)
    {
        KAPPS_CORE_INFO() << "deactivate IpInbound callout object";
        deactivateFilter(_filters.splitCalloutIpInbound, true);
    }

    if (_filters.ipOutbound != zeroGuid)
    {
        KAPPS_CORE_INFO() << "deactivate IpOutbound object";
        deactivateFilter(_filters.ipOutbound, true);
    }

    if (_filters.splitCalloutIpOutbound != zeroGuid)
    {
        KAPPS_CORE_INFO() << "deactivate IpOutbound callout object";
        deactivateFilter(_filters.splitCalloutIpOutbound, true);
    }

    if (_filters.permitInjectedDns != zeroGuid)
    {
        KAPPS_CORE_INFO() << "deactivate permitInjectedDns filter";
        deactivateFilter(_filters.permitInjectedDns, true);
    }

    if (_filters.splitCalloutConnectAuth != zeroGuid)
    {
        KAPPS_CORE_INFO() << "deactivate ConnectAuth callout object";
        deactivateFilter(_filters.splitCalloutConnectAuth, true);
    }

    // Remove all app filters
    removeSplitTunnelAppFilters(excludedApps, "excluded");
    removeSplitTunnelAppFilters(vpnOnlyApps, "VPN-only");
    removeSplitTunnelAppFilters(vpnOnlyResolvers, "resolvers");

    // Delete the old callout and provider context.  WFP does not seem to like
    // reusing the provider context (attempting to reuse it generates an error
    // saying that it does not exist, despite the fact that deleting it
    // succeeds), so out of paranoia we never reuse either object.
    if(_filters.splitCalloutBind != zeroGuid)
    {
        KAPPS_CORE_INFO() << "deactivate bind callout object";
        deactivateFilter(_filters.splitCalloutBind, true);
    }
    if(_filters.splitCalloutConnect != zeroGuid)
    {
        KAPPS_CORE_INFO() << "deactivate connect callout object";
        deactivateFilter(_filters.splitCalloutConnect, true);
    }
    if(_filters.splitCalloutFlowEstablished != zeroGuid)
    {
        KAPPS_CORE_INFO() << "deactivate flow established callout object";
        deactivateFilter(_filters.splitCalloutFlowEstablished, true);
    }
    if(_filters.providerContextKey != zeroGuid)
    {
        KAPPS_CORE_INFO() << "deactivate exclusion provider context object";
        deactivateFilter(_filters.providerContextKey, true);
    }
    if(_filters.vpnOnlyProviderContextKey != zeroGuid)
    {
        KAPPS_CORE_INFO() << "deactivate VPN-only provider context object";
        deactivateFilter(_filters.vpnOnlyProviderContextKey, true);
    }

    // Keep track of the state we used to apply these rules, so we know when to
    // recreate them
    _lastSplitParams = params;
    KAPPS_CORE_INFO() << "Creating split tunnel rules with state" << _lastSplitParams;

    // We can only create exclude rules when the appropriate bind IP address is known
    bool createExcludedRules = !_lastSplitParams._physicalIp.empty() && !newExcludedApps.empty();
    // VPN-only rules are applied even if the last tunnel IP is not known
    // though; we still apply the block rule ("per-app killswitch") until the IP
    // is known.
    bool createVpnOnlyRules = !newVpnOnlyApps.empty() || !newVpnOnlyResolvers.empty();
    // We create bind rules for VPN-only apps when connected and the IP is
    // known; otherwise we just create a block rule (which does not require the
    // callout/context objects).
    bool createVpnOnlyBindRules = _lastSplitParams._hasConnected && !_lastSplitParams._tunnelIp.empty();

    // See Driver.c in desktop-wfp-callout
    struct ContextData
    {
        UINT32 bindIp;
        UINT32 rewriteDnsServer;
        UINT32 dnsSourceIp;
    };

    ContextData bypassContext{}, vpnOnlyContext{};

    bypassContext.bindIp = core::Ipv4Address{_lastSplitParams._physicalIp}.address();
    vpnOnlyContext.bindIp = core::Ipv4Address{_lastSplitParams._tunnelIp}.address();

    // Create the new callout and context objects if any callout rules are
    // needed.
    //
    // These aren't needed if split tunnel is completely inactive, or if we are
    // just blocking VPN-only apps (per-app block rules don't require any
    // callouts).
    if(createExcludedRules || (createVpnOnlyRules && createVpnOnlyBindRules))
    {
        if(_lastSplitParams._forceVpnOnlyDns)
        {
            // The VPN does _not_ have the default route.  Rewrite VPN-only apps
            // to use PIA's configured DNS.
            vpnOnlyContext.rewriteDnsServer = _lastSplitParams._effectiveDnsServers[0].address();
            // If the DNS address is on loopback, use the loopback interface.
            // Otherwise, use the tunnel device.
            //
            // We could consider using the physical interface if the user
            // entered custom DNS that is on-link for that interface, but
            // currently we always route all DNS through the tunnel (even
            // without split tunnel; the VPN methods always route DNS servers
            // into the tunnel).
            core::Ipv4Address rewriteDnsServerAddr{vpnOnlyContext.rewriteDnsServer};
            if(rewriteDnsServerAddr.isLoopback())
                vpnOnlyContext.dnsSourceIp = 0x7F000001;    // 127.0.0.1
            // Check if it's on-link for the physical interface (we've already
            // parsed the physical interface address in the bypass context)
            else if(rewriteDnsServerAddr.inSubnet(bypassContext.bindIp, _lastSplitParams._physicalNetPrefix))
                vpnOnlyContext.dnsSourceIp = bypassContext.bindIp;
            else
                vpnOnlyContext.dnsSourceIp = vpnOnlyContext.bindIp;
            KAPPS_CORE_INFO() << "Rewrite DNS for VPN-only apps to"
                << core::Ipv4Address{vpnOnlyContext.rewriteDnsServer} << "on interface"
                << core::Ipv4Address{vpnOnlyContext.dnsSourceIp};
        }

        if(_lastSplitParams._forceBypassDns)
        {
            // The VPN has the default route - rewrite DNS for 'bypass' apps.
            // We expect them to send to the PIA-configured DNS; rewrite back to
            // the original DNS.  Do this even if PIA's DNS is the same as the
            // existing DNS, because this still ensures that DNS from bypass
            // apps is sent via the physical interface.
            //
            // We should know the existing DNS servers at this point, but it's
            // possible that there weren't any.  If not, then we'll have to use
            // tunnel DNS for bypass apps (skip this rule).
            if(!_lastSplitParams._existingDnsServers.empty())
            {
                bypassContext.rewriteDnsServer = _lastSplitParams._existingDnsServers[0].address();
                // Use the loopback interface for a DNS server on loopback, or
                // the physical interface otherwise.  No need to check whether
                // the address is on-link since we're already using the physical
                // interface anyway.
                if(core::Ipv4Address{bypassContext.rewriteDnsServer}.isLoopback())
                    bypassContext.dnsSourceIp = 0x7F000001;
                else
                    bypassContext.dnsSourceIp = bypassContext.bindIp;
                KAPPS_CORE_INFO() << "Rewrite DNS for bypass apps to"
                    << core::Ipv4Address{bypassContext.rewriteDnsServer}
                    << "on interface" << core::Ipv4Address{bypassContext.dnsSourceIp};
            }
            else
            {
                KAPPS_CORE_INFO() << "Existing DNS servers not configured or not know, bypass apps will use tunnel DNS.";
            }
        }

        if(bypassContext.bindIp)
        {
            ProviderContext splitProviderContext{&bypassContext, sizeof(bypassContext)};
            KAPPS_CORE_INFO() << "activate bypass provider context object";
            activateFilter(_filters.providerContextKey, true, splitProviderContext);
        }
        else
            KAPPS_CORE_INFO() << "Not activating bypass provider context object, IP not known";

        if(vpnOnlyContext.bindIp)
        {
            ProviderContext vpnProviderContext{&vpnOnlyContext, sizeof(vpnOnlyContext)};
            KAPPS_CORE_INFO() << "activate VPN-only provider context object";
            activateFilter(_filters.vpnOnlyProviderContextKey, true, vpnProviderContext);
        }
        else
            KAPPS_CORE_INFO() << "Not activating VPN-only provider context object, IP not known";

        KAPPS_CORE_INFO() << "activate callout objects";
        activateFilter(_filters.splitCalloutBind, true, Callout{FWPM_LAYER_ALE_BIND_REDIRECT_V4, _config.brandInfo.wfpCalloutBindV4});
        activateFilter(_filters.splitCalloutConnect, true, Callout{FWPM_LAYER_ALE_CONNECT_REDIRECT_V4, _config.brandInfo.wfpCalloutConnectV4});
        activateFilter(_filters.splitCalloutFlowEstablished, true, Callout{FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4, _config.brandInfo.wfpCalloutFlowEstablishedV4});
        activateFilter(_filters.splitCalloutConnectAuth, true, Callout{FWPM_LAYER_ALE_AUTH_CONNECT_V4, _config.brandInfo.wfpCalloutConnectAuthV4});
        activateFilter(_filters.splitCalloutIpInbound, true, Callout{FWPM_LAYER_INBOUND_IPPACKET_V4, _config.brandInfo.wfpCalloutIppacketInboundV4});
        activateFilter(_filters.splitCalloutIpOutbound, true, Callout{FWPM_LAYER_OUTBOUND_IPPACKET_V4, _config.brandInfo.wfpCalloutIppacketOutboundV4});
    }
    else
    {
        KAPPS_CORE_INFO() << "Not creating callout objects; not needed: exclude:"
            << createExcludedRules << "- VPN-only:" << createVpnOnlyRules
            << "- VPN-only bind:" << createVpnOnlyBindRules;
    }

    // If we are rewriting DNS for any app rule, activate these filters and
    // stop the DNSCache service if it's running
    if(bypassContext.rewriteDnsServer || vpnOnlyContext.rewriteDnsServer)
    {
        // If DNS leak protection is active, we need to explicitly permit
        // injected DNS, since the existing DNS servers would be blocked
        // normally.
        if(params._blockDNS)
        {
            activateFilter(_filters.permitInjectedDns, true,
                            CalloutDNSFilter<FWP_IP_VERSION_V4>{
                                _config.brandInfo.wfpCalloutConnectAuthV4,
                                FWPM_LAYER_ALE_AUTH_CONNECT_V4, 11});
        }
        // DNS rewriting occurs in the IPPACKET layers.  Outbound packets have
        // to be injected at the IP layer (not at the transport layer) because
        // we have to rewrite the source to the physical interface.  That means
        // inbound also has to be handled at the IPPACKET layer because WFP is
        // not aware of the rewritten UDP flows and would otherwise discard
        // these packets.
        activateFilter(_filters.ipInbound, true, IpInboundFilter{_config.brandInfo.wfpCalloutIppacketInboundV4, zeroGuid, 10});
        activateFilter(_filters.ipOutbound, true, IpOutboundFilter{_config.brandInfo.wfpCalloutIppacketOutboundV4, zeroGuid, 10});
    }

    if(_lastSplitParams._isConnected && (bypassContext.rewriteDnsServer || vpnOnlyContext.rewriteDnsServer))
    {
        // enableDnscache is checked by applyRules(), it suppresses
        // _forceVpnOnlyDns/_forceBypassDns if it wasn't provided
        assert(_config.brandInfo.enableDnscache);
        // When connected with split tunnel DNS active, disable Dnscache.  We
        // can't have a system-wide DNS cache since DNS responses may vary by
        // app.  This causes apps to do their own DNS requests, which we can
        // handle on a per-app basis in the callout driver.
        //
        // We have to wait until we're connected to do this (and restore it when
        // not connected) because Dnscache must be up to apply DNS servers
        // statically - which we do when connecting with WireGuard or the
        // OpenVPN static method.
        _config.brandInfo.enableDnscache(false);
    }
    else
    {
        // enableDnscache is checked by applyRules(), it suppresses
        // _forceVpnOnlyDns/_forceBypassDns if it wasn't provided
        assert(_config.brandInfo.enableDnscache);
        // Restore Dnscache.  We need Dnscache to be up in order to connect with
        // WireGuard or the OpenVPN static (non-DHCP) method.
        _config.brandInfo.enableDnscache(true);
    }

    if(createExcludedRules)
    {
        KAPPS_CORE_INFO() << "Creating exclude rules for" << newExcludedApps.size() << "apps";
        for(const auto &pAppId : newExcludedApps)
        {
            createBypassAppFilters(excludedApps, _filters.providerContextKey,
                                   pAppId, bypassContext.rewriteDnsServer);
        }
    }

    if(createVpnOnlyRules)
    {
        KAPPS_CORE_INFO() << "Creating VPN-only rules for" << newVpnOnlyApps.size() << "apps";
        for(const auto &pAppId : newVpnOnlyApps)
        {
            if(createVpnOnlyBindRules)
            {
                createOnlyVPNAppFilters(vpnOnlyApps, _filters.vpnOnlyProviderContextKey,
                                        pAppId, vpnOnlyContext.rewriteDnsServer);
            }
            else
            {
                createBlockAppFilters(vpnOnlyApps, pAppId);
            }
        }
        KAPPS_CORE_INFO() << "Creating VPN-only rules for" << newVpnOnlyResolvers.size() << "apps";
        for(const auto &pAppId : newVpnOnlyResolvers)
        {
            if(createVpnOnlyBindRules)
            {
                createOnlyVPNAppFilters(vpnOnlyResolvers, _filters.vpnOnlyProviderContextKey,
                                        pAppId,
                                        false);
            }
            else
            {
                createBlockAppFilters(vpnOnlyResolvers, pAppId);
            }
        }
    }
}

}}
