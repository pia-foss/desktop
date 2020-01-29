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
#line HEADER_FILE("win/win_daemon.h")

#ifndef WIN_DAEMON_H
#define WIN_DAEMON_H
#pragma once

#include "daemon.h"
#include "win_appmonitor.h"
#include "win_firewall.h"
#include "win/win_messagewnd.h"
#include "win/servicemonitor.h"

class WinNetworkAdapter : public NetworkAdapter
{
public:
    WinNetworkAdapter(const QString& guid) : NetworkAdapter(guid), _savedMetricValueIPv4(-1), _savedMetricValueIPv6(-1) {}

    quint64 luid;
    QString adapterName;
    QString connectionName;
    bool isCustomTap;
    quint32 ifIndex;
    virtual void setMetricToLowest() override;
    virtual void restoreOriginalMetric() override;
private:
    // Any value less than 5 should work fine. 3 is chosen without any real reason.
    enum { interfaceMetric = 3 };
    int _savedMetricValueIPv4, _savedMetricValueIPv6;
    void setMetric(const ADDRESS_FAMILY family, ULONG metric);
    int getMetric(const ADDRESS_FAMILY);
};

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

class WinDaemon : public Daemon, private MessageWnd
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("win.daemon")

private:
    // Callback passed to NotifyIpInterfaceChange() to be called when an adapter
    // change occurs.
    static void WINAPI ipChangeCallback(PVOID callerContext,
                                        PMIB_IPINTERFACE_ROW pRow,
                                        MIB_NOTIFICATION_TYPE notificationType);

public:
    explicit WinDaemon(const QStringList& arguments, QObject* parent = nullptr);
    explicit WinDaemon(QObject* parent = nullptr);
    ~WinDaemon();

    static WinDaemon* instance() { return static_cast<WinDaemon*>(Daemon::instance()); }

    virtual QSharedPointer<NetworkAdapter> getNetworkAdapter() override;

    static QList<QSharedPointer<WinNetworkAdapter>> getAllNetworkAdapters();

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
        WfpFilterObject splitAppBind; // Filter to invoke callout for app in bind layer
        WfpFilterObject splitAppConnect; // Filter to invoke callout for app in connect layer
    };

private:
    // Check if the adapter is present, and update Daemon's corresponding state
    // (Daemon::adapterValid()).
    void checkNetworkAdapter();
    void onAboutToConnect();

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

protected:
    virtual void applyFirewallRules(const FirewallParams& params) override;
    virtual QJsonValue RPC_inspectUwpApps(const QJsonArray &familyIds) override;
    virtual void RPC_checkCalloutState() override;
    virtual void writePlatformDiagnostics(DiagnosticsFile &file) override;

private:
    void removeSplitTunnelAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                                     const QString &traceType);
    void createBypassAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                                const WfpProviderContextObject &context,
                                const AppIdKey &appId);
    void createOnlyVPNAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                                 const WfpProviderContextObject &context,
                                 const AppIdKey &appId);
    void createBlockAppFilters(std::map<QByteArray, SplitAppFilters> &apps,
                               const AppIdKey &appId);
    void reapplySplitTunnelFirewall(const QString &newSplitTunnelIp,
                                    const QString &newTunnelIp,
                                    const std::set<const AppIdKey*, PtrValueLess> &newExcludedApps,
                                    const std::set<const AppIdKey*, PtrValueLess> &newVpnOnlyApps,
                                    bool hasConnected);

protected:
    FirewallEngine* _firewall;
    struct FirewallFilters
    {
        WfpFilterObject permitPIA[5];
        WfpFilterObject permitAdapter[2];
        WfpFilterObject permitLocalhost[2];
        WfpFilterObject permitDHCP[2];
        WfpFilterObject permitLAN[9];
        WfpFilterObject blockDNS[2];
        WfpFilterObject permitDNS[2];
        WfpFilterObject blockAll[2];
        WfpFilterObject permitHnsd[2];
        WfpFilterObject blockHnsd[2];

        // This is not strictly a filter, but it can in nearly all respects be treated the same way
        // so we store it here for simplicity and so we can re-use the filter-related code
        WfpCalloutObject splitCalloutBind;
        WfpCalloutObject splitCalloutConnect;

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

    // App ID for hnsd, needed when we add a special "split" rule for hnsd.
    AppIdKey _hnsdAppId;

    // The last 'hasConnected' state and local VPN IP address used to create the
    // VPN-only split tunnel rules - the rules are recreated if they change
    bool _lastConnected;
    QString _lastTunnelIp;
    // The last local IP address we used to create split tunnel rules - causes
    // us to recreate the rules if it changes.
    QString _lastSplitTunnelIp;

    // This is contextual information we need to detect invalidation of some of
    // the WFP filters.
    UINT64 _filterAdapterLuid;  // LUID of the TAP adapter used in some rules
    QString _dnsServers[2]; // Permitted DNS server addresses
    HANDLE _ipNotificationHandle;
    // When Windows suspends, the TAP adapter disappears, and it won't be back
    // right away when we resume.  This just suppresses the "TAP adapter
    // missing" error briefly after a system resume.
    WinUnbiasedDeadline _resumeGracePeriod;
    ServiceMonitor _wfpCalloutMonitor;
    WinAppMonitor _appMonitor;
};

#undef g_daemon
#define g_daemon (WinDaemon::instance())

#endif // WIN_DAEMON_H
