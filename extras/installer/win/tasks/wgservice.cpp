// Copyright (c) 2021 Private Internet Access, Inc.
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

#include "wgservice.h"
#include "common.h"
#include "util.h"
#include "tap_inl.h"
#include "tun_inl.h"
#include "service_inl.h"
#include "brand.h"
#include "service.h"

#ifdef UNINSTALLER
void UninstallWgServiceTask::execute()
{
    LOG("Uninstalling WireGuard service");
    _listener->setCaption(IDS_CAPTION_UNREGISTERINGSERVICE);

retry:
    ServiceStatus status = uninstallService(g_wireguardServiceParams.pName);
    if (status != ServiceNotInstalled && status != ServiceRebootNeeded)
        UninstallExistingServiceTask::reinstallOnRollback();
    if (status == ServiceRebootNeeded)
        g_rebootAfterInstall = true;
    if (status == ServiceUninstallFailed || status == ServiceTimeout)
    {
        InstallerError::raise(Abort | Retry, IDS_MB_WIREGUARDUNINSTALLFAILED);
        goto retry;
    }
}

void UninstallWgServiceTask::rollback()
{
    // Handled by UninstallExistingServiceTask; invokes restored daemon's
    // "install" command to restore the services from that version.
}
#endif


#ifdef INSTALLER

void InstallWgServiceTask::execute()
{
    // Skip this task on Win 7.
    if(!isWintunSupported())
    {
        LOG("Skipping WireGuard tasks, not supported on this OS");
        return;
    }

    LOG("Installing WireGuard service");
    _listener->setCaption(IDS_CAPTION_REGISTERINGSERVICE);

retry:
    ServiceStatus status = installWireguardService(g_wgServicePath.c_str(),
                                                   g_daemonDataPath.c_str());

    switch(status)
    {
    case ServiceUpdated:
        // Like InstallServiceTask, let UninstallExistingServiceTask restore the
        // old service(s) on rollback
        UninstallExistingServiceTask::reinstallOnRollback();
        // fallthrough
    case ServiceInstalled:
        return;
    default:
    case ServiceInstallFailed:
    case ServiceUpdateFailed:
        // This error is ignorable; OpenVPN connectivity will still work.
        InstallerError::raise(Abort | Retry | Ignore, IDS_MB_WIREGUARDINSTALLFAILED);
        goto retry;
    case ServiceRebootNeeded:
        InstallerError::abort(IDS_MB_SERVICEREBOOTNEEDED);
    }
}

void InstallWgServiceTask::rollback()
{
    int result = uninstallService(g_wireguardServiceParams.pName);
    LOG("Rollback WireGuard uninstall returned %d", result);
}


#endif
