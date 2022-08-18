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
#line HEADER_FILE("buttons.h")

#ifndef NATIVEACC_BUTTONS_H
#define NATIVEACC_BUTTONS_H

#include "singleactionitem.h"
#include "accutil.h"

namespace NativeAcc {

// These are various button-like types - they're all SingleActionItems with a
// specified role and action.
// All of them emit 'activated()' when activated with their respective actions.
// As in controls.h, moc forces us to spell out a lot of this rather than
// factoring it out further.

// Regular Button - just a plain push button.
// Use when pushing the button just triggers something, use a more specific type
// for checked (toggleable) buttons, menu buttons, links, etc.
class ButtonAttached : public SingleActionItem
{
    Q_OBJECT
public:
    ButtonAttached(QQuickItem &item)
        : SingleActionItem{QAccessible::Role::Button, item,
                           QAccessibleActionInterface::pressAction()}
    {}
};

// Link - just a plain selectable link.  Uses the Link role.
class LinkAttached : public SingleActionItem
{
    Q_OBJECT
public:
    LinkAttached(QQuickItem &item)
        : SingleActionItem{QAccessible::Role::Link, item,
                           QAccessibleActionInterface::pressAction()}
    {}
};

// MenuButton - a button that displays a menu.
// Still uses the 'press' action since the type expresses that it will show a
// menu; "show menu" is usually used for an alternate action in edits, etc.
class MenuButtonAttached : public SingleActionItem
{
    Q_OBJECT
public:
    MenuButtonAttached(QQuickItem &item)
        : SingleActionItem{QAccessible::Role::ButtonMenu, item,
                           QAccessibleActionInterface::pressAction()}
    {}
};

// DropDownButton - a button that displays a list of items.  Has a 'value'
// property to indicate the current value.
//
// This uses the ComboBox role instead of the ButtonDropDown role, which would
// really be more appropriate.  On Mac, these both map to PopUpButton.  On
// Windows, ButtonDropDown is read as just "Button", drop downs are normally
// read as combo boxes.
class DropDownButtonAttached : public SingleActionItem
{
    Q_OBJECT

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    DropDownButtonAttached(QQuickItem &item)
        : SingleActionItem{QAccessible::Role::ComboBox, item,
                           QAccessibleActionInterface::pressAction()}
    {}

public:
    QString value() const {return _value;}
    void setValue(const QString &value);

    // Overrides of AccessibleItem
    virtual QString textValue() const override {return _value;}

signals:
    void valueChanged();

private:
    QString _value;
};

// CheckButton - a button that has a checked state and can be toggled.
// The QML code must respond to activated() and toggle the state.
class CheckButtonAttached : public CheckableActionItem
{
    Q_OBJECT
public:
    CheckButtonAttached(QQuickItem &item)
        : CheckableActionItem{QAccessible::Role::CheckBox, item,
                              QAccessibleActionInterface::toggleAction()}
    {}
};

// RadioButton - a checkable radio button.
// This must use the Press action type, because on Windows, Qt assumes that
// radio button type support it - it issues this action to select the radio
// button.
// (On Mac, Qt assumes that radio buttons support Toggle instead, but we fix
// that in mac_accessibility_decorator.mm.)
class RadioButtonAttached : public CheckableActionItem
{
    Q_OBJECT
public:
    RadioButtonAttached(QQuickItem &item)
        : CheckableActionItem{QAccessible::Role::RadioButton, item,
                              QAccessibleActionInterface::pressAction()}
    {}
};

// Tab - a tab in a tab list
// This actually uses the Radio Button type.
// - On Windows, Qt does not provide the "selected item" interface for page
//   tabs, so there's no way for the screen reader to know which tab is active.
//   It also consequently has no way to select the tab.  Radio buttons do get
//   these interfaces.
// - On Mac, Qt maps the PageTab type to Radio Button anyway.  Mac OS does not
//   seem to have a Tab type, though it does have a PageTabsGroup type, which is
//   not used by Qt.
class TabAttached : public CheckableActionItem
{
    Q_OBJECT
public:
    TabAttached(QQuickItem &item)
        : CheckableActionItem{QAccessible::Role::RadioButton, item,
                              QAccessibleActionInterface::pressAction()}
    {}
};

// ActionMenuItem - a menu item that invokes an action
class ActionMenuItemAttached : public SingleActionItem
{
    Q_OBJECT
    // Like DropDownMenuItemAttached, this "highlighted" property causes the
    // control to act like it's focused.
    //
    // QML Menus actually do focus their menu items normally as the highlight
    // moves, but for some reason they don't do that for disabled menu items.
    // Screen readers rely on this state so they can report changes as the up
    // and down arrow keys are pressed.
    Q_PROPERTY(bool highlighted READ highlighted WRITE setHighlighted NOTIFY highlightedChanged)

public:
    ActionMenuItemAttached(QQuickItem &item)
        : SingleActionItem{QAccessible::Role::MenuItem, item,
                           QAccessibleActionInterface::pressAction()}
    {}

public:
    bool highlighted() const {return forceFocus();}
    void setHighlighted(bool newHighlighted);

signals:
    void highlightedChanged();
};

// DropDownMenuItem - a checkable menu item for use in drop-down settings
class DropDownMenuItemAttached : public CheckableActionItem
{
    Q_OBJECT
    // The "highlighted" property causes the control to act like it's focused.
    //
    // This is a bit of a hack, but the menu items have to generate focus events
    // in order to move the screen reader cursor among the items as arrow keys
    // are pressed, and to highlight the first item initially.
    //
    // Ideally, we would really focus the item on the QML side, like MenuItems
    // do.  For some reason, we can't focus the ComboBox's list item / popup
    // focus scopes, and it's not clear why.  Maybe we could use an actual Menu
    // as the ComboBox's popup, but that would be a pretty major overhaul.
    //
    // The risk with this is that the "real" focus could conflict with this one,
    // since they're not synchronized.  This shouldn't be a problem for drop
    // downs, since any change in the "real" focus closes the popup.
    Q_PROPERTY(bool highlighted READ highlighted WRITE setHighlighted NOTIFY highlightedChanged)

public:
    DropDownMenuItemAttached(QQuickItem &item)
        : CheckableActionItem{QAccessible::Role::MenuItem, item,
                              QAccessibleActionInterface::pressAction()}
    {}

public:
    bool highlighted() const {return forceFocus();}
    void setHighlighted(bool newHighlighted);

signals:
    void highlightedChanged();
};

// MoveButton - Special button for the bookmark icon in the modules list.
// In addition to the normal "press" actions, has custom actions for move up
// and move down (corresponding to the up/down arrow key actions).
class MoveButtonAttached : public ButtonAttached
{
    Q_OBJECT
public:
    MoveButtonAttached(QQuickItem &item);

    // Overrides of QAccessibleActionInterface (from AccessibleItem)
    virtual QStringList actionNames() const override;
    virtual void doAction(const QString &actionName) override;
    virtual QString localizedActionDescription(const QString &actionName) const override;
    virtual QString localizedActionName(const QString &actionName) const override;

signals:
    void moveUp();
    void moveDown();
};

}

NATIVEACC_ATTACHED_PROPERTY_STUB(Button, ButtonAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(Link, LinkAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(MenuButton, MenuButtonAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(DropDownButton, DropDownButtonAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(CheckButton, CheckButtonAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(RadioButton, RadioButtonAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(Tab, TabAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(ActionMenuItem, ActionMenuItemAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(DropDownMenuItem, DropDownMenuItemAttached)
NATIVEACC_ATTACHED_PROPERTY_STUB(MoveButton, MoveButtonAttached)

#endif
