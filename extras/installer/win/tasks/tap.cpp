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

#include "tap.h"
#include "tap_inl.h"

bool TapDriverTask::_rollbackNeedsUninstall = false;
bool TapDriverTask::_rollbackNeedsReinstall = false;

std::wstring TapDriverTask::getInfPath()
{
    return g_installPath + (IsWindows10OrGreater() ? L"\\tap\\win10\\OemVista.inf" : L"\\tap\\win7\\OemVista.inf");
}

void UninstallTapDriverTask::execute()
{
#ifdef UNINSTALLER
    LOG("Uninstalling TAP driver");
    _listener->setCaption(IDS_CAPTION_REMOVINGADAPTER);

retry:
    DriverStatus status = uninstallTapDriver(false, false);
    switch (status)
    {
    case DriverUninstalled:
        _rollbackNeedsReinstall = true;
        return;
    case DriverNothingToUninstall:
        return;
    default:
    case DriverUninstallFailed:
        if (Retry == InstallerError::raise(Abort | Retry | Ignore, IDS_MB_ERRORUNINSTALLINGTAPDRIVER))
            goto retry;
        // Ignored
        _rollbackNeedsReinstall = true;
        break;
    }
#endif
}

void UninstallTapDriverTask::rollback()
{
    if (_rollbackNeedsUninstall)
    {
        int result = uninstallTapDriver(true, false);
        LOG("Rollback TAP uninstall returned %d", result);
    }
    else if (_rollbackNeedsReinstall)
    {
        int result = installTapDriver(getInfPath().c_str(), false, true);
        LOG("Rollback TAP reinstall returned %d", result);
    }
}

#ifdef INSTALLER

void InstallTapDriverTask::execute()
{
    LOG("Installing TAP driver");
    _listener->setCaption(IDS_CAPTION_INSTALLINGADAPTER);

retry_uninstall:
    // If a different version of the TAP adapter is installed, uninstall it
    // before installing the new one.
    //
    // Historically, there have been rare cases where the new driver could not
    // update the old one, but a clean install works.  (Updating from 9.23.3 to
    // 9.24.2 on Windows 10 1507 / LTSB 2015 had this issue.)
    DriverStatus uninstallStatus = uninstallTapDriver(true, true);
    switch(uninstallStatus)
    {
    case DriverUninstalled:
        _rollbackNeedsReinstall = true;
        break;
    case DriverNothingToUninstall:
        break;
    default:
    case DriverUninstallFailed:
        if (Retry == InstallerError::raise(Abort | Retry | Ignore, IDS_MB_ERRORUNINSTALLINGTAPDRIVER))
            goto retry_uninstall;
        // If this failure is ignored, we'll still try to update the TAP adapter
        break;
    }

retry:
    DriverStatus status = installTapDriver(getInfPath().c_str(), false, false);
    switch (status)
    {
    case DriverInstalledReboot:
        g_rebootAfterInstall = true;
        // fallthrough
    case DriverInstalled:
        // We might have already set the reinstall flag for rollback flag if we
        // successfully uninstalled an older driver above.
        if(!_rollbackNeedsReinstall)
            _rollbackNeedsUninstall = true;
        return;
    case DriverUpdatedReboot:
        g_rebootAfterInstall = true;
        // fallthrough
    case DriverUpdated:
        _rollbackNeedsReinstall = true;
        return;
    case DriverUpdateNotNeeded:
        return;
    case DriverUpdateDisallowed:
    case DriverInstallDisallowed:
        InstallerError::raise(Abort | Retry, IDS_MB_TAPDRIVERDECLINED);
        goto retry;
    default:
    case DriverUpdateFailed:
    case DriverInstallFailed:
        // The user can ignore a TAP installation error - the client handles
        // this gracefully since the TAP adapter is known to sometimes disappear
        // during Windows updates anyway.  Historically, there have been rare
        // cases where the TAP adapter would not install during installation,
        // but reinstalling from the client after installation would work, so
        // permit that as a possible workaround.
        if(Retry == InstallerError::raise(Abort | Retry | Ignore, IDS_MB_TAPDRIVERFAILED))
            goto retry;
        // Ignored
        if(status == DriverUpdateFailed)
            _rollbackNeedsReinstall = true;
        else if(!_rollbackNeedsReinstall)
            _rollbackNeedsUninstall = true;
        break;
    }
}

void InstallTapDriverTask::rollback()
{
    // Actual rollback performed in UninstallTapDriverTask::rollback
}

#endif // INSTALLER

#ifdef UNINSTALLER

void CleanupTapDriverTask::execute()
{
    uninstallTapDriverInf();
}

#endif // UNINSTALLER
