// Copyright (c) 2019 London Trust Media Incorporated
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
#line SOURCE_FILE("valuetext.cpp")

#include "valuetext.h"
#include "client.h"

namespace NativeAcc {

namespace
{
    // This is the action identifier passed to Qt.  It's not displayed; the
    // display string is in localizedActionNames().
    const QString copyActionName = QStringLiteral("ValueTextCopy");
}

ValueTextAttached::ValueTextAttached(QQuickItem &item)
    : TextFieldBase{QAccessible::Role::EditableText, item}, _copiable{false}
{
    // Value texts are always read-only.
    setState(StateField::readOnly, true);

    // A retranslate causes the action to change.
    QObject::connect(Client::instance(), &Client::retranslate, this,
        [this](){emitAccEvent(QAccessible::Event::ActionChanged);});
}

void ValueTextAttached::setCopiable(bool copiable)
{
    if(copiable != _copiable)
    {
        _copiable = copiable;
        emit copiableChanged();
        emitAccEvent(QAccessible::Event::ActionChanged);
    }
}

QStringList ValueTextAttached::actionNames() const
{
    QStringList actions = AccessibleItem::actionNames();

    // Copiable value texts get a copy action, which becomes the default.
    if(copiable())
        actions.push_front(copyActionName);

    return actions;
}

void ValueTextAttached::doAction(const QString &actionName)
{
    if(actionName == copyActionName)
        emit copy();
    else
        AccessibleItem::doAction(actionName);
}

QString ValueTextAttached::localizedActionDescription(const QString &actionName) const
{
    if(actionName == copyActionName)
    {
        //: Screen reader description of the "copy" action for IP address/port
        //: fields, etc.  "Copies" refers to copying to the system clipboard and
        //: should use the OS's normal terminology.  Grammatically, the implied
        //: subject is the accessibility action, "[This action] copies the value
        //: [to the clipboard]".
        return tr("Copies the value");
    }

    return AccessibleItem::localizedActionDescription(actionName);
}

QString ValueTextAttached::localizedActionName(const QString &actionName) const
{
    if(actionName == copyActionName)
    {
        //: Screen reader annotation to describe the "copy" action on the IP
        //: address and port fields, etc.  Copies the text to the clipboard,
        //: should be a verb or short verb phrase.
        return tr("Copy");
    }

    return AccessibleItem::localizedActionName(actionName);
}

}
