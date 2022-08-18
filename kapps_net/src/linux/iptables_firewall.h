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
#include <kapps_core/core.h>
#include <kapps_net/net.h>
#include "linux_fwmark.h"
#include "linux_routing.h"
#include "../firewallparams.h"
#include <kapps_core/src/util.h>
#include <unordered_set>
#include <memory>
#include <string>
#include "../firewall.h"

enum class TableEnum
{
    Filter,
    Nat,
    Mangle,
    Raw
};

template <TableEnum tableType> class Table;

class IpTablesFirewall
{
private:
    class Impl;

public:
    enum IPVersion { IPv4, IPv6, Both };

public:
    IpTablesFirewall(const kapps::net::FirewallConfig &config);
    ~IpTablesFirewall();

public:
    const kapps::net::Fwmark &fwmark() const;
    const kapps::net::Routing &routing() const;

    // Install/uninstall the firewall anchors
    void install();
    void uninstall();
    bool isInstalled();
    void ensureRootAnchorPriority(IPVersion ip = Both);
    void updateRules(const kapps::net::FirewallParams &params);
    void updateBypassSubnets(IPVersion ipVersion, const std::unordered_set<std::string> &bypassSubnets, std::unordered_set<std::string> &oldBypassSubnets);
    void setAnchorEnabled(TableEnum tableType, IPVersion ip, const std::string &anchorName, bool enabled) const;
    void replaceAnchor(TableEnum tableType, IPVersion ip, const std::string &anchorName, const std::vector<std::string> &newRules) const;

public:
    const std::string& hnsdGroupName() const;

private:
    std::unique_ptr<Impl> _pImpl;
};
