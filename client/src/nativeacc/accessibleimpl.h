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
#line HEADER_FILE("accessibleimpl.h")

#ifndef NATIVEACC_ACCESSIBLEIMPL_H
#define NATIVEACC_ACCESSIBLEIMPL_H

#include "accutil.h"
#include "interfaces.h"
#include <QAccessible>
#include <QAccessibleObject>
#include <QObject>
#include <QPointer>

namespace NativeAcc {

class AccessibleItem;

// AccessibleImpl is the implementation of QAccessibleInterface for
// AccessibleItem.
//
// As discussed in AccessibleItem, they have to be separate objects so the
// AccessibleItem can be owned by QML, but the AccessibleImpl can be owned by
// QAccessible.
//
// AccessibleImpls are created whenever an accessible interface is requested for
// a QQuickItem, even if no NativeAcc annotation has been applied to it (yet).
// If a NativeAcc annotation is created later, it then attaches to the
// AccessibleImpl that was created.
//
// This is necessary to ensure that we can override default QML Accessible
// annotations provided by some QtQuick controls.  If we don't return a valid
// interface when asked, Qt will then query QML Accessible, and there's no other
// way to prevent that.
class AccessibleImpl : public AccessibleElement
{
    Q_OBJECT

public:
    // Interface factory - registered with QAccessible
    static QAccessibleInterface *interfaceFactory(const QString &className,
                                                  QObject *pObject);

private:
    // Create AccessibleImpl with the QQuickItem that it represents.  This
    // doesn't parent to that object.
    // Initially, the AccessibleImpl is not connected to an AccessibleItem.
    AccessibleImpl(QQuickItem &item);

private:
    QList<QAccessibleInterface*> getAccChildren() const;

public:
    // Attach AccessibleImpl to an AccessibleItem.
    // This causes AccessibleImpl to return data from this AccessibleItem.  This
    // fails if the AccessibleImpl was somehow already attached to an item.
    bool attach(AccessibleItem &accItem);

    // Implementation of QAccessibleInterface
    virtual QAccessibleInterface *child(int index) const override;
    // childAt() provided by QAccessibleObject
    virtual int childCount() const override;
    virtual int indexOfChild(const QAccessibleInterface *child) const override;
    virtual void *interface_cast(QAccessible::InterfaceType type) override;
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

    // Implementation of AccessibleElement
    virtual AccessibleTableFiller *tableFillerInterface() override;
    virtual AccessibleRowFiller *rowFillerInterface() override;

private:
    QPointer<AccessibleItem> _pAccItem;
};

}

#endif
