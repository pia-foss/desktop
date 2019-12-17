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

#include "tap.h"
bool TapDriverTask::_rollbackNeedsUninstall = false;
bool TapDriverTask::_rollbackNeedsReinstall = false;

#define TAP_LOG LOG
#include "tap.inl"

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
    DriverStatus status = uninstallTapDriver(false);
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
        int result = uninstallTapDriver(true);
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
retry:
    DriverStatus status = installTapDriver(getInfPath().c_str(), false, false);
    switch (status)
    {
    case DriverInstalledReboot:
        g_rebootAfterInstall = true;
        // fallthrough
    case DriverInstalled:
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
        InstallerError::raise(Abort | Retry, IDS_MB_TAPDRIVERFAILED);
        goto retry;
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
