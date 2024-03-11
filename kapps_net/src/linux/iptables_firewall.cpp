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

#include "iptables_firewall.h"
#include <kapps_core/src/newexec.h>
#include "linux_cgroup.h"
#include "linux_fwmark.h"
#include "linux_routing.h"
#include <kapps_core/src/newexec.h>
#include <map>
#include <iostream>
#include <unordered_map>
#include "../originalnetworkscan.h"
#include <kapps_core/src/util.h>
#include <kapps_core/src/ipaddress.h>

enum class ChainEnum
{
    PREROUTING,
    INPUT,
    OUTPUT,
    FORWARD,
    POSTROUTING
};

struct EnumClassHash
{
    template <typename T>
    std::size_t operator()(T t) const
    {
        return static_cast<std::size_t>(t);
    }
};

struct AnchorInfo
{
    std::string anchorName;  // i.e 300.allowLAN
    std::string rootChain;   // i.e piavpn.FORWARD
    std::string anchorChain; // i.e piavpn.a.300.allowLAN
    std::string actualChain; // i.e piavpn.300.allowLAN
    std::string ruleChain;   // i.e piavpn.r.300.allowLAN
    std::string oldChain;    // i.e piavpn.o.300.allowLAN
    std::vector<std::string> rules;

    bool operator==(const AnchorInfo& rhs) const;
    bool operator!=(const AnchorInfo& rhs) const { return !(*this == rhs); }
};

bool AnchorInfo::operator==(const AnchorInfo& rhs) const
{
    // This simplified comparison is all we need
    // We're really only using this to check whether AnchorInfo
    // exists at all  - i.e comparing to AnchorNotFound (which is an empty instance)
    return anchorName == rhs.anchorName;
}

// Anchor maps are sorted lexically by anchor names in _descending_ order.  All
// of our anchors are prefixed with "###." to control their priority.
//
// Higher priority values take precedence, so we sort higher priorities first,
// as iptables stops on the first match.  (This is the opposite of BSD PF, which
// applies the _last_ match in the absence of 'quick'.)
using AnchorMap = std::map<std::string, AnchorInfo, std::greater<std::string>>;

namespace
{
    using IPVersion = IpTablesFirewall::IPVersion;

    const std::unordered_map<ChainEnum, std::string, EnumClassHash> kChainMap =
    {
        {ChainEnum::PREROUTING, "PREROUTING"},
        {ChainEnum::INPUT, "INPUT"},
        {ChainEnum::OUTPUT, "OUTPUT"},
        {ChainEnum::FORWARD, "FORWARD"},
        {ChainEnum::POSTROUTING, "POSTROUTING"},
    };


    const std::unordered_map<TableEnum, std::string, EnumClassHash> kTableMap =
    {
        {TableEnum::Filter, "filter"},
        {TableEnum::Nat, "nat"},
        {TableEnum::Mangle, "mangle"},
        {TableEnum::Raw, "raw"},
    };

    std::string getCommand(IPVersion ip)
    {
        return ip == IPVersion::IPv6 ? "ip6tables" : "iptables";
    }

    std::string enumToString(ChainEnum enumVal)
    {
        return kChainMap.at(enumVal);
    }

    std::string enumToString(TableEnum enumVal)
    {
        return kTableMap.at(enumVal);
    }

    // Used to indicate no anchor found
    const AnchorInfo AnchorNotFound{};
}

class IptInterface
{
public:
    IptInterface(const std::string &tableName, const std::string &anchorBase)
    : _tableName{tableName}
    , _anchorBase{anchorBase}
    {}

public:
    // Useful for logging
    std::string anchorNameWithIp(IPVersion ip, const std::string &anchorName)
    {
        return qs::format("%%", anchorName, (ip == IPVersion::IPv6 ? "(IPv6)" : "(IPv4)"));
    }

    void createChain(IPVersion ip, const std::string &chain)
    {
        if(ip == IPVersion::Both)
        {
            createChain(IPVersion::IPv4, chain);
            createChain(IPVersion::IPv6, chain);
            return;
        }

        const std::string cmd = getCommand(ip); // returns iptables or ip6tables
        kapps::core::Exec::bash(qs::format("% -w -N % -t % || % -F % -t %", cmd, chain, _tableName, cmd, chain, _tableName));
    }

    void deleteChain(IPVersion ip, const std::string &chain)
    {
        if (ip == IPVersion::Both)
        {
            deleteChain(IPVersion::IPv4, chain);
            deleteChain(IPVersion::IPv6, chain);
            return;
        }
        const std::string cmd = getCommand(ip);
        kapps::core::Exec::bash(qs::format("if % -w -L % -n -t % > /dev/null 2> /dev/null ; then % -w -F % -t % && % -X % -t %; fi",
            cmd, chain, _tableName, cmd, chain, _tableName, cmd, chain, _tableName));
    }

