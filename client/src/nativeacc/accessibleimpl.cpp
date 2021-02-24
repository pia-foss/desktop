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
#line SOURCE_FILE("accessibleimpl.cpp")

#include "accessibleimpl.h"
#include "accessibleitem.h"
#include "accutil.h"
#include <QQuickWindow> // For conversion to QWindow base class
#include <QTimer>

namespace NativeAcc {

QAccessibleInterface *AccessibleImpl::interfaceFactory(const QString &,
                                                       QObject *pObject)
{
    // If it's any kind of QQuickItem, return a stub.
    // If we don't return an interface now, we miss our chance to handle this
    // object, and QML Accessible might create an interface for it.
    //
    // A few object types rely on this just to suppress a native QML Accessible
    // annotation even though we don't attach a NativeAcc annotation to it.  One
    // specific situation where this occurs is for element-focus events on
    // Windows - scroll panes generate spurious focus events, which causes
    // Narrator to get stuck on Windows because they're not part of the
    // accessibility tree.
    QQuickItem *pItem = dynamic_cast<QQuickItem*>(pObject);
    if(pItem)
    {
        // It's not possible for there to be an AccessibleItem for this item
        // already.  (We don't have to check for notifying an existing one,
        // etc.)
        //
        // When AccessibleItem is created, it calls
        // QAccessible::queryAccessibleInterface() to either attach an existing
        // interface or create it if it doesn't exist yet.  Since that ensures
        // that an interface is created at that time, it's not possible to
        // create one later.
        Q_ASSERT(!AccessibleItem::getAccItem(pObject));
        return new AccessibleImpl{*pItem};
    }

    return nullptr;
}

AccessibleImpl::AccessibleImpl(QQuickItem &item)
    : AccessibleElement{&item}, _pAccItem{nullptr}
{
}

QList<QAccessibleInterface*> AccessibleImpl::getAccChildren() const
{
    if(_pAccItem)
        return _pAccItem->getAccChildren();
    return {};
}

bool AccessibleImpl::attach(AccessibleItem &accItem)
{
    // If there's somehow already an AccessibleItem attached, fail.
    // Note that it's OK in theory for an AccessibleItem to be destroyed and
    // then another AccessibleItem to attach to this object.  (This shouldn't
    // really happen because the AccessibleItem is attached to the QObject, but
    // it would be fine in theory.)
    if(_pAccItem)
    {
        qWarning() << "AccessibleImpl" << this << "is already attached to"
            << _pAccItem;
        return false;
    }

    _pAccItem = &accItem;
    return true;
}

QAccessibleInterface *AccessibleImpl::child(int index) const
{
    const auto &children = getAccChildren();
    if(index >= 0 && index < children.size())
        return children[index];
    return nullptr;
}

int AccessibleImpl::childCount() const
{
    return getAccChildren().size();
}

int AccessibleImpl::indexOfChild(const QAccessibleInterface *child) const
{
    // QList<QAccessibleInterface*>::indexOf() requires a non-const argument...
    return getAccChildren().indexOf(const_cast<QAccessibleInterface*>(child));
}

void *AccessibleImpl::interface_cast(QAccessible::InterfaceType type)
{
    if(!_pAccItem)
        return nullptr;

    switch(type)
    {
        case QAccessible::InterfaceType::ActionInterface:
            return reinterpret_cast<void*>(_pAccItem->actionInterface());
        case QAccessible::InterfaceType::TableInterface:
            return reinterpret_cast<void*>(_pAccItem->tableInterface());
        case QAccessible::InterfaceType::TextInterface:
            return reinterpret_cast<void*>(_pAccItem->textInterface());
        case QAccessible::InterfaceType::ValueInterface:
            return reinterpret_cast<void*>(_pAccItem->valueInterface());
        default:
            return nullptr;
    }
}

bool AccessibleImpl::isValid() const
{
    // Valid if we still have a valid item, even if the item doesn't exist.
    return QAccessibleObject::isValid() && _pAccItem;
}

QAccessibleInterface *AccessibleImpl::parent() const
{
    return _pAccItem ? _pAccItem->parentElement() : nullptr;
}

QRect AccessibleImpl::rect() const
{
    if(!_pAccItem || !_pAccItem->item())
        return {};

    return itemScreenRect(*_pAccItem->item());
}

QVector<QPair<QAccessibleInterface*, QAccessible::Relation>> AccessibleImpl::relations(QAccessible::Relation match) const
{
    // Stub.  "Label" relations do not seem to have any effect on any platform
    // we support.
    return {};
}

QAccessible::Role AccessibleImpl::role() const
{
    return _pAccItem ? _pAccItem->role() : QAccessible::Role::NoRole;
}

void AccessibleImpl::setText(QAccessible::Text, const QString &)
{
}

QAccessible::State AccessibleImpl::state() const
{
    // If the item doesn't exist, return an invalid state.
    if(!_pAccItem || !_pAccItem->accExists())
    {
        QAccessible::State state{};
        state.invalid = true;
        return state;
    }

    return _pAccItem->state();
}

QString AccessibleImpl::text(QAccessible::Text t) const
{
    // Can't do anything if the item is gone
    if(!_pAccItem)
        return {};

    switch(t)
    {
        case QAccessible::Text::Name:
            return _pAccItem->name();
        case QAccessible::Text::Description:
            return _pAccItem->description();
        case QAccessible::Text::Value:
            return _pAccItem->textValue();
        case QAccessible::Text::Help:
        case QAccessible::Text::Accelerator:
        default:
            // Value, Help, and Accelerator aren't currently implemented
            return {};
    }
}

QWindow *AccessibleImpl::window() const
{
    return _pAccItem ? _pAccItem->window() : nullptr;
}

AccessibleTableFiller *AccessibleImpl::tableFillerInterface()
{
    return _pAccItem ? _pAccItem->tableFillerInterface() : nullptr;
}

AccessibleRowFiller *AccessibleImpl::rowFillerInterface()
{
    return _pAccItem ? _pAccItem->rowFillerInterface() : nullptr;
}

}
