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
#line SOURCE_FILE("win/win_main.cpp")

#if defined(PIA_CLIENT) || defined(UNIT_TEST)

// Entry point shouldn't be included for these projects
void dummyWinMain() {}

#else

#include "win_console.h"
#include "win_service.h"
#include "path.h"
#include "win.h"

#include <QTextStream>



int main(int argc, char** argv)
{
    setUtf8LocaleCodec();
    Logger::initialize(true);

    FUNCTION_LOGGING_CATEGORY("win.main");

    if (HRESULT error = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))
        qFatal("CoInitializeEx failed with error 0x%08x.", error);
    if (HRESULT error = ::CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_PKT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL))
        qFatal("CoInitializeSecurity failed with error 0x%08x.", error);

    switch (WinService::tryRun())
    {
    case WinService::SuccessfullyLaunched:
        return 0;
    case WinService::AlreadyRunning:
        qCritical("Service is already running");
        return 1;
    case WinService::RunningAsConsole:
        try
        {
            Path::initializePreApp();
            QCoreApplication app(argc, argv);
            Path::initializePostApp();
            return WinConsole().run();
        }
        catch (const Error& error)
        {
            qCritical(error);
            return 1;
        }
    }
}

#endif