    void linkChain(IPVersion ip, const std::string &chain, const std::string& parentChain, bool mustBeFirst)
    {
        if(ip == IPVersion::Both)
        {
            linkChain(IPVersion::IPv4, chain, parentChain, mustBeFirst);
            linkChain(IPVersion::IPv6, chain, parentChain, mustBeFirst);
            return;
        }

        const std::string cmd = getCommand(ip);

        if(mustBeFirst)
        {
            // This monster shell script does the following:
            // 1. Check if a rule with the appropriate target exists at the top of the parent chain
            // 2. If not, insert a jump rule at the top of the parent chain
            // 3. Look for and delete a single rule with the designated target at an index > 1
            //    (we can't safely delete all rules at once since rule numbers change)
            // TODO: occasionally this script results in warnings in logs "Bad rule (does a matching rule exist in the chain?)" - this happens when
            // the e.g OUTPUT chain is empty but this script attempts to delete things from it anyway. It doesn't cause any problems, but we should still fix at some point..
            auto str = qs::format("if ! % -w -L % -n --line-numbers -t % 2> /dev/null | awk 'int($1) == 1 && $2 == \"%\" { found=1 } END { if(found==1) { exit 0 } else { exit 1 } }' ; then % -w -I % -j % -t % && % -L % -n --line-numbers -t % 2> /dev/null | awk 'int($1) > 1 && $2 == \"%\" { print $1; exit }' | xargs % -w -t % -D % ; fi", cmd, parentChain, _tableName, chain, cmd, parentChain, chain, _tableName, cmd, parentChain, _tableName, chain, cmd, _tableName, parentChain);

            KAPPS_CORE_INFO() << "Executing linkChain with mustbefirst " << str;

            kapps::core::Exec::bash(str);

        }
        else
            kapps::core::Exec::bash(qs::format("if ! % -w -C % -j % -t % 2> /dev/null ; then % -w -A % -j % -t %; fi", cmd, parentChain, chain, _tableName, cmd, parentChain, chain, _tableName));
    }

    void unlinkChain(IPVersion ip, const std::string &chain, const std::string &parent)
    {
        if(ip == IPVersion::Both)
        {
            unlinkChain(IPVersion::IPv4, chain, parent);
            unlinkChain(IPVersion::IPv6, chain, parent);
            return;
        }
        const std::string cmd = getCommand(ip);
        kapps::core::Exec::bash(qs::format("if % -w -C % -j % -t % 2> /dev/null ; then % -w -D % -j % -t %; fi",
            cmd, parent, chain, _tableName, cmd, parent, chain, _tableName));
    }

    void installAnchor(IPVersion ip, const AnchorInfo &anchorInfo)
    {
        if(ip == IPVersion::Both)
        {
            installAnchor(IPVersion::IPv4, anchorInfo);
            installAnchor(IPVersion::IPv6, anchorInfo);
            return;
        }

        // Define the anchor chain and link it into the parent chain
        createChain(ip, anchorInfo.anchorChain);
        linkChain(ip, anchorInfo.anchorChain, anchorInfo.rootChain, false);

        // Define the actual chain - don't link it to the anchor chain yet, that
        // happens if this anchor is enabled
        createChain(ip, anchorInfo.actualChain);

        // Define the rule chain, link it into the actual chain, and populate the
        // initial rules
        createChain(ip, anchorInfo.ruleChain);
        linkChain(ip, anchorInfo.ruleChain, anchorInfo.actualChain, false);

        const std::string cmd = getCommand(ip);
        for(const std::string& rule : anchorInfo.rules)
            kapps::core::Exec::bash(qs::format("% -w -A % % -t %", cmd, anchorInfo.ruleChain, rule, _tableName));
    }

    void uninstallAnchor(IPVersion ip, const AnchorInfo &anchorInfo)
    {
        if(ip == IPVersion::Both)
        {
            uninstallAnchor(IPVersion::IPv4, anchorInfo);
            uninstallAnchor(IPVersion::IPv6, anchorInfo);
            return;
        }

        unlinkChain(ip, anchorInfo.anchorChain, anchorInfo.rootChain);
        deleteChain(ip, anchorInfo.anchorChain);
        deleteChain(ip, anchorInfo.actualChain);
        deleteChain(ip, anchorInfo.ruleChain);
        // shouldn't need to delete this, but try anyway out of paranois
        deleteChain(ip, anchorInfo.oldChain);
    }

