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
#line HEADER_FILE("table.h")

#ifndef NATIVEACC_TABLE_H
#define NATIVEACC_TABLE_H

#include "accessibleitem.h"
#include "accutil.h"
#include "tablecellbase.h"
#include "tablecells.h"
#include <QQuickItem>
#include <QAccessibleTableInterface>
#include <vector>

namespace NativeAcc {

// Vector that doesn't have copy construction/assignment.
//
// The vector<RowDefs> otherwise would try to copy its RowDefs because they have
// implicitly-defined copy constructors, which are not valid to call because the
// OwnedCellPtrs (inside another vector) are not copiable.
//
// This also happens with a std::unordered_multimap<..., std::vector<OwnedCellPtr>>
// used in setRows().
//
// This happens on Mac/clang at least, it's possible other STLs are smarter and
// actually delete the vector's copy operations when its value type is not
// copiable.  Deleting the copy constructors manually works around this.
template<class T>
class MoveVector : public std::vector<T>
{
public:
    MoveVector() = default;
    MoveVector(const MoveVector&) = delete;
    MoveVector &operator=(const MoveVector&) = delete;
    MoveVector(MoveVector&&) = default;
    MoveVector &operator=(MoveVector &&) = default;
    ~MoveVector() = default;
};

// Table models a table interface for the Table role.
//
// The only table in the client currently is the regions list, which is custom
// built from ad-hoc QML objects.  The QML code provides a flat representation
// of the table to the Table type, which implements the table accessibility
// interface and builds cell objects.
//
// The Table cannot have additional accessible children; Table reports the cells
// as its only children.
//
// Table lets the QML code define the rows and columns in the table with the
// 'columns' and 'rows' properties.
//
// Columns are given as an array of property names (and descriptions).
//
// Rows are given as an array of objects, which have an 'id' property (just used
// to identify the row when the model changes) and cell properties using the
// names given in 'columns'.
//
// Each cell property is an object with a 'role', which is one of the
// NativeAcc::Table::CellRole values.  These model different types of values that are
// displayed in cells.  All cell roles have the 'name' and 'item' properties
// (see AccessibleTableCell).  Some cell roles define additional properties.
class TableAttached : public AccessibleItem, public QAccessibleTableInterface,
                      public AccessibleTableFiller
{
    Q_OBJECT

public:
    // Columns - array of objects defining the columns.  These determine the row
    // properties examined to build table cells.
    // Each object has:
    // - property: The name of the property that defines cells for this column
    //   in rows
    // - column: A NativeAcc.ColumnCell object defining the accessibility
    //   element for the column itself (its name, item, etc.)
    // Note that changes to individual items cannot be observed by
    // TableAttached.  Normally the QML code builds this array as a property
    // binding, so any changes cause the property to be reassigned.  (Changes to
    // the ColumnCell objects are observed normally.)
    Q_PROPERTY(QJSValue columns READ columns WRITE setColumns NOTIFY columnsChanged)
    // Rows - array of objects.
    // All row objects have:
    // - 'id': The row's string ID, used to match up rows when the model
    //   changes.
    // - 'row': A NativeAcc.RowCell object defining the accessibility element
    //   for the row itself (its name, outlining characteristics, etc.)
    // - Cell properties: Properties named by 'columns'.  If a row is missing a
    //   cell property, the prior cell property spans the missing cell(s).
    //   (Rows should have the first cell, since there's no prior cell to span
    //   it.)  Properties other than 'id' and those in 'columns' are ignored.
    //
    // Cell properties contain an object of a NativeAcc.*Cell type, which are
    // "cell definitions".  These define the content of a table cell.
    //
    // Note that changes to individual rows' properties cannot be observed
    // by TableAttached.  (Properties of the RowCell objects and cell
    // definitions update normally.)  Normally the QML code builds the entire
    // array as a property binding, so any change causes the property to be reassigned.
    Q_PROPERTY(QJSValue rows READ rows WRITE setRows NOTIFY rowsChanged)
    // The current position highlighted by keyboard navigation.  This causes the
    // table to report this cell as the current focus item - even though cells
    // don't actually gain the keyboard focus in the regions list, Win/Linux
    // screen readers expect focus notifications to move around the table.
    //
    // If this value does not identify a valid cell, no cell is focused.
    Q_PROPERTY(int navigateRow READ navigateRow WRITE setNavigateRow NOTIFY navigateRowChanged)
    Q_PROPERTY(int navigateCol READ navigateCol WRITE setNavigateCol NOTIFY navigateColChanged)

private:
    // Structure that defines a column - display name and cell property.
    struct ColumnDef
    {
        QString property;
        // The accessibility element for the column itself, which is owned by
        // the QML code.
        QPointer<TableColumn> pAccDef;
        OwnedCellPtr pAccElement;
    };
    // Structure that defines a row - ID and cell list
    struct RowDef
    {
        // This row's ID, used to move around rows when the model changes.
        QString id;
        // The JS object that defines this row.  In principle, should be the
        // corresponding element of _rows, but the JS array in _rows could
        // change without us realizing it.  Used when we need to make new
        // columns due to a column change.
        QJSValue rowObj;
        // The cells are stored as their cell accessibility elements.  These
        // objects are owned by this Table, although it registers them with
        // QAccessible and uses QAccessible::deleteAccessibleInterface() to
        // destroy them.
        //
        // Individual cells can be nullptr, which means that row does not have
        // that cell, and the prior cell spans it instead.
        MoveVector<OwnedCellPtr> cells;
        // The accessibility element for the row itself, which is owned by the
        // QML code.
        QPointer<TableRow> pAccDef;
        OwnedCellPtr pAccElement;
    };

public:
    TableAttached(QQuickItem &item);

private:
    // Get the cell definition object for a particular column in a particular
    // row.
    // The object is still owned by the QJSValue inside the row.
    TableCellBase *getCellDefinition(const ColumnDef &column,
                                            const RowDef &row) const;

    // Create an accessibility element for a particular cell definition (which
    // are also used for the row/column elements).
    // The diagnostic names are just used for tracing.
    OwnedCellPtr createDefinitionElement(const QString &rowDiagName,
                                      const QString &colDiagName,
                                      TableCellBase *pDef,
                                      AccessibleElement *pCellAccParent);

    OwnedCellPtr createColumnElement(ColumnDef &column);
    OwnedCellPtr createRowElement(RowDef &row);

    // Create an accessibility element for a particular column in a particular
    // row.
    OwnedCellPtr createCellElement(const ColumnDef &column, const RowDef &row);

    // Try to reattach an existing accessibility element to the current row's
    // cell definition (see TableCellBase::attachInterface())
    bool reattachCellElement(const ColumnDef &column, const RowDef &row,
                             const OwnedCellPtr &pCell);

    // Set the indices and spans for all cells in a row.
    void setRowCellSpans(int rowIndex, const RowDef &row) const;

    // Handle the table being created
    void onTableCreated();

    // Handle the table being destroyed
    void onTableDestroyed();

    // Get the accessible implementation for a specific cell, including handling
    // spanning cells.
    // If the row/column specified is not valid, returns nullptr.
    TableCellImpl *getCellImpl(int row, int colun) const;

    // Update the cell to which we've delegated focus
    void updateFocusDelegateCell();

public:
    QJSValue columns() const {return _columns;}
    void setColumns(const QJSValue &columns);
    QJSValue rows() const {return _rows;}
    void setRows(const QJSValue &rows);
    int navigateRow() const {return _navigateRow;}
    void setNavigateRow(int navigateRow);
    int navigateCol() const {return _navigateCol;}
    void setNavigateCol(int navigateCol);

    // Methods used by TableRow to implement row interface
    // Get the cells in a row - returned by TableRow as its children
    QList<QAccessibleInterface *> getRowCells(int row) const;
    // Get a row's outline parent - returned by TableRow as its disclosing row.
    // Looks for the nearest preceding row with a lower outline level.
    QAccessibleInterface *getRowOutlineParent(int row) const;
    // Get a row's outline children - returned by TableRow as its disclosed rows.
    // Looks for subsequent rows with the next-larger outline level.
    QList<QAccessibleInterface *> getRowOutlineChildren(int row) const;

    // Overrides of AccessibleItem
    // The Table's children depend on the platform.  On Windows/Linux, the cells
    // are its children - the row and column elements are not used.  On Mac, the
    // rows and columns are its children, and the cells are children of the
    // rows.
    virtual QList<QAccessibleInterface*> getAccChildren() const override;
    virtual QAccessibleTableInterface *tableInterface() override {return this;}
    virtual AccessibleTableFiller *tableFillerInterface() override {return this;}

    // Implementation of QAccessibleTableInterface
    virtual QAccessibleInterface *caption() const override;
    virtual QAccessibleInterface *cellAt(int row, int column) const override;
    virtual int columnCount() const override;
    virtual QString columnDescription(int column) const override;
    virtual bool isColumnSelected(int column) const override;
    virtual bool isRowSelected(int row) const override;
    virtual void modelChange(QAccessibleTableModelChangeEvent *event) override;
    virtual int rowCount() const override;
    virtual QString rowDescription(int row) const override;
    virtual bool selectColumn(int column) override;
    virtual bool selectRow(int row) override;
    virtual int selectedCellCount() const override;
    virtual QList<QAccessibleInterface*> selectedCells() const override;
    virtual int selectedColumnCount() const override;
    virtual QList<int> selectedColumns() const override;
    virtual int selectedRowCount() const override;
    virtual QList<int> selectedRows() const override;
    virtual QAccessibleInterface *summary() const override;
    virtual bool unselectColumn(int column) override;
    virtual bool unselectRow(int row) override;

    // Implementation of AccessibleTableFiller
    virtual QList<QAccessibleInterface *> getTableRows() const override;
    virtual QList<QAccessibleInterface *> getTableColumns() const override;
    virtual QList<QAccessibleInterface*> getSelectedRows() const override;

signals:
    void columnsChanged();
    void rowsChanged();
    void navigateRowChanged();
    void navigateColChanged();

private:
    // The actual QJSValues assigned to rows / columns; stored just so we can
    // return them again from the getters.
    // Note that these hold the underlying JS object/array/etc. by reference, so
    // their contents could change at any time.
    QJSValue _columns, _rows;
    int _navigateRow, _navigateCol;
    // The columns read during setColumns() - pairs of property names and
    // display names
    std::vector<ColumnDef> _columnDefs;
    // The rows read during setRows() - pairs of row IDs and cell arrays.
    // The cells are stored as arrays of QAccessibleInterface objects, which are
    // owned by Table (though by using QAccessible to destroy them, see
    // buildRows()).
    // A cell entry can be nullptr, which indicates that the cell is not present
    // in that row, and the prior valid cell should span it.
    std::vector<RowDef> _rowDefs;
};

}

NATIVEACC_ATTACHED_PROPERTY_STUB(Table, TableAttached)

#endif
