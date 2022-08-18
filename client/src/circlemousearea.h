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
#line HEADER_FILE("circlemousearea.h")

#ifndef CIRCLEMOUSEAREA_H
#define CIRCLEMOUSEAREA_H

#include <QQuickItem>

// CircleMouseArea, like MouseArea, is an inivisible QML item that provides
// mouse handling.  Unlike MouseArea, the interactive area is circular.
//
// The circular area is centered on the item and touches the nearest edge.
// Outside of the circle, CircleMouseArea is transparent to mouse movements.
//
// Note that CircleMouseArea can't be implemented by filtering the mouse events
// sent to a real MouseArea. Events such as mousePressEvent() and
// mouseReleaseEvent() could be filtered, but mouseMoveEvent() into the corners
// while the button is down can't be reliably filtered (MouseArea needs to be
// told that the mouse has left the area but the button is still down, and the
// only way to do this would be to alter the event position to be outside the
// rectangle, which would be hacky and unreliable.)
class CircleMouseArea : public QQuickItem
{
    Q_OBJECT
    // Whether CircleMouseArea currently contains the cursor (similar to
    // MouseArea.containsMouse).
    // Unlike MouseArea, CircleMouseArea does not have a hoverEnabled property,
    // it always enables hover events.  (This is needed to set the cursor
    // correctly when it enters or leaves the circle.)
    Q_PROPERTY(bool containsMouse READ containsMouse NOTIFY containsMouseChanged)
    // Whether the cursor was pressed over CircleMouseArea (similar to
    // MouseArea.pressed)
    Q_PROPERTY(bool pressed READ pressed NOTIFY pressedChanged)
    // pressed && containsMouse (similar to MouseArea.containsPress)
    Q_PROPERTY(bool containsPress READ containsPress NOTIFY containsPressChanged)
    // Desired cursor shape (similar to MouseArea.cursorShape)
    Q_PROPERTY(Qt::CursorShape cursorShape READ cursorShape WRITE setCursorShape NOTIFY cursorShapeChanged)

public:
    CircleMouseArea();

signals:
    // 'clicked' signal - emitted when the mouse button has been pressed and
    // released on the mouse area.  Note that unlike MouseArea, this does not
    // provide a QQuickMouseEvent parameter, and can't be propagated.
    void clicked();
    void containsMouseChanged();
    void pressedChanged();
    void containsPressChanged();
    void cursorShapeChanged();

public:
    bool containsMouse() const {return _containsMouse;}
    bool pressed() const {return _pressed;}
    bool containsPress() const {return pressed() && containsMouse();}
    Qt::CursorShape cursorShape() const {return _cursorShape;}

    void setCursorShape(Qt::CursorShape newCursorShape);

private:
    virtual void hoverEnterEvent(QHoverEvent *event) override;
    virtual void hoverLeaveEvent(QHoverEvent *event) override;
    virtual void hoverMoveEvent(QHoverEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;

    // Test if a point (in this item's coordinates) is inside the circle bound.
    bool pointInCircle(const QPointF &pos);

    // Update the control's cursor based on whether the cursor is currently
    // inside the circle.  Returns whether the cursor is currently in the circle
    // (the result of pointInCursor()).
    bool updateCursor(const QPointF &currentCursorPos);

    void handleHoverEvent(QHoverEvent *event);

    // Update one of the cursor properties and send notifications (both for that
    // property and for containsPress if applicable)
    void updateContainsMouse(bool newContainsMouse);
    void updatePressed(bool newPressed);

private:
    // Values of containsMouse and _pressed properties
    bool _containsMouse;
    bool _pressed;
    Qt::CursorShape _cursorShape;
};

#endif
