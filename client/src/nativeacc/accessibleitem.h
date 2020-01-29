// Copyright (c) 2020 Private Internet Access, Inc.
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
#line HEADER_FILE("accessibleitem.h")

#ifndef NATIVEACC_ACCESSIBLEITEM_H
#define NATIVEACC_ACCESSIBLEITEM_H

#include "accutil.h"
#include "interfaces.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>

namespace NativeAcc {

class AccessibleImpl;

// AccessibleItem provides the basic linkage between a QQuickItem and a
// QAccessibleInterface.
//
// AccessibleItem is the base for the various specific accessible attached
// types, like Table, Link, etc.  By creating an AccessibleItem parented to a
// particular QQuickItem, AccessibleItem::interfaceFactory() will be able to
// create a QAccessibleInterface for that Item.
//
// The lifetime management here is nontrivial.  The AccessibleItem needs to be
// parented to the Item, but the QAccessibleInterface is owned by the
// QAccessible framework.  There's no guarantee that they'll be destroyed in any
// particular order, so we have to be sure that either can be destroyed first,
// while still allowing both objects to communicate.
//
// Other interfaces, like text and value interfaces, are typically implemented
// by the derived type.  QAccessibleActionInterface is implemented by
// AccessibleItem itself, because any item that's focusable and enalbed gets a
// setFocus action.  Many types have additional actions, override the methods of
// QAccessibleActionInterface to provide them.
class AccessibleItem : public QObject, protected QAccessibleActionInterface
{
    Q_OBJECT

    // The accessible item's role is determined by the accessible type used -
    // it's set when AccessibleItem is created and can't be changed.
    //
    // Some state properties are controlled by the state of the Item, like:
    // - active (Item.focus)
    // - focused (Item.activeFocus)
    // - focusable (Item.activeFocusOnTab)
    // - disabled (Item.enabled)
    //
    // The item's visible flag (and AccessibleItem::hidden()) cause the element
    // to be destroyed, not marked as 'invisible' - see onItemVisibleChanged().

    // All accessible types have a name.  If the name is empty, no accessibility
    // hints are created.  (This can be used to dynamically create/destroy the
    // control's accessibility hints.)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    // All accessible types can have a description.  This can be empty if no
    // description is needed (name is sufficient).
    // Note that QML Accessible type would use the description as its name if no
    // name is set, AccessibleItem does not do this.
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    // NOTE: QAccessible defines a 'value' text type too, but it's not provided
    // by AccessibleItem; specific role types have to provide it if it's needed.
    // This usually requires fixup code on Mac for it to work correctly, see the
    // NSAccessibilityValueAttribute implementation in
    // mac_accessibility_decorator.mm

    // AccessibleItem provides a 'disabled' property that can also be used to
    // indicate the disabled state.  Occasionally, controls act disabled but do
    // not actually set Item.enabled=false for technical reasons, in that case
    // they should set this disabled property instead.
    //
    // The control is read as "disabled" if either Item.enabled===false or
    // disabled===true.
    Q_PROPERTY(bool disabled READ disabled WRITE setDisabled NOTIFY disabledChanged)

    // AccessibleItem is normally focusable if the QQuickItem has
    // activeFocusOnTab=true.  In rare cases, an AccessibleItem should be
    // focusable by accessibility tools even though it is not otherwise
    // focusable; enable focusable to force this.
    Q_PROPERTY(bool alwaysFocusable READ alwaysFocusable WRITE setAlwaysFocusable NOTIFY alwaysFocusableChanged)

public:
    // Get the AccessibleItem for a QObject, if there is one.
    static AccessibleItem *getAccItem(QObject *pObject);

    // Get the AccessibleItem-based interface to a QObject.
    // If the object is known to an AccessibleItem, and it exists in the
    // accessibility tree, returns its QAccessibleInterface.
    // Note that this does not return an AccessibleImpl attached to a QObject if
    // there is no AccessibleItem for that object, or if that AccessibleItem
    // doesn't "exist" from the screen reader's perspective.
    static AccessibleImpl *getInterface(QObject *pObject);

    // A parent accessibility element is being created or destroyed.  Check this
    // child element (and its descendants as needed) for parent changes, and
    // create/destroy/emit parent changes as needed.
    //
    // (Used by WindowAccImpl since it inspects the children of its contentItem
    // directly.)
    static void parentOfElementCreatedDestroyed(QQuickItem &child);

    // A parent accessibility element is being created or destroyed.  Check
    // elements for children of the parent Item given.  If those items no longer have a
    // parent, they're destroyed; if they gain a parent, they're created; if the
    // parent just changes, they emit a parent-change.
    static void parentElementCreatedDestroyed(QQuickItem &parent);

public:
    // Create an AccessibleItem for a particular item.  The role cannot be
    // NoRole.
    // The AccessibleItem is parented to item.
    // The QQuickItem can't already have an attached AccessibleItem.
    AccessibleItem(QAccessible::Role role, QQuickItem &item);

    ~AccessibleItem();

