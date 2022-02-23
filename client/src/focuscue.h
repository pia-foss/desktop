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
#line HEADER_FILE("focuscue.h")

#ifndef FOCUSCUE_H
#define FOCUSCUE_H

#include <QQuickItem>

class FocusCueAttached;

// FocusCue indicates whether a focus cue should be drawn for a particular item.
//
// The client's focus cues are only drawn when actually using keyboard
// navigation.  FocusCue keeps track of the reason that a control became focused
// and uses that to determine whether the cues should be shown.
//
// The control can also explicitly enable its cue when focused by calling
// reveal().  This reveals the focus cues until the control loses focus again.
// This is used, for example, if a button was clicked with the mouse, but then
// manipulated with the space bar (it received focus due to the mouse, but since
// the user is now using the keyboard, we should show the focus cue).
//
// FocusCue does not actually draw any focus cues, it exposes the 'show'
// property to indicate when they should be drawn.
class FocusCue : public QQuickItem 
{
    Q_OBJECT

public:
    static FocusCueAttached *qmlAttachedProperties(QObject *object);

    // The target control whose focus state we're watching
    // This usually shouldn't change after construction; we can't figure out why
    // the control was focused if it already has focus when the target is set.
    Q_PROPERTY(QQuickItem *control READ control WRITE setControl NOTIFY controlChanged)
    // Whether focus cues should be drawn for the target control
    Q_PROPERTY(bool show READ show NOTIFY showChanged)

public:
    FocusCue();

signals:
    void controlChanged();
    void showChanged();

private:
    void updateState(bool hasFocus, Qt::FocusReason focusReason);
    virtual bool eventFilter(QObject *watched, QEvent *event) override;

public:
    void setControl(QQuickItem *pControl);
    QQuickItem *control() const {return _pControl;}
    bool show() const;
    // Reveal the focus cues for a focused control.  If the control doesn't
    // actually have focus, this has no effect.  When the control loses focus,
    // this effect is reset.
    Q_INVOKABLE void reveal();

private:
    QQuickItem *_pControl;
    // Whether the control currently has focus.
    bool _hasFocus;
    // The reason the control currently has focus.  Meaningful only when
    // _hasFocus is true.
    Qt::FocusReason _focusReason;
};

// The FocusCue.childCueRevealed signal can be used by scrolling views to ensure
// that a focused item is visible when its cue is revealed.
//
// This is signaled when the focus cue becomes visible for any reason, such as
// gaining focus from the keyboard or calling FocusCue::reveal() while focused
// from the mouse.
class FocusCueAttached : public QObject
{
    Q_OBJECT

public:
    // Used by FocusCue - emit the childCueRevealed signal for parents of
    // control
    static void emitChildCueRevealed(QQuickItem &control, FocusCue &focusCue);

public:
    FocusCueAttached(QObject *parent);
    ~FocusCueAttached();
    // Already non-copiable/assignable per QObject

signals:
    // The FocusCue.childCueRevealed signal can be used by scrolling views to
    // ensure that a focused item is visible when its cue is revealed.
    //
    // This is signaled when the focus cue of a child Item becomes visible for
    // any reason, such as gaining focus from the keyboard or calling
    // FocusCue::reveal() while focused from the mouse.
    //
    // The control passed as the parameter is the value of the 'control'
    // property on the FocusCue.  This signal is emitted for visual parents of
    // that control (not necessarily those of the FocusCue itself).
    //
    // The focus cue is also passed, since its bounds may differ from those of
    // the control itself.
    void childCueRevealed(QQuickItem *control, FocusCue *focusCue);

private:
    // The Item that this object is attached to (if any) - used to clean up the
    // map of items to FocusCueAttacheds.
    QQuickItem * const _pAttacher;
};
QML_DECLARE_TYPEINFO(FocusCue, QML_HAS_ATTACHED_PROPERTIES)

#endif
