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

#include "common.h"
#line HEADER_FILE("win/win_daemon.h")

#ifndef WIN_DAEMON_H
#define WIN_DAEMON_H
#pragma once

#include "daemon.h"
#include "networkmonitor.h"
#include "win_appmonitor.h"
#include "win_firewall.h"
#include "win_interfacemonitor.h"
#include "win_dnscachecontrol.h"
#include "win/win_messagewnd.h"
#include "win/servicemonitor.h"
#include "win/win_servicestate.h"

// Deadline timer (like QDeadlineTimer) that does not count time when the system
// is suspended.  (Both clock sources for QDeadlineTimer do count suspend time.)
class WinUnbiasedDeadline
{
public:
    // WinUnbiasedDeadline is initially in the "expired" state.
    WinUnbiasedDeadline();

private:
    ULONGLONG getUnbiasedTime() const;

public:
    // Set the remaining time.  If the time is greater than 0, the timer is now
    // unexpired.  If the time is 0, it is now expired.
    void setRemainingTime(const std::chrono::microseconds &time);

    // Get the remaining time until expiration (0 if the timer is expired).
    std::chrono::microseconds remaining() const;

private:
    ULONGLONG _expireTime;
};

class WinRouteManager : public RouteManager
{
public:
    virtual void addRoute4(const QString &subnet, const QString &gatewayIp, const QString &interfaceName, uint32_t metric=0) const override;
    virtual void removeRoute4(const QString &subnet, const QString &gatewayIp, const QString &interfaceName) const override;

    // TODO: Implement these when we support IPv6
    virtual void addRoute6(const QString &subnet, const QString &gatewayIp, const QString &interfaceName, uint32_t metric=0) const override {}
    virtual void removeRoute6(const QString &subnet, const QString &gatewayIp, const QString &interfaceName) const override {}
private:
    void createRouteEntry(MIB_IPFORWARD_ROW2 &route, const QString &subnet, const QString &gatewayIp, const QString &interfaceName, uint32_t metric) const;
};

class WinDaemon : public Daemon, private MessageWnd
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("win.daemon")

public:
    explicit WinDaemon(QObject* parent = nullptr);
    ~WinDaemon();

    static WinDaemon* instance() { return static_cast<WinDaemon*>(Daemon::instance()); }

    virtual std::shared_ptr<NetworkAdapter> getNetworkAdapter() override;

protected:
    struct SplitAppFilters
    {
        // Filter to permit any IPv4 traffic from app.  Used for "bypass" apps
        // only; only-VPN app traffic is permitted by normal firewall rules
        // (they should only communicate over the tunnel, which is already
        // permitted).
        WfpFilterObject permitApp;
        // Filter to block IPv4 traffic from app, used for only-VPN apps when
        // disconnected
        WfpFilterObject blockAppIpv4;
        // Filter to block IPv6 traffic from app, used for only-VPN apps in all
        // connection states
        WfpFilterObject blockAppIpv6;
        // Filter to invoke callout for app in bind layer.  Used to force UDP
        // sockets to particular interfaces.
        WfpFilterObject splitAppBind;
        // Filter to invoke callout for app in connect layer.  Used to force
        // TCP sockets to particular interfaces, and to capture DNS flow
        // information (DNS packets are rewritten in the IP packet layers, which
        // are not in an application context.)
        WfpFilterObject splitAppConnect;
        // Filter to permit any DNS address for this app - applied if DNS
        // rewriting is active for this type of app, since we will rewrite these
        // DNS queries to the correct address, which also provides leak
        // protection.
        WfpFilterObject appPermitAnyDns;
        // Filter to invoke callout for app in flow established layer.  This is
        // used to inspect DNS flows, which has to be done in this layer in
        // order to attach a flow context and cause WFP to notify us when the
        // flow is removed.
        WfpFilterObject splitAppFlowEstablished;
    };

    struct SplitTunnelFirewallParams : public DebugTraceable<SplitTunnelFirewallParams>
    {
        // Local IP and prefix length of the non-VPN network interface
        QString _physicalIp;
        unsigned _physicalNetPrefix;
        // Local IP of the VPN tunnel device
        QString _tunnelIp;
        // Whether we are connected to the VPN right now.
        bool _isConnected;
        // Whether we have connected since enabling the VPN (FirewallParams::hasConnected)
        // NOTE: _not_ whether we are currently connected right now
        bool _hasConnected;
        // Whether the VPN gets the default route (affects which DNS rewriting
        // rules become active)
        bool _vpnDefaultRoute;
        // Whether DNS leak protection is active
        bool _blockDNS;
        // Whether we need to force "VPN only" apps to use PIA's configured DNS.
        bool _forceVpnOnlyDns;
        // Whether we need to force "bypass" apps to use the existing DNS.
        bool _forceBypassDns;
        // Existing DNS servers on the physical interface
        std::vector<quint32> _existingDnsServers;
        // Effective DNS servers configured in PIA (empty = use existing)
        std::vector<quint32> _effectiveDnsServers;

    public:
        bool operator==(const SplitTunnelFirewallParams &other)
        {
            return _physicalIp == other._physicalIp &&
                _tunnelIp == other._tunnelIp &&
                _isConnected == other._isConnected &&
                _hasConnected == other._hasConnected &&
                _vpnDefaultRoute == other._vpnDefaultRoute &&
                _blockDNS == other._blockDNS &&
                _forceVpnOnlyDns == other._forceVpnOnlyDns &&
                _forceBypassDns == other._forceBypassDns &&
                _existingDnsServers == other._existingDnsServers &&
                _effectiveDnsServers == other._effectiveDnsServers;
        }

        void trace(QDebug &dbg) const
        {
            dbg << "- physical IP known:" << !_physicalIp.isEmpty()
                << "- tunnel IP known:" << !_tunnelIp.isEmpty()
                << "- have connected:" << _hasConnected
                << "- VPN default route:" << _vpnDefaultRoute
                << "- block DNS:" << _blockDNS
                << "- force VPN-only DNS:" << _forceVpnOnlyDns
                << "- force bypass DNS:" << _forceBypassDns
                << "- existing DNS server count:" << _existingDnsServers.size()
                << "- effective DNS server count:" << _effectiveDnsServers.size();
        }
    };

