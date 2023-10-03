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
#line SOURCE_FILE("window.cpp")

#include "window.h"
#include "accutil.h"
#include "accessibleitem.h"
#include <QGuiApplication>

#ifdef Q_OS_MAC
#include "../mac/mac_accessibility.h"
#endif

namespace NativeAcc {

namespace
{
    enum : unsigned
    {
#ifdef Q_OS_WIN
        // Like the rest of QAccessible, the Windows UIA backend is a mess.
        //
        // We have a pretty simple requirement - when accessibility is activated
        // for the first time, emit a Focus event to move the Narrator focus
        // into the dashboard.  Without this, Narrator stays on the taskbar (or
        // whereever it was), and it's very tricky to convince Narrator to go to
        // the dashboard without hiding the dashboard.
        //
        // We should be able to just emit a Focus event when accessibility is
        // activated, but the AccEvent spy shows that the event is not always
        // passed through the Windows UIA backend, it ignores the first few
        // events.  There's no way to tell when it will actually start
        // forwarding the event; it's not even consistent - sometimes the 3rd
        // event goes through, sometimes the 6th, etc.
        //
        // So, as a horrible workaround, we emit the activated() signal up to 10
        // times on Windows and hope that one of these Focus events goes
        // through.  Normally, all 10 of these are used up when the dashboard is
        // initially shown.
        //
        // We don't really want to keep emitting this for every activate, since
        // we get an activate event for every window query (this happens a
        // _lot_).  Narrator seems to be OK even if we do, but Windows screen
        // readers are very fragile in general, so we don't want to trust that
        // they'd be OK with this.
        MaxActivateCount = 10,
#else
        MaxActivateCount = 1,
#endif
    };

