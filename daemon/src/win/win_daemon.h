// Copyright (c) 2019 London Trust Media Incorporated
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

private:
    // Check if the adapter is present, and update Daemon's corresponding state
    // (Daemon::adapterValid()).
    void checkNetworkAdapter();
    void onAboutToConnect();

    virtual LRESULT proc(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

protected:
    virtual void applyFirewallRules(const FirewallParams& params) override;
    virtual QJsonValue RPC_inspectUwpApps(const QJsonArray &familyIds) override;
    virtual void RPC_checkCalloutState() override;
    virtual void writePlatformDiagnostics(DiagnosticsFile &file) override;

protected:
    FirewallEngine* _firewall;
    struct ExcludeAppFilters
    {
        WfpFilterObject permitApp; // Filter to permit traffic from app
        WfpFilterObject splitApp; // Filter to invoke callout for app
    };
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
        WfpCalloutObject splitCallout;

        WfpProviderContextObject providerContextKey;
    } _filters;

    // Each excluded application gets its own permit and split rule.  (Testing
    // shows that "OR" conditions are not reliable on Windows 7.)  The map is
    // ordered so we can check if the keys have changed when updating firewall
    // rules.
    // Keys are copies of AppIdKeys created with AppIdKey::copyData() (the
    // AppIdKeys are managed by _appMonitor).
    std::map<QByteArray, ExcludeAppFilters> excludedApps;
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
