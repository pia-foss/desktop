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
#line SOURCE_FILE("servicemonitor.cpp")

#include "servicemonitor.h"
#include "win/win_util.h"
#include <QProcess>
#include <QRegularExpression>
#include <VersionHelpers.h>

ServiceMonitor *ServiceMonitor::thisFromCallback(void *pNotify)
{
    if(!pNotify)
        return nullptr;
    auto pNotifyStruct = reinterpret_cast<SERVICE_NOTIFY_2W*>(pNotify);
    return reinterpret_cast<ServiceMonitor*>(pNotifyStruct->pContext);
}

void CALLBACK ServiceMonitor::scmNotifyCallback(void *pNotify)
{
    auto pThis = thisFromCallback(pNotify);
    if(!pThis)
        return;

    // Pull out the notification structure - we no longer have a notification
    // requested since one was delivered.
    // Put it in a shared pointer because QMetaObject::invokeMethod() copies
    // the functor (ugh)
    std::shared_ptr<WinServiceNotify> pScmNotify{pThis->_pScmNotify.release()};

    if(!pScmNotify)
    {
        qWarning() << "Notification occurred with invalid data";
        return;
    }

    // Defer this call, since we can't call SCM APIs during this APC (see
    // doc for NotifyServiceStatusChangedW)
    QMetaObject::invokeMethod(pThis,
        [pThis, pScmNotify = std::move(pScmNotify)]
        {
            pThis->serviceCreatedDestroyed(*pScmNotify);
        }, Qt::ConnectionType::QueuedConnection);
}

ServiceMonitor::ServiceMonitor(std::wstring serviceName)
    : _manualChecksOnly{isBrokenWindows10()},
      _lastState{NetExtensionState::NotInstalled},
      _serviceName{std::move(serviceName)}
{
    checkInitialState();
    // Don't keep the SCM handle open if we're doing manual checks
    if(_manualChecksOnly)
        _scm.close();
}

ServiceMonitor::~ServiceMonitor()
{
    // In principle, this handle has to be closed before ServiceMonitor is
    // destroyed, because that ensures that the async notifications are canceled
    // before the SERVICE_NOTIFY_2W is destroyed.
    _scm.close();
}

bool ServiceMonitor::isBrokenWindows10()
{
    // NotifyServiceStatusChangeW() is broken in Windows 10 version 1507.
    // It mostly works, but if the SCM handle is open when the OS tries to
    // shut down, the SCM crashes, which causes the computer to restart.  This
    // includes Windows 10 LTSB 2015, which is still supported until 2020.
    //
    // This occurs before we receive a stop or shutdown control, so we can't try
    // to close the handle just before SCM would crash.  (We could try to close
    // it if the daemon deactivates due to having no clients, but that would not
    // work if the client crashes or is terminated, and it may race with SCM
    // shutdown anyway.)
    //
    // On broken versions of Windows 10, we won't use service installation
    // notifications, we'll just try to re-check the state ourselves when it is
    // likely to have changed (when toggling the split tunnel setting, when
    // connecting to the VPN, or when [re]installing the driver from the
    // client).  This isn't as good as proper notifications, but it's the best
    // we can do with broken APIs.
    //
    // Keep the notifications on other versions of Windows since they are more
    // reliable.

    if(!::IsWindows10OrGreater())
        return false;   // Not Win 10, SCM is fine.

    // Determine if this is a build prior to 1511.
    OSVERSIONINFOEXW versEx{};
    versEx.dwOSVersionInfoSize = sizeof(versEx);
    ::GetVersionExW(reinterpret_cast<OSVERSIONINFO*>(&versEx));
    if(versEx.dwBuildNumber < 10586)
    {
        qInfo() << "Detected Windows 10 build" << versEx.dwBuildNumber
            << "- assuming SCM notifications are broken";
        return true;
    }
    else
    {
        qInfo() << "Detected Windows 10 build" << versEx.dwBuildNumber
            << "- using SCM notifications";
        return false;
    }
}

