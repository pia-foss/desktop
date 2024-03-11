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
#line HEADER_FILE("tablecellimpl.h")

#ifndef NATIVEACC_TABLECELLIMPL_H
#define NATIVEACC_TABLECELLIMPL_H

#include <QQuickItem>
#include <QPointer>
#include "accutil.h"
#include "interfaces.h"

namespace NativeAcc {

class TableAttached;
class TableCellBase;

// TableCellImpl is the implementation of QAccessibleInterface for a table
// cell.  It's connected to a TableCellBase from which it actually sources its
// values.
//
// TableCellImpl objects are created by TableCellBase (or a derived type)
// and owned by TableAttached.  A TableCellImpl may be reattached to another
// TableCellBase later when rows are rebuilt, see
// TableCellBase::attachInterface().
class TableCellImpl : public AccessibleElement,
                      private QAccessibleTableCellInterface
{
    Q_OBJECT

public:
    // Create TableCellImpl with a role, the table that is its parent, and
    // the TableCellBase that defines it.
    // The role and parent can't change while TableCellImpl exists, but the
    // definition may change in reattach().
    // The accessibility parent may not be the table itself - on Mac, it's the
    // row containing this cell.
    TableCellImpl(QAccessible::Role role, TableAttached &parentTable,
                  TableCellBase &definition, AccessibleElement &accParent);

private:
    // Connect to a TableCellBase (including detaching from a prior one if
    // needed).  Connects signals appropriately, but does not update property
    // values or emit changes (done by the constructor and reattach()).
    void connectDef(TableCellBase &definition);

    // Handle property changes from the TableCellBase.
    void onNameChanged();
    void onItemChanged();

protected:
    // Get and set state fields.  (Setting emits a change event if applicable.)
    bool getState(StateField field) const;
    void setState(StateField field, bool value);

public:
    // Get the cell's accessible interface.  Convenient for emitting
    // QAccessibleEvents, which can be constructed with a QObject or a
    // QAccessibleInterface for that object.
    // (TableCellImpl is a QObject because it has signal connections, it is
    // _not_ the accessible interface for that object.)
    QAccessibleInterface &accItf() {return *this;}

    // The QAccessibleInterface::role() override doesn't return the real role on
    // Windows (see role()).  This method returns the real role.
    QAccessible::Role realRole() const {return _role;}

    // Get the table for this cell.  (Note that table() returns the table's
    // QAccessibleInterface, it's overridden from
    // QAccessibleTableCellInterface.)
    TableAttached &parentTable() {return _parentTable;}
    const TableAttached &parentTable() const {return _parentTable;}

    // Get the definition from which this cell was created (or reattached to).
    TableCellBase *definition() const;

    // Reattach to a new TableCellBase.  Used by TableCellBase::attachImpl().
    void reattach(TableCellBase &definition);

    // Additional accessibility interfaces.  Like AccessibleItem, these can be
    // overridden to return role-specific interfaces, which are used in the
    // interface_cast<>() implementation.
    virtual QAccessibleTableInterface *tableInterface() {return nullptr;}
    virtual QAccessibleTextInterface *textInterface() {return nullptr;}
    virtual QAccessibleValueInterface *valueInterface() {return nullptr;}
    virtual QAccessibleActionInterface *actionInterface() {return nullptr;}

    // Set the cell's row/column indices and extents.  These are the values
    // returned by [row|column]Index() and [row|column]Extent().
    // TableCellImpl doesn't really care about these otherwise.
    // row/column are the 0-based indices of the first row/column occupied by
    // this cell.  The extents are the number of rows/columns occupied, which
    // normally should be at least 1.
    void setRange(int row, int rowExtent, int column, int columnExtent);

    // Generate the create/destroy events.
    // These are called by OwnedCellPtr as the interface is being registered/
    // deleted.  We can't do this in the constructor/destructor due to the
    // messy ownership model of QAccessibleCache.
    void emitCreatedEvent();
    void emitDestroyedEvent();

    // Implementation of QAccessibleInterface.
    // Table cells can't have their own children, so many of these do nothing.
    // Some just proxy to the table, like window().
    virtual QAccessibleInterface *child(int index) const override;
    virtual QAccessibleInterface *childAt(int x, int y) const override;
    virtual int childCount() const override;
    virtual int indexOfChild(const QAccessibleInterface *child) const override;
    virtual void *interface_cast(QAccessible::InterfaceType type) override;
    virtual bool isValid() const override;
    virtual QObject *object() const override;
    virtual QAccessibleInterface *parent() const override;
    virtual QRect rect() const override;
    virtual QVector<QPair<QAccessibleInterface *, QAccessible::Relation>> relations(QAccessible::Relation match) const override;
    // Do not use role() from NativeAcc code; on Windows it returns the "Cell"
    // role for cells, not the actual role.
    // (QAccessible's UIA backend only provides the "grid item" and "table item"
    // patterns if the item's role is Cell.)
    virtual QAccessible::Role role() const override;
    virtual void setText(QAccessible::Text t, const QString &text) override;
    virtual QAccessible::State state() const override;
    virtual QString text(QAccessible::Text t) const override;
    virtual QWindow *window() const override;

    // Implementation of QAccessibleTableCellInterface
    virtual int columnExtent() const override;
    virtual QList<QAccessibleInterface*> columnHeaderCells() const override;
    virtual int columnIndex() const override;
    virtual bool isSelected() const override;
    virtual int rowExtent() const override;
    virtual QList<QAccessibleInterface*> rowHeaderCells() const override;
    virtual int rowIndex() const override;
    virtual QAccessibleInterface *table() const override;

private:
    const QAccessible::Role _role;
    // Whether the accessibility element logically "exists" right now (whether
    // we last created or destroyed the element).
    bool _elementExists;
    QAccessible::State _state;
    // TableAttached owns this object, so this remains valid.
    TableAttached &_parentTable;
    // We can't put a hard guarantee on the destruction order of this object
    // and the parent accessibility interface - the parent interface may be
    // owned by QAccessible.  Use a QPointer for safety.
    QPointer<AccessibleElement> _pAccParent;
    // The definition could be destroyed at any time.  If it is, we might be
    // reattached to a new (corresponding) definition, or we might be destroyed
    // if the row no longer exists.
    QPointer<TableCellBase> _pDefinition;
    // Row/column indices and extents
    int _row, _rowExtent, _column, _columnExtent;
    // Property values from TableCellBase, must be stored to detect and emit
    // changes
    QString _name;
};

}

#endif
