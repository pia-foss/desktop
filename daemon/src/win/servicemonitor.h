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
#line HEADER_FILE("servicemonitor.h")

#ifndef SERVICEMONITOR_H
#define SERVICEMONITOR_H

#include <windows.h>
#include "../../../extras/installer/win/service_inl.h"
#include "settings.h"
#include <QObject>

// ServiceMonitor keeps track of whether a given service is installed.  It does
// _not_ track whether the service is running - this is used to monitor kernel
// driver services, and SCM cannot notify state changes for those.
//
// After initializing ServiceMonitor and connecting signal handlers, call
// lastState() to load the initial state.  ServiceMonitor will then emit
// serviceStateChanged() if the state changes.
//
// ServiceMonitor reports the state as a DaemonState::NetExtensionState value.
// If it reports the 'Error' state, it may not be able to report any further
// state changes.
class ServiceMonitor : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("win.servicemonitor");

private:
    // SERVICE_NOTIFY_2W that also cleans up its pszServiceNames when destroyed.
    class WinServiceNotify : public SERVICE_NOTIFY_2W
    {
    public:
        WinServiceNotify() : SERVICE_NOTIFY_2W{} {}
        ~WinServiceNotify()
        {
            if(pszServiceNames)
                ::LocalFree(pszServiceNames);
        }

    private:
        // Not copiable due to pszServiceNames ownership
        WinServiceNotify(const WinServiceNotify &) = delete;
        WinServiceNotify &operator=(const WinServiceNotify &) = delete;
    };

public:
    using NetExtensionState = DaemonState::NetExtensionState;

private:
    static ServiceMonitor *thisFromCallback(void *pNotify);
    static void CALLBACK scmNotifyCallback(void *pNotify);

public:
    ServiceMonitor(std::wstring serviceName);
    ~ServiceMonitor();

private:
    ServiceMonitor(const ServiceMonitor &) = delete;
    ServiceMonitor &operator=(const ServiceMonitor &) = delete;

private:
    // SCM notifications are broken in Windows 10 versions 1507-1511; see
    // implementation for details.  Used to set _manualChecksOnly.
    bool isBrokenWindows10();
    // Check the initial state of the service.  This initially opens the SCM,
    // tries to open the service, and starts notifications.
    void checkInitialState();
    // Check the state of the service - open it if possible and it's not already
    // open, and start state notifications.  Emits a state change.
    // The caller ensures that the SCM is open.
    //
    // Returns false if the service exists but notifications can't be resumed -
    // the caller should try to reopen SCM if possible.  (This includes
    // ERROR_SERVICE_NOTIFY_CLIENT_LAGGING as well as any other unexpected
    // error.)  Otherwise, returns true.
    bool checkServiceState();
    // Set up _pScmNotify and call NotifyServiceStatusChangeW().
    DWORD notifyScm();
    // Handle scmNotifyCallback() - create/destroy notification from SCM
    void serviceCreatedDestroyed(const WinServiceNotify &scmNotify);
    void reportServiceState(NetExtensionState newState);

public:
    // Get the last reported state
    NetExtensionState lastState() const {return _lastState;}
    // Trigger a manual check if needed on builds of Windows that cannot use
    // SCM notifications.  Has no effect if SCM notifications are being used.
    void doManualCheck();

signals:
    void serviceStateChanged(NetExtensionState serviceState);

private:
    // On Windows 10 1507-1511, use manual checks only to workaround broken SCM
    // notifications.  When this is enabled, we never keep the SCM handle open
    // or start notifications (_scm and _pScmNotify are always nullptr).
    bool _manualChecksOnly;
    NetExtensionState _lastState;
    std::wstring _serviceName;
    ServiceHandle _scm;
    // The notification struct is held by reference so it can be pulled out when
    // a notification APC occurs.  We have to defer the actual processing after
    // the APC, so it's possible that we could try to restart notifications
    // while a queued call is outstanding - this ensures that we still process
    // that queued call correctly.
    //
    // The presence of a struct here indicates that we have enabled
    // notifications with NotifyServiceStatusChangeW().
    std::unique_ptr<WinServiceNotify> _pScmNotify;
};

#endif
