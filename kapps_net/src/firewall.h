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

#ifndef KAPPS_NET_FIREWALL_H
#define KAPPS_NET_FIREWALL_H

#include <kapps_net/net.h>
#include "firewallconfig.h"
#include "firewallparams.h"
#include <kapps_core/src/logger.h>
#include <functional>

// ************
// * Firewall *
// ************
//
// The kapps-net Firewall component implements the firewall engine of desktop
// products, including backends for Windows (WFP), macOS (pf), and Linux
// (iptables).  The module exposes a descriptive API using `FirewallParams`
// to describe the desired state of the firewall, which the product can then
// apply using Firewall::applyRules().  `FirewallConfig` is also used to set
// brand-specific parameters, such as layer identifiers, cgroups, etc.
//
// The firewall engine also does some routing where it is very closely related
// to other functionality provided by the firewall, such as for subnet-based
// split tunneling.
//
// The Firewall module implements a lot of VPN-related functionality, including:
// - IPv4 leak protection
// - IPv6 leak protection
// - DNS leak protection
// - Application-based split tunneling
// - Subnet-based split tunneling
//
// "Killswitch" application features can be implemented by enabling leak
// protection rules even when not connected.  Note that this approach avoids any
// possible transient leaky state when a connection is lost (i.e. no action is
// needed to transition from "leak protection" to "killswitch", the rules are
// already in place).
//
// The FirewallParams state description is very similar across all platforms,
// but it is not quite identical - differences are noted in firewallparams.h.
// In some cases, fields are similar but have slightly different meaning (such
// as device LUIDs versus device node names).  In a few cases, specific fields
// only exist on platforms where they are used.
//
// *********
// * Usage *
// *********
//
// 1. Create a `Firewall` object to install firewall anchors and control the
//    firewall state:
//    * `kapps::net::FirewallConfig fwConfig;`
//    * `// set brand configuration in fwConfig
//    * `kapps::net::Firewall fw{std::move{fwConfig}};`
//
// 2. Call `Firewall::applyRules()` with the initial desired firewall state
//    * `kapps::net::FirewallParams fwParams;`
//    * `fw.applyRules(fwParams);`
//
// 3. Call `Firewall::applyRules()` again whenever the desired state changes,
//    or when any other relevant state information changes, such as local IP
//    addresses or VPN state.
//
// 4. Destroy `Firewall` to clean up all rules and uninstall anchors.  This
//    is normally automatic, just don't do anything silly like creating a
//    `Firewall` with `new` and leaking it.
//
// Only one `Firewall` can exist at a time, since it manages global system
// state.
namespace kapps { namespace net {

class KAPPS_NET_EXPORT PlatformFirewall
{
public:
    virtual ~PlatformFirewall() = default;

public:
    virtual void applyRules(const FirewallParams &params) = 0;
    void toggleSplitTunnel(const FirewallParams &params);
    virtual void startSplitTunnel(const FirewallParams& params) = 0;
    virtual void updateSplitTunnel(const FirewallParams &params) = 0;
    virtual void stopSplitTunnel() = 0;

#if defined(KAPPS_CORE_OS_MACOS)
    // macOS only - see Firewall::aboutToConnectToVpn()
    virtual void aboutToConnectToVpn() = 0;
#endif

public:
    bool _enableSplitTunnel{false};
};

class KAPPS_NET_EXPORT Firewall
{
public:
    Firewall(FirewallConfig config);

public:
    void applyRules(const FirewallParams &params);

#if defined(KAPPS_CORE_OS_MACOS)
    // On macOS only, this API should be called just before attempting to
    // connect to the VPN.  This is used when split tunnel is active to cycle
    // the split tunnel device.
    //
    // Unlike other platforms, macOS split tunnel must remain active even when
    // not connected to implement "Only VPN" app blocks.  We still route all
    // flows into the split tunnel device and block anything from "Only VPN"
    // apps.  Other apps' flows are routed to the physical interface.
    //
    // This means we need to break those flows once a connection occurs, or they
    // otherwise would continue to route around the VPN.
    //
    // (On Linux and Windows, Only VPN apps are implemented with app-specific
    // firewall rules, so the regular split tunnel machinery is not active when
    // not connected to the VPN.)
    void aboutToConnectToVpn();
#endif

protected:
    std::unique_ptr<PlatformFirewall> _pPlatformFirewall;
};

}}


#endif
