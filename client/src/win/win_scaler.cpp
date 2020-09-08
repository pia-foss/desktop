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
#line SOURCE_FILE("win/win_scaler.cpp")

#include "win_scaler.h"
#include "win_objects.h"
#include <ShellScalingApi.h>
#include <QTimer>

#ifndef WM_GETDPISCALEDSIZE
    #define WM_GETDPISCALEDSIZE 0x02E4
#endif

#pragma comment(lib, "Gdi32.lib")

MonitorScale::MonitorScale()
    : _getDpiForMonitorAddr{QStringLiteral("shcore.dll"),
                            QByteArrayLiteral("GetDpiForMonitor")}
{
}

qreal MonitorScale::getMonitorScale(HMONITOR monitor) const
{
    Q_ASSERT(monitor);  // Ensured by callers

    const qreal defaultDpi{USER_DEFAULT_SCREEN_DPI};

    // If we have the ::GetDpiForMonitor() function (Windows 8.1+), get the DPI
    // for the monitor where the window is shown.
    // (Note that there's also ::GetDpiForWindow() on Windows 10+, but it's just
    // more convenient, and we have to have this code path already for 8.1+.)
    if(_getDpiForMonitorAddr.get())
    {
        // Y is ignored, but ::GetDpiForMonitor() returns both.
        UINT dpiX{USER_DEFAULT_SCREEN_DPI}, dpiY{USER_DEFAULT_SCREEN_DPI};

        using GDPFMFunc = HRESULT (__stdcall *)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);
        GDPFMFunc pGetDpiForMonitor = reinterpret_cast<GDPFMFunc>(_getDpiForMonitorAddr.get());
        pGetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);

        return static_cast<qreal>(dpiX) / defaultDpi;
    }

    // Windows 7 doesn't support per-monitor DPI.  The only way to get the
    // system-wide setting on 7 is by through a device context.  Use a screen DC
    // because we do not necessarily have any particular window to do this with
    // here.
    HDC winDC{::GetDC(nullptr)};
    UINT systemDpi = ::GetDeviceCaps(winDC, LOGPIXELSX);
    ::ReleaseDC(nullptr, winDC);

    return static_cast<qreal>(systemDpi) / defaultDpi;
}

AWREFDFunc::AWREFDFunc()
    : _adjustWindowRectExForDpiAddr{QStringLiteral("User32.dll"),
                                    QByteArrayLiteral("AdjustWindowRectExForDpi")}
{
}

bool AWREFDFunc::funcAvailable() const
{
    return _adjustWindowRectExForDpiAddr.get();
}

void AWREFDFunc::call(RECT &windowRect, HWND window, UINT dpi) const
{
    Q_ASSERT(window);  // Ensured by callers

    DWORD style{static_cast<DWORD>(::GetWindowLongW(window, GWL_STYLE))};
    DWORD exStyle{static_cast<DWORD>(::GetWindowLongW(window, GWL_EXSTYLE))};

    if(funcAvailable())
    {
        using AWREFDFunc = BOOL (__stdcall*)(LPRECT, DWORD, BOOL, DWORD, UINT);
        AWREFDFunc pAdjWinRectExForDpi = reinterpret_cast<AWREFDFunc>(_adjustWindowRectExForDpiAddr.get());
        pAdjWinRectExForDpi(&windowRect, style, FALSE, exStyle, dpi);
    }
    else
    {
        // AdjustWindowRectEx isn't available.  Ignore the DPI and get the
        // system-wide window decoration size.
        // On Windows 7/8, this is correct because there is just one system-wide
        // DPI, the window decoration size does not change.
        // On Windows 8.1 only, this is not completely correct, because 8.1 has
        // crude support for per-monitor DPI, but does not provide any DPI-aware
        // functions that interact with system metrics.
        //
        // This fallback is only used on Windows 8.1 by WindowMaxSize, so the
        // only side effect of this is that the maximum window size may not be
        // correct on Windows 8.1 with multiple monitors *and* different DPI per
        // monitor.  It should still be close enough that the application is
        // usable though.
        ::AdjustWindowRectEx(&windowRect, style, FALSE, exStyle);
    }
}

double WinScaler::_lockedScale = 0.0;

void WinScaler::lockScale(double scale)
{
    _lockedScale = scale;
}

double WinScaler::checkScaleLock(double realScale)
{
    return _lockedScale != 0.0 ? _lockedScale : realScale;
}

WinScaler::WinScaler(QQuickWindow &window, const QSizeF &logicalSize)
    : NativeWindowScaler{window, logicalSize},
      SubclassWnd{reinterpret_cast<HWND>(window.winId())},
      _logicalSize{logicalSize}, _scale{1.0}
{
    reapplyScale();
}

