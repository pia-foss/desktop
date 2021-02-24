// Copyright (c) 2021 Private Internet Access, Inc.
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
#line HEADER_FILE("windowformat.h")

#ifndef WINDOWFORMAT_H
#define WINDOWFORMAT_H

#include <QQuickWindow>

class WindowFormatAttached;

// The WindowFormat attached type is used to control the requested render format
// of a QQuickWindow.  The dashboard uses this to specify whether it needs an
// alpha channel.
//
// This can't be set any other way, because it has to be set before the
// underlying platform window is created, and other children/properties of
// DashboardPopup will cause it to be created.  (QQuickWindow normally infers
// this from the alpha value of the window's color, but property bindings are
// evaluated too late, the window is already created by that point.
// Component.onCompleted is also too late.
class WindowFormat : public QObject
{
    Q_OBJECT
public:
    static WindowFormatAttached *qmlAttachedProperties(QObject *object);
};

class WindowFormatAttached : public QObject
{
    Q_OBJECT

    // Whether to request an alpha channel for this window.  This property
    // should only be written; the READ method and NOTIFY signal are just stubs
    // to satisfy the property system.  (DashboardPopup never needs to read this
    // property.  QQuickWindow doesn't have a signal for changes in the
    // requested format.  Really, this should just be a method, but QML
    // initialization order forces us to make this a property.)
    Q_PROPERTY(bool hasAlpha READ hasAlpha WRITE setHasAlpha NOTIFY hasAlphaChanged)

public:
    WindowFormatAttached(QQuickWindow &window);

private:
    // Stub - could check window.requestedFormat(), but DashboardPopup doesn't
    // need it and the change signal can't be 100% reliable anyway.
    bool hasAlpha() const {return false;}
    void setHasAlpha(bool hasAlpha);

signals:
    void hasAlphaChanged() const;   // Never emitted; stub

private:
    QQuickWindow &_window;
};

QML_DECLARE_TYPEINFO(WindowFormat, QML_HAS_ATTACHED_PROPERTIES)

#endif
