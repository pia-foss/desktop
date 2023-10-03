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

#include "service.h"
#include "brand.h"
#include "../service_inl.h"
#include "../safemode_inl.h"

bool ServiceTask::_rollbackNeedsReinstall = false;

void ServiceTask::reinstallOnRollback()
{
    _rollbackNeedsReinstall = true;
}

StopExistingServiceTask::StopExistingServiceTask(LPCWSTR pSvcName, bool restartOnRollback)
    : _pSvcName{pSvcName}, _restartOnRollback{restartOnRollback},
      _rollbackNeedsStart{false}
{
}

void StopExistingServiceTask::execute()
{
    LOG("Stopping service");
    _listener->setCaption(IDS_CAPTION_STOPPINGSERVICE);

retry:
    ServiceStatus status = stopService(_pSvcName);
    switch (status)
    {
    case ServiceStopped:
        if(_restartOnRollback)
            _rollbackNeedsStart = true;
        // fallthrough
    case ServiceAlreadyStopped:
    case ServiceNotInstalled:
        return;
    default:
    case ServiceStopFailed:
        InstallerError::raise(Abort | Retry, IDS_MB_FAILEDTOSTOPSERVICE);
        goto retry;
    case ServiceRebootNeeded:
        InstallerError::abort(IDS_MB_SERVICEREBOOTNEEDED);
    }
}

void StopExistingServiceTask::rollback()
{
    if (_rollbackNeedsStart)
    {
        if (PathFileExists(g_servicePath.c_str()))
        {
            int result = runProgram(g_servicePath, { L"start" });
            LOG("Rollback service start returned %d", result);
        }
        else
            LOG("Rollback service start not possible");
    }
}

void UninstallExistingServiceTask::execute()
{
#ifdef UNINSTALLER
    LOG("Uninstalling service");
    _listener->setCaption(IDS_CAPTION_UNREGISTERINGSERVICE);

retry:
    ServiceStatus status = uninstallService(g_daemonServiceParams.pName);
    if (status != ServiceNotInstalled && status != ServiceRebootNeeded)
        _rollbackNeedsReinstall = true;
    if (status == ServiceRebootNeeded)
        g_rebootAfterInstall = true;
    if (status == ServiceUninstallFailed || status == ServiceTimeout)
    {
        InstallerError::raise(Abort | Retry, IDS_MB_SERVICEUNINSTALLFAILED);
        goto retry;
    }
#endif
}

void UninstallExistingServiceTask::rollback()
{
    if (_rollbackNeedsReinstall)
    {
        // Run the service installer from the existing installation, if it exists
        if (PathFileExists(g_servicePath.c_str()))
        {
            int result = runProgram(g_servicePath, { L"install" });
            LOG("Rollback service install returned %d", result);
        }
        else
            LOG("Rollback service install not possible");
    }
}

#ifdef INSTALLER

void InstallServiceTask::execute()
{
    LOG("Installing service");
    _listener->setCaption(IDS_CAPTION_REGISTERINGSERVICE);

retry:
    if (!PathFileExists(g_servicePath.c_str()))
        InstallerError::abort(IDS_MB_SERVICEMISSING);

    ServiceStatus status = installDaemonService(g_servicePath.c_str());
    switch (status)
    {
    case ServiceUpdated:
        // An existing service was updated.  If we roll this back, it occurs in
        // two parts:
        // - InstallServiceTask::rollback() removes the updated service
        // - UninstallExistingServiceTask invokes the restored daemon's
        //   "install" command to restore the old installation's services (which
        //   may differ from this version)
        _rollbackNeedsReinstall = true;
        // fallthrough
    case ServiceInstalled:
        return;
    default:
    case ServiceInstallFailed:
    case ServiceUpdateFailed:
        InstallerError::raise(Abort | Retry, IDS_MB_SERVICEINSTALLFAILED);
        goto retry;
    case ServiceRebootNeeded:
        InstallerError::abort(IDS_MB_SERVICEREBOOTNEEDED);
    }
}

void InstallServiceTask::rollback()
{
    int result = uninstallService(g_daemonServiceParams.pName);
    LOG("Rollback service uninstall returned %d", result);
}

void StartInstalledServiceTask::execute()
{
    LOG("Starting service");
    _listener->setCaption(IDS_CAPTION_STARTINGSERVICE);

    // Skip in safe mode - service can't be started.
    if(getBootMode() != BootMode::Normal)
    {
        LOG("Can't start service in safe mode; skip");
        return;
    }

retry:
    ServiceStatus status = startService(g_daemonServiceParams.pName);
    switch (status)
    {
    case ServiceStarted:
    case ServiceAlreadyStarted:
        return;
    default:
    case ServiceStartFailed:
        if (Retry == InstallerError::raise(Abort | Retry | Ignore, IDS_MB_SERVICEFAILEDTOSTART))
            goto retry;
        // Ignored
        return;
    case ServiceRebootNeeded:
        InstallerError::abort(IDS_MB_SERVICEREBOOTNEEDED);
    }
}

void StartInstalledServiceTask::rollback()
{
    int result = stopService(g_daemonServiceParams.pName);
    LOG("Rollback service stop returned %d", result);
}

#endif // INSTALLER
