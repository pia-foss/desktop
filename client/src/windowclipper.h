// Copyright (c) 2023 Private Internet Access, Inc.
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
#line HEADER_FILE("windowclipper.h")

#ifndef WINDOWCLIPPER_H
#define WINDOWCLIPPER_H

#include <kapps_core/src/winapi.h>
#include <QQuickWindow>
#include <QRegion>

// WindowClipper provides clip bound properties to QML that can be bound to set
// the clip region for a window, which causes it not to receive cursor events
// outside that region.
//
// Qt already supports clipping regions via mask() and setMask(), but they
// can't be bound into QML as provided.  Since we just need a rectangular
// region (possibly rounded), we can expose the rectangle bounds as properties.
//
// Qt does not specify whether this will visually mask the window in addition to
// masking input events.  It does do this on X11 when the X_SHAPE extension is
// available.  We use it to round the dashboard corners on that platform, but we
// still fill them in since they might still be visible.
class WindowClipper : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("clientwindow")

public:
    // Target window to clip
    Q_PROPERTY(QQuickWindow *targetWindow READ targetWindow WRITE setTargetWindow NOTIFY targetWindowChanged)
    // Rectangular clip bound
    Q_PROPERTY(QRect clip READ clip WRITE setClip NOTIFY clipChanged)
    // Rounding radius to apply to the clip bound.  Default is 0, which means
    // the region is an ordinary rectangle.  Positive values cause the region to
    // be a rounded rectangle.
    Q_PROPERTY(int round READ round WRITE setRound NOTIFY roundChanged)

private:
    QQuickWindow *_pTargetWindow = nullptr;
    // The clip rectangle assigned to the clip property
    QRect _clip;
    // The effective clip rectangle - the intersection of the clip rect with the
    // window's client bound.  (Stored to detect changes due to window
    // resizing.)  This does not account for the round radius.
    QRect _effectiveClip;
    int _round = 0;

public:
    using QObject::QObject;

private:
    // Update _effectiveClip - return value indicates whether it changed
    bool updateEffectiveClip();
    // Generate a clip mask using the current _effectiveClip and _round
    QRegion generateClipMask();
    // Apply the current clip mask to the window
    void applyClipMask();
    // Handle a change in the window's width or height
    void onWindowResize();

public:
    QQuickWindow *targetWindow() const {return _pTargetWindow;}
    void setTargetWindow(QQuickWindow *pTargetWindow);
    const QRect &clip() const {return _clip;}
    void setClip(const QRect &clip);
    int round() const {return _round;}
    void setRound(int round);

signals:
    void targetWindowChanged();
    void clipChanged();
    void roundChanged();
};

#endif
