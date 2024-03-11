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
#line HEADER_FILE("win_servicestate.h")

#ifndef WIN_SERVICESTATE_H
#define WIN_SERVICESTATE_H

#include <kapps_core/src/winapi.h>
#include "servicemonitor.h" // WinServiceNotify
#include <common/src/async.h>

// WinServiceState keeps track of the running state of an installed service on
// Windows.  (This is different from ServiceMonitor, which keeps track of
// whether a service is installed, but not whether it is running.)
//
// After initializing WinServiceState and connecting to signals, call
// lastState() to load the initial state.  WinServiceState then emits
// stateChanged() if the state changes.
//
// If WinServiceState can't open the service, or the service is later deleted,
// it goes to the Deleted state.  If the service is later reinstalled,
// WinServiceState does not detect it (WinServiceState must be recreated to try
// and open the service again.)
class WinServiceState : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("win.servicestate")

public:
    enum class State
    {
        // In the Initializing state, we're waiting for the initial notification
        // from SCM.  In rare cases, we could enter this state after reaching
        // one of the other states, if we have to reinitialize SCM due to
        // lagging behind the queue.
        Initializing,
        ContinuePending,
        PausePending,
        Paused,
        Running,
        StartPending,
        StopPending,
        Stopped,
        Deleted
    };
    Q_ENUM(State);

private:
    static void CALLBACK serviceNotifyCallback(void *pParam);

public:
    // Open and monitor the service specified by serviceName.  The initial state
    // is always Initializing or Deleted, any other state is reported
    // asynchronously.
    //
    // The start/stop rights desired can be specified; by default both start and
    // stop are requested.  Occasionally, some rights may need to be omitted to
    // watch a service that prevents LOCAL SYSTEM from obtaining those rights
    // (like Dnscache).  The start/stop actions won't work if those rights are
    // omitted, but the service can still be monitored.
    //
    // WinServiceState always requests the SERVICE_QUERY_INFORMATION right.
    WinServiceState(std::wstring serviceName,
                    DWORD startStopRights = (SERVICE_START|SERVICE_STOP));
    // When destroyed, WinServiceState emits a change to the Deleted state, if
    // it wasn't already in this state.  This ensures that any connected tasks
    // reject instead of hanging indefinitely.
    ~WinServiceState();

private:
    // Open SCM, the service, and start notifications.  This updates _lastState
    // to either Deleted or Initializing (but does not emit stateChanged).
    void startNotifications();

    // Request service change notifications from SCM using
    // NotifyServiceStatusChangeW().
    //
    // If SCM indicates that the client is lagging, this returns false, and the
    // caller can try to reinitialize if possible.
    bool requestNotifications();

    void serviceChanged(const WinServiceNotify &notify);

    void updateState(State newState, DWORD newPid);

public:
    State lastState() const {return _lastState;}
    DWORD lastPid() const {return _lastPid;}

    // Start the service, and return a task that resolves when the service
    // reaches the Running state.  If the service can't be started, returns a
    // rejected Task.
    Async<void> startService();

    // Wait for a service to start.  This doesn't attempt to start the service,
    // it just resolves if the service is started.
    Async<void> waitForStart();

    // Wait for a service to stop.  Like waitForStart(), doesn't attempt to stop
    // the service, just resolves when the service is stopped.
    Async<void> waitForStop();

    // Stop the service, and return a task that resolves when the service
    // reaches the Stopped state.  If the service can't be stopped, returns a
    // rejected Task.
    //
    // Note that this does not check the current state of the service.  If the
    // service isn't running, the task will likely reject, since the Stop
    // control is not allowed in this state.
    //
    // This should be used when the caller expects that the service is running
    // (such as if the caller started it).  Use stopIfRunning() to stop the
    // service only if it is running based on the last observed state.
    Async<void> stopService();

    // Stop the service if it is running.  This is usually used when the caller
    // is not aware of the current state of the service, but wants to ensure
    // that it is stopped.
    //
    // Checks the last observed state of the service:
    // - StartPending - waits for the service to finish starting, then calls
    //   stopService()
    // - StopPending - just waits for the service to reach Stopped, does not
    //   send a Stop control
    // - Stopped - resolves synchronously, service already stopped
    // - Any other state - calls stopService()
    //
    // If the service state hasn't been loaded (State::Initializing), it waits
    // for the state to be available before deciding whether to stop the
    // service.
    Async<void> stopIfRunning();

signals:
    void stateChanged(State newState, DWORD newPid);

private:
    DWORD _startStopRights;
    State _lastState;
    DWORD _lastPid;
    std::wstring _serviceName;
    ServiceHandle _scm;
    ServiceHandle _service;
    // Like ServiceMonitor, the notification struct has to be pulled out when a
    // notification APC occurs.  We have to defer the actual processing after
    // the APC, so the struct is pulled out to ensure that we don't try to do
    // something with it after a change has been signaled.
    //
    // The presence of a struct here indicates that we have enabled
    // notifications with NotifyServiceStatusChangeW().
    std::unique_ptr<WinServiceNotify> _pScmNotify;
};

#endif