    void enableAnchor(IPVersion ip, const AnchorInfo &anchorInfo)
    {
        if(ip == IPVersion::Both)
        {
            enableAnchor(IPVersion::IPv4, anchorInfo);
            enableAnchor(IPVersion::IPv6, anchorInfo);
            return;
        }

        const std::string cmd = getCommand(ip);
        const std::string anchorIpStr = anchorNameWithIp(ip, anchorInfo.anchorName);

        kapps::core::Exec::bash(qs::format("if % -w -C % -j % -t % 2> /dev/null ; then echo '%: ON' ; else echo '%: OFF -> ON' ; % -w -A % -j % -t %; fi",
            cmd, anchorInfo.anchorChain, anchorInfo.actualChain, _tableName, anchorIpStr, anchorIpStr, cmd, anchorInfo.anchorChain, anchorInfo.actualChain, _tableName));
    }

    void disableAnchor(IPVersion ip, const AnchorInfo &anchorInfo)
    {
        if(ip == IPVersion::Both)
        {
            disableAnchor(IPVersion::IPv4, anchorInfo);
            disableAnchor(IPVersion::IPv6, anchorInfo);
            return;
        }

        const std::string cmd = getCommand(ip);
        const std::string anchorIpStr = anchorNameWithIp(ip, anchorInfo.anchorName);

        kapps::core::Exec::bash(qs::format("if ! % -w -C % -j % -t % 2> /dev/null ; then echo '%: OFF' ; else echo '%: ON -> OFF' ; % -w -F % -t %; fi",
            cmd, anchorInfo.anchorChain, anchorInfo.actualChain, _tableName, anchorIpStr, anchorIpStr, cmd, anchorInfo.anchorChain, _tableName));
    }

    void replaceAnchor(IPVersion ip, const AnchorInfo &anchorInfo, const std::vector<std::string> &newRules)
    {
        if(ip == IPVersion::Both)
        {
            replaceAnchor(IPVersion::IPv4, anchorInfo, newRules);
            replaceAnchor(IPVersion::IPv6, anchorInfo, newRules);
            return;
        }

        const std::string cmd = getCommand(ip);
        // To replace the anchor atomically:
        // 1. Rename the old "rule" chain (see model in installAnchor())
        // 2. Define a new "rule" chain and populate it
        // 3. Replace the anchor in the "actual" chain to point to the new rule chain
        //    ^ This is key, this atomically pivots from one rule set to the other.
        // 4. Flush and delete the old chain

        // Rename the old chain
        kapps::core::Exec::bash(qs::format("% -w -E % % -t %", cmd, anchorInfo.ruleChain, anchorInfo.oldChain, _tableName));
        // Create a new rule chain
        createChain(ip, anchorInfo.ruleChain);
        // Populate the new chain
        for(const auto &rule : newRules)
        {
            kapps::core::Exec::bash(qs::format("% -w -A % % -t %", cmd, anchorInfo.ruleChain, rule, _tableName));
        }
        // Pivot the actual chain to the new rule chain.  The actual chain should always have
        // exactly 1 rule (the anchor to the rule chain).
        kapps::core::Exec::bash(qs::format("% -w -R % 1 -j % -t %", cmd, anchorInfo.actualChain, anchorInfo.ruleChain, _tableName));

        // Clean up - flush and delete the old chain
        deleteChain(ip, anchorInfo.oldChain);
    }

private:
    std::string _tableName;
    std::string _anchorBase;
};

