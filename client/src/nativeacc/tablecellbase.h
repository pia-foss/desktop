// Copyright (c) 2019 London Trust Media Incorporated
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
#line HEADER_FILE("tablecellbase.h")

#ifndef NATIVEACC_TABLECELLBASE_H
#define NATIVEACC_TABLECELLBASE_H

#include <QQuickItem>
#include <QPointer>
#include "accutil.h"
#include "interfaces.h"

namespace NativeAcc {

class TableAttached;
class TableCellImpl;

// TableCellBase is the base definition for any NativeAcc::Cell cell type.
// Cells are defined with these QObjects so they can have normal property
// definitions, change signals, and emitted signals (for buttons).
//
// These aren't QQuickItems because they don't represent the actual cells, these
// are just the definitions of what the cell contains.  These provide methods to
// create a corresponding QAccessibleInterface - this is used by Table, which
// owns the interface objects created.
class TableCellBase : public QObject
{
    Q_OBJECT

public:
    // All cells have some basic properties.
    // 'name' is the name of the accessibility element, like
    // AccessibleItem::name().
    // Unlike AccessibleItem::name(), clearing the name does not destroy the
    // accessibility element, it just exists without a name.  (Cells should
    // always have a name).
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    // 'item' is the QQuickItem that visually represents this cell.  This
    // determines the bounding rectangle returned for the cell.
    Q_PROPERTY(QQuickItem *item READ item WRITE setItem NOTIFY itemChanged)

public:
    TableCellBase(QAccessible::Role role);

signals:
    void nameChanged();
    void itemChanged();

protected:
    // Reattach an interface in attachInterface().  (attachInterface() ensures
    // that the interface's role has not changed.)
    //
    // The interface must be hooked up to this object as if it had been returned
    // by createInterface().  If it can't be reattached, return false.
    //
    // Implementations should check that it is exactly the type returned by
    // createInterface() (not a derived type), and then call a method
    // corresponding to TableCellImpl::reattach() to do the reattachment.
    // Do not call the base implementation; the reattach() method should call
    // its base.
    virtual bool attachImpl(TableCellImpl &cellImpl);

public:
    QAccessible::Role role() const {return _role;}
    QString name() const {return _name;}
    void setName(const QString &name);
    QQuickItem *item() const {return _pItem;}
    void setItem(QQuickItem *pItem);

    // Create a QAccessibleInterface for this cell with the given element as its
    // parent.  TableCellBase's implementation returns a TableCellImpl.
    // Some simple types just use this, more complex types return a type
    // derived from TableCellImpl.
    //
    // Currently, the accessible parent is always the table for rows/columns and
    // the row for cells.  Qt seems to implement tables with their cells as
    // their children, but this doesn't work with either VoiceOver on Mac or MS
    // Narrator on Windows, they both expect rows.
    //
    // The returned object is owned by the Table that created it.  Usually a
    // cell only has one interface at any given time, but this isn't a hard
    // guarantee.
    virtual TableCellImpl *createInterface(TableAttached &table,
                                               AccessibleElement &accParent);

    // Attach a QAccessibleInterface that was previously created with
    // createInterface() to this object.
    //
    // This happens when rows are rebuilt in a table; the new rows are logically
    // the same, but the cell definition objects themselves may be different
    // because they were rebuilt (though they represent the same cell).  To
    // ensure a smooth transition, we have to keep the existing objects around
    // rather than destroy and recreate them.
    //
    // If the element is the right type for this type of cell definition,
    // reattach it to this cell definition and return true.  Otherwise, return
    // false, so TableAttached will destroy and recreate the cell (which is not
    // good UX but is the only way to handle a role change with QAccessible).
    //
    // Derived types override attachImpl() to implement this.
    bool attachInterface(TableCellImpl &cellImpl);

private:
    const QAccessible::Role _role;
    QString _name;
    QPointer<QQuickItem> _pItem;
};

// OwnedCellPtr just owns a TableCellImpl object by registering it with
// QAccessible and destroying it with QAccessible::deleteAccessibleInterface().
//
// This can't own an arbitrary QAccessibleInterface, it only works as long as
// QAccessibleInterface::object() returns nullptr (because QAccessible itself
// will delete the interface if it references and object, and that object is
// destroyed).
//
// OwnedCellPtr also emits the cells' created/destroyed accessibility events.
//
// This whole construct is necessary because QAccessible's lifetime management
// is a horrible mess.  QAccessibleCache "sort of" owns the QAccessible objects:
// - it deletes them with the delete operator when deleteInterface() is called
// - it deletes them at exit (if they're still in the cache)
// - QAccessible::uniqueId() implicitly registers the interface if it hasn't
//   been registered yet (UGH)
//
// However, the table decides when the cell elements are destroyed (since it
// knows the state of the cells), so the table sort of "logically" owns the
// cells - indirectly through QAccessibleCache anyway.
//
// This means that TableCellImpl itself can't register/remove itself or emit its
// destroy event.  (There is no way to just remove an interface without deleting
// it, so TableCellImpl's destructor can't remove the interface.  The destructor
// can't emit the destroy event because the interface has already been removed
// at that point; this would probably cause the interface to be re-registered
// during construction, with horrifying consequences.)
class OwnedCellPtr
{
public:
    OwnedCellPtr() : _pCell{nullptr} {}
    OwnedCellPtr(TableCellImpl *pCell) : OwnedCellPtr{} {reset(pCell);}
    // Movable, but not copiable
    OwnedCellPtr(OwnedCellPtr &&other) : OwnedCellPtr{} {*this = std::move(other);}
    OwnedCellPtr &operator=(OwnedCellPtr &&other) {std::swap(_pCell, other._pCell); return *this;}
    ~OwnedCellPtr() {reset();}

public:
    OwnedCellPtr &operator=(TableCellImpl *pCell) {reset(pCell); return *this;}

    TableCellImpl *get() const {return _pCell;}
    void reset(TableCellImpl *pCell = nullptr);

    explicit operator bool() const {return _pCell;}
    bool operator!() const {return !_pCell;}
    TableCellImpl &operator*() const {Q_ASSERT(_pCell); return *_pCell;}
    TableCellImpl* operator->() const {Q_ASSERT(_pCell); return _pCell;}

private:
    TableCellImpl *_pCell;
};

}

#endif