void ServiceMonitor::checkInitialState()
{
    // Close handles if they were open.
    _scm.close();
    _pScmNotify.reset();

    // Open the SCM.
    _scm.reset(::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
    if(_scm == nullptr)
    {
        WinErrTracer error{::GetLastError()};
        qWarning() << "Unable to open SCM to monitor" << _serviceName
            << "-" << error;
        reportServiceState(NetExtensionState::Unknown);
        // At this point we're dead - we couldn't subscribe to any
        // notifications, so we'll never try to open SCM again.
        return;
    }

    // Subscribe to notifications about service creation/deletion using the SCM.
    if(!_manualChecksOnly)
    {
        auto scmNotifyResult = notifyScm();
        if(scmNotifyResult != ERROR_SUCCESS)
        {
            // There's an ERROR_SERVICE_NOTIFY_CLIENT_LAGGING code that
            // indicates we should reopen the SCM handle, but in this case we
            // just opened it - bail out rather than looping.
            WinErrTracer error{scmNotifyResult};
            qWarning() << "Unable to monitor service changes -" << error;
            _scm.close();
            _pScmNotify.reset();
            reportServiceState(NetExtensionState::Unknown);
            return;
        }
    }

    // Now check the service itself.
    if(!checkServiceState())
    {
        // As above, we're not going to reopen the SCM here since we just opened
        // it.
        qWarning() << "Cannot reopen SCM to monitor service" << _serviceName
            << "since it was just opened";
        reportServiceState(NetExtensionState::Unknown);
    }
}

bool ServiceMonitor::checkServiceState()
{
    Q_ASSERT(_scm != nullptr);  // Ensured by callers

    // Try to open the service to see if it's installed.
    ServiceHandle service{::OpenServiceW(_scm, _serviceName.c_str(), SERVICE_QUERY_STATUS)};
    if(service != nullptr)
    {
        qInfo() << "Service" << _serviceName << "is installed";
        reportServiceState(NetExtensionState::Installed);
        return true;
    }

    WinErrTracer openError{::GetLastError()};
    if(openError.code() == ERROR_SERVICE_DOES_NOT_EXIST ||
        openError.code() == ERROR_SERVICE_MARKED_FOR_DELETE)
    {
        // This is normal, service isn't installed.
        qInfo() << "Service" << _serviceName << "is not installed -"
            << openError;
        reportServiceState(NetExtensionState::NotInstalled);
        return true;
    }

    qWarning() << "Unable to open service" << _serviceName << "-"
        << openError.code() << openError.message();
    // The caller can try to reopen SCM for this failure.  If they don't, leave
    // SCM open, it may still be able to notify us about service
    // creation/deletion.
    return false;
}

DWORD ServiceMonitor::notifyScm()
{
    Q_ASSERT(_scm != nullptr);  // Ensured by caller
    Q_ASSERT(!_pScmNotify); // Ensured by caller

    _pScmNotify.reset(new WinServiceNotify{});
    _pScmNotify->dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
    _pScmNotify->pfnNotifyCallback = &scmNotifyCallback;
    _pScmNotify->pContext = reinterpret_cast<void*>(this);
    return ::NotifyServiceStatusChangeW(_scm,
        SERVICE_NOTIFY_CREATED|SERVICE_NOTIFY_DELETED, _pScmNotify.get());
}

void ServiceMonitor::serviceCreatedDestroyed(const WinServiceNotify &scmNotify)
{
    // It's possible the SCM might have been closed if we queued up both a
    // service and SCM notification, and we had to close SCM during the service
    // notification.
    if(_scm == nullptr)
    {
        qWarning() << "SCM was already closed before handling notification";
        return;
    }

    // Similarly, we might have already requested notifications again while
    // handling a service notification.
    if(_pScmNotify)
    {
        qWarning() << "Already requested SCM notifications again";
        return;
    }

    // If an error was reported, try to reinitialize.
    if(scmNotify.dwNotificationStatus != ERROR_SUCCESS)
    {
        WinErrTracer error{scmNotify.dwNotificationStatus};
        qWarning() << "Notification reported error, try to reinitialize -"
            << error;
        checkInitialState();
        return;
    }

    WinErrTracer notifyResult = notifyScm();
    if(notifyResult.code() == ERROR_SERVICE_NOTIFY_CLIENT_LAGGING)
    {
        // We were not processing notifications fast enough and need to
        // reinitialize.
        qWarning() << "Couldn't resume SCM notifications, try to reinitialize -"
            << notifyResult;
        checkInitialState();
        return;
    }
    else if(notifyResult.code() != ERROR_SUCCESS)
    {
        // Couldn't resume notifications due to some other error.
        // Do *not* try to reinitialize.  User reports show that this can happen
        // on Windows 10 version 1507 at OS shutdown, and SCM generally seems to
        // be pretty fragile, so avoid doing anything too surprising.
        qWarning() << "Couldn't resume SCM notifications, assuming that SCM is dead -"
            << notifyResult;
        _scm.close();
        qInfo() << "Closed SCM";
        reportServiceState(NetExtensionState::Unknown);
        qInfo() << "Reported unknown net extension state";
        return;
    }

    // If it's not a created or deleted notification, we don't care about it.
    if((scmNotify.dwNotificationTriggered & (SERVICE_NOTIFY_CREATED|SERVICE_NOTIFY_DELETED)) == 0)
    {
        qInfo() << "Not a create/delete notification -" << scmNotify.dwNotificationTriggered;
        return;
    }

    if(!scmNotify.pszServiceNames)
    {
        qWarning() << "Create/delete notification did not have associated service names";
        return;
    }

    // Parse the service names.  This is a MULTI_SZ - null-delimited strings
    // that are double-null-terminated
    const wchar_t *nameStart = scmNotify.pszServiceNames;
    // When nameStart points to a null character, that's the double-null at the
    // end of the list.
    while(*nameStart)
    {
        const wchar_t *nameEnd = nameStart;
        while(*nameEnd)
            ++nameEnd;

        // A slash indicates a created service
        if(*nameStart == '/')
        {
            ++nameStart;
            QStringView service{nameStart, nameEnd};
            if(service.compare(_serviceName, Qt::CaseSensitivity::CaseInsensitive) == 0)
            {
                qInfo() << "Service created:" << service;
                reportServiceState(NetExtensionState::Installed);
            }
            else
            {
                qDebug() << "Service created:" << service;
            }
        }
        else
        {
            QStringView service{nameStart, nameEnd};
            if(service.compare(_serviceName, Qt::CaseSensitivity::CaseInsensitive) == 0)
            {
                qInfo() << "Service deleted:" << service;
                reportServiceState(NetExtensionState::NotInstalled);
            }
            else
            {
                qDebug() << "Service deleted:" << service;
            }
        }
        // Advance to the next name (or to the second null of the double-null
        // terminator)
        nameStart = nameEnd + 1;
    }
}

void ServiceMonitor::reportServiceState(NetExtensionState newState)
{
    if(_lastState != newState)
    {
        _lastState = newState;
        emit serviceStateChanged(_lastState);
    }
}

void ServiceMonitor::doManualCheck()
{
    // Only do this if we're only using manual checks, rely on SCM notifications
    // otherwise.
    if(_manualChecksOnly)
    {
        qInfo() << "Recheck service state manually due to hint";
        // Since we're using manual notifications, just re-do the initial state
        // check and then close SCM.
        checkInitialState();
        _scm.close();
    }
}
