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
#include <kapps_net/net.h>
#include <kapps_core/src/winapi.h>
#include <string>
#include <functional>

namespace kapps::net {

// Brand configuration used by the firewall.  These can't be changed once a
// `Firewall` is created.
//
// Many of these parameters are platform-specific; see descriptions below.
struct KAPPS_NET_EXPORT BrandInfo
{
    std::string code;         // i.e "pia"
    std::string identifier;   // i.e "com.privateinternetaccess.vpn"

#if defined(KAPPS_CORE_OS_WINDOWS)
    // Product name and description used in WFP filters.  These don't impact
    // functionality, they're just visible in WFP dumps.  These can be changed
    // between product releases if needed, WinFirewall recreates all rules the
    // first time the firewall rules are applied.
    //
    // (These are all non-const pointers due to the WFP API, it probably does
    // not modify the data but we don't have a guarantee.)
    wchar_t *pWfpFilterName;
    wchar_t *pWfpFilterDescription;
    // Name and description for the WFP provider context.
    wchar_t *pWfpProviderCtxName;
    wchar_t *pWfpProviderCtxDescription;
    // Name and description for the WFP callout object.  (This is brand-
    // specific like the other display strings, it is not related to the brand
    // of the callout driver itself.)
    wchar_t *pWfpCalloutName;
    wchar_t *pWfpCalloutDescription;
    // These GUIDs specify the WFP provider and sublayer used by this
    // application.  These should be brand-specific, so generate new GUIDs for
    // each brand.
    //
    // Do not change these after shipping a product for a given brand;
    // WinFirewall is able to recover from system crashes / improper cleanup /
    // etc. by cleaning up the firewall rules for the provider/sublayer.
    GUID wfpBrandProvider;
    GUID wfpBrandSublayer;
    // These GUIDs identify the callout filters defined by the PIA WFP callout
    // driver.
    //
    // These MUST match the GUIDs registered by the driver.  If you are using
    // the PIA build of the callout, you MUST use the original GUIDs.
    //
    // If you build your own branded callout driver, you can should change the
    // GUIDs in your driver, and provide those GUIDs here.
    GUID wfpCalloutBindV4;
    GUID wfpCalloutConnectV4;
    GUID wfpCalloutFlowEstablishedV4;
    GUID wfpCalloutConnectAuthV4;
    GUID wfpCalloutIppacketInboundV4;
    GUID wfpCalloutIppacketOutboundV4;

    // On Windows, WinFirewall must disable the Dnscache service for this to
    // work, since we can't have a system-wide DNS cache.  Disabling Dnscache
    // causes apps to send their own DNS requests to UDP/TCP 53, which we then
    // rewrite in the callout driver.  However, Dnscache provides some other
    // functionality too:
    // - NetBIOS name resolution does not work.
    // - DHCP negotiations, and some netsh configuration commands, do not work.
    //
    // Because of the DHCP/netsh issue, Dnscache usually must be brought back up
    // in order to reconnect the physical interface or VPN.  WinFirewall will
    // restore Dnscache whenever the VPN is not connected to permit
    // reconnection.
    //
    // This functor is used to enable/disable the Dnscache service when split
    // tunnel DNS is enabled and required.  (WinDnsCacheControl provides this,
    // but it has not been moved out to kapps::net due to heavy Qt dependencies
    // via QTimer and Task.)
    //
    // The bool parameter indicates whether we want Dnscache to be up (true) or
    // down (false).  If this isn't provided, split tunnel DNS cannot be
    // enabled.
    //
    // If the service is already in the requested state, this should do nothing;
    // it's called whenever firewall rules are reapplied even if the requested
    // state has not changed.
    std::function<void(bool)> enableDnscache;
#elif defined(KAPPS_CORE_OS_LINUX)
    unsigned cgroupBase{};
    unsigned fwmarkBase{};
#endif
};

struct KAPPS_NET_EXPORT FirewallConfig
{
    std::string daemonDataDir; // "/Library/Application Support/com.privateinternetaccess.vpn"
    std::string resourceDir;  // "/Applications/Private Internet Access.app/Contents/Resources"
    std::string executableDir;
    std::string installationDir;

#if defined(KAPPS_CORE_OS_WINDOWS)
    // Product executables to permit through killswitch/leak protection on
    // Windows.  (On Mac/Linux, product executables are permitted by running as
    // a specific Unix group.)
    std::vector<std::wstring> productExecutables;
    // Resolver executables to permit through firewall on Windows.  These are
    // allowed to send DNS (UDP/TCP 53) traffic anywhere to act as recursive
    // resolvers (DNS is blocked from other processes for DNS leak protection).
    // The Handshake communication port is also permitted; all other traffic is
    // blocked.  (On Mac/Linux, a Unix group is used instead like for product
    // executables.)
    std::vector<std::wstring> resolverExecutables;
#elif defined(KAPPS_CORE_OS_LINUX)
    std::string bypassFile;
    std::string vpnOnlyFile;
    std::string defaultFile;
#elif defined(KAPPS_CORE_OS_MACOS)
    std::string unboundDnsStubConfigFile;
    std::string unboundExecutableFile;
#endif

    BrandInfo brandInfo;
};

}
