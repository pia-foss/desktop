// Copyright (c) 2021 Private Internet Access, Inc.
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

#ifndef RULE_UPDATER_H
#define RULE_UPDATER_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <QSocketNotifier>
#include <QPointer>
#include <QPair>
#include <QTimer>
#include "daemon.h"
#include "posix/posix_firewall_pf.h"
#include "mac/mac_splittunnel_types.h"
#include "mac/utun.h"
#include "processrunner.h"
#include "settings.h"
#include "exec.h"
#include "vpn.h"

class MacSplitTunnel;
class UpdateStrategy;

class RuleUpdater
{
private:
    std::array<PortSet, 2> _ports;
    OriginalNetworkScan _netScan;
    QSet<QString> _bypassIpv4Subnets;
    QSet<QString> _bypassIpv6Subnets;
    std::unique_ptr<UpdateStrategy> _strategy;
    MacSplitTunnel *_pMacSplitTunnel;
public:
    RuleUpdater(std::unique_ptr<UpdateStrategy> strategy, MacSplitTunnel *pMacSplitTunnel)
        : _strategy{std::move(strategy)}
        , _pMacSplitTunnel{pMacSplitTunnel}
    {}

    void update(IPVersion ipVersion, const PortSet &ports);
    void forceUpdate(IPVersion ipVersion, const PortSet &ports) const;
    void clearRules(IPVersion ipVersion);
    void clearAllRules();
};

class UpdateStrategy
{
    MacSplitTunnel *_pMacSplitTunnel;
public:
    UpdateStrategy(MacSplitTunnel *pMacSplitTunnel)
    : _pMacSplitTunnel{pMacSplitTunnel}
    {}
    virtual ~UpdateStrategy() = default;
    virtual QStringList rules(IPVersion ipVersion, const PortSet &ports) const;
    virtual QStringList routingRule(IPVersion ipVersion) const = 0;
    virtual QString anchorNameFor(IPVersion ipVersion) const = 0;
    virtual QString tagNameFor(IPVersion ipVersion) const = 0;
protected:
    const FirewallParams& params() const;
    const QString& tunnelDeviceName() const;
    QString protocolFor(IPVersion ipVersion) const { return ipVersion == IPv4 ? QStringLiteral("inet") : QStringLiteral("inet6"); }
    QString icmpVersion(IPVersion ipVersion) const { return (ipVersion == IPv4 ? "icmp" : "icmp6"); }
    QString gatewayIp(IPVersion ipVersion) const { return (ipVersion == IPv4 ? params().netScan.gatewayIp() : params().netScan.gatewayIp6()); }
};

class BypassStrategy : public UpdateStrategy
{
public:
    using UpdateStrategy::UpdateStrategy;
protected:
    virtual QStringList rules(IPVersion ipVersion, const PortSet &ports) const override;
    virtual QString tagNameFor(IPVersion ipVersion) const override;
    virtual QString anchorNameFor(IPVersion ipVersion) const override;
    virtual QStringList routingRule(IPVersion ipVersion) const override;
};

class VpnOnlyStrategy : public UpdateStrategy
{
public:
    using UpdateStrategy::UpdateStrategy;
protected:
    virtual QString tagNameFor(IPVersion ipVersion) const override;
    virtual QString anchorNameFor(IPVersion ipVersion) const override;
    virtual QStringList routingRule(IPVersion ipVersion) const override;
};

class DefaultStrategy : public UpdateStrategy
{
public:
    using UpdateStrategy::UpdateStrategy;
protected:
    virtual QStringList rules(IPVersion ipVersion, const PortSet &ports) const override;
    virtual QString tagNameFor(IPVersion ipVersion) const override;
    virtual QString anchorNameFor(IPVersion ipVersion) const override;
    virtual QStringList routingRule(IPVersion ipVersion) const override;
protected:
    QStringList bypassSubnetsFor(IPVersion ipVersion) const;
    QStringList lanSubnetsFor(IPVersion ipVersion) const;
};

#endif