qreal WinScaler::applyInitialScale()
{
    HWND winHandle = reinterpret_cast<HWND>(targetWindow().winId());
    HMONITOR windowMonitor = ::MonitorFromWindow(winHandle,
                                                 MONITOR_DEFAULTTONEAREST);
    _scale = checkScaleLock(_monitorScale.getMonitorScale(windowMonitor));

    // Apply the initial scale factor
    reapplyScale();

    return _scale;
}

void WinScaler::updateLogicalSize(const QSizeF &logicalSize)
{
    _logicalSize = logicalSize;
    reapplyScale();
}

void WinScaler::reapplyScale() const
{
    targetWindow().resize((_logicalSize * _scale).toSize());
}

LRESULT WinScaler::onGetDpiScaledSizeMsg(WPARAM wParam, LPARAM lParam)
{
    // This message was added in Windows 10 along with
    // AdjustWindowRectExForDpi(). Just bail out if we could not find this
    // function for some reason.
    if(!_awrefd.funcAvailable())
    {
        qWarning() << "Could not find AdjustWindowRectExForDpi(), WM_GETDPISCALEDSIZE will have default behavior";
        return FALSE;
    }

    // Calculate the size for the new DPI based on the logical size with the new
    // DPI and the size of the nonclient area for the new DPI.
    //
    // If we don't handle this message, Windows by default just scales our size
    // linearly.  That seems to work OK for windows that are resizeable, but it
    // does not work well for windows that are intended to have a specific size.
    //
    // The problem is that Windows attempts to choose a DPI based on the monitor
    // that the center point of the window is on, but resizing the window to
    // account for the DPI can move the window's center point to another
    // monitor.
    //
    // It appears that Windows attempts to detect this by preventing the window
    // from immediately switching back to the previous monitor in response to a
    // DPI-induced resize.  However, when the window overlaps *three* monitors,
    // that code breaks down, the resize can still cause another DPI change to
    // occur.  Windows' default handling of WM_GETDPISCALEDSIZE breaks down
    // catastrophically.
    //
    // This can even be observed in File Explorer and Settings, they both
    // exhibit strange behavior when dragged around the intersection of three
    // monitors that have at least two different DPIs.
    //
    // Calculating the size based on the desired logical size mitigates this,
    // but we can't quite work around all of Windows' bugs, because sometimes it
    // fails to send us a second WM_GETDPISCALEDSIZE during that transition.
    // Handling this message ourselves means that we recover the next time the
    // window changes DPI though; the default behavior would leave us stuck at
    // the wrong size since it just scales from the previous size.
    const qreal defaultDpi{USER_DEFAULT_SCREEN_DPI};
    UINT newDpi = static_cast<UINT>(wParam);
    qreal newScale = checkScaleLock(static_cast<qreal>(newDpi)/defaultDpi);
    QSize newClientSize = (_logicalSize * newScale).toSize();

    // Apply the nonclient area size using ::AdjustWindowRectExForDpi().
    RECT nonClientRect{0, 0, newClientSize.width(), newClientSize.height()};
    _awrefd.call(nonClientRect, getHwnd(), newDpi);

    SIZE *pNewSize = reinterpret_cast<SIZE*>(lParam);
    pNewSize->cx = nonClientRect.right - nonClientRect.left;
    pNewSize->cy = nonClientRect.bottom - nonClientRect.top;

    qInfo() << "scaling - DPI:" << newDpi << "scale:" << newScale;
    qInfo() << "log size:" << _logicalSize.width() << "x" << _logicalSize.height();
    qInfo() << "new client:" << newClientSize.width() << "x" << newClientSize.height();
    qInfo() << "new size:" << pNewSize->cx << "x" << pNewSize->cy;

    // TRUE indicates that we've written a new size and Windows should use that.
    return TRUE;
}

