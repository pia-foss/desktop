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
#line HEADER_FILE("singleactionitem.h")

#ifndef NATIVEACC_SINGLEACTIONITEM_H
#define NATIVEACC_SINGLEACTIONITEM_H

#include <QQuickItem>
#include <QAccessibleActionInterface>
#include "accessibleitem.h"
#include "accutil.h"

namespace NativeAcc {

// SingleActionItem models an accessible Item with a single "activate" action
// (in addition to the default "set focus" action, if the item is focusable).
// The "activate" action can vary but is usually Press or Toggle.
//
// Triggering the activate action signals activated().
//
// The action and role are set at construction and can't be changed.  If the
// control is disabled, the actions are removed.
class SingleActionItem : public AccessibleItem
{
    Q_OBJECT

public:
    // The action is identified by name.  Often this is a stock action, such as
    // QAccessibleAction::pressAction() or QAccessibleAction::toggleAction().
    // If it's a custom action, localizedActionDescription() and
    // localizedActionName() must be overridden to provide the action's
    // localized description and name.
    SingleActionItem(QAccessible::Role actionRole, QQuickItem &item,
                     QString actionName);

private:
    void onStateChanged(StateField field);

public:
    // Overrides of QAccessibleActionInterface (from AccessibleItem)
    virtual QStringList actionNames() const override;
    virtual void doAction(const QString &actionName) override;

signals:
    void activated();

private:
    const QString _activateAction;
};

// CheckableActionItem - a CheckableActionItem with an additional 'checked'
// state.
// The action is usually 'press' or 'toggle' depending on the role. Used to
// implement check boxes, toggle buttons, tabs, and radio buttons.
class CheckableActionItem : public SingleActionItem
{
    Q_OBJECT

    Q_PROPERTY(bool checked READ checked WRITE setChecked NOTIFY checkedChanged)

public:
    CheckableActionItem(QAccessible::Role actionRole, QQuickItem &item,
                        QString activateAction);

public:
    bool checked() const {return getState(StateField::checked);}
    void setChecked(bool newChecked);

signals:
    void checkedChanged();
};

}

#endif
