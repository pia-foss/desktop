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
#line SOURCE_FILE("draghandle.cpp")

#include "draghandle.h"
#include <QGuiApplication>
#include <QStyleHints>
#include <QCursor>
#include <QtGlobal>

DragHandle::DragHandle()
    : _state{DragState::None}
{
    setAcceptedMouseButtons(Qt::MouseButton::LeftButton);
    setCursor(QCursor{Qt::CursorShape::OpenHandCursor});
    // Once we grab the cursor (implicitly by accepting a "press" event), do not
    // allow anything else to grab it.  Otherwise, when the dashboard has to
    // scroll, the Flickable might steal the cursor grab from us (even though we
    // do not enable flick gestures).
    setKeepMouseGrab(true);
    // Qt synthesizes mouse events for unhandled touch events, but some contexts
    // still appear to check keepTouchGrab() for these events.  Set this too for
    // consistency, although we don't have any touch devices to test :-/
    setKeepTouchGrab(true);
}

void DragHandle::mouseMoveEvent(QMouseEvent *event)
{
    Q_ASSERT(event);    // Qt guarantee

    switch(_state)
    {
        default:
        case DragState::None:
            return;
        case DragState::Started:
        {
            // Check if we can go to the Active state
            Q_ASSERT(QGuiApplication::styleHints());    // Guarantee of QGuiApplication
            int dragDist = QGuiApplication::styleHints()->startDragDistance();
            // The drag distance is a Manhattan distance
            if((_cursorInitialScreenPos - event->globalPos()).manhattanLength() < dragDist)
                return; // Don't do anything yet; haven't moved far enough.
            _state = DragState::Active; // Begin the drag
            // Set the closed hand as the application-wide override cursor,
            // so it applies during the drag even if the cursor is outside of
            // this item.
            QGuiApplication::setOverrideCursor(QCursor{Qt::CursorShape::ClosedHandCursor});
            // Drop through to emit the first change
            Q_FALLTHROUGH();
        }
        case DragState::Active:
            // The cursor's current position is the desired screen position of
            // the drag point; no fancy logic here.
            emit dragUpdatePosition(_dragPos, event->globalPos());
            break;
    }
}

void DragHandle::mousePressEvent(QMouseEvent *event)
{
    Q_ASSERT(event);    // Qt guarantee
    // Qt guarantee - press only delivered for an accepted button
    Q_ASSERT(event->button() == Qt::MouseButton::LeftButton);

    if(_state == DragState::None)
    {
        _state = DragState::Started;
        // Map the cursor position to an item position to determine the drag
        // position.
        // Though this depends on the item's absolute position, there's no need
        // to handle any changes in absolute position; this only depends on the
        // absolute position at this moment as the drag begins.
        _dragPos = mapFromGlobal(QPointF{event->globalPos()});
        _cursorInitialScreenPos = event->globalPos();
    }
}

void DragHandle::mouseReleaseEvent(QMouseEvent *event)
{
    Q_ASSERT(event);    // Qt guarantee
    // Qt guarantee - release only delivered for an accepted button
    Q_ASSERT(event->button() == Qt::MouseButton::LeftButton);

    // mouseReleaseEvent() is overridden just so the QQuickItem implementation
    // doesn't ignore the event (we want to accept it, which is the default).
    // The actual drag-end logic occurs in mouseUngrabEvent() to ensure that it
    // takes place if the grab is stolen from us for some reason.  Handling a
    // release event implicitly ungrabs the mouse.
}

void DragHandle::mouseUngrabEvent()
{
    if(_state == DragState::Active)
    {
        // Restore the cursor.  The override cursor uses a stack, so we're
        // relying on the fact that we don't use the override cursor for any
        // other cursor shapes right now (otherwise they could get out of sync
        // if we don't pop in the right order).
        QGuiApplication::restoreOverrideCursor();
        emit dragEnded();
    }

    _state = DragState::None;
}
