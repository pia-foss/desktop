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
#line SOURCE_FILE("singleactionitem.cpp")

#include "singleactionitem.h"

namespace NativeAcc {

SingleActionItem::SingleActionItem(QAccessible::Role actionRole, QQuickItem &item,
                                   QString actionName)
    : AccessibleItem{actionRole, item}, _activateAction{std::move(actionName)}
{
    // 'disabled' affects the actions list
    QObject::connect(this, &AccessibleItem::stateChanged, this,
                     &SingleActionItem::onStateChanged);
}

void SingleActionItem::onStateChanged(StateField field)
{
    switch(field)
    {
        case StateField::disabled:
            // The 'disabled' field affects the action list since it controls
            // the press/toggle action.
            // (AccessibleItem itself emits this if the change causes the
            // setFocus action to change, but this doesn't happen if the control
            // isn't focusable.)
            emitAccEvent(QAccessible::Event::ActionChanged);
            break;
        default:
            break;
    }
}

QStringList SingleActionItem::actionNames() const
{
    // Contains setFocus if that action applies
    QStringList actions = AccessibleItem::actionNames();

    if(!getState(StateField::disabled))
    {
        // Press/toggle is the preferred action, put this first
        actions.insert(0, _activateAction);
    }

    return actions;
}

void SingleActionItem::doAction(const QString &actionName)
{
    if(actionName == _activateAction)
        emit activated();
    else
        AccessibleItem::doAction(actionName);
}

CheckableActionItem::CheckableActionItem(QAccessible::Role actionRole,
                                         QQuickItem &item, QString activateAction)
    : SingleActionItem{actionRole, item, std::move(activateAction)}
{
    // Always checkable
    setState(StateField::checkable, true);
}

void CheckableActionItem::setChecked(bool newChecked)
{
    if(checked() != newChecked)
    {
        setState(StateField::checked, newChecked);
        emit checkedChanged();
    }
}

}
