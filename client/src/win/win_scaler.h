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

#include <common/src/common.h>
#line HEADER_FILE("win/win_scaler.h")

#ifndef WIN_SCALER_H
#define WIN_SCALER_H

#include "win_objects.h"
#include "win_subclasswnd.h"
#include "../nativewindowscaler.h"
#include "../windowmaxsize.h"
#include <common/src/win/win_messagewnd.h>
#include <common/src/win/win_util.h>

// MonitorScale queries for the scale factor to use when rendering to a given
// monitor.
//
// This is a class instead of a free function because it probes for
// ::GetDpiForMonitor() at runtime, meaning it has to hold open an HMODULE to
// shcore.dll.  It wouldn't be very efficient to load/unload that module every
// time a monitor's DPI is queried; this object holds it open.
class MonitorScale
{
public:
    MonitorScale();

public:
    // Get the scale factor for a monitor (the monitor's DPI divided by 96.0).
    // 'monitor' must be a valid monitor handle.
    qreal getMonitorScale(HMONITOR monitor) const;

private:
    ProcAddress _getDpiForMonitorAddr;
};

// Encapsulates AdjustWindowRectExForDpi(), which is loaded dynamically with
// GetProcAddress(), and calls it with a window's actual style.
class AWREFDFunc
{
public:
    AWREFDFunc();

public:
    // Indicates whether AdjustWindowRectExForDpi() is actually available on
    // this platform.
    bool funcAvailable() const;

    // Call AdjustWindowRectExForDpi() using the given window's style (and no
    // menu).  If AdjustWindowRectExForDpi() isn't available, this ignores the
    // DPI and calls AdjustWindowRectEx() instead.
    void call(RECT &windowRect, HWND window, UINT dpi) const;

private:
    ProcAddress _adjustWindowRectExForDpiAddr;
};

// WinScaler is an implementaiton of NativeWindowScaler for Windows.
//
// On Windows 8.1+, it supports per-monitor DPI using the WM_DPICHANGED message
// sent to the target window.  It finds the initial DPI by finding the monitor
// that window is on and querying its DPI.
//
// When the window DPI changes, WinScaler calculates the new size from the
// desired logical size.  This is critical to ensure that no error occurs in the
// window size due to Windows' default linear scaling, which doesn't know the
// actual desired logical size.
//
// On older versions of Windows, which do not have per-monitor DPI support, it
// reads the system DPI at startup and uses that value.
class WinScaler : public NativeWindowScaler, private SubclassWnd
{
    CLASS_LOGGING_CATEGORY("winscaler")

private:
    static double _lockedScale;

public:
    // Lock scaling to 100% - do not scale up with DPI.  Call this before
    // creating any WinScaler objects.
    //
    // Qt's software-rendered backend on Windows has a handful of issues when
    // rendering inside a scale transformation.
    //
    // The worst issue is that list view items can show one-pixel gaps between
    // them, this seems to be due to round-off errors when painting individual
    // list items, and no good workaround has been found to fix this.
    //
    // Rounded rectangles also pixelate when scaled up with the software
    // backend, although this doesn't really look quite as bad as it sounds.
    //
    // Hopefully, the software backend *and* high-DPI displays are a pretty
    // uncommon combination.  Software rendering a 4K framebuffer is pretty
    // unbearably slow, so it stands to reason that this probably isn't a very
    // common combination.
    //
    // We could probably fix this by scaling all of the UI elements for the
    // current scale factor instead of wrapping them in an Item with a scale
    // transformation, but that'll take a pretty significant overhaul since
    // there are so many hard-coded metrics in there right now.
    static void lockScale(double scale = 1.0);
    // Check the scale lock - returns the passed 'real' scale factor if it's not
    // locked, or the locked scale if it is.
    // Also used by NativeTrayWin, because it also has to compute a scale factor
    // for the monitor that the tray icon is on
    static double checkScaleLock(double realScale);

public:
    WinScaler(QQuickWindow &window, const QSizeF &logicalSize);

public:
    virtual qreal applyInitialScale() override;
    virtual void updateLogicalSize(const QSizeF &logicalSize) override;

private:
    void reapplyScale() const;
    LRESULT onGetDpiScaledSizeMsg(WPARAM wParam, LPARAM lParam);
    LRESULT onDpiChangedMsg(WPARAM wParam, LPARAM lParam);
    virtual LRESULT proc(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    QSizeF _logicalSize;
    qreal _scale;
    MonitorScale _monitorScale;
    AWREFDFunc _awrefd;
};

// Windows parts of maximum-size algorithm.
// Monitors for WM_DISPLAYCHANGE, which is sent when the virtual screen layout
// changes, including for screen resolution changes, DPI changes, monitor
// enable/disable, etc.
class WinWindowMetrics : public NativeWindowMetrics, private MessageWnd
{
    Q_OBJECT

public:
    WinWindowMetrics();

public:
    virtual double calcScreenScaleFactor(const PlatformScreens::Screen &screen) const override;
    virtual QMarginsF calcDecorationSize(const QWindow &window,
                                         double screenScale) const override;

private:
    virtual LRESULT proc(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    AWREFDFunc _awrefd;
    MonitorScale _monitorScale;
};

#endif
