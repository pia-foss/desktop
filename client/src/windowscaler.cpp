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
#line SOURCE_FILE("windowscaler.cpp")

#include "windowscaler.h"
#include <QGuiApplication>
#include <QScreen>

void WindowScaler::setTargetWindow(QQuickWindow *pTargetWindow)
{
    if(pTargetWindow == _pTargetWindow)
        return;

    _pTargetWindow = pTargetWindow;
    emit targetWindowChanged();

    // Create (or recreate) the NativeWindowScaler and find the initial scale
    // for the new window.
    if(_pTargetWindow)
    {
        _pNativeScaler = NativeWindowScaler::create(*_pTargetWindow, _logicalSize);
        connect(_pNativeScaler.get(), &NativeWindowScaler::scaleChanged, this,
                &WindowScaler::updateScale);
        connect(_pNativeScaler.get(), &NativeWindowScaler::closeClicked, this,
                &WindowScaler::closeClicked);
        updateScale(_pNativeScaler->applyInitialScale());
    }
    else
    {
        _pNativeScaler.reset();
        updateScale(1.0);
    }
}

void WindowScaler::setLogicalSize(QSizeF logicalSize)
{
    if(logicalSize == _logicalSize)
        return;

    _logicalSize = logicalSize;
    if(_pNativeScaler)
        _pNativeScaler->updateLogicalSize(_logicalSize);
    emit logicalSizeChanged();
}

void WindowScaler::updateScale(qreal scale)
{
    if(scale != _scale)
    {
        qInfo() << "scale changed:" << _scale << "->" << scale;
        _scale = scale;
        emit scaleChanged();
    }
}
