// Copyright (c) 2020 Private Internet Access, Inc.
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

#include "common.h"
#line HEADER_FILE("mac/mac_splittunnel.h")

#ifndef MACSPLITTUNNEL_H
#define MACSPLITTUNNEL_H

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
#include "mac/utun.h"
#include "processrunner.h"
#include "settings.h"
#include "exec.h"
#include "vpn.h"
#include "pid_finder.h"
#include "packet.h"

namespace
{
    enum IPVersion { IPv4, IPv6 };

    inline QString ipToString(IPVersion ipVersion) { return (ipVersion == IPv4 ? "IPv4" : "IPv6"); }
}

using PortSet = QSet<quint16>;

class AppCache
{
public:
    void addEntry(IPVersion ipVerrsion, pid_t newPid, quint16 srcPort);
    PortSet ports(IPVersion ipVersion) const;
    void refresh(IPVersion ipVersion);
    void clear(IPVersion ipVersion) { _cache[ipVersion].clear(); }
    void clearAll() { _cache[IPv4].clear(); _cache[IPv6].clear();}
private:
private:
     using PortLookupTable = QHash<pid_t, PortSet>;
     std::array<PortLookupTable, 2> _cache;
};

class SplitTunnelIp
{
public:
    SplitTunnelIp(const QString &ipv4AddressStr,
                  const QString &ipv6AddressStr)
    {
        _ipAddress[IPv4] = ipv4AddressStr;
        _ipAddress[IPv6] = ipv6AddressStr;
    }

    void refresh()
    {
        _ipAddress[IPv4] = nextAddress(_ipAddress[IPv4]);
        _ipAddress[IPv6] = nextAddress(_ipAddress[IPv6]);
    }

    const QString& ip4() const {return _ipAddress[IPv4];}
    const QString& ip6() const {return _ipAddress[IPv6];}
private:
    QString nextAddress(const QString &addressStr) const;
private:
    std::array<QString, 2> _ipAddress;
};

class MacSplitTunnel;

class RuleUpdater
{
public:
    RuleUpdater(MacSplitTunnel *pMacSplitTunnel)
        : _pMacSplitTunnel{pMacSplitTunnel}
    {}

    void update(IPVersion ipVersion, const PortSet &ports);
    void forceUpdate(IPVersion ipVersion, const PortSet &ports) const;
    void clearRules(IPVersion ipVersion);
    void clearAllRules();

protected:
    virtual QString anchorNameFor(IPVersion ipVersion) const = 0;
    virtual QString tagNameFor(IPVersion ipVersion) const = 0;
    virtual QStringList routingRule(IPVersion ipVersion) const = 0;
    virtual QString protocolFor(IPVersion ipVersion) const {return ipVersion == IPv4 ? QStringLiteral("inet") : QStringLiteral("inet6");}
    virtual QStringList rules(IPVersion ipVersion, const PortSet &ports) const;
    const FirewallParams &params() const;
    const QString& tunnelDeviceName() const;
private:
    std::array<PortSet, 2> _ports;
    OriginalNetworkScan _netScan;
    QSet<QString> _bypassIpv4Subnets;
    QSet<QString> _bypassIpv6Subnets;
    MacSplitTunnel *_pMacSplitTunnel;
};

class BypassRuleUpdater : public RuleUpdater
{
public:
    using RuleUpdater::RuleUpdater;
protected:
    virtual QStringList rules(IPVersion ipVersion, const PortSet &ports) const override;
    virtual QString tagNameFor(IPVersion ipVersion) const override;
    virtual QString anchorNameFor(IPVersion ipVersion) const override;
    virtual QStringList routingRule(IPVersion ipVersion) const override;
};
class VpnOnlyRuleUpdater : public RuleUpdater
{
public:
    using RuleUpdater::RuleUpdater;
protected:
    virtual QString tagNameFor(IPVersion ipVersion) const override;
    virtual QString anchorNameFor(IPVersion ipVersion) const override;
    virtual QStringList routingRule(IPVersion ipVersion) const override;
};

class DefaultRuleUpdater : public RuleUpdater
{
public:
    using RuleUpdater::RuleUpdater;
protected:
    virtual QString tagNameFor(IPVersion ipVersion) const override;
    virtual QString anchorNameFor(IPVersion ipVersion) const override;
    virtual QStringList routingRule(IPVersion ipVersion) const override;
};

class MacSplitTunnel : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("MacSplitTunnel")

public:
    static Executor _executor;
    enum RuleType
    {
        BypassVPN,
        VPNOnly
    };
    Q_ENUM(RuleType)

public:
    MacSplitTunnel(QObject *pParent);

    ~MacSplitTunnel()
    {
        qInfo() << "MacSplitTunnel destructor called";
        shutdownConnection();
    }

    const OriginalNetworkScan& netScan() const {return _params.netScan;}
    const FirewallParams& params() const {return _params;}
    const QString& tunnelDeviceName() const {return _tunnelDeviceName;}
    const QString& tunnelDeviceLocalAddress() const {return _tunnelDeviceLocalAddress;}

public slots:
    void initiateConnection(const FirewallParams &params, QString tunnelDeviceName,
                            QString tunnelDeviceLocalAddress);
    void shutdownConnection();
    void updateSplitTunnel(const FirewallParams &params, QString tunnelDeviceName,
                           QString tunnelDeviceLocalAddress);
    void aboutToConnectToVpn();
private:
    void readFromTunnel(int socket);
    void updateApps(QVector<QString> excludedApps, QVector<QString> vpnOnlyApps);
    void updateNetwork(const FirewallParams &params, QString tunnelDeviceName,
                               QString tunnelDeviceLocalAddress);
    bool isSplitPort(quint16 port,
                     const PortSet &bypassPorts,
                     const PortSet &vpnOnlyPorts);
    void handleIp4(std::vector<unsigned char> buffer, int packetSize);
    void handleIp6(std::vector<unsigned char> buffer, int packetSize);
    void setupIpForwarding(const QString &setting, const QString &value, QString &storedValue);
    void teardownIpForwarding(const QString &setting, QString &storedValue);
    QString sysctl(const QString &setting, const QString &value);

private:
    enum class State
    {
        Active,
        Inactive
    };

private:
    std::unique_ptr<QSocketNotifier> _readNotifier;
    int _rawFd4{-1};
    UTun _utun;
    State _state{State::Inactive};
    QVector<QString> _excludedApps;
    QVector<QString> _vpnOnlyApps;
    QSet<QString> _bypassIpv4Subnets;
    QString _tunnelDeviceName;
    QString _tunnelDeviceLocalAddress;
    AppCache _defaultAppsCache;
    BypassRuleUpdater _bypassRuleUpdater;
    VpnOnlyRuleUpdater _vpnOnlyRuleUpdater;
    DefaultRuleUpdater _defaultRuleUpdater;
    SplitTunnelIp _splitTunnelIp;
    FirewallParams _params;

    QString _ipForwarding4;
    QString _ipForwarding6;
    bool _routesUp{false};

    using AddressPair = QPair<quint32, quint16>;
    AddressPair _lastPacketAddress4{};
    quint32 _lastPacketCount4{0};
};
#endif
