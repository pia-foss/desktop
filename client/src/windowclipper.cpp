// Copyright (c) 2026 Private Internet Access, Inc.
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
#line SOURCE_FILE("windowclipper.cpp")

#include "windowclipper.h"

bool WindowClipper::updateEffectiveClipRect()
{
    QRect newEffectiveClipRect = _clipRect;
    QRect windowClient{0, 0, 0, 0};
    if(_pTargetWindow)
        windowClient.setSize(_pTargetWindow->geometry().size());

    // On Linux, X doesn't store the part of the mask that's outside the window
    // bound.  (If we set a mask before the window is that large, then resize
    // the window so it now covers the whole mask, the bottom of the mask was
    // lost and the window is clipped.)
    //
    // By clipping the mask to the window client bound and detecting window
    // resizing, we know when to reapply the mask due to the window being
    // resized.
    newEffectiveClipRect &= windowClient;

    if(newEffectiveClipRect != _effectiveClipRect)
    {
        _effectiveClipRect = newEffectiveClipRect;
        return true;
    }

    return false;
}

QRegion WindowClipper::generateClipMask()
{
    // If the clip rectangle is too small for two rounded radii, just ignore the
    // round.
    // This would need a special case to handle (we'd have to intersect the
    // corner ellipses that would overlap), and it never applies currently.
    // (Only Linux uses the radius, and it never sizes the dashboard this
    // small.)
    if(_round <= 0 || _effectiveClipRect.width() < 2*_round || _effectiveClipRect.height() < 2*_round)
    {
        // Regular non-rounded region
        return QRegion{_effectiveClipRect};
    }

    // To build a rounded rectangle region, generate 4 circular regions for the
    // corners and 2 rectangular regions for the middle.
    QRegion clipMask{_effectiveClipRect.left() + _round, _effectiveClipRect.top(),
                     _effectiveClipRect.width() - 2*_round, _effectiveClipRect.height()};
    clipMask |= QRegion{_effectiveClipRect.left(), _effectiveClipRect.top() + _round,
                        _effectiveClipRect.width(), _effectiveClipRect.height() - 2*_round};

    QRect cornerCircle{_effectiveClipRect.left(), _effectiveClipRect.top(), _round*2, _round*2};
    clipMask |= QRegion{cornerCircle, QRegion::RegionType::Ellipse};
    cornerCircle.moveRight(_effectiveClipRect.right());
    clipMask |= QRegion{cornerCircle, QRegion::RegionType::Ellipse};
    cornerCircle.moveBottom(_effectiveClipRect.bottom());
    clipMask |= QRegion{cornerCircle, QRegion::RegionType::Ellipse};
    cornerCircle.moveLeft(_effectiveClipRect.left());
    clipMask |= QRegion{cornerCircle, QRegion::RegionType::Ellipse};

    return clipMask;
}

void WindowClipper::applyClipMask()
{
    if(_pTargetWindow)
        _pTargetWindow->setMask(generateClipMask());
}

void WindowClipper::onWindowResize()
{
    // If the resize causes the effective mask to change, update the window mask
    if(updateEffectiveClipRect())
        applyClipMask();
}

void WindowClipper::setTargetWindow(QQuickWindow *pTargetWindow)
{
    if(pTargetWindow == _pTargetWindow)
        return;

    if(_pTargetWindow)
    {
        _pTargetWindow->setMask({});    // No longer clipping this window
        QObject::disconnect(_pTargetWindow, &QWindow::widthChanged, this,
                            &WindowClipper::onWindowResize);
        QObject::disconnect(_pTargetWindow, &QWindow::heightChanged, this,
                            &WindowClipper::onWindowResize);
    }

    _pTargetWindow = pTargetWindow;
    emit targetWindowChanged();

    // Don't care whether the effective clip bound changed, because the window
    // has changed - always update the mask.
    updateEffectiveClipRect();
    applyClipMask();
    if(_pTargetWindow)
    {
        QObject::connect(_pTargetWindow, &QWindow::widthChanged, this,
                         &WindowClipper::onWindowResize);
        QObject::connect(_pTargetWindow, &QWindow::heightChanged, this,
                         &WindowClipper::onWindowResize);
    }
}

void WindowClipper::setClipRect(const QRect &clipRect)
{
    if(clipRect == _clipRect)
        return; // Nothing to do

    _clipRect = clipRect;
    emit clipRectChanged();

    // If the effective clip bound changed, apply the new mask.
    if(updateEffectiveClipRect())
        applyClipMask();
}

void WindowClipper::setRound(int round)
{
    if(round == _round)
        return;

    _round = round;
    emit roundChanged();

    // Don't care whether the effective clip bound changed, because the round
    // radius has changed.  Always update the window mask.
    updateEffectiveClipRect();
    applyClipMask();
}
