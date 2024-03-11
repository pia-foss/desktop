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

#include <common/src/common.h>
#line HEADER_FILE("win/win_daemon.h")

#ifndef WIN_DAEMON_H
#define WIN_DAEMON_H
#pragma once

#include "../daemon.h"
#include "../networkmonitor.h"
#include <kapps_net/src/win/win_appmonitor.h>
#include "win_interfacemonitor.h"
#include "win_dnscachecontrol.h"
#include <common/src/win/win_messagewnd.h>
#include "servicemonitor.h"
#include "win_servicestate.h"

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

public:
    explicit WinDaemon(QObject* parent = nullptr);
    ~WinDaemon();

    static WinDaemon* instance() { return static_cast<WinDaemon*>(Daemon::instance()); }

    virtual std::shared_ptr<NetworkAdapter> getNetworkAdapter() override;

private:
    // Check if the adapter is present, and update Daemon's corresponding state
    // (Daemon::adapterValid()).
    void checkNetworkAdapter();
    void onAboutToConnect();

    virtual LRESULT proc(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    // Firewall implementation and supporting methods
protected:
    virtual void applyFirewallRules(kapps::net::FirewallParams params) override;

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

    // Update split tunnel rules in WinAppMonitor
    void updateSplitTunnelRules();

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
    nullable_t<kapps::net::Firewall> _pFirewall;
    // Controller used to disable/restore the Dnscache service as needed for
    // split tunnel DNS
    WinDnsCacheControl _dnsCacheControl;

    // When Windows suspends, the TAP adapter disappears, and it won't be back
    // right away when we resume.  This just suppresses the "TAP adapter
    // missing" error briefly after a system resume.
    WinUnbiasedDeadline _resumeGracePeriod;
    ServiceMonitor _wfpCalloutMonitor;
    std::unique_ptr<WinServiceState> _pMsiServiceState;
    kapps::net::WinAppMonitor _appMonitor;
};

#undef g_daemon
#define g_daemon (WinDaemon::instance())

#endif // WIN_DAEMON_H
