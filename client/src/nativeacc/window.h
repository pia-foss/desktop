// Copyright (c) 2024 Private Internet Access, Inc.
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
#line HEADER_FILE("window.h")

#ifndef NATIVEACC_WINDOW_H
#define NATIVEACC_WINDOW_H

#include "accessibleitem.h"
#include "accutil.h"
#include "interfaces.h"
#include <kapps_core/src/winapi.h>
#include <QAccessibleObject>
#include <QQuickWindow>
#include <QPointer>

namespace NativeAcc {

// We have to observe when QAccessible is activated in order to emit a focus
// change then.  This is critical for MS Narrator to move to the dashboard when
// it's launched; otherwise it fails to do this.
//
// There's no other point to emit that initial event; the window can't emit it
// when it's created or shown, because QAccessible isn't active yet.  It's
// activated when a screen reader queries a window, which only occurs after a
// window is shown.
//
// On top of that, QAccessible uses an interface+hook-install sort of pattern,
// apparently a signal would have been asking too much.  It's _also_ too dumb
// to call this only when initially activated, it's called on Windows for every
// WM_GETOBJECT message that's handled.
//
// So this shim fixes that up and emits a signal only when activated the first
// time.  (On Windows, it actually emits this for more than one activation, but
// does stop eventually, see MaxActivateCount.)
//
// This class is an implementation detail of WindowAccImpl but has to be in the
// header for MOC (for combined-source compilation anyway).
class ActivateObserver : public QObject, private QAccessible::ActivationObserver
{
    Q_OBJECT
public:
    ActivateObserver();
    // Not assignable/copiable per QObject
    // Must remove activation observer in dtor
    virtual ~ActivateObserver() override;

private:
    // Implementation of QAccessible::ActivationObserver
    virtual void accessibilityActiveChanged(bool active) override;

signals:
    void activated();

private:
    unsigned _activateCount;
};

// Implementation of QAccessibleInterface for QQuickWindow that includes
// NativeAcc-implemented children.
// (The default implementation only includes children accessible with the QML
// Accessible type.)
class WindowAccImpl : public AccessibleElement
{
private:
    static WindowAccImpl *getAccImpl(QQuickWindow &window);

public:
    static QAccessibleInterface *interfaceFactory(const QString &className,
                                                  QObject *pObject);

    // AccessibleItem calls this to notify that a (possibly indirect) child of a
    // window is created or destroyed.  This can cause the window to switch to
    // or from the overlay layer.
    static void childCreatedDestroyed(QQuickWindow &window);

    // AccessibleItem calls these notify that it has gained or lost focus
    // within its window.
    //
    // We can't use QQuickWindow::activeFocusItem() for this.  That changes
    // before the item is notified that it's focused, so we would send the focus
    // change before the accessibility element actually says it's focused, which
    // does not work.
    //
    // We also can't just have the item emit the focus event, because it
    // shouldn't be emitted when the window isn't focused, and it needs to be
    // emitted later if the window is focused when an item is already focused.
    //
    // This may emit a focused-element event (if the window is also visible and
    // focused).  WindowAccImpl will also emit a focused-element event later if
    // the window becomes focused.
    static void elementGainedFocus(QQuickWindow &window, AccessibleItem &item);
    static void elementLostFocus(QQuickWindow &window, AccessibleItem &item);

public:
    WindowAccImpl(QQuickWindow &window);

private:
    // Test if an item is a Qt Quick overlay layer.
    //
    // Detecting the overlay layer is nontrivial - QQuickOverlay isn't part of
    // the Qt API.  Theoretically it's possible to ask the QQuickEngine to
    // evaluate 'Overlay.overlay' for the QQuickWindow, but that's pretty
    // complex.  Instead, we find it by class name.
    bool isOverlayLayer(const QQuickItem &item) const;

    // Get accessibility elements for the overlay or non-overlay layer, based on
    // overlayLayer.
    void appendLayerChildren(QList<QAccessibleInterface*> &accChildren, bool overlayLayer) const;
    // Get accessibility elements for the overlay layer only.
    void appendOverlayChildren(QList<QAccessibleInterface*> &accChildren) const;
    // Get non-overlay accessibility children (used when _showingOverlay is false)
    void appendNonOverlayChildren(QList<QAccessibleInterface*> &accChildren) const;

