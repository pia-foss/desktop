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
#line SOURCE_FILE("textfield.cpp")

#include "textfield.h"

namespace NativeAcc {

TextFieldAttached::TextFieldAttached(QQuickItem &item)
    : TextFieldBase{QAccessible::Role::EditableText, item}
{
    // Read-only defaults to false, so editable has to default to true.
    setState(StateField::editable, true);
}

void TextFieldAttached::setReadOnly(bool readOnly)
{
    if(readOnly != getState(StateField::readOnly))
    {
        // Yes, Qt really does have two flags for this, and we need to set both
        // correctly.  (editable seems to only affect Mac.)
        setState(StateField::readOnly, readOnly);
        setState(StateField::editable, !readOnly);
        emit readOnlyChanged();
    }
}

void TextFieldAttached::setPasswordEdit(bool passwordEdit)
{
    if(passwordEdit != getState(StateField::passwordEdit))
    {
        setState(StateField::passwordEdit, passwordEdit);
        emit passwordEditChanged();
    }
}

void TextFieldAttached::setSearchEdit(bool searchEdit)
{
    if(searchEdit != getState(StateField::searchEdit))
    {
        setState(StateField::searchEdit, searchEdit);
        emit searchEditChanged();
    }
}

QStringList TextFieldAttached::actionNames() const
{
    QStringList actions = TextFieldBase::actionNames();

    // Focusable text fields also get a show-menu action.  We could expose a
    // property to control this if necessary, but this is fine right now.
    // The set-focus action is still the default for TextFields.
    if(hasSetFocusAction())
        actions.push_back(QAccessibleActionInterface::showMenuAction());

    return actions;
}

void TextFieldAttached::doAction(const QString &actionName)
{
    if(actionName == QAccessibleActionInterface::showMenuAction())
        emit showMenu();
    else
        TextFieldBase::doAction(actionName);
}

}
