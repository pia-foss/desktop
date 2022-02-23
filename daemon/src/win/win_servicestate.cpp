// Copyright (c) 2022 Private Internet Access, Inc.
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
#line SOURCE_FILE("win_servicestate.cpp")

#include "win_servicestate.h"
#include "win/win_util.h"
#include <QElapsedTimer>

namespace
{
    // Wait for a service to start or stop.  The service state is monitored
    // for the specified "pending" and "final" states:
    //
    // - If the service is ever in the Deleted state, the task rejects
    // - The Initializing state is ignored
    // - Initially, the service can be in any state (other than Deleted)
    // - Once the service reaches the "pending" state, a transition to any other
    //   state than the final state rejects the task (service failed to start or
    //   stop)
    // - If the service reaches the final state (even if it never reached the
    //   pending state) the task is resolved
    class WinServiceStateTask : public Task<void>
    {
    public:
        WinServiceStateTask(WinServiceState &monitor,
                            WinServiceState::State pendingState,
                            WinServiceState::State finalState)
            : _pendingState{pendingState}, _finalState{finalState},
              _observedPending{false}
        {
            _elapsed.start();
            connect(&monitor, &WinServiceState::stateChanged, this,
                    &WinServiceStateTask::checkState);
            checkState(monitor.lastState());
        }

    private:
        void checkState(WinServiceState::State newState)
        {
            if(!isPending())
                return;
            if(newState == WinServiceState::State::Initializing)
                return;

            if(newState == WinServiceState::State::Deleted)
            {
                qWarning() << "Service was deleted during start/stop operation";
                reject({HERE, Error::Code::TaskRejected});
            }
            // Check if we've reached the pending state
            else if(newState == _pendingState)
            {
                qInfo() << "Service start/stop is pending in state"
                    << traceEnum(newState) << "after"
                    << traceMsec(_elapsed.elapsed());
                _observedPending = true;
            }
            // Check if we've reached the final state
            else if(newState == _finalState)
            {
                qInfo() << "Service start/stop completed in state"
                    << traceEnum(newState) << "after"
                    << traceMsec(_elapsed.elapsed());
                resolve();
            }
            // Otherwise, we're in some state other than pending or final.  If
            // we had observed the pending state, reject the task, the operation
            // failed.
            else if(_observedPending)
            {
                qWarning() << "Service operation failed, expected"
                    << traceEnum(_pendingState) << "->"
                    << traceEnum(_finalState) << "- got"
                    << traceEnum(newState);
                reject({HERE, Error::Code::TaskRejected});
            }
            // The task may be destroyed at this point since we may have
            // resolved or rejected above.
        }

    private:
        QElapsedTimer _elapsed;
        // The "pending" and "final" states for the requested operation.
        WinServiceState::State _pendingState, _finalState;
        bool _observedPending;
    };

    // Task to wait for the service state to be loaded - resolves when the state
    // is not Initializing
    class WinServiceStateInitializedTask : public Task<void>
    {
    public:
        WinServiceStateInitializedTask(WinServiceState &monitor)
        {
            connect(&monitor, &WinServiceState::stateChanged, this,
                    &WinServiceStateInitializedTask::checkState);
            checkState(monitor.lastState());
        }
    public:
        void checkState(WinServiceState::State newState)
        {
            if(!isPending())
                return;
            if(newState != WinServiceState::State::Initializing)
            {
                qInfo() << "Service state loaded:" << traceEnum(newState);
                resolve();
                // May have deleted *this
            }
        }
    };
}

void CALLBACK WinServiceState::serviceNotifyCallback(void *pParam)
{
    WinServiceState *pThis{nullptr};
    if(pParam)
    {
        WinServiceNotify *pNotify = reinterpret_cast<WinServiceNotify*>(pParam);
        pThis = reinterpret_cast<WinServiceState*>(pNotify->pContext);
    }

    if(!pThis)
    {
        qWarning() << "Invalid context during service notification callback:"
            << reinterpret_cast<qintptr>(pParam);
        return;
    }

    // Pull out the notification structure - we no longer have a notification
    // queued.  Put it in a shared pointer since invokeMethod() will copy the
    // functor
    std::shared_ptr<WinServiceNotify> pTriggeredNotify{pThis->_pScmNotify.release()};
    if(!pTriggeredNotify)
    {
        qWarning() << "Unexpected notification context -"
            << reinterpret_cast<qintptr>(pParam) << "!="
            << reinterpret_cast<qintptr>(pTriggeredNotify.get());
        return;
    }

    // Defer the actual processing, we can't call SCM functions during this APC
    // callback.
    QMetaObject::invokeMethod(pThis,
        [pThis, pTriggeredNotify = std::move(pTriggeredNotify)]()
        {
            pThis->serviceChanged(*pTriggeredNotify);
        }, Qt::ConnectionType::QueuedConnection);
}

WinServiceState::WinServiceState(std::wstring serviceName, DWORD startStopRights)
    : _startStopRights{startStopRights}, _lastState{State::Deleted},
      _lastPid{0}, _serviceName{std::move(serviceName)}
{
    startNotifications();
}

