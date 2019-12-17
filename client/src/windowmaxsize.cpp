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

#include "common.h"
#line SOURCE_FILE("windowmaxsize.cpp")

#include "windowmaxsize.h"
#include <QGuiApplication>

#if defined(Q_OS_MAC)
#include "mac/mac_window.h"
#elif defined(Q_OS_WIN)
#include "win/win_scaler.h"
#elif defined(Q_OS_LINUX)
#include "linux/linux_scaler.h"
#endif

namespace
{
    // We don't really want to size windows exactly to the work area size, even
    // if they fit, because you'd have to drag the window to the exact top of
    // the work area in order to see the whole thing.  (This is especially true
    // for the dashboard, since the expand button is at the very bottom.).
    //
    // This value adds some vertical padding; there's always at least 30 px of
    // free space around the top/bottom of the window.  This causes windows to
    // scroll slightly sooner than they would have to otherwise.
    //
    // This value is in logical pixels.
    const double maxSizeVertAllowance = 30.0;
}

std::unique_ptr<NativeWindowMetrics> createNativeWindowMetrics()
{
#if defined(Q_OS_MAC)
    return macCreateWindowMetrics();
#elif defined(Q_OS_WIN)
    return std::unique_ptr<NativeWindowMetrics>{new WinWindowMetrics{}};
#elif defined(Q_OS_LINUX)
    return std::unique_ptr<NativeWindowMetrics>{new LinuxWindowMetrics{}};
#else
    #error "No NativeWindowMetrics for this platform"
#endif
}

WindowMaxSize::WindowMaxSize(QObject *pParent)
    : QObject{pParent}, _pWindow{nullptr}
{
    Q_ASSERT(qGuiApp);  // Can't create these before the app is created

    _pWindowMetrics = createNativeWindowMetrics();

    // Update the effective size if the workspace changes or if any native
    // metrics change
    connect(&_workspaceChange, &WorkspaceChange::workspaceChanged, this,
            &WindowMaxSize::updateEffectiveSize);
    connect(_pWindowMetrics.get(), &NativeWindowMetrics::displayChanged, this,
            &WindowMaxSize::updateEffectiveSize);
}

QSizeF WindowMaxSize::calcDecorationSize(double screenScale) const
{
    if(!_pWindow)
        return {0, 0};

    QMarginsF decMargins = _pWindowMetrics->calcDecorationSize(*_pWindow, screenScale);
    return QSizeF{decMargins.left() + decMargins.right(),
                  decMargins.top() + decMargins.bottom()};
}

QSizeF WindowMaxSize::calcEffectiveSize() const
{
    // For any properties of QScreen that we use here, we need to connect to the
    // corresponding signals in WorkspaceChange::connectScreen().

    QSizeF maxWorkSize{0, 0};
    // The maximum screen size is chosen by the visible area of the window.
    double maxVisibleArea = 0.0;

    for(auto *pScreen : qGuiApp->screens())
    {
        if(!pScreen)
            continue;

        // Calculate the logical work size for this screen using the work area
        // and scale factor.
        QSizeF workSize = pScreen->availableSize();
        double screenScale = _pWindowMetrics->calcScreenScaleFactor(*pScreen);
        workSize /= screenScale;

        // Remove the size of the window decoration.
        QSizeF decorationSize = calcDecorationSize(screenScale);
        workSize -= decorationSize;

        // Also remove the vertical allowance.
        workSize.rheight() -= maxSizeVertAllowance;

        // Limit to the preferred size - both for the max visible area
        // computation, and so we do not return a size larger than the preferred
        // size.
        workSize.setWidth(std::min(workSize.width(), _preferredSize.width()));
        workSize.setHeight(std::min(workSize.height(), _preferredSize.height()));

        // Calculate the area of the window that would be visible
        double visibleArea = workSize.width() * workSize.height();
        if(visibleArea > maxVisibleArea)
        {
            maxVisibleArea = visibleArea;
            maxWorkSize = workSize;
        }
    }

    return maxWorkSize;
}

void WindowMaxSize::updateEffectiveSize()
{
    QSizeF newSize = calcEffectiveSize();
    if(newSize != _effectiveSize)
    {
        _effectiveSize = newSize;
        emit effectiveSizeChanged();
    }
}

void WindowMaxSize::setWindow(QWindow *pWindow)
{
    if(pWindow == _pWindow)
        return;
    _pWindow = pWindow;
    emit windowChanged();
    updateEffectiveSize();
}

void WindowMaxSize::setPreferredSize(const QSizeF &preferredSize)
{
    if(preferredSize == _preferredSize)
        return;
    _preferredSize = preferredSize;
    emit preferredSizeChanged();
    updateEffectiveSize();
}