// Model of a table in iptables - allows creating prioritized anchors in the
// table's chains.
//
// Provides primitive operations that the rest of the firewall implementation is
// built with:
// - Define anchors
// - Install all anchors
// - Uninstall all anchors
// - Enable/disable anchors (without affecting anchor priority)
// - Replace content of anchor atomically
//
// For each table, Linux only uses a subset of all available chains.  The table
// and chain combinations are validated statically (which is why the table and
// chain are template parameters).
//
// To implement the prioritized anchors and atomic replacement, up to four
// iptables chains are used to implement each logical anchor.  Two "links" are
// needed that we can create, delete or replace:
// 1. the link between the anchor's "prioritized position" (position in the
//    top-level PIA chain) and the anchor content - used to enable/disable the
//    anchor without affecting priority
// 2. the link between the anchor content and its actual rules - used to
//    atomically pivot from one set of rules to another
//
// The four chains used are:
// 1. The "anchor" chain (.a.), which stays in the proper place in the root chain
// 2. The "content" chain (no infix), which always exists, and is either linked
//    or not linked from the "anchor" chain to enable/disable the anchor
// 3. The "rule" chain (.r.), which holds the actual rules.  This is always
//    linked from the content chain, except when the contents are replaced.
// 4. The "old rule" chain (.o.), which exists temporarily during a replacement.
//    The rule chain is renamed to the old-rule chain, then a new rule chain is
//    constructed.  The replacement occurs by atomically replacing the
//    content->rule jump to the new chain, then the old-rule chain is cleaned
//    up.
template <TableEnum tableType>
class Table
{
public:
    Table(const std::string &anchorBase)
    : _anchorBase{anchorBase}
    , _tableName{enumToString(tableType)}
    , _iptInterface{_tableName, _anchorBase}
    {}

public:
    template <ChainEnum chainType>
    Table& anchor(IPVersion ipVersion, const std::string &anchorName, std::vector<std::string> rules)
    {
        checkTableAndChains<chainType>();

        AnchorInfo info{};
        info.rootChain = rootChainNameFor(chainType); // i.e piavpn.FORWARD

        // These are the four chains we use to implement the logical anchor, as
        // discussed above
        info.anchorChain = qs::format("%.a.%", _anchorBase, anchorName);
        info.actualChain = qs::format("%.%", _anchorBase, anchorName);
        info.ruleChain = qs::format("%.r.%", _anchorBase, anchorName);
        info.oldChain = qs::format("%.o.%", _anchorBase, anchorName);
        info.anchorName = anchorName; // i.e "000.allowLoopback"

        info.rules = rules;

        _rootChains.insert(chainType);

        if(ipVersion == IPVersion::Both)
        {
            _anchorMap4.insert({anchorName, info});
            _anchorMap6.insert({anchorName, info});
        }
        else if(ipVersion == IPVersion::IPv4)
            _anchorMap4.insert({anchorName, info});
        else if(ipVersion == IPVersion::IPv6)
            _anchorMap6.insert({anchorName, info});

        return *this;
    }

private:
    template <ChainEnum chainType>
    void checkTableAndChains()
    {
        // Filter table
        if constexpr(tableType == TableEnum::Filter)
        {
            static_assert((chainType == ChainEnum::INPUT || chainType == ChainEnum::OUTPUT || chainType == ChainEnum::FORWARD),
                "Invalid chain type for filter table, must be: INPUT, OUTPUT or FORWARD");
        }
        // Nat table
        else if constexpr(tableType == TableEnum::Nat)
        {
            static_assert((chainType == ChainEnum::PREROUTING || chainType == ChainEnum::OUTPUT || chainType == ChainEnum::POSTROUTING),
                "Invalid chain type for nat table, must be: PREROUTING, OUTPUT or POSTROUTING");
        }
        // Mangle table
        else if constexpr(tableType == TableEnum::Mangle)
        {
            static_assert((chainType == ChainEnum::PREROUTING || chainType == ChainEnum::INPUT || chainType == ChainEnum::FORWARD ||
                chainType == ChainEnum::OUTPUT || chainType == ChainEnum::POSTROUTING),
                "Invalid chain type for mangle table, must be: PREROUTING, INPUT, FORWARD, OUTPUT or POSTROUTING");
        }
        // Raw table
        else if constexpr(tableType == TableEnum::Raw)
        {
            static_assert((chainType == ChainEnum::PREROUTING || chainType == ChainEnum::OUTPUT),
                "Invalid chain type for raw table, must be: PREROUTING or OUTPUT");
        }
    }

    std::string rootChainNameFor(ChainEnum chain) const
    {
        const std::string &chainName{kChainMap.at(chain)};
        return qs::format("%.%", _anchorBase, chainName);
    }

    const AnchorInfo &getAnchorInfo(IPVersion ip, const std::string &anchorName) const
    {
        const auto &anchorMap = (ip == IPVersion::IPv4 ? _anchorMap4 : _anchorMap6);

        if(anchorMap.find(anchorName) != anchorMap.end())
            return anchorMap.at(anchorName);
        else
            return AnchorNotFound;
    }

private:
    void installRootChains()
    {
        for(auto rootChain : _rootChains)
        {
            _iptInterface.createChain(IPVersion::Both, rootChainNameFor(rootChain));
            _iptInterface.linkChain(IPVersion::Both, rootChainNameFor(rootChain), kChainMap.at(rootChain), true);
        }
    }

    void uninstallRootChains()
    {
        for(auto rootChain : _rootChains)
        {
            _iptInterface.unlinkChain(IPVersion::Both, rootChainNameFor(rootChain), kChainMap.at(rootChain));
            _iptInterface.deleteChain(IPVersion::Both, rootChainNameFor(rootChain));
        }
    }

    void installAnchors()
    {
        for(const auto &pair : _anchorMap4)
        {
            // Poor man's structured bindings
            const auto &anchorInfo{pair.second};
            _iptInterface.installAnchor(IPVersion::IPv4, anchorInfo);
        }

        for(const auto &pair : _anchorMap6)
        {
            const auto &anchorInfo{pair.second};
            _iptInterface.installAnchor(IPVersion::IPv6, anchorInfo);
        }
    }