LRESULT WinScaler::onDpiChangedMsg(WPARAM wParam, LPARAM lParam)
{
    // Windows passes a linearly-scaled rectangle in lParam as our new size.
    // This is the window rectangle (including the nonclient area).
    //
    // We *MUST* use this exact rectangle to place the window, and the placement
    // *MUST* occur as a result of the WM_DPICHANGED message.  (We can't hand
    // over this change to the QML code to be applied, such as by updating the
    // scale and letting QML handle the window size using the scale.)
    //
    // This is the only way to ensure that during a drag, the window keeps the
    // correct position relative to the cursor, and that the resize will not
    // cause Windows to choose a different monitor's DPI if we are spanning two
    // monitors right now.  (If we don't do this exactly right, we can end up in
    // horrible DPI-change loops that effectively render the system unusable
    // since our process has captured the cursor to handle the drag.)
    //
    // Qt actually has code to apply this change itself, but for whatever reason
    // it does not do this for windows with Qt::MSWindowsFixedSizeDialogHint.
    const RECT *pNewSize = reinterpret_cast<const RECT*>(lParam);
    ::SetWindowPos(reinterpret_cast<HWND>(targetWindow().winId()), nullptr,
                   pNewSize->left, pNewSize->top,
                   pNewSize->right - pNewSize->left,
                   pNewSize->bottom - pNewSize->top,
                   SWP_NOZORDER|SWP_NOACTIVATE);

    // Capture the new scaling factor from the new DPI the system has told us
    // about.
    const qreal defaultDpi{USER_DEFAULT_SCREEN_DPI};
    // The wParam contains the X and Y DPI values in the low and high words, but
    // they're always identical (per doc).
    WORD newDpi{LOWORD(wParam)};
    _scale = checkScaleLock(static_cast<qreal>(newDpi) / defaultDpi);
    emit scaleChanged(_scale);

    qInfo() << "new dpi:" << newDpi << "rect:" << pNewSize->left << pNewSize->top
        << pNewSize->right - pNewSize->left << pNewSize->bottom - pNewSize->top;
    return 0;
}

LRESULT WinScaler::proc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_CLOSE:
            // QtQuick.Window destroys itself when the window is closed, which
            // causes problems because our subclass is then lost, and we can't
            // tell when the QWindow creates another underlying window to
            // re-subclass it.  This isn't what we want anyway, so we eat the
            // WM_CLOSE and emit it as an event that the QML code can handle.
            emit closeClicked();
            return 0;
        case WM_GETDPISCALEDSIZE:
            return onGetDpiScaledSizeMsg(wParam, lParam);
        case WM_DPICHANGED:
            return onDpiChangedMsg(wParam, lParam);
        case WM_SHOWWINDOW:
            // Windows tends to ignore the new size computed during a DPI change
            // when a window is hidden - we're computing it correctly in
            // WM_GETDPISCALEDSIZE, but it ignores it and gives us the same
            // rectangle we had in WM_DPICHANGED.  To work around this, we
            // reapply the size whenever a window is about to be shown.
            if(wParam)
                reapplyScale();
            return SubclassWnd::proc(uMsg, wParam, lParam);
        default:
            return SubclassWnd::proc(uMsg, wParam, lParam);
    }
}

WinWindowMetrics::WinWindowMetrics()
    : MessageWnd{MessageWnd::WindowType::Invisible}
{
}

double WinWindowMetrics::calcScreenScaleFactor(const PlatformScreens::Screen &screen) const
{
    // On Windows, layout occurs in physical pixels.  Although QScreen actually
    // has devicePixelRatio, it always returns 1.0 since we are not having Qt
    // apply its own DPI scaling (which is terrible).  We have to get the actual
    // DPI from the system.
    const auto &geometry = screen.geometry();
    RECT screenBound{geometry.left(), geometry.top(), geometry.right(),
                     geometry.bottom()};
    HMONITOR monitorHandle = ::MonitorFromRect(&screenBound,
                                               MONITOR_DEFAULTTONEAREST);
    return _monitorScale.getMonitorScale(monitorHandle);
}

QMarginsF WinWindowMetrics::calcDecorationSize(const QWindow &window,
                                               double screenScale) const
{
    // Note that this function ignores whether the scale is locked; this is
    // correct because the decoration still scales.

    HWND winHandle = reinterpret_cast<HWND>(window.winId());
    // If the window hasn't been created yet, we'll just assume it has no
    // decoration.
    if(!winHandle)
        return {};

    double actualDpiF = USER_DEFAULT_SCREEN_DPI * screenScale;
    int actualDpi = static_cast<int>(std::round(actualDpiF));

    // We just want the decoration size, use a default client size and adjust
    // it.
    QSize clientSize{600, 500};
    RECT clientRect{0, 0, 600, 500};
    RECT windowRect{clientRect};

    _awrefd.call(windowRect, winHandle, actualDpi);

    QMarginsF decMargins{static_cast<double>(clientRect.left - windowRect.left),
                         static_cast<double>(clientRect.top - windowRect.top),
                         static_cast<double>(windowRect.right - clientRect.right),
                         static_cast<double>(windowRect.bottom - clientRect.bottom)};
    // Divide by the scale factor to get the logical size
    decMargins /= screenScale;
    return decMargins;
}

LRESULT WinWindowMetrics::proc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(uMsg == WM_DISPLAYCHANGE)
        emit displayChanged();
    return MessageWnd::proc(uMsg, wParam, lParam);
}
