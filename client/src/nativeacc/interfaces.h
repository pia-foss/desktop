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
#line HEADER_FILE("interfaces.h")

#ifndef NATIVEACC_INTERFACES_H
#define NATIVEACC_INTERFACES_H

#include <QAccessibleObject>
#include <QAccessibleInterface>

namespace NativeAcc {

// This is a filler interface for tables; it supplements
// QAccessibleTableInterface with additional functionality needed to properly
// implement tables on Mac.
class AccessibleTableFiller
{
public:
    // Get the rows and columns in the table.
    // Both rows and columns are returned as the table's children on Mac OS.
    // The rows also return the columns as their own columns, and vice-versa.
    // The cells themselves are children of the rows.
    virtual QList<QAccessibleInterface*> getTableRows() const = 0;
    virtual QList<QAccessibleInterface*> getTableColumns() const = 0;
    virtual QList<QAccessibleInterface*> getSelectedRows() const = 0;
};

// Filler interface for table rows.  Provides outlining information for
// expandable row groups.
class AccessibleRowFiller
{
public:
    // Get the row's outlining level.
    virtual int getOutlineLevel() const = 0;
    // Get the row's expansion state.  Rows that do not have children should
    // return false.
    virtual bool getExpanded() const = 0;
    // Get the row's outline parent - the row that would hide this row when
    // collapsed.
    virtual QAccessibleInterface *getOutlineParent() const = 0;
    // Get the row's outline children - the rows that this row would hide when
    // collapsed.  A collapsed row should return no children.
    virtual QList<QAccessibleInterface *> getOutlineChildren() const = 0;
};

// This is a QAccessibleInterface with:
// - a QObject base (so AccessibleItem::_pParentElement can be a QPointer;
//   see _pParentElement)
// - additional methods needed by NativeAcc
class AccessibleElement : public QObject, public QAccessibleObject
{
    Q_OBJECT
    // This allows qobject_cast<QAccessibleInterface>() to work on an
    // AccessibleImpl - it's mainly to silence a MOC warning
    Q_INTERFACES(QAccessibleInterface)

public:
    // Pass the QAccessibleObject's object in the ctor.  These do not parent to
    // that object, QAccessibleInterface objects are owned by QAccessible.  The
    // object can be nullptr if the interface does not correspond to a specific
    // object.
    using QAccessibleObject::QAccessibleObject;

    // Filler interfaces needed to fix up missing functionality on Mac.  These
    // behave like the QAccessible role-based interfaces (tableInterface(),
    // textInterface(), etc.)
    // These return nullptr by default; objects implementing these interfaces
    // should return the implementation.
    virtual AccessibleTableFiller *tableFillerInterface() {return nullptr;}
    virtual AccessibleRowFiller *rowFillerInterface() {return nullptr;}
};

// Both QObject and QAccessibleInterface are streamable; resolve ambiguity for
// AccessibleElement
QDebug &operator<<(QDebug &d, AccessibleElement *pImpl);

}

#endif