    void uninstallAnchors()
    {
        for(const auto &pair : _anchorMap4)
        {
            // Poor man's structured bindings
            const auto &anchorInfo{pair.second};
            _iptInterface.uninstallAnchor(IPVersion::IPv4, anchorInfo);
        }

        for(const auto &pair : _anchorMap6)
        {
            const auto &anchorInfo{pair.second};
            _iptInterface.uninstallAnchor(IPVersion::IPv6, anchorInfo);
        }
    }

public:
    bool isInstalled() const
    {
        // Could be any chain - we're just checking whether our FW is installed
        auto outputChain = enumToString(ChainEnum::OUTPUT);
        auto outputRootChain = rootChainNameFor(ChainEnum::OUTPUT);
        auto str = qs::format("iptables -w -C % -j % 2> /dev/null", outputChain, outputRootChain);
        auto result = kapps::core::Exec::bash(str);

        return result == 0;
    }

    void install()
    {
        KAPPS_CORE_INFO() << "Installing root chains for" << _tableName;
        installRootChains();
        KAPPS_CORE_INFO() << "Installing anchors for" << _tableName;
        installAnchors();
    }

    void uninstall()
    {
        KAPPS_CORE_INFO() << "Uninstalling root chains for" << _tableName;
        uninstallRootChains();
        KAPPS_CORE_INFO() << "Uninstalling anchors for" << _tableName;
        uninstallAnchors();
    }

    void showAllAnchors(IPVersion ip)
    {
        const auto &anchorMap = (ip == IPVersion::IPv4 ? _anchorMap4 : _anchorMap6);
        for(const auto &pair : anchorMap)
        {
            KAPPS_CORE_INFO() << "found installed anchor " << _iptInterface.anchorNameWithIp(ip, pair.first);
        }
    }

    void setAnchorEnabled(IPVersion ip, const std::string &anchorName, bool enabled)
    {
        const auto &anchorInfo{getAnchorInfo(ip, anchorName)};
        if(anchorInfo == AnchorNotFound)
        {
            KAPPS_CORE_WARNING() << "Could not find anchor: " << _iptInterface.anchorNameWithIp(ip, anchorName);
            // This is a bug, this should never happen.
            assert(false);
            return;
        }

        if(enabled)
            _iptInterface.enableAnchor(ip, anchorInfo);
        else
            _iptInterface.disableAnchor(ip, anchorInfo);
    }

    void replaceAnchor(IPVersion ip, const std::string &anchorName, const std::vector<std::string> &newRules)
    {
        const auto& anchorInfo{getAnchorInfo(ip, anchorName)};
        if(anchorInfo == AnchorNotFound)
        {
            KAPPS_CORE_WARNING() << "Could not find anchor: " << _iptInterface.anchorNameWithIp(ip, anchorName);
            // This is a bug, this should never happen.
            //assert(false);
            return;
        }

        _iptInterface.replaceAnchor(ip, anchorInfo, newRules);
    }


    void ensureRootAnchorPriority()
    {
        for(auto rootChain : _rootChains)
        {
            _iptInterface.linkChain(IPVersion::Both, rootChainNameFor(rootChain), kChainMap.at(rootChain), true);
        }
    }

private:
    std::string _anchorBase; // e.g piavpn
    std::string _tableName;
    AnchorMap _anchorMap4;
    AnchorMap _anchorMap6;
    std::set<ChainEnum> _rootChains;
    IptInterface _iptInterface;
};

// Firewall implementation on Linux using iptables.  Note that this also handles
// some aspects of routing that are closely related to firewall rules.
class IpTablesFirewall::Impl
{
public:
    Impl(const kapps::net::FirewallConfig &config);

private:
    // Generate iptables rules to permit DNS to the specified servers.
    // vpnAdapterName is used for non-local DNS; traffic is only permitted
    // through the tunnel.  For local DNS, we permit it on any adapter.
    // No rules are created if the VPN adapter name is not known yet.
    std::vector<std::string> getDNSRules(const std::string &vpnAdapterName, const std::vector<std::string>& servers);
    int execute(const std::string& command, bool ignoreErrors = false);

public:
    Table<TableEnum::Filter> &filterTable() {return _filterTable;}
    Table<TableEnum::Nat> &natTable() {return _natTable;}
    Table<TableEnum::Mangle> &mangleTable() {return _mangleTable;}
    Table<TableEnum::Raw> &rawTable() {return _rawTable;}

public:
    // Install/uninstall the firewall anchors
    void install();
    void uninstall();
    bool isInstalled() const;
    void ensureRootAnchorPriority(IPVersion ip = IPVersion::Both);
    void updateRules(const kapps::net::FirewallParams &params);
    void updateBypassSubnets(IPVersion ipVersion, const std::unordered_set<std::string> &bypassSubnets, std::unordered_set<std::string> &oldBypassSubnets);

