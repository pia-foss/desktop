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
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <kapps_net/net.h>
#include <kapps_core/core.h>
#include <kapps_core/src/posix/posix_objects.h>
#include "pf_firewall.h"
#include "utun.h"
#include "port_finder.h"
#include "rule_updater.h"
#include "mac_splittunnel_types.h"
#include "packet.h"
#include "../originalnetworkscan.h"
#include "../firewallparams.h"
#include <kapps_core/src/newexec.h>
#include <kapps_core/src/posix/posixfdnotifier.h>
#include "flow_tracker.h"

namespace kapps { namespace net {
struct KAPPS_NET_EXPORT AboutToConnect
{
};

struct KAPPS_NET_EXPORT UpdateSplitTunnel
{
    FirewallParams params;
};

class KAPPS_NET_EXPORT AppCache
{
public:
    void addEntry(IPVersion ipVersion, pid_t newPid, std::uint16_t srcPort);
    PortSet ports(IPVersion ipVersion) const;
    void refresh(IPVersion ipVersion, const OriginalNetworkScan &netScan);
    void clear(IPVersion ipVersion) { _cache[ipVersion].clear(); }
    void clearAll() { _cache[IPv4].clear(); _cache[IPv6].clear();}

private:
     using PortLookupTable = std::unordered_map<pid_t, PortSet>;
     std::array<PortLookupTable, 2> _cache;
};

class KAPPS_NET_EXPORT SplitTunnelIp
{
public:
    SplitTunnelIp(const std::string &ipv4AddressStr,
                  const std::string &ipv6AddressStr)
    {
        _ipAddress[IPv4] = ipv4AddressStr;
        _ipAddress[IPv6] = ipv6AddressStr;
    }

    void refresh()
    {
        _ipAddress[IPv4] = nextAddress(_ipAddress[IPv4], IPv4);
        _ipAddress[IPv6] = nextAddress(_ipAddress[IPv6], IPv6);
    }

    const std::string& ip4() const {return _ipAddress[IPv4];}
    const std::string& ip6() const {return _ipAddress[IPv6];}
private:
    std::string nextAddress(const std::string &addressStr, IPVersion ipVersion) const;
private:
    std::array<std::string, 2> _ipAddress;
};

class KAPPS_NET_EXPORT MacSplitTunnel
{
public:
    MacSplitTunnel() = delete;
    MacSplitTunnel(const FirewallParams& params, const std::string &executableDir,
                   PFFirewall &filter);

    ~MacSplitTunnel()
    {
        shutdownConnection();
        KAPPS_CORE_INFO() << "Finished destroying MacSplitTunnel";
    }

private:
    // Cycle the split tunnel ip and interface - this breaks existing connections
    bool cycleSplitTunnelDevice();

public:
    void aboutToConnectToVpn();
    void initiateConnection(const FirewallParams &params);
    void shutdownConnection();
    void updateSplitTunnel(const FirewallParams &params);

private:
    // Read input available from the utun device and process it.
    void readFromSocket();
    void updateApps(std::vector<std::string> excludedApps, std::vector<std::string> vpnOnlyApps);
    std::string sysctl(const std::string &setting, const std::string &value);
    void setupIpForwarding(const std::string &setting, const std::string &value, std::string &storedValue);
    void teardownIpForwarding(const std::string &setting, std::string &storedValue);
    void updateNetwork(const FirewallParams &params);
    bool isSplitPort(std::uint16_t port,
                     const PortSet &bypassPorts,
                     const PortSet &vpnOnlyPorts);
    void handleIp6(std::vector<unsigned char> buffer);
    void handleIp4(std::vector<unsigned char> buffer);
    void handleTunnelPacket(std::vector<unsigned char> buffer);

private:
    enum class State
    {
        Active,
        Inactive
    };

private:
    PFFirewall &_filter;
    kapps::core::nullable_t<kapps::core::PosixFd> _rawFd4;
    kapps::core::nullable_t<UTun> _pUtun;
    kapps::core::PosixFdNotifier _utunNotifier;
    State _state{State::Inactive};
    std::vector<std::string> _excludedApps;
    std::vector<std::string> _vpnOnlyApps;
    AppCache _defaultAppsCache;
    RuleUpdater _bypassRuleUpdater;
    RuleUpdater _vpnOnlyRuleUpdater;
    RuleUpdater _defaultRuleUpdater;
    SplitTunnelIp _splitTunnelIp;
    FirewallParams _params;

    std::string _ipForwarding4;
    std::string _ipForwarding6;
    bool _routesUp{false};

    FlowTracker _flowTracker;
    std::string _executableDir;
};

}}
