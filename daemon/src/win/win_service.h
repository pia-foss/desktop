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

#include "common.h"
#line HEADER_FILE("win/win_service.h")

#ifndef WIN_SERVICE_H
#define WIN_SERVICE_H
#pragma once

#include "win_daemon.h"

#include <QStringList>


class WinService : public WinDaemon
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("win.service")

public:
    enum RunResult {
        SuccessfullyLaunched,
        RunningAsConsole,
        AlreadyRunning,
    };

public:
    using WinDaemon::WinDaemon;

    static WinService* instance() { return static_cast<WinService*>(WinDaemon::instance()); }

    static RunResult tryRun();

    static int installService();
    static int uninstallService();
    static int startService();
    static int stopService();

public:
    virtual void stop() override;
};

#define g_service (WinService::instance())

#endif // WIN_SERVICE_H
