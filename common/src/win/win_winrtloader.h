// Copyright (c) 2023 Private Internet Access, Inc.
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

#include "../common.h"
#line HEADER_FILE("win_winrtloader.h")

#ifndef WIN_WINRTLOADER_H
#define WIN_WINRTLOADER_H

#include "../../extras/winrtsupport/src/winrtsupport.h"
#include "../thread.h"
#include "win_util.h"

// UWP apps are identified in split tunnel rules with a "file path" of the form
// `uwp:<family_id>`.  (UWP apps don't really have a meaningful file path that
// we could persist.)
extern COMMON_EXPORT const QString uwpPathPrefix;

// Loader for WinRT support DLL.  Loads entry points only if this is Win10+.
// Safe to use from any thread after initialization (immutable).
// When running on any Windows version prior to 10, all methods are stubbed.
class COMMON_EXPORT WinRTSupport
{
public:
    WinRTSupport();

public:
    // WinRT must be initialized on any thread it's used on.  It _cannot_
    // be used from the app main thread, because the main thread
    // initializes COM with STA threading as required by Qt, and the async
    // WinRT objects used require MTA.
    void initWinRt() const;

    std::vector<EnumeratedUwpApp> getUwpApps() const;
    QString loadAppDisplayName(const QString &family) const;
    std::vector<std::uint8_t> loadAppIcon(const QString &family,
                                          float width, float height) const;
    std::vector<QString> adminGetInstallDirs(const QString &family) const;

private:
    const WinRtSupportEntryPoints *_pEntryPoints;
    // Though we don't call getWinRtEntryPoints() after initialization, we do
    // need to keep this around to keep the module loaded.
    ProcAddress _getWinRtEntryPointsAddr;
};

// winRtSupport can be used directly as long as the caller calls initWinRt() on
// the appropriate thread.  It's initialized on first use.
COMMON_EXPORT const WinRTSupport &getWinRtSupport();

// WinRT data loader; synchronously dispatches to an MTA worker thread for
// either icon or display name loads.
// This is used by the client's icon/display name providers, which are invoked
// from the main thread and share this worker thread.  (The app scanner uses its
// own worker thread since it is asynchronous.)
class COMMON_EXPORT WinRTLoader
{
public:
    WinRTLoader();

public:
    QString loadAppDisplayName(const QString &family);
    std::vector<std::uint8_t> loadAppIcon(const QString &family, float width,
                                          float height);
    std::vector<QString> adminGetInstallDirs(const QString &family);

private:
    // UWP app icons are loaded on a shared MTA worker thread.
    // COM is initialized with a single-threaded apartment on the main
    // thread; the asynchronous WinRT components can't be used on this
    // thread.  (Loads are still synchronous.)
    RunningWorkerThread _mtaWorkerThread;
};

// The WinRTLoader worker thread is initialized when first used.
COMMON_EXPORT WinRTLoader &getWinRtLoader();

#endif