WinServiceState::~WinServiceState()
{
    // Make sure any connected start/stop tasks are resolved
    qInfo() << "Shutting down service state monitor for" << _serviceName;
    updateState(State::Deleted, 0);
}

void WinServiceState::startNotifications()
{
    // If we can't initialize, we'll consider the service deleted
    _lastState = State::Deleted;

    _scm.reset(::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
    if(!_scm)
    {
        WinErrTracer error{::GetLastError()};
        qWarning() << "Unable to open SCM to monitor service" << _serviceName
            << "-" << error;
        return;
    }

    DWORD accessRights = SERVICE_QUERY_STATUS|_startStopRights;

    _service.reset(::OpenServiceW(_scm, _serviceName.c_str(), accessRights));
    if(!_service)
    {
        WinErrTracer error{::GetLastError()};
        qWarning() << "Unable to open service" << _serviceName
            << "to monitor state:" << error;
        _scm.close();
        return;
    }

    // Start the first notification request.
    if(!requestNotifications())
    {
        // Got an SCM lagging error somehow.  Can't reinitialize here, this was
        // a brand new SCM handle.
        qWarning() << "Unable to start notifications for service"
            << _serviceName;
        // Remain in the Deleted state since we're hosed
        return;
    }

    // Otherwise, if requestNotifications() requested a notification
    // successfully, go to the Initializing state.  (If it couldn't request a
    // notification, we're hosed and consider the service deleted.)
    if(_pScmNotify)
        _lastState = State::Initializing;
}

bool WinServiceState::requestNotifications()
{
    _pScmNotify.reset(new WinServiceNotify{});
    _pScmNotify->dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
    _pScmNotify->pfnNotifyCallback = &serviceNotifyCallback;
    _pScmNotify->pContext = reinterpret_cast<void*>(this);
    auto serviceNotifyResult = ::NotifyServiceStatusChangeW(_service,
        SERVICE_NOTIFY_CONTINUE_PENDING | SERVICE_NOTIFY_DELETE_PENDING |
        SERVICE_NOTIFY_PAUSED | SERVICE_NOTIFY_RUNNING |
        SERVICE_NOTIFY_START_PENDING | SERVICE_NOTIFY_STOP_PENDING |
        SERVICE_NOTIFY_STOPPED, _pScmNotify.get());

    if(serviceNotifyResult == ERROR_SUCCESS)
    {
        // Not traced, we get duplicate notifications a lot, so this would be
        // pretty verbose
        return true;
    }

    // In all other cases, close the handles and discard the notification struct
    // that was allocated
    _service.close();
    _scm.close();
    _pScmNotify.reset();
    switch(serviceNotifyResult)
    {
        case ERROR_SERVICE_MARKED_FOR_DELETE:
            // The service must have been marked for deletion between opening
            // the handle and requesting notifications.  We just have to close
            // the handle (which we did).
            qInfo() << "Service" << _serviceName
                << "was marked for deletion before waiting on notifications";
            break;
        case ERROR_SERVICE_NOTIFY_CLIENT_LAGGING:
            // SCM client is lagging - we should try to reinitialize from
            // scratch.
            qWarning() << "SCM client is lagging for service" << _serviceName;
            // This is the only case in which we return 'false' to indicate that
            // the caller may try to reinitialize.
            return false;
        default:
        {
            WinErrTracer error{serviceNotifyResult};
            qWarning() << "Unable to monitor service" << _serviceName << "-"
                << error;
            // We could concievably try to reinitialize in the presence of some
            // other error, but SCM tends to be pretty fragile, so we don't do
            // that to avoid doing anything too surprising.  Just assume we're
            // hosed and consider the service deleted.
            break;
        }
    }
    return true;
}

void WinServiceState::serviceChanged(const WinServiceNotify &notify)
{
    // Class invariant - handles valid, since we were able to request a
    // notification
    Q_ASSERT(_scm);
    Q_ASSERT(_service);
    // Class invariant - do not have another notification queued, since we
    // received this one and haven't processed it yet
    Q_ASSERT(!_pScmNotify);

    // If the service is being deleted, we have to close the handle.
    // If any other error occurs, we consider it deleted and shut down.
    if(notify.dwNotificationStatus != ERROR_SUCCESS)
    {
        if(notify.dwNotificationStatus == ERROR_SERVICE_MARKED_FOR_DELETE)
        {
            qWarning() << "Service" << _serviceName
                << "is marked for deletion in state" << traceEnum(_lastState);
        }
        else
        {
            qWarning() << "Service" << _serviceName
                << "reported error"
                << WinErrTracer{notify.dwNotificationStatus} << "in state"
                << traceEnum(_lastState);
        }
        updateState(State::Deleted, 0);
        _service.close();
        _scm.close();
        return;
    }

    // Check the new state.  Avoid tracing duplicate notifications, these occur
    // a lot.
    State newState = State::Initializing;
    switch(notify.ServiceStatus.dwCurrentState)
    {
        case SERVICE_CONTINUE_PENDING:
            newState = State::ContinuePending;
            break;
        case SERVICE_PAUSE_PENDING:
            newState = State::PausePending;
            break;
        case SERVICE_PAUSED:
            newState = State::Paused;
            break;
        case SERVICE_RUNNING:
            newState = State::Running;
            break;
        case SERVICE_START_PENDING:
            newState = State::StartPending;
            break;
        case SERVICE_STOP_PENDING:
            newState = State::StopPending;
            break;
        case SERVICE_STOPPED:
            newState = State::Stopped;
            break;
        default:
            qWarning() << "Unexpected service state"
                << notify.ServiceStatus.dwCurrentState << "for service"
                << _serviceName;
            break;
    }

    // Ignore duplicate changes, or unknown states (represented here by
    // State::Initializing)
    if(newState != _lastState && newState != State::Initializing)
    {
        qInfo() << "Service reported state"
            << notify.ServiceStatus.dwCurrentState << "-"
            << traceEnum(_lastState) << "->" << traceEnum(newState);
        updateState(newState, notify.ServiceStatus.dwProcessId);
    }

    // Resume notifications.  If this fails due to the client lagging, we can
    // try to reinitialize.
    if(!requestNotifications())
    {
        qWarning() << "Couldn't request notifications for" << _serviceName
            << "due to client lagging, try to reinitialize";
        startNotifications();
        // This updates the state to either Deleted or Initializing, emit the
        // change
        emit stateChanged(_lastState, 0);
    }
    // Otherwise, if requestNotifications() wasn't able to start a notification
    // we're hosed, go to the Deleted state
    else if(!_pScmNotify)
    {
        qWarning() << "Couldn't request notifications for" << _serviceName
            << "- assuming service is lost";
        updateState(State::Deleted, 0);
    }
}

void WinServiceState::updateState(State newState, DWORD newPid)
{
    if(newState != _lastState || newPid != _lastPid)
    {
        qInfo() << "Service" << _serviceName << "updated from"
            << traceEnum(_lastState) << "(pid" << _lastPid << ") to"
            << traceEnum(newState) << "(pid" << newPid << ")";
        _lastState = newState;
        _lastPid = newPid;
        emit stateChanged(_lastState, _lastPid);
    }
}

Async<void> WinServiceState::startService()
{
    if(!_service)
    {
        qWarning() << "Can't start service" << _serviceName
            << "- service was not open";
        return Async<void>::reject({HERE, Error::Code::TaskRejected});
    }

    if(!::StartServiceW(_service, 0, nullptr))
    {
        WinErrTracer error{::GetLastError()};
        qWarning() << "Can't start service" << _serviceName
            << "-" << error;
        // This error is known to occur in some cases when attempting to restart
        // Dnscache and must be handled specifically by WinDnsCacheControl
        if(error.code() == ERROR_INCOMPATIBLE_SERVICE_SID_TYPE)
            return Async<void>::reject({HERE, Error::Code::WinServiceIncompatibleSidType});
        return Async<void>::reject({HERE, Error::Code::TaskRejected});
    }

    return waitForStart();
}

Async<void> WinServiceState::waitForStart()
{
    return Async<WinServiceStateTask>::create(*this, State::StartPending,
                                              State::Running);
}

Async<void> WinServiceState::waitForStop()
{
    return Async<WinServiceStateTask>::create(*this, State::StopPending,
                                              State::Stopped);
}

Async<void> WinServiceState::stopService()
{
    if(!_service)
    {
        qWarning() << "Can't stop service" << _serviceName
            << "- service was not open";
        return Async<void>::reject({HERE, Error::Code::TaskRejected});
    }

    SERVICE_STATUS status;  // Ignored but required by ControlService()
    if(!::ControlService(_service, SERVICE_CONTROL_STOP, &status))
    {
        WinErrTracer error{::GetLastError()};
        qWarning() << "Can't stop service" << _serviceName
            << "-" << error;
        return Async<void>::reject({HERE, Error::Code::TaskRejected});
    }

    return waitForStop();
}

Async<void> WinServiceState::stopIfRunning()
{
    return Async<WinServiceStateInitializedTask>::create(*this)
        ->then(this, [this]() -> Async<void>
        {
            switch(_lastState)
            {
                case State::StartPending:
                    qInfo() << "Service" << _serviceName
                        << "is starting, stop it after it starts";
                    return Async<WinServiceStateTask>::create(*this, State::StartPending,
                                                              State::Running)
                        ->then(this, [this]()
                        {
                            qInfo() << "Service" << _serviceName
                                << "startup finished, stop it now";
                            TraceStopwatch stopwatch{"Stop service after startup"};
                            return stopService();
                        });
                case State::StopPending:
                    qInfo() << "Service" << _serviceName
                        << "is already stopping, just wait for it to stop";
                    return Async<WinServiceStateTask>::create(*this, State::StopPending,
                                                              State::Stopped);
                case State::Stopped:
                    qInfo() << "Service" << _serviceName << "is already stopped";
                    return Async<void>::resolve();
                default:
                    qInfo() << "Service" << _serviceName << "is in state"
                        << traceEnum(_lastState) << "- stop it now";
                    return stopService();
            }
        });
}