    // Not copiable/assignable from QObject, would violate invariant of one
    // AccessibleItem per QQuickItem

private:
    // Find the parent accessibility element for this item.  Sets up connections
    // to onItemParentChainChanged() for all the items observed that led to the
    // parent element.
    // Updates _pParentElement.
    void findParentElement();

    // Handle an ObjectCreate/ObjectDestroy event - emit that event as well as
    // parent changes for the children.
    //
    // WindowAccImpl is notified that a child was created/destroyed in
    // _pItemWindow.  _pItemWindow is still valid when this is called as
    // documented in handleCreateDestroyEvent() / onItemWindowChanged().
    void handleCreateDestroyEvent(QAccessible::Event eventType);

    // Check for creation/destruction after applying a change that affects
    // accExists().  Pass the result of accExists() before applying the change.
    // Returns true if a creation/destruction event was sent, meaning the caller
    // does not have to notify for the change that was applied too.
    bool checkCreateDestroy(bool oldAccExists);

    // Calculate the control's disabled state from the item's enabled flag and
    // the disabled property.
    bool calculateDisabled() const;

    // Calculate the control's focusable state from the item's activeFocusOnTab
    // and the alwaysFocusable property.
    bool calculateFocusable() const;

    // Calculate the control's focus flag from the item's focus flag and
    // _forceFocus.
    bool calculateFocused() const;

    // After the focus delegate has changed, send another "gain focus" to the
    // window if necessary.
    void handleFocusDelegateChanged();

    // Check for the parent changing (including to/from nullptr).
    void checkParentChange();

    // Emit a QAccessibleStateChangeEvent for the given field.
    void emitStateChange(StateField field);

    // Set a State field and emit the state change.
    // (The private implementation can set any state field, including those used
    // by AccessibleItem itself.)
    void setStateImpl(StateField field, bool value);

    void onItemVisibleChanged();
    void onItemParentChainChanged();
    void onItemWindowChanged();

protected:
    // Set a State field.  Emits a QAccessibleStateChangeEvent.
    // Normally, this should be hooked up to one or more QML properties, so it
    // updates as the QML changes.
    // The field can't be a field that's managed by AccessibleItem itself.
    void setState(StateField field, bool value);

    // Get a State field.
    bool getState(StateField field) const;

    // Emit an accessibility event with the given type.
    void emitAccEvent(QAccessible::Event eventType);

    // A 'hidden' override is provided for derived classes' use.  Setting this
    // to true causes the control to report as 'hidden' even if the Item is
    // visible.
    // ScrollBarAttached needs this to hide the scroll bar hints when the scroll
    // bar doesn't appear, the QtQuick Controls 2 ScrollBar does not actually
    // hide itself.
    bool hidden() const {return _hidden;}
    void setHidden(bool hidden);

    // The 'forceFocus' override is used by DropDownMenuItem to work around
    // issues in QML that prevent the drop down's menu items from actually being
    // focused.
    // The item will report itself as focused if this is enabled and it's
    // visible, including reporting focus events to the window.  This is only
    // safe to use if it can be assumed that the "real" focus won't change while
    // this is active.
    // See DropDownMenuItemAttached for details.
    bool forceFocus() const {return _forceFocus;}
    void setForceFocus(bool forceFocus);

    // Table is focusable, but needs to report its cells as the actual focused
    // element on Linux.
    //
    // An Item can set a "focus delegate" which will be specified as the actual
    // focused item if this item gains the focus.  The default is nullptr, which
    // means this item is reported as the focused item normally.
    //
    // This implementation isn't perfect; it's just necessary to have any table
    // functionality at all on Linux.  In particular, this item will still have
    // the 'focused' bit set in its state.
    AccessibleElement *focusDelegate() const {return _pFocusDelegate;}
    void setFocusDelegate(AccessibleElement *pFocusDelegate);

    // Whether the setFocus action is currently applicable
    bool hasSetFocusAction() const;

    // Implementation of QAccessibleActionInterface.
    // Derived types can override these to provide additional actions; normally
    // they should call the base class implementation too for the setFocus
    // action.  If the added actions can change, also emit
    // QAccessible::Event::ActionChanged as appropriate.

    // Get the action names - returns the setFocus action when applicable.
    // An override of this method can add additional actions.  Keep in mind that
    // action order is important, more commonly-used actions should be first.
    virtual QStringList actionNames() const override;
    // Implement actions - implements the setFocus action, and traces for
    // unexpected actions.  An override should implement its additional actions;
    // call the base implementation for actions not handled.
    virtual void doAction(const QString &actionName) override;
    virtual QStringList keyBindingsForAction(const QString &actionName) const override;

public:
    QQuickItem *item() const {return _pItem;}
    // The item's window.  Use this instead of item()->window() so that
    // transients are handled correctly.
    QQuickWindow *window() const {return _pItemWindow;}
    // The item's parent element.
    QAccessibleInterface *parentElement() const {return _pParentElement;}
    QAccessible::Role role() const {return _role;}
    const QAccessible::State &state() const {return _state;}

    // Get the QAccessibleInterface for this AccessibleItem if it exists in
    // the accessibility tree.
    AccessibleImpl *getInterface();