private:
    // Check if the adapter is present, and update Daemon's corresponding state
    // (Daemon::adapterValid()).
    void checkNetworkAdapter();
    void onAboutToConnect();

    std::vector<quint32> findExistingDNS(const std::vector<quint32> &piaDNSServers);

    void doVpnExclusions(std::set<const AppIdKey*, PtrValueLess> newExcludedApps, bool hasConnected);
    void doVpnOnly(std::set<const AppIdKey*, PtrValueLess> newVpnOnlyApps, bool hasConnected);

    bool areAppsUnchanged(std::set<const AppIdKey*, PtrValueLess> newApps,
                          std::map<QByteArray, SplitAppFilters> oldAppMap)
    {
        // Compare these element-wise; valid because both containers are sorted
        // lexically.
        return std::equal(oldAppMap.begin(), oldAppMap.end(),
            newApps.begin(), newApps.end(),
            [](const auto &existingApp, const AppIdKey *pNewApp)
            {
                return *pNewApp == existingApp.first;
            });
    }

    virtual LRESULT proc(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    // Firewall implementation and supporting methods
protected:
    virtual void applyFirewallRules(const FirewallParams& params) override;

private:
    void updateAllBypassSubnetFilters(const FirewallParams &params);
    void updateBypassSubnetFilters(const QSet<QString> &subnets, QSet<QString> &oldSubnets,
                                   std::vector<WfpFilterObject> &subnetBypassFilters, FWP_IP_VERSION ipVersion);
    void removeSplitTunnelAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                                     const QString &traceType);
    void createBypassAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                                const WfpProviderContextObject &context,
                                const AppIdKey &appId, bool rewriteDns);
    void createOnlyVPNAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                                 const WfpProviderContextObject &context,
                                 const AppIdKey &appId, bool rewriteDns);
    void createBlockAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                               const AppIdKey &appId);
    void reapplySplitTunnelFirewall(const SplitTunnelFirewallParams &params,
                                    const std::set<const AppIdKey*, PtrValueLess> &newExcludedApps,
                                    const std::set<const AppIdKey*, PtrValueLess> &newVpnOnlyApps);

    // Other Daemon overrides and supporting methods
protected:
    virtual QJsonValue RPC_inspectUwpApps(const QJsonArray &familyIds) override;
    virtual void RPC_checkDriverState() override;
    virtual void writePlatformDiagnostics(DiagnosticsFile &file) override;

