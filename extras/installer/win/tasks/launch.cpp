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

#include "launch.h"
#include "safemode_inl.h"
#include "util.h"

void LaunchClientTask::execute()
{
    if(getBootMode() == BootMode::Normal)
    {
        // Launch the client non-elevated
        launchProgramAsDesktopUser(g_clientPath, {L"--clear-cache"});
    }
    else
    {
        // Can't start the service in safe mode, so just indicate that the
        // user needs to restart.
        messageBox(IDS_MB_RESTARTTOUSE, IDS_MB_CAP_INSTALLCOMPLETE, 0, MB_ICONINFORMATION, IDOK);
    }
}
