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

#include <common/src/common.h>
#line HEADER_FILE("posix/posix_daemon.h")

#ifndef POSIX_DAEMON_H
#define POSIX_DAEMON_H
#pragma once

#include "../daemon.h"
#include <common/src/posix/unixsignalhandler.h>
#include <common/src/filewatcher.h>
#include <kapps_net/src/firewall.h>

#if defined(Q_OS_MAC)
#include "../mac/mac_dns.h"
#elif defined(Q_OS_LINUX)
#include "../linux/linux_modsupport.h"
#include <kapps_net/src/linux/linux_cn_proc.h>
#endif

class QSocketNotifier;

class PosixDaemon : public Daemon
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("posix.daemon")
public:
    PosixDaemon();
    ~PosixDaemon();

    static PosixDaemon* instance() { return static_cast<PosixDaemon*>(Daemon::instance()); }

    virtual std::shared_ptr<NetworkAdapter> getNetworkAdapter() override;

protected slots:
    void handleSignal(int sig) Q_DECL_NOEXCEPT;

protected:
    virtual void applyFirewallRules(kapps::net::FirewallParams params) override;
    virtual void writePlatformDiagnostics(DiagnosticsFile &file) override;

private:
#if defined(Q_OS_LINUX)
    void updateExistingDNS();
#endif

    // Check whether the host supports advanced features (split tunnel,
    // automation) and record errors.
    // This function will also attempt to create the net_cls VFS on Linux if it
    // doesn't exist
    void checkFeatureSupport();

    void onAboutToConnect();

#ifdef Q_OS_LINUX
    void checkLinuxModules();
#endif

private:
    UnixSignalHandler _signalHandler;

    // Temporary, remove
    QTimer _tempTestTimer;

#ifdef Q_OS_LINUX
    FileWatcher _resolvconfWatcher;
    LinuxModSupport _linuxModSupport;
    // Used to test if the running kernel is configured with cn_proc; there's no
    // way to figure this out other than to try to connect to it and see if we
    // get the initial notification.
    nullable_t<kapps::net::CnProc> _pCnProcTest;
#endif

#ifdef Q_OS_MAC
    MacDns _macDnsMonitor;
    // Network scan last used to create bound route (see applyFirewallRules())
#endif

    // The firewall implementation from kapps::net.  Note that unlike WinDaemon,
    // this can be nullptr; it's cleared early if we receive a signal that will
    // shut down the daemon.
    nullable_t<kapps::net::Firewall> _pFirewall;
};

void setUidAndGid();

#endif // POSIX_DAEMON_H
