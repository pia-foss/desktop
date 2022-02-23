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
#line SOURCE_FILE("buttons.cpp")

#include "buttons.h"
#include "client.h"

namespace NativeAcc {

namespace
{
    // Custom move up/down action names for MoveButtonAttached
    const QString moveUpActionName = QStringLiteral("MoveButtonUp");
    const QString moveDownActionName = QStringLiteral("MoveButtonDown");
}

void DropDownButtonAttached::setValue(const QString &value)
{
    if(value == _value || !item())
        return;

    _value = value;
    emit valueChanged();

    QAccessibleValueChangeEvent valueChange{item(), QVariant::fromValue(_value)};
    QAccessible::updateAccessibility(&valueChange);
}

void ActionMenuItemAttached::setHighlighted(bool newHighlighted)
{
    if(newHighlighted != highlighted())
    {
        setForceFocus(newHighlighted);
        emit highlightedChanged();
    }
}

void DropDownMenuItemAttached::setHighlighted(bool newHighlighted)
{
    if(newHighlighted != highlighted())
    {
        setForceFocus(newHighlighted);
        emit highlightedChanged();
    }
}

MoveButtonAttached::MoveButtonAttached(QQuickItem &item)
    : ButtonAttached{item}
{
    // A retranslate causes the action list to change.
    QObject::connect(Client::instance(), &Client::retranslate, this,
        [this](){emitAccEvent(QAccessible::Event::ActionChanged);});
}

QStringList MoveButtonAttached::actionNames() const
{
    QStringList actions = ButtonAttached::actionNames();

    actions.push_back(moveUpActionName);
    actions.push_back(moveDownActionName);

    return actions;
}

void MoveButtonAttached::doAction(const QString &actionName)
{
    if(actionName == moveUpActionName)
        emit moveUp();
    else if(actionName == moveDownActionName)
        emit moveDown();
    else
        ButtonAttached::doAction(actionName);
}

QString MoveButtonAttached::localizedActionDescription(const QString &actionName) const
{
    if(actionName == moveUpActionName)
    {
        //: Screen reader description of the "move up" action used to move a
        //: tile up in the list.  Grammatically, the implied subject is the
        //: accessibility action, "[This action] moves the tile up".
        return tr("Moves the tile up");
    }
    else if(actionName == moveDownActionName)
    {
        //: Screen reader description of the "move down" action used to move a
        //: tile down in the list.  Grammatically, the implied subject is the
        //: accessibility action, "[This action] moves the tile down".
        return tr("Moves the tile down");
    }

    return ButtonAttached::localizedActionDescription(actionName);
}

QString MoveButtonAttached::localizedActionName(const QString &actionName) const
{
    if(actionName == moveUpActionName)
    {
        //: Screen reader annotation of the "move up" action used to move a
        //: tile up in the list.  Should be a verb or short verb phrase.
        return tr("Move up");
    }
    else if(actionName == moveDownActionName)
    {
        //: Screen reader annotation of the "move down" action used to move a
        //: tile down in the list.  Should be a verb or short verb phrase.
        return tr("Move down");
    }

    return ButtonAttached::localizedActionName(actionName);
}

}
