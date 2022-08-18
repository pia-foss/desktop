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
#line SOURCE_FILE("accutil.cpp")

#include "accutil.h"
#include "accessibleitem.h"
#include "accessibleimpl.h"
#include <QQuickWindow>

namespace NativeAcc {

QQuickItem *castObjectToItem(QObject *pObject)
{
    QQuickItem *pItem = qobject_cast<QQuickItem*>(pObject);
    if(!pItem)
        qWarning() << "Attempted to attach NativeAcc type to non-Item" << pObject;

    return pItem;
}

void appendAccElementOrChildren(QQuickItem *pStartItem, QList<QAccessibleInterface *> &accChildren)
{
    if(!pStartItem)
        return;

    // We _could_ include any accessibility annotations here (either QML
    // Accessible-based or NativeAcc-based), but QML Accessible annotations
    // restrict their children to other Accessible-based annotations.
    //
    // Ignore them instead and just return NativeAcc annotations.  This
    // means we lose some default annotations provided by stock controls,
    // but most of those are not very good anyway, this is beneficial for
    // things like QML Menus.
    QAccessibleInterface *pAccItf = AccessibleItem::getInterface(pStartItem);
    if(pAccItf)
        accChildren.append(pAccItf);
    else
    {
        // This item isn't accessible, so recurse into its children.
        appendAccChildren(pStartItem, accChildren);
    }
}

void appendAccChildren(QQuickItem *pItem, QList<QAccessibleInterface *> &accChildren)
{
    if(!pItem)
        return;

    const auto &childItems = pItem->childItems();
    for(QQuickItem *pChild : childItems)
        appendAccElementOrChildren(pChild, accChildren);
}

QList<QAccessibleInterface*> getAccChildren(QQuickItem *pItem)
{
    QList<QAccessibleInterface*> accChildren;
    appendAccChildren(pItem, accChildren);
    return accChildren;
}

QRect itemScreenRect(const QQuickItem &item)
{
    // Map the item's bounds to global coordinates.  This applies scale
    // transformations if the UI is scaled.
    //
    // Do this by mapping the top-left and bottom-right corners since QQuickItem
    // doesn't have a rect version of mapToGlobal().
    //
    // Fortunately, we don't have to notify when this changes, the accessibility
    // tools don't have a way to do that.

    auto topLeftScreen = item.mapToGlobal(QPointF{0, 0}).toPoint();
    auto bottomRightScreen = item.mapToGlobal(QPointF{item.width(), item.height()}).toPoint();

    // The item could be rotated or reflected, so the coordinates may have been
    // flipped now.
    // This works fine for any scale/reflection, and for rotations of 90 degree
    // increments.
    // If the item is rotated some other amount, technically this will result in
    // a rectangle bounding the original top-left and bottom-right, which may
    // not necessarily bound the complete item.  (To handle that, we'd have to
    // transform all four corners and find a bound around them.)

    if(bottomRightScreen.x() < topLeftScreen.x())
        std::swap(bottomRightScreen.rx(), topLeftScreen.rx());
    if(bottomRightScreen.y() < topLeftScreen.y())
        std::swap(bottomRightScreen.ry(), topLeftScreen.ry());

    return {topLeftScreen, bottomRightScreen};
}

DynProp::DynProp(QObject &object, const char *propertyName)
    : _object{object}
{
    // To get the changed signal for the property, we find its QMetaProperty,
    // which gives a QMetaMethod for the change signal.
    // QObject can connect to a QMetaMethod, but only when using a QMetaMethod
    // as the slot type also.  (That's the main reason DynProp exists, so
    // the owner can connect normally to DynProp::changed().)
    const QMetaObject *pObjMeta = object.metaObject();
    // All objects have a meta-object, even if it's just QObject's meta-object
    Q_ASSERT(pObjMeta);

    // Find the property
    int propIndex = pObjMeta->indexOfProperty(propertyName);
    _property = pObjMeta->property(propIndex);

    if(_property.isValid())
    {
        // Find the notify signal
        QMetaMethod propNotify = _property.notifySignal();
        // It might not have one, which is fine, it could be a constant
        // property.
        if(propNotify.isValid())
        {
            // Signals can be used as slots too, conveniently this also allows
            // us to use QMetaMethod::fromSignal() to connect to changed.
            QObject::connect(&object, propNotify, this,
                             QMetaMethod::fromSignal(&DynProp::changed));
        }
    }
    else
    {
        qWarning() << "No such property" << propertyName << "in object"
            << &object << "(meta:" << pObjMeta << ")";
    }
}

QVariant DynProp::get() const
{
    return _property.read(&_object);
}

void DynProp::set(const QVariant &value)
{
    _property.write(&_object, value);
}

bool getStateField(const QAccessible::State &state, StateField field)
{
    switch(field)
    {
        case StateField::disabled: return state.disabled;
        case StateField::selected: return state.selected;
        case StateField::focusable: return state.focusable;
        case StateField::focused: return state.focused;
        case StateField::pressed: return state.pressed;
        case StateField::checkable: return state.checkable;
        case StateField::checked: return state.checked;
        case StateField::checkStateMixed: return state.checkStateMixed;
        case StateField::readOnly: return state.readOnly;
        case StateField::hotTracked: return state.hotTracked;
        case StateField::defaultButton: return state.defaultButton;
        case StateField::expanded: return state.expanded;
        case StateField::collapsed: return state.collapsed;
        case StateField::busy: return state.busy;
        case StateField::expandable: return state.expandable;
        case StateField::marqueed: return state.marqueed;
        case StateField::animated: return state.animated;
        case StateField::invisible: return state.invisible;
        case StateField::offscreen: return state.offscreen;
        case StateField::sizeable: return state.sizeable;
        case StateField::movable: return state.movable;
        case StateField::selfVoicing: return state.selfVoicing;
        case StateField::selectable: return state.selectable;
        case StateField::linked: return state.linked;
        case StateField::traversed: return state.traversed;
        case StateField::multiSelectable: return state.multiSelectable;
        case StateField::extSelectable: return state.extSelectable;
        case StateField::passwordEdit: return state.passwordEdit;
        case StateField::hasPopup: return state.hasPopup;
        case StateField::modal: return state.modal;
        case StateField::active: return state.active;
        case StateField::invalid: return state.invalid;
        case StateField::editable: return state.editable;
        case StateField::multiLine: return state.multiLine;
        case StateField::selectableText: return state.selectableText;
        case StateField::supportsAutoCompletion: return state.supportsAutoCompletion;
        case StateField::searchEdit: return state.searchEdit;
        default:
            Q_ASSERT(false);
            return false;
    }
}

void setStateField(QAccessible::State &state, StateField field, bool value)
{
    int valInt = value ? 1 : 0;

    switch(field)
    {
        case StateField::disabled: state.disabled = valInt; break;
        case StateField::selected: state.selected = valInt; break;
        case StateField::focusable: state.focusable = valInt; break;
        case StateField::focused: state.focused = valInt; break;
        case StateField::pressed: state.pressed = valInt; break;
        case StateField::checkable: state.checkable = valInt; break;
        case StateField::checked: state.checked = valInt; break;
        case StateField::checkStateMixed: state.checkStateMixed = valInt; break;
        case StateField::readOnly: state.readOnly = valInt; break;
        case StateField::hotTracked: state.hotTracked = valInt; break;
        case StateField::defaultButton: state.defaultButton = valInt; break;
        case StateField::expanded: state.expanded = valInt; break;
        case StateField::collapsed: state.collapsed = valInt; break;
        case StateField::busy: state.busy = valInt; break;
        case StateField::expandable: state.expandable = valInt; break;
        case StateField::marqueed: state.marqueed = valInt; break;
        case StateField::animated: state.animated = valInt; break;
        case StateField::invisible: state.invisible = valInt; break;
        case StateField::offscreen: state.offscreen = valInt; break;
        case StateField::sizeable: state.sizeable = valInt; break;
        case StateField::movable: state.movable = valInt; break;
        case StateField::selfVoicing: state.selfVoicing = valInt; break;
        case StateField::selectable: state.selectable = valInt; break;
        case StateField::linked: state.linked = valInt; break;
        case StateField::traversed: state.traversed = valInt; break;
        case StateField::multiSelectable: state.multiSelectable = valInt; break;
        case StateField::extSelectable: state.extSelectable = valInt; break;
        case StateField::passwordEdit: state.passwordEdit = valInt; break;
        case StateField::hasPopup: state.hasPopup = valInt; break;
        case StateField::modal: state.modal = valInt; break;
        case StateField::active: state.active = valInt; break;
        case StateField::invalid: state.invalid = valInt; break;
        case StateField::editable: state.editable = valInt; break;
        case StateField::multiLine: state.multiLine = valInt; break;
        case StateField::selectableText: state.selectableText = valInt; break;
        case StateField::supportsAutoCompletion: state.supportsAutoCompletion = valInt; break;
        case StateField::searchEdit: state.searchEdit = valInt; break;
        default:
            Q_ASSERT(false);
            break;
    }
}

}
