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
#line SOURCE_FILE("table.cpp")

#include "table.h"
#include "tablecellbase.h"
#include "tablecellimpl.h"
#include "accessibleimpl.h"
#include <unordered_map>

namespace NativeAcc {

TableAttached::TableAttached(QQuickItem &item)
    : AccessibleItem{QAccessible::Role::Table, item},
      _navigateRow{-1}, _navigateCol{-1}
{
    // If the whole table is destroyed or created, we have to destroy/create all
    // the child elements.
    QObject::connect(this, &AccessibleItem::elementCreated, this,
                     &TableAttached::onTableCreated);
    QObject::connect(this, &AccessibleItem::elementDestroyed, this,
                     &TableAttached::onTableDestroyed);
}

TableCellBase *TableAttached::getCellDefinition(const ColumnDef &column,
                                                       const RowDef &row) const
{
    // Get the cell definition object - it's the row's property for this column
    QJSValue cellObj;
    if(row.rowObj.isObject())
        cellObj = row.rowObj.property(column.property);

    // That object should be a TableCellBase, which is a QObject.
    QObject *pDefBaseQObj = cellObj.toQObject();
    return dynamic_cast<TableCellBase*>(pDefBaseQObj);

}

OwnedCellPtr TableAttached::createDefinitionElement(const QString &rowDiagName,
                                                 const QString &colDiagName,
                                                 TableCellBase *pDef,
                                                 AccessibleElement *pCellAccParent)
{
    // If the table doesn't exist right now, do not create any child elements.
    if(!accExists())
        return nullptr;

    // If it wasn't a valid cell definition, we can't create this cell.
    if(!pDef)
        return nullptr;

    // This shouldn't fail if the table exists; the table and rows exist in this
    // state.
    if(!pCellAccParent)
    {
        qWarning() << "Can't create cell" << rowDiagName << "/" << colDiagName
            << "in table" << this << "- can't get parent element interface";
        return nullptr;
    }

    return pDef->createInterface(*this, *pCellAccParent);
}

OwnedCellPtr TableAttached::createColumnElement(ColumnDef &column)
{
    return createDefinitionElement(QStringLiteral("column element"),
                                   column.property, column.pAccDef,
                                   getInterface());
}

OwnedCellPtr TableAttached::createRowElement(RowDef &row)
{
    return createDefinitionElement(row.id, QStringLiteral("row element"),
                                   row.pAccDef, getInterface());
}

OwnedCellPtr TableAttached::createCellElement(const ColumnDef &column,
                                           const RowDef &row)
{
    // Get the cell definition for this cell.
    TableCellBase *pDefBase = getCellDefinition(column, row);

    // The cell parent is the row.
    AccessibleElement *pCellAccParent = row.pAccElement.get();

    return createDefinitionElement(row.id, column.property, pDefBase, pCellAccParent);
}

bool TableAttached::reattachCellElement(const ColumnDef &column,
                                        const RowDef &row,
                                        const OwnedCellPtr &pCell)
{
    // If there wasn't an element for this cell in the first place, we can't do
    // anything.
    if(!pCell)
        return false;

    // Get the cell definition for this cell.
    TableCellBase *pDefBase = getCellDefinition(column, row);

    // If it wasn't a cell definition, we can't do anything.
    if(!pDefBase)
        return false;

    // Try to reattach the element.
    return pDefBase->attachInterface(*pCell);
}

void TableAttached::setRowCellSpans(int rowIndex, const RowDef &row) const
{
    // If the table's element doesn't exist, there are no cell elements, don't
    // print a spurious warning.
    if(!accExists())
        return;

    // For each nullptr entry in the row's cells, the preceding valid cell gets
    // an extra 'extent' - it spans those cells.
    int extent = 0;
    int column = row.cells.size();
    for(auto itCell = row.cells.rbegin(); itCell != row.cells.rend(); ++itCell)
    {
        ++extent;
        --column;
        const OwnedCellPtr &pCell = *itCell;   // Iterator-to-pointer
        if(pCell)
        {
            // Valid cell - assign position and reset extent
            pCell->setRange(rowIndex, 1, column, extent);
            extent = 0;
        }
    }

    // If there's no cell in the first column, some columns are unoccupied,
    // which is allowed by the interface but isn't ideal.
    if(extent)
    {
        qWarning() << "Row" << rowIndex << "has" << extent
            << "unoccupied leading cells in table" << this;
    }
}

void TableAttached::onTableCreated()
{
    // Create all the row, column, and cell elements.  None of these should
    // exist at this point.
    int colIdx = 0;
    for(auto &column : _columnDefs)
    {
        Q_ASSERT(!column.pAccElement);
        column.pAccElement = createColumnElement(column);
        if(column.pAccElement)
            column.pAccElement->setRange(0, 0, colIdx, 1);
        ++colIdx;
    }
    int rowIdx = 0;
    for(auto &row : _rowDefs)
    {
        Q_ASSERT(!row.pAccElement);
        row.pAccElement = createRowElement(row);
        if(row.pAccElement)
            row.pAccElement->setRange(rowIdx, 1, 0, 0);

        // Sized to match columns by setColumns/setRows
        Q_ASSERT(row.cells.size() == _columnDefs.size());
        colIdx = 0;
        for(auto &column : _columnDefs)
        {
            Q_ASSERT(!row.cells[colIdx]);
            row.cells[colIdx] = createCellElement(column, row);
            ++colIdx;
        }

        setRowCellSpans(rowIdx, row);

        ++rowIdx;
    }

    updateFocusDelegateCell();
}

void TableAttached::onTableDestroyed()
{
    // Destroy all the elements.  Any of these could already be nullptr if the
    // definitions from QML were not valid.
    for(auto &row : _rowDefs)
    {
        for(auto &pCell : row.cells)
            pCell.reset();
        row.pAccElement.reset();
    }
    for(auto &column : _columnDefs)
        column.pAccElement.reset();

    // No need for updateFocusDelegateCell(); destroying the cell element resets
    // the focus delegate.
}

TableCellImpl *TableAttached::getCellImpl(int row, int column) const
{
    // For some reason Qt likes to use signed indices; negatives never make any
    // sense.
    if(row < 0 || column < 0)
        return nullptr;

    unsigned rowIdx = static_cast<unsigned>(row);
    unsigned colIdx = static_cast<unsigned>(column);

    if(rowIdx >= _rowDefs.size())
        return nullptr;

    const auto &rowDef = _rowDefs[rowIdx];

    if(colIdx >= rowDef.cells.size())
        return nullptr;

    // nullptr entries are spanned by the previous cell, so walk backward until
    // we find a valid cell.  If there aren't any, stop on 0 and just return
    // nullptr.
    while(!rowDef.cells[colIdx] && colIdx > 0)
        --colIdx;
    return rowDef.cells[colIdx].get();
}

void TableAttached::updateFocusDelegateCell()
{
    // The cells need to gain focus as keyboard navigation occurs.  Screen
    // readers expect this to happen.
    //
    // - On Linux, Orca has no table navigation functionality whatsoever.  The
    //   only way to work with the table is through keyboard nav, and Orca
    //   expects the individual cells to be focused as the user navigates.
    //   -  Note that Orca's "flat navigation" mode can't be used to navigate
    //      the table; it only allows access to the rows scrolled into view, and
    //      it doesn't seem to update correctly if the view is scrolled.  (It's
    //      also extremely cumbersome.)
    // - On Windows, both Narrator and NVDA have decent table navigation that
    //   works without this, but JAWS does not, it relies on this.  Even for
    //   Narrator/NVDA, their navigation is clunky and users expect arrow key
    //   navigation to work.
    // - On Mac, VoiceOver has really good table support that works without
    //   this, but enabling this allows regular arrow key navigation to work
    //   too.
    setFocusDelegate(getCellImpl(_navigateRow, _navigateCol));
}

void TableAttached::setColumns(const QJSValue &columns)
{
    // QJSValue could be compared with QJSValue::strictlyEquals(), but since we
    // can't directly observe changes in the property of the object underneath,
    // assume it has changed.
    _columns = columns;

    // Get the old column indices, organized by name.
    std::vector<ColumnDef> oldColumns;
    oldColumns.swap(_columnDefs);
    std::unordered_map<QString, unsigned> oldColumnIndices;
    for(unsigned i=0; i<oldColumns.size(); ++i)
        oldColumnIndices.emplace(oldColumns[i].property, i);

    // Read the new column definitions, and build an array that maps new column
    // indices to the old column indices.
    unsigned colsLength = _columns.property(QStringLiteral("length")).toUInt();
    QVector<unsigned> newColumnOldIndices;
    newColumnOldIndices.reserve(colsLength);
    for(unsigned i=0; i<colsLength; ++i)
    {
        _columnDefs.push_back({});
        auto &newColumn = _columnDefs.back();

        QJSValue colValue = _columns.property(i);

        // If the value isn't an object, display and property are left as empty
        // strings.
        if(!colValue.isObject())
        {
            qWarning() << "Invalid column definition" << i << "in table" << this;
        }
        else
        {
            newColumn.property = colValue.property(QStringLiteral("property")).toString();
            QObject *pColAccDefObj = colValue.property(QStringLiteral("column")).toQObject();
            newColumn.pAccDef = dynamic_cast<TableColumn*>(pColAccDefObj);
        }

        auto itOldIndex = oldColumnIndices.find(newColumn.property);
        if(itOldIndex != oldColumnIndices.end())
        {
            newColumnOldIndices.push_back(itOldIndex->second);
            Q_ASSERT(itOldIndex->second < oldColumns.size());   // Valid indices
            ColumnDef &oldColumn = oldColumns[itOldIndex->second];
            newColumn.pAccElement = std::move(oldColumn.pAccElement);

            // Took this entry
            oldColumnIndices.erase(itOldIndex);
        }
        else
        {
            // This is a new column - put in a dummy value; it's not a valid
            // column so we will always make a new cell
            newColumnOldIndices.push_back(std::numeric_limits<unsigned>::max());
        }

        // Reattach or recreate the column element
        if(newColumn.pAccDef)
        {
            if(!newColumn.pAccElement || !newColumn.pAccDef->attachInterface(*newColumn.pAccElement))
                newColumn.pAccElement = createColumnElement(newColumn);
            if(newColumn.pAccElement)
                newColumn.pAccElement->setRange(0, 0, static_cast<int>(i), 1);
        }
    }

    // Rebuild the rows' cell lists by pulling out the cells that still exist
    // and creating new cells.
    int rowIdx = 0;
    for(auto &row : _rowDefs)
    {
        // Create a new cell array of the new size in row.cells.
        // Any elements that we don't take from the old array will be destroyed
        // by the OwnedCellPtrs.
        std::vector<OwnedCellPtr> oldCells;
        oldCells.swap(row.cells);
        row.cells.reserve(_columnDefs.size());

        // Move the elements that still exist to the new array.
        unsigned newColIdx = 0;
        for(unsigned oldIndex : newColumnOldIndices)
        {
            if(oldIndex < oldCells.size() && oldCells[oldIndex])
            {
                // Old entry, take existing
                row.cells.push_back(std::move(oldCells[oldIndex]));
            }
            else
            {
                // New entry, create the cell
                // Index is valid because _columnDefs and newColumnOldIndices
                // have the same size
                Q_ASSERT(newColIdx < _columnDefs.size());
                const auto &col = _columnDefs[newColIdx];
                row.cells.push_back(createCellElement(col, row));
            }
            ++newColIdx;
        }

        // Update the elements' indices and spans.
        setRowCellSpans(rowIdx, row);
        ++rowIdx;
    }

    emit columnsChanged();

    updateFocusDelegateCell();
}

void TableAttached::setRows(const QJSValue &rows)
{
    // Like the columns, assume the rows have changed.
    _rows = rows;

    // Map the row IDs to the existing rows.  We don't want setRows() to be
    // O(N^2), it's called every time the table changes (including latency
    // measurements, etc.)  Making a map ahead of time keeps it to O(N*log(N)).
    //
    // This does handle the possibility of duplicate row IDs, but the cells will
    // be moved arbitrarily between old and new rows with the same ID if that
    // happens.
    std::unordered_multimap<QString, RowDef> oldRows;
    oldRows.reserve(_rowDefs.size());
    for(auto &oldRow : _rowDefs)
    {
        // Copy the ID since we're about to move the row
        QString oldRowId{oldRow.id};
        oldRows.emplace(oldRowId, std::move(oldRow));
    }
    _rowDefs.clear();

    // Read the rows in the new order
    unsigned rowsLength = _rows.property(QStringLiteral("length")).toUInt();
    _rowDefs.reserve(rowsLength);
    for(unsigned rowIdx=0; rowIdx<rowsLength; ++rowIdx)
    {
        _rowDefs.push_back({});

        auto &newRow = _rowDefs.back();

        // Get the row object
        newRow.rowObj = _rows.property(rowIdx);

        // Get the row's ID - leave it empty if the row object is invalid
        if(newRow.rowObj.isObject())
        {
            newRow.id = newRow.rowObj.property(QStringLiteral("id")).toString();
            QObject *pRowAccDefObj = newRow.rowObj.property(QStringLiteral("row")).toQObject();
            newRow.pAccDef = dynamic_cast<TableRow*>(pRowAccDefObj);
        }

        // If the row existed before (and hasn't already been taken), grab the
        // existing cells.  (If there's more than one with this ID, this chooses
        // one arbitrarily.)
        auto itOldRow = oldRows.find(newRow.id);
        if(itOldRow != oldRows.end())
        {
            newRow.cells = std::move(itOldRow->second.cells);
            newRow.pAccElement = std::move(itOldRow->second.pAccElement);
            oldRows.erase(itOldRow);
        }
        else
        {
            newRow.cells.resize(_columnDefs.size());
        }

        // Reattach or recreate the row element
        if(newRow.pAccDef)
        {
            if(!newRow.pAccElement || !newRow.pAccDef->attachInterface(*newRow.pAccElement))
                newRow.pAccElement = createRowElement(newRow);
            if(newRow.pAccElement)
                newRow.pAccElement->setRange(rowIdx, 1, 0, 0);
        }

        // Reattach or recreate the cells
        for(unsigned cellIdx=0; cellIdx<_columnDefs.size(); ++cellIdx)
        {
            if(!reattachCellElement(_columnDefs[cellIdx], newRow, newRow.cells[cellIdx]))
                newRow.cells[cellIdx] = createCellElement(_columnDefs[cellIdx], newRow);
        }

        // Update the elements' indices and spans
        setRowCellSpans(rowIdx, newRow);
    }

    emit rowsChanged();

    updateFocusDelegateCell();
}

void TableAttached::setNavigateRow(int navigateRow)
{
    if(navigateRow == _navigateRow)
        return;

    _navigateRow = navigateRow;
    emit navigateRowChanged();

    updateFocusDelegateCell();
}

void TableAttached::setNavigateCol(int navigateCol)
{
    if(navigateCol == _navigateCol)
        return;

    _navigateCol = navigateCol;
    emit navigateColChanged();

    updateFocusDelegateCell();
}

QList<QAccessibleInterface*> TableAttached::getRowCells(int row) const
{
    if(row < 0)
        return {};
    unsigned rowIdx = static_cast<unsigned>(row);
    if(rowIdx < _rowDefs.size())
    {
        const auto &rowDef = _rowDefs[rowIdx];

        QList<QAccessibleInterface*> cells;
        cells.reserve(rowDef.cells.size());
        for(const auto &pCell : rowDef.cells)
        {
            if(pCell)
                cells.push_back(pCell.get());
        }
        return cells;
    }
    return {};
}

QAccessibleInterface *TableAttached::getRowOutlineParent(int row) const
{
    if(row < 0)
        return nullptr;
    unsigned rowIdx = static_cast<unsigned>(row);
    if(rowIdx >= _rowDefs.size())
        return nullptr;

    // Look for the first parent at a lower outline level
    if(!_rowDefs[rowIdx].pAccDef)
        return nullptr;
    int childOutlineLevel = _rowDefs[rowIdx].pAccDef->outlineLevel();

    while(rowIdx > 0)
    {
        --rowIdx;
        if(_rowDefs[rowIdx].pAccDef && _rowDefs[rowIdx].pAccDef->outlineLevel() < childOutlineLevel)
            return _rowDefs[rowIdx].pAccElement.get();
    }

    return nullptr; // No parent
}

QList<QAccessibleInterface*> TableAttached::getRowOutlineChildren(int row) const
{
    if(row < 0)
        return {};
    unsigned rowIdx = static_cast<unsigned>(row);
    if(rowIdx >= _rowDefs.size())
        return {};

    // Get the level for this row's direct children
    if(!_rowDefs[rowIdx].pAccDef)
        return {};
    int requestedOutlineLevel = _rowDefs[rowIdx].pAccDef->outlineLevel() + 1;

    QList<QAccessibleInterface *> children;

    ++rowIdx;
    while(rowIdx < _rowDefs.size())
    {
        if(!_rowDefs[rowIdx].pAccDef)
            continue;
        int rowOutlineLevel = _rowDefs[rowIdx].pAccDef->outlineLevel();

        // If it's the child level, add it.
        if(rowOutlineLevel == requestedOutlineLevel)
            children.push_back(_rowDefs[rowIdx].pAccElement.get());
        // If it's above the child level, we've found a sibling of the original
        // row, so all children have been found.
        else if(rowOutlineLevel < requestedOutlineLevel)
            break;
        // Otherwise, it's a deeper child - keep looking for more direct
        // children, but don't add this row as a direct child.

        ++rowIdx;
    }

    return children;
}

QList<QAccessibleInterface*> TableAttached::getAccChildren() const
{
    QList<QAccessibleInterface*> children;

    // The table's children are its rows and columns instead of its cells.
    // This is different from what the Qt Widgets tables seem to do (they seem
    // to make the children the cells directly), but this is what is expected by
    // both VoiceOver on Mac and MS Narrator on Windows.  (VoiceOver can't read
    // the cells at all without the rows; Narrator can reach them in "normal"
    // navigation but can't navigate by row/column.)
    children.reserve(_rowDefs.size() + _columnDefs.size());
    for(const auto &row : _rowDefs)
    {
        if(row.pAccElement)
            children.push_back(row.pAccElement.get());
    }
    for(const auto &column : _columnDefs)
    {
        if(column.pAccElement)
            children.push_back(column.pAccElement.get());
    }

    return children;
}

QAccessibleInterface *TableAttached::caption() const
{
    return nullptr;
}

QAccessibleInterface *TableAttached::cellAt(int row, int column) const
{
    return getCellImpl(row, column);
}

int TableAttached::columnCount() const
{
    return _columnDefs.size();
}

QString TableAttached::columnDescription(int column) const
{
    if(column < 0)
        return {};

    unsigned colIdx = static_cast<unsigned>(column);
    if(colIdx < _columnDefs.size() && _columnDefs[colIdx].pAccDef)
        return _columnDefs[colIdx].pAccDef->name();
    return {};
}

bool TableAttached::isColumnSelected(int) const
{
    // We don't select columns in this table, only whole rows.
    return false;
}

bool TableAttached::isRowSelected(int row) const
{
    if(row < 0)
        return false;
    unsigned rowIdx = static_cast<unsigned>(row);
    if(rowIdx >= _rowDefs.size())
        return false;

    return _rowDefs[rowIdx].pAccDef ? _rowDefs[rowIdx].pAccDef->selected() : false;
}

void TableAttached::modelChange(QAccessibleTableModelChangeEvent *)
{
    // For whatever reason, QAccessible echoes back model change events to us
    // with this method.
    //
    // It seems that the table in Qt Widgets uses this to apply model changes,
    // because they're emitted by a base abstract item view.
    //
    // TableAttached applies changes when it emits the events though, so there
    // is nothing to do here.
}

int TableAttached::rowCount() const
{
    return _rowDefs.size();
}

QString TableAttached::rowDescription(int row) const
{
    if(row < 0)
        return {};

    unsigned rowIdx = static_cast<unsigned>(row);
    if(rowIdx < _rowDefs.size() && _rowDefs[rowIdx].pAccDef)
        return _rowDefs[rowIdx].pAccDef->name();
    return {};
}

bool TableAttached::selectColumn(int)
{
    // This table doesn't select columns
    return false;
}

bool TableAttached::selectRow(int)
{
    // Rows aren't selected this way, this interface is more like a spreadsheet-
    // style table, the user wouldn't expect it to take an action (like changing
    // locations and returning to the connect page).
    //
    // Instead, rows in the regions table are selected by pressing the button in
    // the first cell.
    return false;
}

int TableAttached::selectedCellCount() const
{
    return 0;
}

QList<QAccessibleInterface*> TableAttached::selectedCells() const
{
    return {};
}

int TableAttached::selectedColumnCount() const
{
    return 0;
}

QList<int> TableAttached::selectedColumns() const
{
    return {};
}

int TableAttached::selectedRowCount() const
{
    return 0;
}

QList<int> TableAttached::selectedRows() const
{
    return {};
}

QAccessibleInterface *TableAttached::summary() const
{
    return nullptr;
}

bool TableAttached::unselectColumn(int)
{
    return false;
}

bool TableAttached::unselectRow(int)
{
    return false;
}

QList<QAccessibleInterface *> TableAttached::getTableRows() const
{
    QList<QAccessibleInterface *> rowElements;
    rowElements.reserve(_rowDefs.size());
    for(const auto &row : _rowDefs)
    {
        if(row.pAccElement)
            rowElements.push_back(row.pAccElement.get());
    }
    return rowElements;
}

QList<QAccessibleInterface *> TableAttached::getTableColumns() const
{
    QList<QAccessibleInterface *> colElements;
    colElements.reserve(_columnDefs.size());
    for(const auto &column : _columnDefs)
    {
        if(column.pAccElement)
            colElements.push_back(column.pAccElement.get());
    }
    return colElements;
}

QList<QAccessibleInterface *> TableAttached::getSelectedRows() const
{
    QList<QAccessibleInterface *> rowElements;
    for(const auto &row : _rowDefs)
    {
        if(row.pAccDef && row.pAccDef->selected() && row.pAccElement)
            rowElements.push_back(row.pAccElement.get());
    }
    return rowElements;
}

}
