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
#line SOURCE_FILE("windowformat.cpp")

#include "windowformat.h"
#include <QQuickWindow>

WindowFormatAttached *WindowFormat::qmlAttachedProperties(QObject *object)
{
    QQuickWindow *pWindow = dynamic_cast<QQuickWindow*>(object);
    if(!pWindow)
    {
        qWarning() << "Attaching WindowFormat to invalid window:" << object;
        return nullptr;
    }

    return new WindowFormatAttached{*pWindow};
}

WindowFormatAttached::WindowFormatAttached(QQuickWindow &window)
    : _window{window}
{
}

void WindowFormatAttached::setHasAlpha(bool hasAlpha)
{
    auto windowFmt = _window.requestedFormat();
    windowFmt.setAlphaBufferSize(hasAlpha ? 8 : -1);
    _window.setFormat(windowFmt);
}