    std::string existingDNS();

public:
    const kapps::net::Fwmark &fwmark() const {return _cgroup.fwmark();}
    const kapps::net::Routing &routing() const {return _cgroup.routing();}
    const std::string &hnsdGroupName() const { return _hnsdGroupName; }

private:
    // Last state used by updateRules(); allows us to detect when the rules must
    // be updated
    std::string _adapterName;
    std::vector<std::string> _dnsServers;
    std::string _ipAddress6;
    std::unordered_set<std::string> _bypassIpv4Subnets;
    std::unordered_set<std::string> _bypassIpv6Subnets;

    std::string _anchorBase;
    std::string _hnsdGroupName;
    kapps::net::CGroupIds _cgroup;

    Table<TableEnum::Filter> _filterTable;
    Table<TableEnum::Nat> _natTable;
    Table<TableEnum::Mangle> _mangleTable;
    Table<TableEnum::Raw> _rawTable;
};

IpTablesFirewall::IpTablesFirewall(const kapps::net::FirewallConfig &config)
: _pImpl{std::make_unique<Impl>(config)}
{
}

IpTablesFirewall::~IpTablesFirewall() = default;

const kapps::net::Fwmark &IpTablesFirewall::fwmark() const
{
    return _pImpl->fwmark();
}

const kapps::net::Routing &IpTablesFirewall::routing() const
{
    return _pImpl->routing();
}

void IpTablesFirewall::install()
{
    _pImpl->install();
}

void IpTablesFirewall::uninstall()
{
    _pImpl->uninstall();
}

bool IpTablesFirewall::isInstalled()
{
    return _pImpl->isInstalled();
}

void IpTablesFirewall::updateRules(const kapps::net::FirewallParams &params)
{
    _pImpl->updateRules(params);
}

const std::string& IpTablesFirewall::hnsdGroupName() const
{
    return _pImpl->hnsdGroupName();
}

void IpTablesFirewall::ensureRootAnchorPriority(IPVersion ip)
{
    _pImpl->ensureRootAnchorPriority(ip);
}

void IpTablesFirewall::setAnchorEnabled(TableEnum tableType, IPVersion ip, const std::string &anchorName, bool enabled) const
{
    switch(tableType)
    {
        case TableEnum::Filter:
            _pImpl->filterTable().setAnchorEnabled(ip, anchorName, enabled);
            break;
        case TableEnum::Nat:
            _pImpl->natTable().setAnchorEnabled(ip, anchorName, enabled);
            break;
        case TableEnum::Mangle:
            _pImpl->mangleTable().setAnchorEnabled(ip, anchorName, enabled);
            break;
        case TableEnum::Raw:
            _pImpl->rawTable().setAnchorEnabled(ip, anchorName, enabled);
            break;
    }
}

void IpTablesFirewall::replaceAnchor(TableEnum tableType, IPVersion ip, const std::string &anchorName, const std::vector<std::string> &newRules) const
{
    switch(tableType)
    {
        case TableEnum::Filter:
            _pImpl->filterTable().replaceAnchor(ip, anchorName, newRules);
            break;
        case TableEnum::Nat:
            _pImpl->natTable().replaceAnchor(ip, anchorName, newRules);
            break;
        case TableEnum::Mangle:
            _pImpl->mangleTable().replaceAnchor(ip, anchorName, newRules);
            break;
        case TableEnum::Raw:
            _pImpl->rawTable().replaceAnchor(ip, anchorName, newRules);
            break;
    }
}

