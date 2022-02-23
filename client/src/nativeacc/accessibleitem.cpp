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
#line SOURCE_FILE("accessibleitem.cpp")

#include "accessibleitem.h"
#include "accessibleimpl.h"
#include "accutil.h"
#include "window.h"
#include <unordered_map>

#ifdef Q_OS_MAC
#include "mac/mac_accessibility.h"
#endif

namespace NativeAcc {

namespace
{
    // Map of all active AccessibleItems and their QQuickItems.
    // The QQuickItems are stored as QObjects since the interface factory
    // receives QObjects.
    // There are no null keys or values in this map.
    std::unordered_map<QObject*, AccessibleItem*> accessibleItems;
}

AccessibleItem *AccessibleItem::getAccItem(QObject *pObject)
{
    const auto &itItem = accessibleItems.find(pObject);
    if(itItem != accessibleItems.end())
    {
        Q_ASSERT(itItem->second);   // Class invariant, no nullptrs here
        return itItem->second;
    }

    return nullptr;
}

AccessibleImpl *AccessibleItem::getInterface(QObject *pObject)
{
    AccessibleItem *pAccItem = getAccItem(pObject);
    if(pAccItem)
        return pAccItem->getInterface();

    return nullptr;
}

void AccessibleItem::parentOfElementCreatedDestroyed(QQuickItem &child)
{
    // Is there an AccessibleItem for this item?
    const auto &itAccItem = accessibleItems.find(&child);
    if(itAccItem == accessibleItems.end())
    {
        // No, recurse into this item's children.
        parentElementCreatedDestroyed(child);
    }
    else
    {
        // Notify this item.  It may recurse into its own children if it
        // does not exist and is not created.
        Q_ASSERT(itAccItem->second);    // No nullptrs in map
        itAccItem->second->checkParentChange();
    }
}

void AccessibleItem::parentElementCreatedDestroyed(QQuickItem &parent)
{
    const auto &childItems = parent.childItems();

    for(QQuickItem *pChild : childItems)
    {
        if(!pChild)
            continue;

        parentOfElementCreatedDestroyed(*pChild);
    }
}

AccessibleItem::AccessibleItem(QAccessible::Role role, QQuickItem &item)
    : QObject{&item},    // Parent to item
      _role{role}, _pItem{&item}, _pParentElement{nullptr}, _disabled{false},
      _alwaysFocusable{false},  _hidden{false}, _forceFocus{false},
      _pFocusDelegate{nullptr}, _itemVisible{true}, _pItemWindow{nullptr}
{
    // Doesn't make sense to create an AccessibleItem with no role.  (We'd have
    // to check this and avoid creating an interface.)
    Q_ASSERT(_role != QAccessible::NoRole);

    // There shouldn't already be an AccessibleItem for this QQuickItem, but we
    // can't guarantee it since the QML code could try to use more than one
    // NativeAcc attached type on the same Item.
    // If it happens, log a warning, and don't add the second type to
    // accessibleItems (the first one will determine the returned interface).
    if(accessibleItems.count(_pItem))
    {
        qWarning() << "Attempted to attach more than one accessible type to"
            << &item;
        // This AccessibleItem can't do anything, wipe out _pItem since another
        // AccessibleItem is already attached to it.  (Stay parented to the Item
        // though.)
        _pItem = nullptr;
        return;
    }

    // Attach the AccessibleImpl for this object.  (Get the existing one if it
    // exists, or create it now if it hasn't been created yet.)
    // Do this before putting this item in accessibleItems, which allows
    // AccessibleImpl to verify that it's never created later after an
    // AccessibleItem already exists.
    QAccessibleInterface *pAccItf = QAccessible::queryAccessibleInterface(_pItem);
    AccessibleImpl *pAccImpl = dynamic_cast<AccessibleImpl*>(pAccItf);
    if(!pAccImpl || !pAccImpl->attach(*this))
    {
        // The interface wasn't an AccessibleImpl - somehow we failed to
        // override QML Accessible, etc.
        // Or, we couldn't attach the interface, somehow it was already attached
        // to another AccessibleImpl.
        // (Or, no interface was created at all, but this should never happen
        // because AccessibleImpl _always_ creates an interface for any
        // QQuickItem.)
        qWarning() << "Could not attach accessible interface for" << _pItem
            << "interface:" << pAccImpl;
        // This annotation can't do anything, we're hosed.  Wipe out _pItem and
        // don't add to accessibleItems.
        _pItem = nullptr;
        return;
    }

    _pImpl = pAccImpl;
    accessibleItems[_pItem] = this;

    // Hook up and initialize state-determining properties.

    // The 'active' state represents focus within a group - analogous to
    // Item.focus.
    QObject::connect(_pItem, &QQuickItem::focusChanged, this,
                     [this](){setStateImpl(StateField::active, _pItem->hasFocus());});
    setStateImpl(StateField::active, _pItem->hasFocus());

    // The 'focused' state represents actual active focus, like
    // Item.activeFocus.
    QObject::connect(_pItem, &QQuickItem::activeFocusChanged, this,
                     [this](){setStateImpl(StateField::focused, calculateFocused());});
    setStateImpl(StateField::focused, calculateFocused());

    // focusable -> activeFocusOnTab
    QObject::connect(_pItem, &QQuickItem::activeFocusOnTabChanged, this,
                     [this](){setStateImpl(StateField::focusable, calculateFocusable());});
    setStateImpl(StateField::focusable, calculateFocusable());

    // disabled -> (!enabled || disabled)
    QObject::connect(_pItem, &QQuickItem::enabledChanged, this,
                     [this](){setStateImpl(StateField::disabled, calculateDisabled());});
    setStateImpl(StateField::disabled, calculateDisabled());

    // The visible flag creates/destroys the accessibility element
    QObject::connect(_pItem, &QQuickItem::visibleChanged, this,
                     &AccessibleItem::onItemVisibleChanged);
    _itemVisible = _pItem->isVisible();

    // If the item's window changes, we have to check for
    // creation/destruction.
    QObject::connect(_pItem, &QQuickItem::windowChanged, this,
                     &AccessibleItem::onItemWindowChanged);
    // The item may already have a window.
    _pItemWindow = _pItem->window();

    // Figure out our current parent accessibility element.
    // _pItem's parent always affects this, connect that signal and do not
    // add it to the parent chain.
    QObject::connect(_pItem, &QQuickItem::parentChanged, this,
                     &AccessibleItem::onItemParentChainChanged);
    findParentElement();

    // We don't need to check for creation now - it's not possible for the
    // item to exist yet since it has no name.
    Q_ASSERT(!accExists());
}

AccessibleItem::~AccessibleItem()
{
    if(!_pItem)
        return;

    // We're not emitting an ObjectDestroyed event here, even though the Item is
    // being destroyed.  Seems like it'd make sense to emit it, but the backends
    // don't seem to expect it.  (The Mac and Win32 backends do not use
    // ObjectDestroyed at all, and the Mac backend crashes if we emit it here.)
    // Presumably they detect destruction from the QObject itself in this case.

    auto itMapEntry = accessibleItems.find(_pItem);
    // This can only fail if there was more than one AccessibleItem created for
    // the same QQuickItem.
    if(itMapEntry != accessibleItems.end() && itMapEntry->second == this)
        accessibleItems.erase(itMapEntry);
}

void AccessibleItem::findParentElement()
{
    if(!_pItem)
        return;

    // Disconnect everything from the parent chain changed slot; we're
    // rebuilding these connections
    for(const QPointer<QQuickItem> &pParentChainItem : _parentChain)
    {
        if(pParentChainItem)
        {
            QObject::disconnect(pParentChainItem, &QQuickItem::parentChanged,
                                this, &AccessibleItem::onItemParentChainChanged);
        }
    }
    _parentChain.clear();

    // The connection from _pItem remains, its parent property always matters.

    // Walk parents from _pItem until we find an accessible one.
    QQuickItem *pParent = _pItem->parentItem();
    while(pParent)
    {
        AccessibleImpl *pParentAccItf = AccessibleItem::getInterface(pParent);
        if(pParentAccItf)
        {
            // Found a parent, we're done.  Don't need to connect to this item's
            // parent change.
            _pParentElement = pParentAccItf;
            return;
        }

        // Go to this item's parent.  Traverse its 'parentItem' property.
        QObject::connect(pParent, &QQuickItem::parentChanged, this,
                         &AccessibleItem::onItemParentChainChanged);
        _parentChain.push_back(pParent);
        pParent = pParent->parentItem();
    }

    // Didn't find any accessible parent, check the window.
    QAccessibleInterface *pWindowAccItf = nullptr;
    if(_pItemWindow)
        pWindowAccItf = QAccessible::queryAccessibleInterface(_pItemWindow);
    // This should be a WindowAccImpl; ask it if it's a valid parent, it ignores
    // non-overlay children when the overlay is active.
    WindowAccImpl *pWindowAccImpl = dynamic_cast<WindowAccImpl*>(pWindowAccItf);
    if(pWindowAccImpl && pWindowAccImpl->isAccParentOf(*_pItem))
    {
        // The window is the parent.  Keep the parent chain connections, if one
        // of them changes parent, that might give us a new parent.
        _pParentElement = pWindowAccImpl;
    }
    else
    {
        // There's no parent, the window didn't exist or rejected this item.
        _pParentElement = nullptr;
    }
}

void AccessibleItem::handleCreateDestroyEvent(QAccessible::Event eventType)
{
    Q_ASSERT(_pItem);   // Checked by caller

    // This only makes sense for a Create or Destroy event
    Q_ASSERT(eventType == QAccessible::Event::ObjectCreated ||
             eventType == QAccessible::Event::ObjectDestroyed);

    // Notify children of the creation/destruction, so they can emit parent
    // changes, create/destroy themselves, etc.
    parentElementCreatedDestroyed(*_pItem);

    // Emit the event.  (Don't use emitAccEvent, it would skip a 'destroy'
    // event because the object no longer exists.)
    // Note that this will cause the QAccessibleInterface to be created if it
    // hasn't already been created (and it can be created).
    QAccessibleEvent event{_pItem, eventType};
    QAccessible::updateAccessibility(&event);

#ifdef Q_OS_MAC
    // QAccessible doesn't emit the create/destroy events properly on Mac, we
    // have to pick up the slack ourselves.
    if(_pImpl)
    {
        if(eventType == QAccessible::Event::ObjectCreated)
            macPostAccCreated(*_pImpl);
        else
            macPostAccDestroyed(*_pImpl);
    }
#endif

    // Notify the window that a child was created or destroyed.
    // checkCreateDestroy() and onItemWindowChanged() ensure that _pItemWindow
    // is valid at this point, even if we are being destroyed.
    WindowAccImpl::childCreatedDestroyed(*_pItemWindow);

    if(eventType == QAccessible::Event::ObjectCreated)
        emit elementCreated();
    else
        emit elementDestroyed();
}

bool AccessibleItem::checkCreateDestroy(bool oldAccExists)
{
    bool newAccExists = accExists();
    if(!oldAccExists && newAccExists)
    {
        // For a creation, the item's window is valid because the element exists
        // now.
        Q_ASSERT(_pItemWindow);
        handleCreateDestroyEvent(QAccessible::Event::ObjectCreated);
        return true;
    }
    else if(oldAccExists && !newAccExists)
    {
        // Destruction due to the window becoming nullptr is handled by
        // onItemWindowChanged(); in that case we notify the prior window.  For
        // any other type of destruction, the window is still valid, because
        // some other change just caused the element to be destroyed.
        Q_ASSERT(_pItemWindow);
        handleCreateDestroyEvent(QAccessible::Event::ObjectDestroyed);
        return true;
    }

    // _pItemWindow might not be valid if we were not created or destroyed (if
    // the element didn't exist and still doesn't exist).

    return false;
}

bool AccessibleItem::calculateDisabled() const
{
    bool itemEnabled = _pItem ? _pItem->isEnabled() : false;
    return !itemEnabled || _disabled;
}

bool AccessibleItem::calculateFocusable() const
{
    bool itemFocusable = _pItem ? _pItem->activeFocusOnTab() : false;
    return itemFocusable || _alwaysFocusable;
}

bool AccessibleItem::calculateFocused() const
{
    bool itemFocused = _pItem ? _pItem->hasActiveFocus() : false;
    return itemFocused || _forceFocus;
}

void AccessibleItem::handleFocusDelegateChanged()
{
    // The focus delegate has changed, so if this item is focused, tell the
    // window.  It will emit a focus change if appropriate.
    if(_pItemWindow && getState(StateField::focused))
        WindowAccImpl::elementGainedFocus(*_pItemWindow, *this);
}

void AccessibleItem::checkParentChange()
{
    if(!_pItem)
        return;

    bool oldAccExists = accExists();
    // Get the new parent element.
    findParentElement();

    // Check if this causes this element to be created or destroyed.  If it
    // does, the create/destroy event notifies our own children to check their
    // parent again, since we were created or destroyed.
    if(!checkCreateDestroy(oldAccExists))
    {
        // We weren't created or destroyed.  If this element exists, its parent
        // has changed.
        // If this element doesn't exist, we have to notify our own children,
        // because they would use _pParentElement as their parent too.
        // (If we do exist, they use us as their parent, so they do not observe
        // any change.)
        if(accExists())
            emitAccEvent(QAccessible::Event::ParentChanged);
        else
            parentElementCreatedDestroyed(*_pItem);
    }
}

void AccessibleItem::emitStateChange(StateField field)
{
    Q_ASSERT(_pItem);   // Checked by caller

#ifdef Q_OS_MACOS
    // Qt stupidly maps the StateChange type to
    // NSAccessibilityValueChangedNotification, even if it's a non-value state
    // flag like 'focused' or 'active'. For example, navigating through a series
    // of check/radio buttons would cause VO to announce the _previous_ item
    // each time the focus changes due to this bogus event.
    //
    // It seems that the only state flags that actually affect the "value" on
    // Mac are checked/checkStateMixed.
    switch(field)
    {
        case StateField::checked:
        case StateField::checkStateMixed:
            // Makes sense for value; continue
            break;
        default:
            // Not a value flag.  Don't emit the event.  (There isn't another
            // AppKit event we should emit; in particular focus/active is
            // handled just by the window emitting a focus change.)
            return;
    }
#endif

    // Emit a change with this field flagged.
    // It's possible to batch these up if multiple changes occur at once,
    // but we mostly connect these to independent properties, so we just
    // emit them individually.
    QAccessible::State stateChange{};
    setStateField(stateChange, field, true);
    QAccessibleStateChangeEvent stateEvent{_pItem, stateChange};
    QAccessible::updateAccessibility(&stateEvent);
}

void AccessibleItem::setStateImpl(StateField field, bool value)
{
    if(_pItem && getStateField(_state, field) != value)
    {
        bool oldHasSetFocus = hasSetFocusAction();

        setStateField(_state, field, value);

        // Does this item "exist" for QAccessible?
        if(accExists())
        {
            // Emit accessibility events for this field.  Mainly this is just
            // the QAccessibleStateChangeEvent itself, but a few fields have
            // special behavior per doc.
            switch(field)
            {
                case StateField::invisible:
                    // The Qt doc doesn't clarify this, but per the MS doc, the
                    // 'invisible' flag only emits show/hide events, not a
                    // regular state change event.
                    // https://docs.microsoft.com/en-us/windows/desktop/WinAuto/event-constants
                    emitAccEvent(value ? QAccessible::Event::ObjectHide : QAccessible::Event::ObjectShow);
                    break;
                case StateField::focused:
                    // Inform the window of the focus change
                    if(_pItemWindow)
                    {
                        if(value)
                            WindowAccImpl::elementGainedFocus(*_pItemWindow, *this);
                        else
                            WindowAccImpl::elementLostFocus(*_pItemWindow, *this);
                    }
                    // Still emit a normal state change
                    emitStateChange(field);
                    break;
                default:
                    // Just a normal state change
                    emitStateChange(field);
                    break;
            }

            emit stateChanged(field);

            // If the actions list changed, emit that change too.
            if(hasSetFocusAction() != oldHasSetFocus)
                emitAccEvent(QAccessible::Event::ActionChanged);
        }
    }
}

void AccessibleItem::onItemVisibleChanged()
{
    // Valid, connected to a signal from this item
    Q_ASSERT(_pItem);

    // Item visibility is handled by destroying the accessibility element, _not_
    // with the 'invisible' state flag.  The invisible state flag does not work
    // well on any tested platform:
    // - Windows (Narrator) - Completely ignores 'invisible', navigates the
    //   controls anyway.  Behaves very poorly on the Settings window's stack
    //   layout.
    // - Mac OS (VoiceOver) - Qt maps 'invisible' to 'ignored'.  VoiceOver
    //   respects 'ignored' when navigating, but does not care if the item under
    //   the VoiceOver cursor becomes ignored.  If the item is destroyed, it
    //   moves to a nearby item that exists, which is better UX.  (Note that the
    //   visual indicator does not move, but the actual VO cursor does - for
    //   example, if you're in the settings window on a check box and press
    //   Cmd+Shift+} to move to another tab, then read the current item with
    //   VO+A, the cursor has moved back to the Settings tabs.)

    bool oldAccExists = accExists();
    _itemVisible = _pItem->isVisible();
    checkCreateDestroy(oldAccExists);
}

void AccessibleItem::onItemParentChainChanged()
{
    checkParentChange();
}

void AccessibleItem::onItemWindowChanged()
{
    // Valid, connected to a signal from this item
    Q_ASSERT(_pItem);

    QQuickWindow *pNewWindow = _pItem->window();
    if(pNewWindow == _pItemWindow)
        return;

    // If we exist currently, destroy the item.  (If the item is moving to a new
    // window, we'll then create it again.)
    if(accExists())
    {
        // _pItemWindow is still valid because the element exists; we haven't
        // cleared it yet so the window can be notified of destruction.
        Q_ASSERT(_pItemWindow);
        handleCreateDestroyEvent(QAccessible::Event::ObjectDestroyed);
    }

    _pItemWindow = pNewWindow;

    // If the item exists now, create it.
    if(accExists())
    {
        // _pItemWindow is set to the new window now, valid because the element
        // exists.
        Q_ASSERT(_pItemWindow);
        handleCreateDestroyEvent(QAccessible::Event::ObjectCreated);
    }
}

void AccessibleItem::setState(StateField field, bool value)
{
    // A derived type can't set a state field managed by AccessibleItem.
    Q_ASSERT(field != StateField::active);
    Q_ASSERT(field != StateField::focused);
    Q_ASSERT(field != StateField::focusable);
    Q_ASSERT(field != StateField::offscreen);
    Q_ASSERT(field != StateField::disabled);
    // Don't use invisible, hide the item instead with setHidden().  See
    // onItemVisibleChanged().
    Q_ASSERT(field != StateField::invisible);
    setStateImpl(field, value);
}

bool AccessibleItem::getState(StateField field) const
{
    return getStateField(_state, field);
}

void AccessibleItem::emitAccEvent(QAccessible::Event eventType)
{
    // Only emit the event if this object actually 'exists' from QAccessible's
    // perspective (we indicate that it exists when name is set)
    if(_pItem && accExists())
    {
        QAccessibleEvent event{_pItem, eventType};
        QAccessible::updateAccessibility(&event);
    }
}

void AccessibleItem::setHidden(bool hidden)
{
    if(!_pItem || _hidden == hidden)
        return;

    bool oldAccExists = accExists();

    _hidden = hidden;
    checkCreateDestroy(oldAccExists);
}

void AccessibleItem::setForceFocus(bool forceFocus)
{
    if(!_pItem || forceFocus == _forceFocus)
        return;

    _forceFocus = forceFocus;
    setStateImpl(StateField::focused, calculateFocused());
}

void AccessibleItem::setFocusDelegate(AccessibleElement *pFocusDelegate)
{
    if(_pFocusDelegate == pFocusDelegate)
        return;

    if(_pFocusDelegate)
    {
        QObject::disconnect(_pFocusDelegate, &QObject::destroyed, this, nullptr);
        _pFocusDelegate = nullptr;
    }

    _pFocusDelegate = pFocusDelegate;

    if(_pFocusDelegate)
    {
        // Reset back to this item if the delegate is destroyed
        QObject::connect(_pFocusDelegate, &QObject::destroyed, this,
            [this]()
            {
                // Wipe out _pFocusDelegate - do not call setFocusDelegate
                // because the AccessibleElement that was pointed to is no
                // longer valid.
                _pFocusDelegate = nullptr;
                handleFocusDelegateChanged();
            });
    }

    handleFocusDelegateChanged();
}

bool AccessibleItem::hasSetFocusAction() const
{
    return getState(StateField::focusable) && !getState(StateField::disabled);
}

QStringList AccessibleItem::actionNames() const
{
    QStringList actions;

    if(hasSetFocusAction())
        actions.push_back(QAccessibleActionInterface::setFocusAction());

    return actions;
}

void AccessibleItem::doAction(const QString &actionName)
{
    if(actionName == QAccessibleActionInterface::setFocusAction())
    {
        // Act as if the focus was received from the keyboard.  We definitely
        // want this to scroll parent views to this item; using the tab focus
        // reason causes the focus cue to be revealed, which causes the view to
        // scroll.  Revealing the focus cue seems to be the typical behavior in
        // most apps too, but it's not 100% consistent.
        if(item())
            item()->forceActiveFocus(Qt::FocusReason::TabFocusReason);
    }
    else
    {
        qWarning() << "Unknown action" << actionName << "activated on" << this
            << "->" << item();
    }
}

QStringList AccessibleItem::keyBindingsForAction(const QString &) const
{
    // Stub, key bindings not supported right now
    return {};
}

AccessibleImpl *AccessibleItem::getInterface()
{
    // If the item doesn't exist, do not return an interface.  We don't want
    // items that no longer exist to be reported as parents/children of other
    // items.
    if(!accExists())
        return nullptr;

    // If the object exists, we should have a valid interface - the constructor
    // would have created it.  (QAccessible shouldn't be destroying the
    // AccessibleImpl objects before the QObject is destroyed - if it did, the
    // interface factory would have to handle re-attaching them to an existing
    // AccessibleItem.)
    Q_ASSERT(_pImpl);
    return _pImpl;
}

bool AccessibleItem::accExists() const
{
    // - AccessibleItem must be managing an item (_pItem)
    // - The item must be visible (_itemVisible)
    // - The item must not be hidden by a derived type (!_hidden)
    // - The item must have a parent (_pParentElement)
    // - The item must have a window (_pItemWindow)
    // - The item must have a name (!_name.isEmpty())
    return _pItem && _itemVisible && !_hidden && _pParentElement && _pItemWindow
        && !_name.isEmpty();
}

void AccessibleItem::setName(const QString &name)
{
    // If the name hasn't changed, there's nothing to do.
    // If we aren't managing an Item, there's also nothing to do.
    if(_name == name || !_pItem)
        return;

    bool oldAccExists = accExists();

    _name = name;
    emit nameChanged();

    // If the object is created or destroyed due to this change, no need to emit
    // a name change.
    if(!checkCreateDestroy(oldAccExists))
        emitAccEvent(QAccessible::Event::NameChanged);
}

void AccessibleItem::setDescription(const QString &description)
{
    if(_description == description || !_pItem)
        return;

    _description = description;
    emit descriptionChanged();
    emitAccEvent(QAccessible::Event::DescriptionChanged);
}

void AccessibleItem::setDisabled(bool disabled)
{
    if(disabled == _disabled || !_pItem)
        return;

    _disabled = disabled;
    emit disabledChanged();
    setStateImpl(StateField::disabled, calculateDisabled());
}

void AccessibleItem::setAlwaysFocusable(bool alwaysFocusable)
{
    if(alwaysFocusable == _alwaysFocusable || !_pItem)
        return;

    _alwaysFocusable = alwaysFocusable;
    emit alwaysFocusableChanged();
    setStateImpl(StateField::focusable, calculateFocusable());
}

AccessibleElement *AccessibleItem::getFocusNotifyInterface()
{
    if(_pFocusDelegate)
        return _pFocusDelegate;
    return getInterface();
}

QList<QAccessibleInterface*> AccessibleItem::getAccChildren() const
{
    return NativeAcc::getAccChildren(_pItem);
}

QString AccessibleItem::textValue() const
{
    return {};
}

}
