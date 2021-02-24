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
#line HEADER_FILE("win_appscanner.h")

#ifndef WIN_APPSCANNER_H
#define WIN_APPSCANNER_H

#include "../appscanner.h"
#include "../../extras/winrtsupport/src/winrtsupport.h"
#include "../thread.h"

class WinAppScanner : public AppScanner
{
    Q_OBJECT

private:
    // Scan for native and UWP apps on a worker thread.  The COM APIs used to do
    // this take a measurable amount of time, so this is done on a worker thread
    // asynchronously.  (It can't access any members of WinAppScanner, which is
    // why it's static.)
    static void scanOnThread(WinAppScanner *pScanner);

public:
    WinAppScanner();

private:
    // Finish the app scan on the main thread.  This receives the results from
    // the asynchronous app scan.  A daemon RPC must be made to inspect the UWP
    // apps and add them, which must occur on the main thread.
    void completeScan(QJsonArray nativeApps,
                      std::vector<EnumeratedUwpApp> uwpApps);

public:
    virtual void scanApplications() override;

private:
    RunningWorkerThread _workerThread;
};

std::unique_ptr<QQuickImageProvider> createWinAppIconProvider();
QString getWinAppName(const QString &path);

#endif
