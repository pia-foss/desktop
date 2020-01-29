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
#line SOURCE_FILE("tablecellbase.cpp")

#include "tablecellbase.h"
#include "tablecellimpl.h"
#include <typeinfo>
#include <unordered_map>

namespace NativeAcc {

TableCellBase::TableCellBase(QAccessible::Role role)
    : _role{role}
{
}

bool TableCellBase::attachImpl(TableCellImpl &cellImpl)
{
    // Only attach if this interface is a plain TableCellImpl, do not
    // reattach the base part of a derived type.
    if(typeid(cellImpl) == typeid(TableCellImpl))
    {
        cellImpl.reattach(*this);
        return true;
    }
    return false;
}

void TableCellBase::setName(const QString &name)
{
    if(name != _name)
    {
        _name = name;
        emit nameChanged();
    }
}

void TableCellBase::setItem(QQuickItem *pItem)
{
    if(pItem != _pItem)
    {
        _pItem = pItem;
        emit itemChanged();
    }
}

TableCellImpl *TableCellBase::createInterface(TableAttached &table,
                                                   AccessibleElement &accParent)
{
    return new TableCellImpl{role(), table, *this, accParent};
}

bool TableCellBase::attachInterface(TableCellImpl &cellImpl)
{
    // Attach only if the role has not changed - a role change requires
    // destroying the object, there's no way to notify this to QAccessible.
    return cellImpl.realRole() == role() && attachImpl(cellImpl);
}

void OwnedCellPtr::reset(TableCellImpl *pCell)
{
    // Nothing to do; in particular do not try to destroy/reattach the object if
    // assigning the same object
    if(pCell == _pCell)
        return;

    // Destroy the old interface through QAccessible.
    if(_pCell)
    {
        // Destroy the accessibility element before destroying the interface.
        // (TableCellImpl's destructor would be too late.)
        _pCell->emitDestroyedEvent();
        QAccessible::deleteAccessibleInterface(QAccessible::uniqueId(_pCell));
    }

    _pCell = pCell;

    // Register the new interface with QAccessible.
    if(_pCell)
    {
        // The interface can't reference an object.  As long as this holds,
        // QAccessible doesn't destroy the object on its own, so we own it here.
        //
        // If it did reference an object, QAccessible would destroy it on its
        // own when that object is destroyed.  Note that even using the
        // interface's uniqueId instead of a raw pointer isn't safe in this
        // case, because IDs can be reused if the interface is destroyed.
        //
        // TableCellImpl (and the types deriving from it) guarantee
        // that this holds.
        Q_ASSERT(!_pCell->object());

        // At this point, register the accessibility interface.
        // This is stupidly complex.  QAccessible lifetime management is a mess.
        //
        // What we _should_ do here is call
        // QAccessible::registerAccessibleInterface() since we have just created
        // the interface, and logically this corresponds to
        // deleteAccessibleInterface() above.
        //
        // However, QAccessible is too stupid to handle duplicate
        // registerAccessibleInterface() calls correctly (it just assigns a
        // second ID and crashes when cleaning up the cache), _and_
        // QAccessible::uniqueId() actually registers the interface implicitly
        // if it hasn't been registered yet, rather than just returning 0.
        //
        // Trying to prevent any possible call of uniqueId() before this point
        // is too fragile.  Instead, just rely on emitting the create event to
        // register the interface - QAccessibleEvent calls uniqueId() in its
        // constructor, which implicitly registers the interface.
        //
        // Create the accessibility element (must be done after the interface is
        // registered; TableCellImpl's constructor would be too early)
        _pCell->emitCreatedEvent();
    }
}

}
