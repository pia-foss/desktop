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
#include <kapps_core/core.h>
#include "originalnetworkscan.h"
#include <string>
#include <set>
#include <vector>
#include <memory>

#if defined(KAPPS_CORE_OS_WINDOWS)
#include "win/appidkey.h"
#endif

namespace kapps { namespace net {

// Descriptor for a set of firewall rules to be applied.
//
struct KAPPS_NET_EXPORT FirewallParams
{
    // These parameters are specified by VPNConnection (some are passed through
    // from the VPNMethod)

    // VPN network interface information
    std::string tunnelDeviceName;
    std::string tunnelDeviceLocalAddress;
    std::string tunnelDeviceRemoteAddress;

    // Linux only
    // Whether routed packets (i.e Docker) should go over VPN or not
    // if set to false then routed packets bypass VPN
    bool routedPacketsOnVPN;

    // The following flags indicate which general rulesets are needed. Note that
    // this is after some sanity filtering, i.e. an allow rule may be listed
    // as not needed if there were no block rules preceding it. The rulesets
    // should be thought of as in last-match order.
    bool leakProtectionEnabled; // Apply leak protection (on for KS=always, or for KS=auto when connected)
    bool blockAll;      // Block all traffic by default
    bool allowVPN;      // Exempt traffic through VPN tunnel
    bool allowDHCP;     // Exempt DHCP traffic
    bool blockIPv6;     // Block all IPv6 traffic
    bool allowLAN;      // Exempt LAN traffic, including IPv6 LAN traffic
    bool blockDNS;      // Block all DNS traffic except specified DNS servers
    bool allowPIA;      // Exempt PIA executables
    bool allowLoopback; // Exempt loopback traffic
    bool allowResolver; // Exempt local DNS resolver traffic

    // Whether we are connected to the VPN right now.  Note that this differs
    // from 'hasConnected'.
    bool isConnected;
    // Have we connected to the VPN since it was enabled?  (Some rules are only
    // activated once we successfully connect, but remain active even if we lose
    // the connection while reconnecting.)
    bool hasConnected;
    // Whether to bypass the VPN for apps with no split tunnel rules.
    // When split tunnel is enabled, or when not connecting/connected, this is
    // false.
    bool bypassDefaultApps;
    // Whether the default route has or will be set to the VPN (false when not
    // connected or connecting).  Note that this _really_ refers to the routing
    // table itself, not the intended default app behavior (which is indicated
    // by bypassDefaultApps, and may not be the same on Mac).
    bool setDefaultRoute;

    // Whether to enable split tunnel.  Split tunnel is enabled whenever the
    // setting is enabled, even if the PIA client is not logged in.  This is
    // important to block Only VPN apps - otherwise, they may leak even after
    // PIA is started/connected, because they could have existing connections
    // that were permitted.
    //
    // Bypass VPN apps though are only affected when we connect
    // - persistent connections from those apps would be
    // fine since they bypass the VPN anyway.
    bool enableSplitTunnel;
    // Original network information used (among other things) to manage apps for split
    // tunnel.
    OriginalNetworkScan netScan;

    // Whether to enable split tunnel DNS.  When enabled, 'bypass' applications
    // will use the system's existing DNS, while 'VPN only' applications will
    // use the VPN DNS.
    //
    // This is mainly important when the VPN DNS redirects some hostnames to
    // proxies only reachable through the VPN - this would break bypass apps
    // attempting to use those same services.
    //
    // This necessitates disabling or avoiding any system-wide DNS cache,
    // because DNS responses for a particular hostname may vary by application.
    //
    // Support for this feature varies by platform:
    // - Linux: Fully supported; app DNS requests are redirected to the
    //   appropriate DNS server (and interface) using iptables.  This also
    //   bypasses any local caching DNS resolver, if present.
    // - macOS: Not supported.  There is no way to disable the system-wide DNS
    //   cache.
    // - Windows: Supported, but there are caveats.  WinFirewall disables the
    //   Dnscache service when connected with this feature enable;
    //   BrandInfo::enableDnscache must be provided.
    bool splitTunnelDnsEnabled;

    // Mtu of the tunnel interface
    int mtu;

    // DNS servers
    std::vector<std::string> effectiveDnsServers;

#if defined(KAPPS_CORE_OS_POSIX)
    // On POSIX, split tunnel apps are defined with ordinary file paths.  On
    // macOS, they are folders (normally app bundles, but they can be any
    // folder - executables are matched using path prefixes).  On Linux, they
    // are exact executable paths - child processes inherit the parent's cgroup
    // assignments by default.
    std::vector<std::string> excludeApps;
    std::vector<std::string> vpnOnlyApps;
#elif defined(KAPPS_CORE_OS_WINDOWS)
    // On Windows split tunnel apps are defined using AppIdKeys, which are
    // normally provided by WinAppMonitor.  It monitors app executions using the
    // current split tunnel rules to identify child processes and include them
    // in the app ID set.
    AppIdSet excludeApps;
    AppIdSet vpnOnlyApps;
#endif

    std::set<std::string> bypassIpv4Subnets; // IPv4 subnets to bypass VPN
    std::set<std::string> bypassIpv6Subnets; // IPv6 subnets to bypass VPN
    // macos only - indentifies the primary interface
    std::string macosPrimaryServiceKey;

    // The DNS servers prior to connecting
    std::vector<uint32_t> existingDNSServers;
#if defined(KAPPS_CORE_OS_MACOS)
    // This parameter controls if Split Tunnel transparent proxy logs
    // are saved in the same folder as pia-daemon 
    bool transparentProxyLogEnabled;
#endif
};

}}
