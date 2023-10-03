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
#line SOURCE_FILE("focuscue.cpp")

#include "focuscue.h"
#include <QHash>

namespace
{
    // All attached FocusCueAttachedTypes with the Items they're attached to.
    // Used as a uniquely-keyed map, QML only creates one attached object per
    // attacher.
    QHash<QQuickItem*, FocusCueAttached*> focusCueAttachers;
}

FocusCueAttached *FocusCue::qmlAttachedProperties(QObject *object)
{
    return new FocusCueAttached(object);
}

FocusCue::FocusCue()
    : _pControl{}, _hasFocus{false},
      _focusReason{Qt::FocusReason::OtherFocusReason}
{
}

void FocusCue::updateState(bool hasFocus, Qt::FocusReason focusReason)
{
    bool oldShow = show();

    _hasFocus = hasFocus;
    _focusReason = focusReason;

    bool newShow = show();
    if(oldShow != newShow)
    {
        emit showChanged();
        if(newShow)
        {
            // Can only be shown when there is a valid control
            Q_ASSERT(_pControl);
            FocusCueAttached::emitChildCueRevealed(*_pControl, *this);
        }
    }
}

bool FocusCue::eventFilter(QObject *watched, QEvent *event)
{
    // Class invariant; only one watched object
    Q_ASSERT(watched == _pControl);

    QFocusEvent *pFocusEvent = dynamic_cast<QFocusEvent*>(event);
    if(pFocusEvent)
    {
        bool newHasFocus = pFocusEvent->type() == QEvent::Type::FocusIn;
        Qt::FocusReason newReason = newHasFocus ? pFocusEvent->reason() : Qt::FocusReason::OtherFocusReason;
        updateState(newHasFocus, newReason);
    }

    return false;
}

void FocusCue::setControl(QQuickItem *pControl)
{
    if(pControl == _pControl)
        return;

    if(_pControl)
        _pControl->removeEventFilter(this);

    if(pControl)
        pControl->installEventFilter(this);

    _pControl = pControl;
    emit controlChanged();

    // Always go to the 'unfocused' state.  We could set this based on the
    // current state of _pControl, but we can't figure out the focus reason, so
    // this would only partially work.  This should generally just be set when
    // the controls are created, it shouldn't be changed later.
    updateState(false, Qt::FocusReason::OtherFocusReason);
}

bool FocusCue::show() const
{
    if(!_hasFocus)
        return false;
    switch(_focusReason)
    {
        default:
        case Qt::FocusReason::MouseFocusReason:
        case Qt::FocusReason::ActiveWindowFocusReason:
        case Qt::FocusReason::PopupFocusReason:
        case Qt::FocusReason::MenuBarFocusReason:
        case Qt::FocusReason::OtherFocusReason:
            return false;
        case Qt::FocusReason::TabFocusReason:
        case Qt::FocusReason::BacktabFocusReason:
        case Qt::FocusReason::ShortcutFocusReason:
            return true;
    }
}

void FocusCue::reveal()
{
    // If the control is focused, just set the focus reason to one that will
    // reveal the focus cues.  We don't provide any access to the actual focus
    // reason, so this isn't observable.
    if(_hasFocus)
        updateState(true, Qt::FocusReason::TabFocusReason);
}

void FocusCueAttached::emitChildCueRevealed(QQuickItem &control, FocusCue &focusCue)
{
    // Walk the control's visual parents and emit the signal for any that have
    // an attached FocusCueAttached
    QQuickItem *pNextItem = &control;
    while(pNextItem)
    {
        FocusCueAttached *pAttached = focusCueAttachers.value(pNextItem);
        if(pAttached)
            emit pAttached->childCueRevealed(&control, &focusCue);
        pNextItem = pNextItem->parentItem();
    }
}

FocusCueAttached::FocusCueAttached(QObject *parent)
    : QObject{parent}, _pAttacher{qobject_cast<QQuickItem*>(parent)}
{
    if(_pAttacher)
        focusCueAttachers.insert(_pAttacher, this);
    else
        qWarning() << "Cannot use FocusCue attached properties on non-Item:" << parent;
}

FocusCueAttached::~FocusCueAttached()
{
    if(_pAttacher)
    {
        // Should still be 'this', otherwise more than one attached object was
        // created for the same Item somehow
        Q_ASSERT(focusCueAttachers.value(_pAttacher) == this);
        focusCueAttachers.remove(_pAttacher);
    }
}
