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

#include "callout.h"
#include "../tap_inl.h"

std::wstring CalloutDriverTask::getInfPath()
{
    return g_installPath + (IsWindows10OrGreater() ? L"\\wfp_callout\\win10\\PiaWfpCallout.inf" : L"\\wfp_callout\\win7\\PiaWfpCallout.inf");
}

#ifdef UNINSTALLER

void UninstallCalloutDriverTask::execute()
{
    LOG("Uninstalling callout driver");
    _listener->setCaption(IDS_CAPTION_REMOVINGCALLOUT);

    bool retry = true;
    while(retry)
    {
        retry = false;

        DriverStatus status = uninstallCalloutDriver(getInfPath().c_str(), false);
        if(status == DriverStatus::DriverUninstallFailed &&
            Retry == InstallerError::raise(Abort | Retry | Ignore, IDS_MB_ERRORUNINSTALLINGCALLOUT))
        {
            retry = true;
        }
    }
}

void UninstallCalloutDriverTask::rollback()
{
}

#endif

#ifdef INSTALLER

void UpdateCalloutDriverTask::execute()
{
    LOG("Checking for callout driver update");

    // Stop the service if it is installed so we can update it.
    auto stopResult = stopCalloutDriver();
    switch(stopResult)
    {
        case ServiceStatus::ServiceNotInstalled:
            // Nothing to update
            LOG("Callout driver is not installed, nothing to update");
            return;
        case ServiceStatus::ServiceStopFailed:
            // This result is used if we can't open SCM or the service, or if
            // the service is installed but can't be stopped.
            // Leave the driver alone since we can't stop it.
            LOG("Could not stop callout driver, not checking for update");
            return;
        case ServiceStatus::ServiceStopped:
        case ServiceStatus::ServiceAlreadyStopped:
            LOG("Callout driver is stopped, updating");
            break;
        default:
            LOG("Could not stop callout driver, skipping update (%d)", stopResult);
            return;
    }

    // Unlike the TAP adapter, the user can choose to ignore a failure to update
    // the callout driver, the daemon/driver are designed to be compatible
    // (with degraded functionality if necessary).  Updating usually requires a
    // reboot, so this is normal anyway.
    bool retry = true;
    while(retry)
    {
        retry = false;
        DriverStatus status = installCalloutDriver(getInfPath().c_str(), false);
        switch(status)
        {
            case DriverInstalledReboot:
                // Don't set g_rebootAfterInstall because the app can still be
                // used (potentially with degraded functionality), a reboot
                // prompt appears in-app if necessary.
                // (This reboot requirement is also relatively common so we try
                // to reduce its impact.)
                break;
            case DriverInstalled:
                break;
            default:
            case DriverInstallFailed:
                if(Retry == InstallerError::raise(Abort | Retry | Ignore, IDS_MB_CALLOUTDRIVERFAILED))
                    retry = true;
                break;
        }
    }
}

void UpdateCalloutDriverTask::rollback()
{
}

#endif