    // Does the item exist in the accessibility annotations?
    // (It doesn't exist when it has no name.)
    // This doesn't necessarily mean that an interface exists right now; it
    // means we told QAccessible that the object was created.
    bool accExists() const;

    QString name() const {return _name;}
    void setName(const QString &name);
    QString description() const {return _description;}
    void setDescription(const QString &description);
    bool disabled() const {return _disabled;}
    void setDisabled(bool disabled);
    bool alwaysFocusable() const {return _alwaysFocusable;}
    void setAlwaysFocusable(bool alwaysFocusable);

    // Get the QAccessibleInterface to use in a focus notification - the one for
    // this object if there is no focus delegate, or the focus delegate
    // otherwise.  Used by WindowAccImpl.
    AccessibleElement *getFocusNotifyInterface();

    // Get accessible children of this AccessibleItem.  The default
    // implementation finds these from the QQuickItem heirarchy, but some types
    // override it to return other children.  (TableAttached returns its table
    // cells.)
    virtual QList<QAccessibleInterface*> getAccChildren() const;

    // Textual value - provided as the QAccessible::Text::Value value by
    // AccessibleImpl.  Roles that have a text-based value (like ComboBox)
    // normally implement this, though it usually requires fixup code to work on
    // Mac.
    virtual QString textValue() const;

    // Additional accessibility interfaces.  These can be overridden to return a
    // valid interface, which enables AccessibleImpl::interface_cast() to return
    // that interface when requested.
    //
    // If a type implements any of these, it is responsible for sending the
    // relevant accessible events for that interface.
    //
    // Qt does not store references to these specific interfaces, so they can be
    // owned by the AccessibleItem.
    virtual QAccessibleTableInterface *tableInterface() {return nullptr;}
    virtual QAccessibleTextInterface *textInterface() {return nullptr;}
    virtual QAccessibleValueInterface *valueInterface() {return nullptr;}
    // actionInterface() doesn't need to be overridden since AccessibleItem
    // itself provides this; this method is just for AccessibleImpl's
    // convenience.
    QAccessibleActionInterface *actionInterface() {return this;}
    // Filler interfaces added by AccessibleElement
    virtual AccessibleTableFiller *tableFillerInterface() {return nullptr;}
    virtual AccessibleRowFiller *rowFillerInterface() {return nullptr;}

signals:
    void nameChanged();
    void descriptionChanged();
    void disabledChanged();
    void alwaysFocusableChanged();
    void labelForChanged();

    // State changes are emitted as a regular signal so derived classes can
    // easily react to them (SingleActionItem does this).
    // 'field' is the affected field.
    void stateChanged(StateField field);

    // This accessibility element is being created.  (Used by items that create
    // custom children like TableAttached; create the children now.)
    void elementCreated();
    // This accessibility element is being destroyed.  (Used by items that create
    // custom children like TableAttached; destroy the children now.)
    void elementDestroyed();

private:
    // The role of this item.
    // This can't change; there's no way to report role changes to QAccessible.
    const QAccessible::Role _role;
    // The item represented by this AccessibleItem.
    // Can be nullptr if there was already an AccessibleItem for this Item.
    QQuickItem *_pItem;
    // When an AccessibleImpl exists, it's identified here.
    // AccessibleItem only has one AccessibleImpl at a time, but it's possible
    // for it to be destroyed and re-created, such as if the item's name or role
    // is cleared and reset.
    QPointer<AccessibleImpl> _pImpl;
    // The object's current state.
    QAccessible::State _state;
    // The QQuickItems traversed to find the parent element - needed only to
    // clean up signal connections when we find the parent element again.  Does
    // not include _pItem.
    QVector<QPointer<QQuickItem>> _parentChain;
    // The current parent element (which may exist even if this element does not
    // exist right now).  This is nullptr if there is no valid parent element.
    // Used to detect parent changes in checkParentChange().
    //
    // AccessibleElements (being QAccessibleInterfaces) are owned by
    // QAccessible; there's no guarantee about the order they'll be destroyed at
    // shutdown, so this is a QPointer just for safety there.
    //
    // In principle, it might make sense to actually watch this object's
    // QObject::destroyed() and check for a parent change at that time.
    // However, this really isn't necessary because the actual destruction of an
    // accessibility element will cause either a parentElementCreatedDestroyed()
    // or a change in the parent chain before the interface is actually
    // destroyed.
    QPointer<AccessibleElement> _pParentElement;

    // Property implementations
    QString _name, _description;
    bool _disabled, _alwaysFocusable;
    // When _pLabelFor is set, an entry for the labelled item is added to
    // the labelledItems map pointing to this AccessibleItem, which allows the
    // labelled item to report its relationship back to the label.
    // AccessibleItem clears this if the labelled QQuickItem is destroyed.
    QQuickItem *_pLabelFor;

    bool _hidden, _forceFocus;
    AccessibleElement *_pFocusDelegate;

    // Item properties - needed in order to handle changes properly, otherwise
    // we wouldn't be able to tell if the property change caused a change in
    // accExists().
    // Always use these values instead of the item's actual properties; this
    // handles transients correctly as the properties change.
    bool _itemVisible;
    QPointer<QQuickWindow> _pItemWindow;
};

}

#endif
