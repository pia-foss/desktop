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
#line HEADER_FILE("win/win_console.h")

#ifndef WIN_CONSOLE_H
#define WIN_CONSOLE_H
#pragma once

#include "win_daemon.h"


class WinConsole : public QObject, public Singleton<WinConsole>
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("win.console")

public:
    explicit WinConsole(QObject* parent = nullptr);

    static int installTapDriver(bool force = false);
    static int uninstallTapDriver();
    static int reinstallTapDriver();
    static int installCalloutDriver();
    static int uninstallCalloutDriver();
    static int reinstallCalloutDriver();
    static int showHelp();

    int run();

public slots:
    void stopDaemon();

private:
    int runDaemon();

    WinDaemon* _daemon;
    QStringList _arguments;
};

#define g_console (WinConsole::instance())

#endif // WIN_CONSOLE_H
