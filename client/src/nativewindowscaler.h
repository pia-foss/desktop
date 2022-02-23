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
#line HEADER_FILE("nativewindowscaler.h")

#ifndef NATIVEWINDOWSCALER_H
#define NATIVEWINDOWSCALER_H

#include <QQuickWindow>
#include <memory>

// NativeWindowScaler provides an interface to detect an OS-specific scaling
// factor on a per-window basis, and to notify when that scaling factor should
// change.
//
// WinScaler implements this on Windows for per-monitor DPI support.  There is
// no implementation of this for OS X, as the OS handles DPI scaling for us.
class NativeWindowScaler : public QObject
{
    Q_OBJECT

public:
    static std::unique_ptr<NativeWindowScaler> create(QQuickWindow &window,
                                                      const QSizeF &logicalSize);

public:
    NativeWindowScaler(QQuickWindow &window, const QSizeF &logicalSize);

public:
    // WindowScaler stores and updates the property values that interface to
    // QML.  It uses getScale() and updateLogicalSize() to connect these
    // properties to the NativeWindowScaler implementation.

    // applyInitialScale() is used initially after the NativeWindowScaler is
    // created to update the scale property to reflect the value detected for
    // the newly-assigned target window.
    //
    // The implementation should determine the window's initial scale factor,
    // apply it with the initial logical size (if it didn't already do this in
    // the constructor), and return the initial scale factor.
    virtual qreal applyInitialScale();
    // updateLogicalSize() is used if the QML code changes the desired logical
    // size.  The NativeWindowScaler should (if necessary) update the actual
    // window size to reflect the new value.
    virtual void updateLogicalSize(const QSizeF &logicalSize);

public:
    QQuickWindow &targetWindow() const {return _targetWindow;}

signals:
    // The NativeWindowScaler implementation should emit scaleChanged() if the
    // window's scale factor changes for any reason.  This updates the scale
    // property that the QML code reads.  The implementation should update the
    // window's actual size when this happens.
    void scaleChanged(qreal scale);
    // On Windows, this event is emitted when the user clicks the close button.
    // This is necessary because QtQuick.Window otherwise destroys the window,
    // meaning WinScaler loses its subclass on the window to handle DPI scaling.
    // Instead, we emit this as an event and let the QML code hide the window.
    void closeClicked();

private:
    QQuickWindow &_targetWindow;
};

#endif
