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
#line HEADER_FILE("win_dnscachecontrol.h")

#ifndef WIN_DNSCACHECONTROL_H
#define WIN_DNSCACHECONTROL_H

#include "win_servicestate.h"
#include "win/win_util.h"
#include <QTimer>

// WinDnsCacheControl disables or restores the DnsCache service, which is
// necessary to implement split tunnel DNS on Windows.  This is the only way to
// identify DNS traffic per app - disabling the DnsCache service causes apps to
// perform their own DNS.
//
// Microsoft does have documentation indicating that the system can run without
// the DNS cache service:
//   https://docs.microsoft.com/en-us/windows-server/networking/dns/troubleshoot/disable-dns-client-side-caching
//
// However, the documentation is somewhat out of date, the methods listed no
// longer work.  We generally avoid trying to be too "clever" on Windows, but
// there's no other option in this case, and splitting app DNS is important for
// streaming services to work with split tunnel by default.
//
// Permanently disabling the DnsCache service is not an option:
// - It has some side effects besides losing DNS caching: apparently the
//   DnsCache service plays a role in configuring per-adapter DNS.  ipconfig and
//   netsh are unable to display or set DNS servers when DnsCache is disabled.
//   netsh functionality is needed to connect with WireGuard or the OpenVPN
//   'static' method.
// - It requires a system reboot to take effect, this means it's difficult for
//   users to recover if disabling the DNS cache creates a problem.
// - It's fragile - it would be hard to keep track of whether PIA had disabled
//   DnsCache or the user had (some users do this), and it risks causing issues
//   on the user's system that are hard to recover from.
//
// Instead, PIA takes a different approach.  SCM may not let us stop the
// DnsCache service, but we are able to kill the process, and we can temporarily
// replace the service with a stub so it won't come back when SCM tries to
// restart it.  (Note that the system still tries to restart DnsCache on its own
// even if we remove the recovery settings from the service, so we can't
// generally prevent the service from being restarted.)
//
// This avoids making any permanent changes - we can immediately restore the
// original service target, so in the worst case a system restart will restore
// normal behavior.  It also interacts well with users that manually disable
// DnsCache, since we don't have to touch the enabled state of the service.
// It's also simple to implement a recovery failsafe in case PIA would fail to
// restore the default setting, we can easily detect the presence of
// pia-winsvcstub in the DnsCache configuration and restore it.
class WinDnsCacheControl : public QObject
{
    Q_OBJECT

private:
    enum class DnsCacheState
    {
        // The real Dnscache service is running.
        Normal,
        // The stubbed Dnscache service is running.
        Stubbed,
        // The service is not running, either temporarily or permanently.
        Down,
    };

public:
    WinDnsCacheControl();

private:
    std::pair<std::wstring, DWORD> getDnscacheRegKeys();
    bool setDnscacheRegExpandSz(const std::wstring &valueName,
                                const std::wstring &value);
    bool setDnscacheRegKeys(const std::wstring &imagePath, DWORD type,
                            const std::wstring &rollbackImagePath);
    std::wstring getDnscacheRunningBasename(DWORD pid, WinHandle &dnscacheProcess);
    void serviceStateChanged(WinServiceState::State newState, DWORD newPid);

    // Stub the Dnscache service - temporarily point it to the stub, then kill
    // it and let the system start the stub
    void stubDnsCache(const WinHandle &dnscacheProcess, DWORD dnscachePid);
    bool killServiceIfNetworkService(ServiceHandle &scm,
                                     std::vector<unsigned char> &configBuffer,
                                     const ENUM_SERVICE_STATUS_PROCESSW &svcStatus);
    bool killNetworkService();

    // Start the Dnscache service after it was terminated.  Handles
    // ERROR_INCOMPATIBLE_SERVICE_SID_TYPE (which is known to occur on some
    // systems) by terminating any existing NetworkService svchost and retrying.
    void startDnsCache();
    // Restart the Dnscache service by terminating the process, then starting
    // the service again
    void restartDnsCache(const WinHandle &dnscacheProcess, DWORD dnscachePid);

public:
    // Disable the DNS cache (if it hasn't already been disabled).
    void disableDnsCache();
    // Restore the DNS cache to the default state.
    void restoreDnsCache();

private:
    std::wstring _dnsCacheStubImagePath;
    // When we've been told to disable the DNS cache service, this is set.  If
    // we see the service go back to the normal state, we'll stub it again.
    bool _stubEnabled;
    WinHKey _dnscacheRegKey;
    WinServiceState _serviceState;
    // Whenever we observe a new state or PID from WinServiceState, we check
    // whether the process is svchost.exe or the PIA stub.  (This ensures we
    // don't have to constantly recheck it for every firewall update.)
    DnsCacheState _lastDnsCacheState;
    QTimer _restartDelayTimer;
};

#endif
