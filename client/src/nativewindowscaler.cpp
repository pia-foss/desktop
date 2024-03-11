// Copyright (c) 2024 Private Internet Access, Inc.
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
#line SOURCE_FILE("nativewindowscaler.cpp")

#include "nativewindowscaler.h"
#ifdef Q_OS_WIN
#include "win/win_scaler.h"
#elif defined(Q_OS_LINUX)
#include "linux/linux_scaler.h"
#endif

auto NativeWindowScaler::create(QQuickWindow &window, const QSizeF &logicalSize)
    -> std::unique_ptr<NativeWindowScaler>
{
#ifdef Q_OS_WIN
    return std::make_unique<WinScaler>(window, logicalSize);
#elif defined(Q_OS_LINUX)
    return std::make_unique<LinuxWindowScaler>(window, logicalSize);
#else
    return std::make_unique<NativeWindowScaler>(window, logicalSize);
#endif
}

NativeWindowScaler::NativeWindowScaler(QQuickWindow &window,
                                       const QSizeF &logicalSize)
    : _targetWindow{window}
{
    // Apply the initial size now since we don't scale the window.
    targetWindow().resize(logicalSize.toSize());
}

// By default, NativeWindowScaler just reports a scale factor of 1.0 that never
// changes.  This is used on Mac, where the OS handles scaling for us.
qreal NativeWindowScaler::applyInitialScale()
{
    return 1.0;
}

// The default implementation of updateLogicalSize() just applies the size to
// the window directly, since no scaling is performed.
void NativeWindowScaler::updateLogicalSize(const QSizeF &logicalSize)
{
    QSize sizeInt = logicalSize.toSize();

    targetWindow().resize(sizeInt);
}