    ActivateObserver activateObserver;
}

ActivateObserver::ActivateObserver()
    : _activateCount{0}
{
    QAccessible::installActivationObserver(this);
}

ActivateObserver::~ActivateObserver()
{
    // Might have already been removed if QAccessible was activated
    QAccessible::removeActivationObserver(this);
}

void ActivateObserver::accessibilityActiveChanged(bool active)
{
    if(active)
    {
        // Stop emitting this after we reach the max. count - normally 1, but
        // see MaxActivateCount for Windows details
        if(++_activateCount >= MaxActivateCount)
            QAccessible::removeActivationObserver(this);
        emit activated();
    }
}

WindowAccImpl *WindowAccImpl::getAccImpl(QQuickWindow &window)
{
    // Since WindowAccImpl applies to all QQuickWindows, just get the object
    // from QAccessible rather than maintaining our own map.
    QAccessibleInterface *pWindowAcc = QAccessible::queryAccessibleInterface(&window);
    return dynamic_cast<WindowAccImpl*>(pWindowAcc);
}

QAccessibleInterface *WindowAccImpl::interfaceFactory(const QString &,
                                                      QObject *pObject)
{
    QQuickWindow *pObjWindow = qobject_cast<QQuickWindow*>(pObject);
    if(pObjWindow)
        return new WindowAccImpl{*pObjWindow};
    return nullptr;
}

void WindowAccImpl::childCreatedDestroyed(QQuickWindow &window)
{
    WindowAccImpl *pThis = getAccImpl(window);
    if(pThis)
        pThis->childCreatedDestroyed();
}

void WindowAccImpl::elementGainedFocus(QQuickWindow &window, AccessibleItem &item)
{
    WindowAccImpl *pThis = getAccImpl(window);
    if(pThis)
        pThis->elementGainedFocus(item);
}

void WindowAccImpl::elementLostFocus(QQuickWindow &window, AccessibleItem &item)
{
    WindowAccImpl *pThis = getAccImpl(window);
    if(pThis)
        pThis->elementLostFocus(item);
}

WindowAccImpl::WindowAccImpl(QQuickWindow &window)
    : AccessibleElement{&window}, _pWindow{&window}, _showingOverlay{false},
      _windowFocused{false}, _pFocusItem{nullptr}
{
    // Handle changes in the focused window
    QObject::connect(static_cast<QGuiApplication*>(QGuiApplication::instance()),
                     &QGuiApplication::focusWindowChanged, this,
                     &WindowAccImpl::checkWindowFocus);
    // Check if the window is already focused
    checkWindowFocus();

    // If QAccessible becomes activated, and we already have a focused item,
    // emit a focused item event.
    // Note that on Windows, this event actually triggers more than once, see
    // MaxActivateCount above.
    QObject::connect(&activateObserver, &ActivateObserver::activated, this,
        [this]()
        {
            if(canNotifyFocusedItem())
                notifyItemFocused();
        });
}

bool WindowAccImpl::isOverlayLayer(const QQuickItem &item) const
{
    // The item can only possibly be an overlay layer if it's a direct
    // child of contentItem.  An overlay layer farther down in the heirarchy
    // wouldn't make sense, it wouldn't render as intended.
    //
    // Technically, this test is required for correctness if that was to happen,
    // since isAccParentOf() walks the parent chain looking for the overlay
    // layer, while childCreatedDestroyed() just looks at direct children.
    if(!_pWindow || !_pWindow->contentItem())
        return false;
    if(item.parentItem() != _pWindow->contentItem())
        return false;

    // Check if the object is a QQuickOverlay.
    const QMetaObject *pItemMeta = item.metaObject();
    return pItemMeta && pItemMeta->className() == QLatin1Literal("QQuickOverlay");
}

void WindowAccImpl::appendLayerChildren(QList<QAccessibleInterface*> &accChildren,
                                        bool overlayLayer) const
{
    QQuickItem *pContentItem = _pWindow ? _pWindow->contentItem() : nullptr;
    if(!pContentItem)
        return;

    const auto &contentChildren = pContentItem->childItems();

    for(QQuickItem *pChild : contentChildren)
    {
        // If this item is the in the desired layer, append it (or its
        // accessible children if it isn't accessible).
        if(isOverlayLayer(*pChild) == overlayLayer)
            appendAccElementOrChildren(pChild, accChildren);
    }
}

void WindowAccImpl::appendOverlayChildren(QList<QAccessibleInterface*> &accChildren) const
{
    appendLayerChildren(accChildren, true);
}

void WindowAccImpl::appendNonOverlayChildren(QList<QAccessibleInterface*> &accChildren) const
{
    appendLayerChildren(accChildren, false);
}

QList<QAccessibleInterface*> WindowAccImpl::getActiveChildren() const
{
    // The client has at least three types of "in-window modals":
    // - the overlay dialogs in settings (DNS, TAP adapter)
    // - drop down menus (settings)
    // - popup menus (edits, header)
    //
    // For these types, we need:
    // - the tool to announce that the user is now in a modal somehow (so it's
    //   clear that a major change has occurred)
    // - to prevent the user from navigating outside of the modal when it's
    //   active
    // - to move the user back to the normal controls when the modal ends
    //
    // QAccessible doesn't offer a good way to do this.  The Dialog/PopupMenu
    // roles are both pretty broken.
    // - On Mac, Dialog is ignored completely (it maps to Window which Qt always
    //   ignores).  VoiceOver can't differentiate the overlay controls from the
    //   non-overlay controls, so they're intermixed.  We can't convince it to
    //   move to the popup when it's shown, 'modal' and the show events have no
    //   effect on Mac.
    // - On Windows, the Dialog type does act like a group, and Narrator moves
    //   to it when the 'show' event occurs, but the user can still easily
    //   accidentally navigate out to the blocked controls.  We don't really
    //   want Narrator to move to the dialog itself either, we want it on the
    //   first focused control.
    //
    // Even Qt Creator's dropdowns in its preferences do not work at all with
    // VoiceOver, it seems QAccessible just can't really handle these right.
    //
    // The best we can do to meet this is to detect when the QML Overlay layer
    // is active, and only report overlay-layer objects when it is.

    QList<QAccessibleInterface*> accChildren;

    // Always include the overlay children.
    //
    // During a transient (while we're transitioning to _showingOverlay=true),
    // both sets of children could be present at the same time; this simplifies
    // the transitions between states.
    appendOverlayChildren(accChildren);
    if(!_showingOverlay)
        appendNonOverlayChildren(accChildren);

    return accChildren;
}

void WindowAccImpl::childCreatedDestroyed()
{
#ifdef Q_OS_MAC
    // Subclass the QMacAccessibilityElement for the window itself
    macSubclassInterfaceElement(*this);
#endif

    // This is a somewhat naive implementation - if we're showing the overlay,
    // just check if there are no more overlay children; or if we're not showing
    // the overlay, check if there are overlay children now.
    //
    // In principle, this check could be optimized, such as by checking where
    // the item is in the heirarchy (only direct accessibility descendants in
    // the overlay layer have any effect) or the type of event (destruction in
    // the overlay layer doesn't matter when _showingOverlay==false, etc.)
    QList<QAccessibleInterface*> overlayChildren;
    appendOverlayChildren(overlayChildren);
    bool hasOverlayChildren = !overlayChildren.isEmpty();

    if(_showingOverlay != hasOverlayChildren)
    {
        // Change to or from the overlay layer.
        _showingOverlay = hasOverlayChildren;

        // Notify the non-overlay children that their parent has been
        // created or destroyed, since WindowAccImpl::isAccParentOf() has
        // changed whether it will accept them.
        // Note that we do this on the QQuickItems, not accessibility
        // interfaces, because the accessibility interfaces do not exist for
        // these items yet if we're switching to the non-overlay layer.
        QQuickItem *pContentItem = _pWindow ? _pWindow->contentItem() : nullptr;
        if(pContentItem)
        {
            const auto &contentChildren = pContentItem->childItems();
            for(QQuickItem *pChild : contentChildren)
            {
                if(pChild && !isOverlayLayer(*pChild))
                    AccessibleItem::parentOfElementCreatedDestroyed(*pChild);
            }
        }
    }
}

void WindowAccImpl::clearFocusItem()
{
    if(_pFocusItem)
    {
        _pFocusItem->disconnect(this);
        _pFocusItem = nullptr;
    }
}

bool WindowAccImpl::canNotifyFocusedItem() const
{
    // We can notify that an item is focused if:
    // - the window is focused
    // - there is a focused item
    // - the focused item's acc element exists
    return _windowFocused && _pFocusItem && _pFocusItem->accExists();
}

void WindowAccImpl::elementGainedFocus(AccessibleItem &item)
{
    // Even if item is already in _pFocusItem, we still want to send a
    // notification - for whatever reason, it thinks it has gained the focus
    // again.  This can happen for objects like tables that delegate their focus
    // notification to a child object (a cell in the table).

    clearFocusItem();

    _pFocusItem = &item;

    // If the element is created later (while it's still focused), emit a
    // focus notification.
    // Do this even if we emit a focus notification now, because the element
    // could be destroyed and recreated while it still has the focus.
    QObject::connect(_pFocusItem.data(), &AccessibleItem::elementCreated, this,
        [this]()
        {
            if(canNotifyFocusedItem())
                notifyItemFocused();
        });

    // If we're ready to notify about the focused element, do it now.
    if(canNotifyFocusedItem())
        notifyItemFocused();
}

void WindowAccImpl::elementLostFocus(AccessibleItem &item)
{
    // If the item actually is the focused item, just clear it.  We don't rely
    // on any particular notification order between an element gaining focus and
    // an element losing focus, so we might have already moved to a new item.
    if(_pFocusItem == &item)
        clearFocusItem();
}

AccessibleItem *WindowAccImpl::findFocusedItem() const
{
    if(!_pWindow)
        return nullptr;

    // If this window isn't focus, we don't report a focused accessibility
    // element.
    if(_pWindow != QGuiApplication::focusWindow())
        return nullptr;

    QQuickItem *pActiveFocusItem = _pWindow->activeFocusItem();
    if(!pActiveFocusItem)
        return nullptr;

    return AccessibleItem::getAccItem(pActiveFocusItem);
}

void WindowAccImpl::checkWindowFocus()
{
    bool oldWindowFocused = _windowFocused;
    auto currentState = state();
    _windowFocused = currentState.active && !currentState.invisible;

    if(oldWindowFocused != _windowFocused)
    {
        qInfo() << "window" << (_pWindow ? _pWindow->title() : "nullptr") << "focus" << oldWindowFocused << "->" << _windowFocused;
    }

    // If the window has become focused, an item is focused in this window, and
    // the item's accessibility element exists, emit an element-focus event.
    if(!oldWindowFocused && canNotifyFocusedItem())
        notifyItemFocused();
}

AccessibleElement *WindowAccImpl::focusedElement() const
{
    if(!canNotifyFocusedItem())
        return nullptr;

    // Consequences of canNotifyFocusedItem():
    Q_ASSERT(_pFocusItem);
    Q_ASSERT(_pFocusItem->accExists());

    // Normally this is the interface to the item itself, but some controls
    // like Table need to delegate focus to child elements (the Table's cells).
    return _pFocusItem->getFocusNotifyInterface();
}

void WindowAccImpl::notifyItemFocused() const
{
    // Callers already checked canNotifyFocusedItem()
    Q_ASSERT(canNotifyFocusedItem());
    // Consequences of canNotifyFocusedItem() assumed below:
    Q_ASSERT(_windowFocused);
    Q_ASSERT(_pFocusItem);
    Q_ASSERT(_pFocusItem->accExists());

    // Callers already checked canNotifyFocusedItem(), so this is almost
    // certainly valid, but it theoretically could fail if an object fails to
    // create its own accessibility interface.  That shouldn't be possible by
    // this point, but there's no hard guarantee.
    // Usually this is just _pFocusItem, but it could be a child item for Table.
    AccessibleElement *pFocusElement = focusedElement();
    if(!pFocusElement)
    {
        qInfo() << "can't notify focus, object did not return a focused element interface"
            << _pFocusItem;
        return;
    }

    // Resolve ambiguous QAccessibleEvent constructor
    QAccessibleInterface *pFocusElementItf = pFocusElement;

    QAccessibleEvent focusEvent{pFocusElementItf, QAccessible::Event::Focus};
    QAccessible::updateAccessibility(&focusEvent);
}

bool WindowAccImpl::isAccParentOf(QQuickItem &item) const
{
    // If we're not showing the overlay, any child item is valid.  (Overlay
    // items can always be shown, since they'll hide non-overlay items; in this
    // state non-overlay items can be shown too.)
    if(!_showingOverlay)
        return true;

    // We're showing the overlay; figure out if this item is in the overlay
    // layer.
    QQuickItem *pParent = &item;
    while(pParent)
    {
        if(isOverlayLayer(*pParent))
            return true;    // It's in the overlay layer, show it
        pParent = pParent->parentItem();
    }

    // It's not in the overlay layer, hide it.
    return false;
}

QAccessibleInterface *WindowAccImpl::child(int index) const
{
    const auto &children = getActiveChildren();
    if(index >= 0 && index < children.size())
        return children[index];
    return nullptr;
}

int WindowAccImpl::childCount() const
{
    // Some of these child method implementations are pretty naive, they
    // shouldn't be a significant performance impact though.
    return getActiveChildren().size();
}

QAccessibleInterface *WindowAccImpl::focusChild() const
{
    return focusedElement();
}

int WindowAccImpl::indexOfChild(const QAccessibleInterface *child) const
{
    // QList<QAccessibleInterface*>::indexOf() requires a non-const argument...
    return getActiveChildren().indexOf(const_cast<QAccessibleInterface*>(child));
}

void *WindowAccImpl::interface_cast(QAccessible::InterfaceType)
{
    return nullptr;
}

bool WindowAccImpl::isValid() const
{
    return _pWindow && QAccessibleObject::isValid();
}

QAccessibleInterface *WindowAccImpl::parent() const
{
    // All our windows are top-level windows
    return QAccessible::queryAccessibleInterface(qApp);
}

QRect WindowAccImpl::rect() const
{
    return _pWindow ? _pWindow->geometry() : QRect{};
}

QVector<QPair<QAccessibleInterface*, QAccessible::Relation>> WindowAccImpl::relations(QAccessible::Relation) const
{
    return {};
}

QAccessible::Role WindowAccImpl::role() const
{
    return QAccessible::Role::Window;
}

void WindowAccImpl::setText(QAccessible::Text, const QString &)
{
}

QAccessible::State WindowAccImpl::state() const
{
    QAccessible::State winState{};
    winState.active = _pWindow && _pWindow == QGuiApplication::focusWindow();
    winState.invisible = !_pWindow || _pWindow->visibility() == QWindow::Visibility::Hidden;
    return winState;
}

QString WindowAccImpl::text(QAccessible::Text t) const
{
    if(t == QAccessible::Text::Name && _pWindow)
        return _pWindow->title();
    return {};
}

QWindow *WindowAccImpl::window() const
{
    return _pWindow;
}

}