    // Get the current accessibility children based on _showingOverlay.
    QList<QAccessibleInterface*> getActiveChildren() const;

    // A child of this window was created or destroyed; check if we need to
    // switch to or from the overlay layer.
    void childCreatedDestroyed();

    // Clear _pFocusItem and disconnect signals; used to implement
    // element[Gained|Lost]Focus().
    void clearFocusItem();

    bool canNotifyFocusedItem() const;

    // A child of this window gained or lost focus.
    void elementGainedFocus(AccessibleItem &item);
    void elementLostFocus(AccessibleItem &item);

    // Determine the current focused accessible item.  The item's accessibility
    // element may or may not exist yet.  If there isn't one, this returns
    // nullptr, which could be for a number of reasons:
    // - this window isn't focused
    // - the focused item isn't annotated with an AccessibleItem
    // - there is no focused item
    AccessibleItem *findFocusedItem() const;

    // Check if the window's focused state has changed, emit a focused-element
    // notification if the window becomes focused.
    void checkWindowFocus();

    // Get the AccessibleElement for the element that we actually report as the
    // focused element.  Returns nullptr if we are not reporting a focused
    // element right now (because the window isn't focused, the focused element
    // doesn't exist in the accessibility tree, etc.).
    // Usually returns _pFocusItem when it is reported as focused, but for Table
    // this could return a child element (see
    // AccessibleElement::getFocusNotifyInterface()).
    AccessibleElement *focusedElement() const;

    // Send a notification that the focus item has been focused.
    void notifyItemFocused() const;

public:
    // When an item is locating its parent, if it has no intermediate parent,
    // the window would be its parent.
    // AccessibleItem::findParentElement() uses this method to check if the
    // window can actually be a parent to this item.  When non-overlay items are
    // hidden, WindowAccImpl suppresses those items by returning false here.
    bool isAccParentOf(QQuickItem &item) const;

    // Implementation of QAccessibleInterface
    virtual QAccessibleInterface *child(int index) const override;
    // childAt() is provided by QAccessibleObject
    virtual int childCount() const override;
    virtual QAccessibleInterface *focusChild() const override;
    virtual int indexOfChild(const QAccessibleInterface *child) const override;
    virtual void *interface_cast(QAccessible::InterfaceType) override;
    virtual bool isValid() const override;
    // object() provided by QAccessibleObject
    virtual QAccessibleInterface *parent() const override;
    virtual QRect rect() const override;
    virtual QVector<QPair<QAccessibleInterface *, QAccessible::Relation>> relations(QAccessible::Relation match) const override;
    virtual QAccessible::Role role() const override;
    virtual void setText(QAccessible::Text t, const QString &text) override;
    virtual QAccessible::State state() const override;
    virtual QString text(QAccessible::Text t) const override;
    virtual QWindow *window() const override;

private:
    QPointer<QQuickWindow> _pWindow;
    // WindowAccImpl hides non-overlay items when any overlay item is shown, so
    // in-window popups like dialogs, menus, etc. work reasonably.
    // This flag indicates that we're showing the overlay, so non-overlay items
    // are hidden.
    bool _showingOverlay;
    // Whether the window is currently visible and focused (or in general,
    // whether it should be reporting focused elements within the window).
    // Tracked to detect changes properly.
    bool _windowFocused;
    // WindowAccImpl is responsible for notifying that the focused element has
    // changed.  Keep track of the focused element as notified by
    // element[Gained|Lost]Focus here.
    //
    // If, somehow, this item is destroyed, _pFocusItem becomes nullptr, which
    // is fine because there is no focus-lost event to send.
    //
    // The element here has not necessarily been notified to the screen reader.
    // For example, if the window is not visible or focused when this element
    // becomes focused, no notification is sent.  It's also possible that the
    // accessibility element for the focused item doesn't exist yet when the
    // item is focused.  In any case, we'll send another notification later when
    // the window gains focus, the element is created, etc.
    QPointer<AccessibleItem> _pFocusItem;
};

}

#endif