IpTablesFirewall::Impl::Impl(const kapps::net::FirewallConfig &config)
: _anchorBase{config.brandInfo.code + "vpn"}
, _hnsdGroupName{config.brandInfo.code + "hnsd"}
, _cgroup{config}
, _filterTable{_anchorBase}
, _natTable{_anchorBase}
, _mangleTable{_anchorBase}
, _rawTable{_anchorBase}
{
    assert(!config.brandInfo.code.empty());

    // filter table + OUTPUT chain
    _filterTable
        .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "999.allowLoopback", {
            "-o lo+ -p udp -m udp --dport 53 -j RETURN",
            "-o lo+ -p tcp -m tcp --dport 53 -j RETURN",
            "-o lo+ -j ACCEPT"
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "400.allowPIA", {
            qs::format("-m owner --gid-owner % -j ACCEPT", _anchorBase)
        })
        // Allow all packets with the wireguard mark
        // Though another process could also mark packets with this fwmark to permit
        // them, it would have to have root privileges to do so, which means it
        // could install its own firewall rules anyway.
        .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "390.allowWg", {
            qs::format("-m mark --mark % -j ACCEPT", fwmark().wireguardFwmark())
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "350.allowHnsd", {
            // Updated at run-time in updateRules()
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "350.cgAllowHnsd", {
            // Port 13038 is the handshake control port
            qs::format("-m owner --gid-owner % -m cgroup --cgroup % -p tcp --match multiport --dports 53,13038 -j ACCEPT", _hnsdGroupName, _cgroup.vpnOnlyId()),
            qs::format("-m owner --gid-owner % -m cgroup --cgroup % -p udp --match multiport --dports 53,13038 -j ACCEPT", _hnsdGroupName, _cgroup.vpnOnlyId()),
            qs::format("-m owner --gid-owner % -j REJECT", _hnsdGroupName),
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "340.blockVpnOnly", {
            qs::format("-m cgroup --cgroup % -j REJECT", _cgroup.vpnOnlyId()),
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::IPv4, "320.allowDNS", {
            // Updated at run-time
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "310.blockDNS", {
            "-p udp --dport 53 -j REJECT",
            "-p tcp --dport 53 -j REJECT"
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "305.allowSubnets", {
            // Updated at run-time
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::IPv4, "300.allowLAN", {
            "-d 10.0.0.0/8 -j ACCEPT",
            "-d 169.254.0.0/16 -j ACCEPT",
            "-d 172.16.0.0/12 -j ACCEPT",
            "-d 192.168.0.0/16 -j ACCEPT",
            "-d 224.0.0.0/4 -j ACCEPT",
            "-d 255.255.255.255/32 -j ACCEPT",
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::IPv6, "300.allowLAN", {
            "-d fc00::/7 -j ACCEPT",
            "-d fe80::/10 -j ACCEPT",
            "-d ff00::/8 -j ACCEPT",
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::IPv6, "299.allowIPv6Prefix", {
            // Updated at run-time
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::IPv4, "290.allowDHCP", {
            "-p udp -d 255.255.255.255 --sport 68 --dport 67 -j ACCEPT",
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::IPv6, "290.allowDHCP", {
            "-p udp -d ff00::/8 --sport 546 --dport 547 -j ACCEPT",
        })
        // This rule exists as the 100.blockAll rule can be toggled off if killswitch=off.
        // However we *always* want to block IPVersion::IPv6 traffic in any situation (until we properly support IPVersion::IPv6)
        .anchor<ChainEnum::OUTPUT>(IPVersion::IPv6, "250.blockIPv6", {
            "! -o lo+ -j REJECT",
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::IPv4, "230.allowBypassApps", {
            qs::format("-m cgroup --cgroup % -j ACCEPT", _cgroup.bypassId())
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "200.allowVPN", {
            // To be added at runtime, dependent upon vpn method (i.e openvpn or wireguard)
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "100.blockAll", {
            "-j REJECT",
        })

        // filter table + INPUT chain
        .anchor<ChainEnum::INPUT>(IPVersion::IPv4, "100.protectLoopback", {
            qs::format("! -i lo -o lo -j REJECT")
        });

     _natTable
         .anchor<ChainEnum::PREROUTING>(IPVersion::Both, "80.fwdSplitDNS", {
             // Updated dynamically (see updateRules)
         })
         .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "80.splitDNS", {
             // Updated dynamically (see updateRules))
         })
         .anchor<ChainEnum::POSTROUTING>(IPVersion::Both, "90.snatDNS", {
             // Updated dynamically (see updateRules)
         })
         .anchor<ChainEnum::POSTROUTING>(IPVersion::Both, "90.fwdSnatDNS", {
             // Updated dynamically (see updateRules)
         })
         .anchor<ChainEnum::POSTROUTING>(IPVersion::Both, "100.transIp", {
             // This anchor is set at run-time by split-tunnel ProcTracker class
         });

    // Mangle rules
    // This rule is for "bypass subnets". The approach we use for
    // allowing subnets to bypass the VPN is to tag packets heading towards those subnets
    // with the "excludePacketTag" (same approach we use for bypass apps).
    // Interestingly, in order to allow correct interaction between bypass subnets and vpnOnly apps
    // ("vpnOnly apps always wins") we need to tag the subnets BEFORE we subsequently tag vpnOnly apps,
    // hence tagPkts appearing after tagSubnets. If we were to apply the tags the other way round, then the
    // "last tag wins", so the vpnOnly packet would have its tag replaced with excludePacketTag.
    // Tagging bypass subnets (with excludePacketTag) first means that vpnOnly tags get priority.
    _mangleTable
        // Marks all forwarded packets
        .anchor<ChainEnum::PREROUTING>(IPVersion::Both, "100.tagFwd", {
            qs::format("-j MARK --set-mark %", fwmark().forwardedPacketTag())
        })
        // Mark forwarded packets to bypass IPs as "bypass" packets instead
        .anchor<ChainEnum::PREROUTING>(IPVersion::Both, "200.tagFwdSubnets", {
            // Updated at runtime
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "90.tagSubnets", {
            // Updated at runtime
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "100.tagBypass", {
            // Split tunnel
            qs::format("-m cgroup --cgroup % -j MARK --set-mark %", _cgroup.bypassId(),
                fwmark().excludePacketTag()),
        })
        .anchor<ChainEnum::OUTPUT>(IPVersion::Both, "100.tagVpnOnly", {
            // Inverse split tunnel
            qs::format("-m cgroup --cgroup % -j MARK --set-mark %", _cgroup.vpnOnlyId(),
                fwmark().vpnOnlyPacketTag())
        });

    // A rule to mitigate CVE-2019-14899 - drop packets addressed to the local
    // VPN IP but that are not actually received on the VPN interface.
    // See here: https://seclists.org/oss-sec/2019/q4/122
    _rawTable
        .anchor<ChainEnum::PREROUTING>(IPVersion::Both, "100.vpnTunOnly", {
            // To be replaced at runtime
            "-j ACCEPT"
        });
}

void IpTablesFirewall::Impl::install()
{
    // Clean up any existing rules if they exist.
    uninstall();

    _filterTable.install();
    _natTable.install();
    _mangleTable.install();
    _rawTable.install();

    // Ensure LAN traffic is always managed by the 'main' table.  This is needed
    // to ensure LAN routing for:
    // - Split tunnel.  Otherwise, split tunnel rules would send LAN traffic via
    //   the default gateway.
    // - Wireguard.  Otherwise, LAN traffic would be sent via the Wireguard
    //   interface.
    //
    // This has no effect for OpenVPN without split tunnel, or when disconnected
    // without split tunnel.  We may need this even if the daemon is not active,
    // because some split tunnel rules are still applied even when inactive
    // ("only VPN" rules).
    //
    // Note that we use "suppress_prefixlength 1", not 0 as is typical, because
    // we also suppress the /1 gateway override routes applied by OpenVPN.
    kapps::core::Exec::bash(qs::format("ip rule add lookup main suppress_prefixlength 1 prio %", kapps::net::Routing::Priorities::suppressedMain));
    kapps::core::Exec::bash(qs::format("ip -6 rule add lookup main suppress_prefixlength 1 prio %", kapps::net::Routing::Priorities::suppressedMain));

    // Route forwarded packets
    kapps::core::Exec::bash(qs::format("ip rule add from all fwmark % lookup % prio %", 
        fwmark().forwardedPacketTag(), routing().forwardedTable(), kapps::net::Routing::Priorities::forwarded));
    kapps::core::Exec::bash(qs::format("ip -6 rule add from all fwmark % lookup % prio %", 
        fwmark().forwardedPacketTag(), routing().forwardedTable(), kapps::net::Routing::Priorities::forwarded));
}

void IpTablesFirewall::Impl::uninstall()
{
    namespace Exec = kapps::core::Exec;

    Exec::bash(qs::format("ip rule del lookup main suppress_prefixlength 1 prio %", kapps::net::Routing::Priorities::suppressedMain));
    Exec::bash(qs::format("ip -6 rule del lookup main suppress_prefixlength 1 prio %", kapps::net::Routing::Priorities::suppressedMain));

    // Remove forwarded packets policy
    Exec::bash(qs::format("ip rule del from all fwmark % lookup % prio %",
        fwmark().forwardedPacketTag(), routing().forwardedTable(), kapps::net::Routing::Priorities::forwarded));
    Exec::bash(qs::format("ip -6 rule del from all fwmark % lookup % prio %",
        fwmark().forwardedPacketTag(), routing().forwardedTable(), kapps::net::Routing::Priorities::forwarded));

    _filterTable.uninstall();
    _natTable.uninstall();
    _mangleTable.uninstall();
    _rawTable.uninstall();
}

void IpTablesFirewall::Impl::updateRules(const kapps::net::FirewallParams &params)
{
    //
}

bool IpTablesFirewall::Impl::isInstalled() const
{
    return _filterTable.isInstalled();
}

void IpTablesFirewall::Impl::ensureRootAnchorPriority(IPVersion ip)
{
    _filterTable.ensureRootAnchorPriority();
    _natTable.ensureRootAnchorPriority();
    _mangleTable.ensureRootAnchorPriority();
    _rawTable.ensureRootAnchorPriority();
}

int IpTablesFirewall::Impl::execute(const std::string &command, bool ignoreErrors)
{
    return kapps::core::Exec::bash(command, ignoreErrors);
}
