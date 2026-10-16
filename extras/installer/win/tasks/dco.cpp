// Copyright (c) 2026 Private Internet Access, Inc.
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

#include "dco.h"
#include "../dco_inl.h"
#include "../service_inl.h"
#include <windows.h>
#define WIN32_LEAN_AND_MEAN
bool DcoDriverTask::_rollbackNeedsUninstall = false;
bool DcoDriverTask::_rollbackNeedsReinstall = false;

std::wstring DcoDriverTask::getInfPath()
{
    std::wstring subdir = IsWindows11() ? L"win11" : L"win10";
    LOG("Using %ls for DCO driver installation", subdir.c_str());
    return g_installPath + L"\\dco\\" + subdir + L"\\" DCO_DEVICE_NAME L".inf";
}

std::wstring DcoDriverTask::getTapInstallPath()
{
    return g_installPath + L"\\tapinstall.exe";
}

void UninstallDcoDriverTask::execute()
{
#ifdef UNINSTALLER
    LOG("Uninstalling Dco driver");
    _listener->setCaption(IDS_CAPTION_REMOVINGDCO);

retry:
    std::wstring deviceName = DCO_DEVICE_NAME;
    DriverStatus status =
        uninstallDcoDriver(getInfPath().c_str(), deviceName.c_str(), getTapInstallPath().c_str());
    switch(status)
    {
        case DriverUninstalledReboot:
            // The removal completes on the next reboot; that's still a
            // successful uninstall as far as this task is concerned.
            g_rebootAfterInstall = true;
            // fallthrough
        case DriverUninstalled: _rollbackNeedsReinstall = true; return;
        default:
        case DriverUninstallFailed:
            if(Retry == InstallerError::raise(Abort | Retry | Ignore, IDS_MB_ERRORUNINSTALLINGDCO))
            {
                goto retry;
            }
            // Ignored
            _rollbackNeedsReinstall = true;
            break;
    }
#endif
}

void UninstallDcoDriverTask::rollback()
{
    std::wstring deviceName = DCO_DEVICE_NAME;
    if(_rollbackNeedsUninstall)
    {
        DriverStatus result = uninstallDcoDriver(getInfPath().c_str(), deviceName.c_str(),
                                                 getTapInstallPath().c_str());
        LOG("Rollback Dco uninstall returned %d", result);
    }
    else if(_rollbackNeedsReinstall)
    {
        DriverStatus result =
            installDcoDriver(getInfPath().c_str(), deviceName.c_str(), getTapInstallPath().c_str());
        LOG("Rollback Dco reinstall returned %d", result);
    }
}

#ifdef INSTALLER

void InstallDcoDriverTask::execute()
{
    LOG("Installing Dco driver");
    _listener->setCaption(IDS_CAPTION_INSTALLINGADAPTER);

    std::wstring deviceName = DCO_DEVICE_NAME;

    // Only replace a driver that's actually there.  tapinstall "remove" exits 0
    // whether or not it matched anything, so without this probe a clean install
    // would look like a replacement and set _rollbackNeedsReinstall - an
    // aborted installation would then put the driver back instead of removing
    // it, leaving a stray network adapter behind.
    if(dcoDeviceInstalled())
    {
    retry_uninstall:
        DriverStatus uninstallStatus =
            uninstallDcoDriver(getInfPath().c_str(), deviceName.c_str(), getTapInstallPath().c_str());
        switch(uninstallStatus)
        {
            case DriverUninstalledReboot:
                // The removal only completes on the next reboot, but nothing
                // reads g_rebootAfterInstall, so the install can't be deferred
                // to a later run - that would leave the user with no DCO driver
                // and no prompt.  Try to install now; if the driver file is
                // still in use, the install fails below and reports it.
                g_rebootAfterInstall = true;
                // fallthrough
            case DriverUninstalled:
                _rollbackNeedsReinstall = true;
                LOG("Uninstalled previous Dco adapter driver, continue to install new driver");
                break;
            default:
            case DriverUninstallFailed:
                if(Retry ==
                   InstallerError::raise(Abort | Retry | Ignore, IDS_MB_ERRORUNINSTALLINGDCO))
                {
                    goto retry_uninstall;
                }
                // If this failure is ignored, we'll still try to install the
                // new Dco adapter driver
                break;
        }
    }
    else
    {
        LOG("No Dco adapter driver installed yet, install it now");
    }

retry:
    DriverStatus status =
        installDcoDriver(getInfPath().c_str(), deviceName.c_str(), getTapInstallPath().c_str());
    switch(status)
    {
        case DriverInstalledReboot:
            g_rebootAfterInstall = true;
            // fallthrough
        case DriverInstalled:
            // We might have already set the reinstall flag for rollback flag if we
            // successfully uninstalled an older driver above.
            if(!_rollbackNeedsReinstall)
            {
                _rollbackNeedsUninstall = true;
            }
            return;
        default:
        case DriverInstallFailed:
            // tapinstall.exe reports any failure the same way, including the
            // user declining the driver signature prompt, so this is the only
            // failure dialog we can show.
            if(Retry == InstallerError::raise(Abort | Retry | Ignore, IDS_MB_DCODRIVERFAILED))
            {
                goto retry;
            }
            // Ignored
            if(!_rollbackNeedsReinstall)
            {
                _rollbackNeedsUninstall = true;
            }
            break;
    }
}

void InstallDcoDriverTask::rollback()
{
    // Actual rollback performed in UninstallDcoDriverTask::rollback
}

#endif // INSTALLER

#ifdef UNINSTALLER

void CleanupDcoDriverTask::execute()
{
    uninstallDriverInf(L"" DCO_DEVICE_NAME);
}

#endif // UNINSTALLER
