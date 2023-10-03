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
#include <kapps_net/net.h>
#include <set>
#include <memory>
#include <array>
#include <string>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "mac_splittunnel_types.h"
#include "utun.h"
#include "../originalnetworkscan.h"
#include "../firewallparams.h"
#include "pf_firewall.h"
#include <kapps_core/src/stringslice.h>

class UpdateStrategy;

class RuleUpdater
{
private:
    std::array<PortSet, 2> _ports;
    OriginalNetworkScan _netScan;
    std::set<std::string> _bypassIpv4Subnets;
    std::set<std::string> _bypassIpv6Subnets;
    std::unique_ptr<UpdateStrategy> _strategy;
    kapps::net::PFFirewall &_filter;

public:
    RuleUpdater(std::unique_ptr<UpdateStrategy> strategy, kapps::net::PFFirewall &filter)
        : _strategy{std::move(strategy)},
          _filter{filter}
    {}

    void update(IPVersion ipVersion, const PortSet &ports,
                const kapps::net::FirewallParams &params);
    void forceUpdate(IPVersion ipVersion, const PortSet &ports,
                     const kapps::net::FirewallParams &params) const;
    void clearRules(IPVersion ipVersion);
    void clearAllRules();
};

class UpdateStrategy
{
public:
    using StringVector = std::vector<std::string>;

public:
    UpdateStrategy(kapps::net::PFFirewall &filter)
    : _filter{filter}
    {}
    virtual ~UpdateStrategy() = default;
    virtual StringVector rules(IPVersion ipVersion, const PortSet &ports,
                               const kapps::net::FirewallParams &params) const;
    virtual StringVector routingRule(IPVersion ipVersion,
                                     const kapps::net::FirewallParams &params) const = 0;
    virtual kapps::core::StringSlice anchorNameFor(IPVersion ipVersion) const = 0;
    virtual kapps::core::StringSlice tagNameFor(IPVersion ipVersion) const = 0;

protected:
    kapps::core::StringSlice protocolFor(IPVersion ipVersion) const { return ipVersion == IPv4 ? ("inet") : ("inet6"); }
    kapps::core::StringSlice icmpVersion(IPVersion ipVersion) const { return (ipVersion == IPv4 ? "icmp" : "icmp6"); }
    const std::string &gatewayIp(IPVersion ipVersion, const kapps::net::FirewallParams &params) const
    {
        return (ipVersion == IPv4 ? params.netScan.gatewayIp() : params.netScan.gatewayIp6());
    }

protected:
    kapps::net::PFFirewall &_filter;
};

class BypassStrategy : public UpdateStrategy
{
public:
    using UpdateStrategy::UpdateStrategy;
protected:
    virtual kapps::core::StringSlice tagNameFor(IPVersion ipVersion) const override;
    virtual kapps::core::StringSlice anchorNameFor(IPVersion ipVersion) const override;
    virtual StringVector routingRule(IPVersion ipVersion,
                                     const kapps::net::FirewallParams &params) const override;
};

class VpnOnlyStrategy : public UpdateStrategy
{
public:
    using UpdateStrategy::UpdateStrategy;
protected:
    virtual kapps::core::StringSlice tagNameFor(IPVersion ipVersion) const override;
    virtual kapps::core::StringSlice anchorNameFor(IPVersion ipVersion) const override;
    virtual StringVector routingRule(IPVersion ipVersion,
                                     const kapps::net::FirewallParams &params) const override;
};

class DefaultStrategy : public UpdateStrategy
{
public:
    using UpdateStrategy::UpdateStrategy;
protected:
    virtual StringVector rules(IPVersion ipVersion, const PortSet &ports,
                               const kapps::net::FirewallParams &params) const override;
    virtual kapps::core::StringSlice tagNameFor(IPVersion ipVersion) const override;
    virtual kapps::core::StringSlice anchorNameFor(IPVersion ipVersion) const override;
    virtual StringVector routingRule(IPVersion ipVersion,
                                     const kapps::net::FirewallParams &params) const override;
protected:
    std::set<std::string> bypassSubnetsFor(IPVersion ipVersion,
                                           const kapps::net::FirewallParams &params) const;
    std::set<std::string> lanSubnetsFor(IPVersion ipVersion,
                                        const kapps::net::FirewallParams &params) const;
};
