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

#include <common/src/common.h>
#line HEADER_FILE("draghandle.h")

#ifndef DRAGHANDLE_H
#define DRAGHANDLE_H

#include <QQuickItem>

// DragHandle is a non-visual handle that can be dragged with the cursor to move
// an item.
//
// DragHandle works by expressing a "drag position" and "desired position" to
// QML, which can then move the window or item based on this desired position.
// This value is expressed by emitting the dragUpdatePosition() signal.
//
// The desired position is expressed as a signal instead of a property, because
// the window doesn't move if the DragHandle changes position outside of a drag.
// For example, the DragHandle might be repositioned in the window, which
// shouldn't move the window.  If the desired position was expressed as a
// property, it would have to provide a new value now to reflect the change in
// position, which in principle should be signaled as a change.  Theoretically,
// this would precisely cancel out with the change that occurred, meaning the
// window would not end up moving.  However, it'd be easy to cause binding loops
// when handling this change.
//
// This makes sense from a theoretical standpoint - dragging is inherently an
// imperative user action, not a declaratively constructed state.
class DragHandle : public QQuickItem
{
    Q_OBJECT

private:
    // Drag states
    enum class DragState
    {
        // Not dragging at all
        None,
        // The mouse button has been pressed, but the cursor hasn't moved enough
        // to actually begin a drag yet.
        Started,
        // A drag is ongoing; the mouse button has been pressed and the cursor
        // has moved enough to trigger a drag.
        Active,
    };

public:
    DragHandle();

signals:
    // The desired position has changed during a drag (or a drag has started; it
    // is possible for a drag to begin without actually changing the desired
    // position yet).
    //
    // dragPos is the position in the DragHandle that is being dragged. This is
    // in the DragHandle's coordinates.
    //
    // desiredScreenPos is the current desired screen position of that location.
    // The drag handle does not necessarily have to be positioned here, such as
    // if the point is too close to the screen edge, outside the window bounds,
    // etc.  It will still preserve the correct drag location if this happens.
    //
    // Note that expressing the location this way means it does not depend on
    // the DragHandle's absolute position within the window.  (If we omitted
    // dragPos and provided the desired position of the top-left corner instead,
    // this would depend on the window's scale, because it could change the
    // screen-coordinate offset between the drag location and top-left corner.
    // The window scale could change during a drag.)
    void dragUpdatePosition(QPointF dragPos, QPoint desiredScreenPos);

    // The drag has ended.
    void dragEnded();

private:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;

private:
    DragState _state;
    // Position where the item is being dragged - in item coordinates.
    // Valid in Started and Active states.
    QPointF _dragPos;
    // Initial position of the cursor when a drag started - in screen
    // coordinates.  Valid in Started and Active states.
    QPoint _cursorInitialScreenPos;
};

#endif
