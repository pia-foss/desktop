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

#include "common.h"
#line HEADER_FILE("windowmaxsize.h")

#ifndef WINDOWMAXSIZE_H
#define WINDOWMAXSIZE_H

#include <QWindow>
#include <QQuickWindow>
#include "platformscreens.h"
#include "workspacechange.h"

// Platform dependent parts of this algorithm
class NativeWindowMetrics : public QObject
{
    Q_OBJECT

public:
    // Determine a screen's logical scale factor.  (This logic is
    // platform-dependent since QScreen doesn't directly indicate this.)
    virtual double calcScreenScaleFactor(const PlatformScreens::Screen &screen) const = 0;
    // Calculate the logical size of a window decoration on one possible screen,
    // which will be removed from the work area size.  The given window's style
    // is used to determine this size, but its size/position/etc. don't affect
    // the returned value.
    virtual QMarginsF calcDecorationSize(const QWindow &window,
                                         double screenScale) const = 0;

signals:
    // Emit this signal if the information calculated above might have changed.
    void displayChanged();
};

// WindowMaxSize tells a window the maximum size that it should use for its
// client area.  The size is determined from the screen's work area, scale
// factor, and the size of a window decoration on that screen (given the
// specified window's style).
//
// For multiple-monitor systems, this is the largest such size among all
// screens.  This ensures that there is at least one screen where the whole
// window is visible, and that the screen won't scroll unnecessarily on a large
// display.  This does mean that if the user has multiple displays of different
// (small) sizes, there may be some displays where the whole window is not
// visible, but solving that problem in general would require resizing the
// window on different displays (which can easily cause binding loops or even
// freeze up the window manager on Windows).  The current fixed-size behavior is
// not surprising at the least, and the cases where this matters are pretty
// unlikely (users with multiple monitors are unlikely to be using very small
// monitors, and it's even less likely that the primary monitor will be a small
// one, so the window will at least be fully visible initially).
//
// The size does not depend on the screen that the window is currently on.
class WindowMaxSize : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("windowmaxsize")

public:
    // The window being sized - this determines the decoration size taken into
    // account when computing the client size.
    Q_PROPERTY(QWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    // The preferred size of the window, in logical pixels.  This determines
    // which screen is chosen on multiple-monitor systems; the screen that
    // shows the most of the window is chosen.
    Q_PROPERTY(QSizeF preferredSize READ preferredSize WRITE setPreferredSize NOTIFY preferredSizeChanged)
    // The actual size that the window should use.  Never larger than
    // preferredSize.
    // Note that this _does_ depend on preferredSize (even the limiting size
    // does, though it isn't expressed as a property), so the window cannot
    // allow preferredSize to depend on effectiveSize.
    Q_PROPERTY(QSizeF effectiveSize READ effectiveSize NOTIFY effectiveSizeChanged)

public:
    WindowMaxSize(QObject *pParent = nullptr);

private:
    QSizeF calcDecorationSize(double screenScale) const;
    QSizeF calcEffectiveSize() const;
    void updateEffectiveSize();

public:
    QWindow *window() const {return _pWindow;}
    void setWindow(QWindow *pWindow);
    QSizeF preferredSize() const {return _preferredSize;}
    void setPreferredSize(const QSizeF &preferredSize);
    QSizeF effectiveSize() const {return _effectiveSize;}

signals:
    void windowChanged();
    void preferredSizeChanged();
    void effectiveSizeChanged();

private:
    WorkspaceChange _workspaceChange;
    QSizeF _effectiveSize;
    QSizeF _preferredSize;
    QWindow *_pWindow;
    std::unique_ptr<NativeWindowMetrics> _pWindowMetrics;
};

#endif
