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
#line SOURCE_FILE("tablecellimpl.cpp")

#include "tablecellimpl.h"
#include "accessibleimpl.h"
#include "tablecellbase.h"
#include "table.h"

#ifdef Q_OS_MAC
#include "mac/mac_accessibility.h"
#endif

namespace NativeAcc {

TableCellImpl::TableCellImpl(QAccessible::Role role, TableAttached &parentTable,
                             TableCellBase &definition,
                             AccessibleElement &accParent)
    : AccessibleElement{nullptr}, _role{role}, _elementExists{false},
      _parentTable{parentTable}, _pAccParent{&accParent}, _pDefinition{},
      _row{0}, _rowExtent{0}, _column{0}, _columnExtent{0}
{
    connectDef(definition);
    // Just load properties, don't try to emit changes in constructor
    _name = definition.name();

    // On Windows, all table cells say "editable" by default since we have to
    // report them with the "cell" role.
    // Most of the cells in the client are not editable though - default to
    // read-only.  (The only "editable" cell is the check-button cell.)
    setState(StateField::readOnly, true);
}

void TableCellImpl::connectDef(TableCellBase &definition)
{
    if(_pDefinition)
        QObject::disconnect(_pDefinition, nullptr, this, nullptr);

    _pDefinition = &definition;
    QObject::connect(_pDefinition, &TableCellBase::nameChanged, this,
                     &TableCellImpl::onNameChanged);
    QObject::connect(_pDefinition, &TableCellBase::itemChanged, this,
                     &TableCellImpl::onItemChanged);
}

void TableCellImpl::onNameChanged()
{
    // Valid because the signal is connected
    Q_ASSERT(_pDefinition);
    QString newName = _pDefinition->name();
    if(newName != _name)
    {
        _name = newName;
        // TODO - acc event
    }
}

void TableCellImpl::onItemChanged()
{
    // TODO - Should emit a location change if the location actually changed
}

bool TableCellImpl::getState(StateField field) const
{
    return getStateField(_state, field);
}

void TableCellImpl::setState(StateField field, bool value)
{
    // A few fields have custom change events that aren't implemented by
    // TableCellImpl (because no cell uses them).
    Q_ASSERT(field != StateField::invisible);
    Q_ASSERT(field != StateField::focused);

    if(getState(field) != value)
    {
        setStateField(_state, field, value);

        // If the element has been created, emit changes.  (No need to do
        // this if it doesn't exist yet, which can occur during construction).
        //
        // There is no way to check whether the interface has been registered
        // (which is equivalent here), so we have to keep track of this state
        // on our own.
        if(_elementExists)
        {
            QAccessible::State stateChangeFlag;
            setStateField(stateChangeFlag, field, true);
            QAccessibleStateChangeEvent stateEvent{&accItf(), stateChangeFlag};
            QAccessible::updateAccessibility(&stateEvent);
        }
    }
}

TableCellBase *TableCellImpl::definition() const
{
    // Implemented out-of-line because QPointer's various getters need the
    // object type to be a complete.  Sigh.
    // (It's not complete in the class definition because TableCellBase and
    // TableCellImpl each reference each other.)
    return _pDefinition;
}

void TableCellImpl::reattach(TableCellBase &definition)
{
    // TableCellBase::attachInterface() ensures that the role has not changed.
    Q_ASSERT(realRole() == definition.role());

    connectDef(definition);
    // Check for property changes
    onNameChanged();
    onItemChanged();
}

void TableCellImpl::setRange(int row, int rowExtent, int column, int columnExtent)
{
    _row = row;
    _rowExtent = rowExtent;
    _column = column;
    _columnExtent = columnExtent;
    // There doesn't appear to be a change event for this.
}

void TableCellImpl::emitCreatedEvent()
{
    // The object must not have been created already (ensured by OwnedCellPtr)
    Q_ASSERT(!_elementExists);
    QAccessibleEvent createEvent{&accItf(), QAccessible::Event::ObjectCreated};
    QAccessible::updateAccessibility(&createEvent);
#ifdef Q_OS_MAC
    macPostAccCreated(*this);
#endif

    _elementExists = true;
}

void TableCellImpl::emitDestroyedEvent()
{
    // The object must have been created (ensured by OwnedCellPtr)
    Q_ASSERT(_elementExists);
    QAccessibleEvent destroyEvent{&accItf(), QAccessible::Event::ObjectDestroyed};
    QAccessible::updateAccessibility(&destroyEvent);
#ifdef Q_OS_MAC
    macPostAccDestroyed(*this);
#endif

    _elementExists = false;
}

QAccessibleInterface *TableCellImpl::child(int) const
{
    return nullptr;
}

QAccessibleInterface *TableCellImpl::childAt(int, int) const
{
    return nullptr;
}

int TableCellImpl::childCount() const
{
    return 0;
}

int TableCellImpl::indexOfChild(const QAccessibleInterface *) const
{
    return -1;
}

void *TableCellImpl::interface_cast(QAccessible::InterfaceType type)
{
    switch(type)
    {
        default:
        case QAccessible::TextInterface:
        case QAccessible::ValueInterface:
        case QAccessible::ActionInterface:
        case QAccessible::TableInterface:
            return nullptr;
        case QAccessible::TableCellInterface:
        {
            QAccessibleTableCellInterface *pThisItf = this;
            return reinterpret_cast<void*>(pThisItf);
        }
    }
}

bool TableCellImpl::isValid() const
{
    return true;
}

QObject *TableCellImpl::object() const
{
    return nullptr;
}

QAccessibleInterface *TableCellImpl::parent() const
{
    return _pAccParent;
}

QRect TableCellImpl::rect() const
{
    QQuickItem *pItem = _pDefinition ? _pDefinition->item() : nullptr;
    if(!pItem)
        return {};
    return itemScreenRect(*pItem);
}

auto TableCellImpl::relations(QAccessible::Relation) const
    -> QVector<QPair<QAccessibleInterface*, QAccessible::Relation>>
{
    return {};
}

QAccessible::Role TableCellImpl::role() const
{
#ifdef Q_OS_WIN
    // Tables on Windows only work with the real Cell type in the cells.  This
    // is probably because Qt only provides the Table Item / Grid Item patterns
    // if the item's role is really Cell.
    // Unfortunately this loses a lot of information about what the controls do,
    // but that's not that unusual for Windows screen readers.
    // (Rows and columns are still Row/Column.)
    if(_role != QAccessible::Role::Row && _role != QAccessible::Role::Column)
        return QAccessible::Role::Cell;
#endif

    return _role;
}

void TableCellImpl::setText(QAccessible::Text, const QString &)
{
    // Ignored; no settable text
}

QAccessible::State TableCellImpl::state() const
{
    // The Table manages the focused cell state.  Set the focused bit if this
    // cell is currently the table's focus delegate.  (See
    // TableAttached::updateFocusDelegateCell().)
    //
    // This is necessary for keyboard navigation of the regions table to work
    // with screen readers on Windows.  It doesn't affect Orca on Linux.
    //
    // The focused bit stored in _state isn't used for anything.
    QAccessible::State cellState = _state;
    cellState.focused = _parentTable.state().focused && _parentTable.getFocusNotifyInterface() == this;
    return cellState;
}

QString TableCellImpl::text(QAccessible::Text t) const
{
    switch(t)
    {
        case QAccessible::Text::Name:
            return _name;
        case QAccessible::Text::Description:
        case QAccessible::Text::Value:
        case QAccessible::Text::Help:
        case QAccessible::Text::Accelerator:
        default:
            // Description, Value, Help, and Accelerator aren't currently
            // implemented
            return {};
    }
}

QWindow *TableCellImpl::window() const
{
    return _parentTable.window();
}

int TableCellImpl::columnExtent() const
{
    return _columnExtent;
}

QList<QAccessibleInterface*> TableCellImpl::columnHeaderCells() const
{
    // Headers aren't used by TableAttached.
    return {};
}

int TableCellImpl::columnIndex() const
{
    return _column;
}

bool TableCellImpl::isSelected() const
{
    // TODO
    // - add selected row field to table
    // - add selected state to cell impl
    return false;
}

int TableCellImpl::rowExtent() const
{
    return _rowExtent;
}

QList<QAccessibleInterface*> TableCellImpl::rowHeaderCells() const
{
    return {};
}

int TableCellImpl::rowIndex() const
{
    return _row;
}

QAccessibleInterface *TableCellImpl::table() const
{
    return _parentTable.getInterface();
}

}
