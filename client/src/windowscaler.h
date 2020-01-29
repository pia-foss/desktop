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
#line HEADER_FILE("windowscaler.h")

#ifndef WINDOWSCALER_H
#define WINDOWSCALER_H

#include "nativewindowscaler.h"
#include <QtGlobal>
#include <QQuickWindow>

// WindowScaler is used in QML to size a QtQuick.Window at a logical size
// independent of the screen DPI.
//
// On OS X, this just sizes the window to the logical size, because the OS
// handles scaling for us.  On Windows however, scaling for per-monitor DPI
// support is incredibly complex.
//
// The QML window should create a WindowScaler targeting the QtQuick.Window and
// set the logicalSize property to the desired window size.  The QML code must
// *NOT* set or bind the Window's size properties, but it can (and should) read
// them to find out the window's actual client size.
//
// The window must then size its contents to the actual window size, such as:
// - by using an Item wrapping the entire content of the window at the logical
//   size that scales to the window size
// - using only relative layout and sizing for components inside the window
// - by applying the window's scale factor (the scale property, which is the
//   actual size / logical size) to the components' layout metrics.
//
// The logicalSize can be updated to resize the window.
class WindowScaler : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("windowscaler")

public:
    // The target window can be updated dynamically, but if it is, the
    // WindowScaler just abandons the old window in whatever state it's in.
    // Generally, this should just be used to set the window initially.
    Q_PROPERTY(QQuickWindow *targetWindow READ targetWindow WRITE setTargetWindow NOTIFY targetWindowChanged)
    Q_PROPERTY(QSizeF logicalSize READ logicalSize WRITE setLogicalSize NOTIFY logicalSizeChanged)
    Q_PROPERTY(qreal scale READ scale NOTIFY scaleChanged)

public:
    using QObject::QObject;

public:
    QQuickWindow *targetWindow() const {return _pTargetWindow;}
    void setTargetWindow(QQuickWindow *pTargetWindow);
    QSizeF logicalSize() const {return _logicalSize;}
    void setLogicalSize(QSizeF logicalSize);
    qreal scale() const {return _scale;}

signals:
    void targetWindowChanged();
    void logicalSizeChanged();
    void scaleChanged();
    // The close button was clicked on Windows.  The QML code handles this by
    // hiding the window (instead of destroying it).  (See NativeWindowScaler
    // and WinScaler.)
    void closeClicked();

private:
    void updateScale(qreal scale);

private:
    QQuickWindow *_pTargetWindow = nullptr;
    // Default this to something sane to avoid "can't size window to requested
    // geometry" warnings before the size bindings have been evaluated.  Note
    // that 100x100 is too small for decorated windows on Windows and would
    // still generate warnings.
    QSizeF _logicalSize{200, 200};
    qreal _scale = 1.0;
    std::unique_ptr<NativeWindowScaler> _pNativeScaler;
};

#endif