private:
    // Check whether WinTUN is currently installed.  There's no way to be
    // notified of this, so WireguardServiceBackend hints to us to check it in
    // some circumstances.
    void checkWintunInstallation();

    // Trace memory usage of client processes.
    void traceClientMemory();

public:
    // WireguardServiceBackend calls these methods to hint to us to consider
    // re-checking the WinTUN installation state.

    // Indicates that a WG connection failed after having started the service
    // (not that it failed during the authentication stage, we only check
    // WinTUN when the service fails to start).
    void wireguardServiceFailed();
    // Indicates that a WG connection succeeded.
    void wireguardConnectionSucceeded();

    void handlePendingWinTunInstall();

protected:
    FirewallEngine* _firewall;
    struct FirewallFilters
    {
        // If hnsd is excluded from this build, there is only one set of
        // resolver filters (for Unbound)
        enum : std::size_t
        {
#if INCLUDE_FEATURE_HANDSHAKE
            ResolverFilterCount = 2,
#else
            ResolverFilterCount = 1,
#endif
        };
        WfpFilterObject permitPIA[6];
        WfpFilterObject permitAdapter[2];
        WfpFilterObject permitLocalhost[2];
        WfpFilterObject permitDHCP[2];
        WfpFilterObject permitLAN[10];
        WfpFilterObject blockDNS[2];
        WfpFilterObject permitInjectedDns;
        WfpFilterObject ipInbound;
        WfpFilterObject ipOutbound;
        WfpFilterObject permitDNS[2];
        WfpFilterObject blockAll[2];
        WfpFilterObject permitResolvers[ResolverFilterCount];
        WfpFilterObject blockResolvers[ResolverFilterCount];

        // This is not strictly a filter, but it can in nearly all respects be treated the same way
        // so we store it here for simplicity and so we can re-use the filter-related code
        WfpCalloutObject splitCalloutBind;
        WfpCalloutObject splitCalloutConnect;
        WfpCalloutObject splitCalloutFlowEstablished;
        WfpCalloutObject splitCalloutConnectAuth;
        WfpCalloutObject splitCalloutIpInbound;
        WfpCalloutObject splitCalloutIpOutbound;

        WfpProviderContextObject providerContextKey;
        WfpProviderContextObject vpnOnlyProviderContextKey;

    } _filters;

    // Each excluded application gets its own permit and split rule.  (Testing
    // shows that "OR" conditions are not reliable on Windows 7.)  The map is
    // ordered so we can check if the keys have changed when updating firewall
    // rules.
    // Keys are copies of AppIdKeys created with AppIdKey::copyData() (the
    // AppIdKeys are managed by _appMonitor).
    std::map<QByteArray, SplitAppFilters> excludedApps;
    std::map<QByteArray, SplitAppFilters> vpnOnlyApps;

    QSet<QString> _bypassIpv4Subnets;
    QSet<QString> _bypassIpv6Subnets;

    std::vector<WfpFilterObject> _subnetBypassFilters4;
    std::vector<WfpFilterObject> _subnetBypassFilters6;

    // App IDs for resolvers, needed when we add a special "split" rule.
    AppIdKey _unboundAppId;
#if INCLUDE_FEATURE_HANDSHAKE
    AppIdKey _hnsdAppId;
#endif

    // Inputs to reapplySplitTunnelFirewall() - the last set of inputs used is
    // stored so we know when to recreate the firewall rules.
    SplitTunnelFirewallParams _lastSplitParams;
    // Controller used to disable/restore the Dnscache service as needed for
    // split tunnel DNS
    WinDnsCacheControl _dnsCacheControl;

    // This is contextual information we need to detect invalidation of some of
    // the WFP filters.
    UINT64 _filterAdapterLuid;  // LUID of the TAP adapter used in some rules
    QString _dnsServers[2]; // Permitted DNS server addresses
    // When Windows suspends, the TAP adapter disappears, and it won't be back
    // right away when we resume.  This just suppresses the "TAP adapter
    // missing" error briefly after a system resume.
    WinUnbiasedDeadline _resumeGracePeriod;
    ServiceMonitor _wfpCalloutMonitor;
    std::unique_ptr<WinServiceState> _pMsiServiceState;
    WinAppMonitor _appMonitor;

    SubnetBypass _subnetBypass;

    // Trace memory usage of client processes periodically
    QTimer _clientMemTraceTimer;
};

#undef g_daemon
#define g_daemon (WinDaemon::instance())

#endif // WIN_DAEMON_H
