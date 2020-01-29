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
#line HEADER_FILE("accsuppressor.h")

#ifndef NATIVEACC_ACCSUPPRESSOR_H
#define NATIVEACC_ACCSUPPRESSOR_H

#include <QAccessibleInterface>
#include <QPointer>

namespace NativeAcc {

// AccSuppressor is a stub QAccessibleInterface that just suppresses QML
// Accessible annotations when they're not overridden by NativeAcc.
//
// This is necessary to prevent some events from being generated by QML
// Accessible.
//
// By overriding the accessibility interface with one that's never valid, we
// prevent the focus event from being sent to the platform API.
class AccSuppressor : public QAccessibleInterface
{
public:
    static QAccessibleInterface *interfaceFactory(const QString &classname, QObject *pObject);

public:
    AccSuppressor(QObject *pObj) : _pObject{pObj} {}

public:
    // Implementation of QAccessibleInterface - all stubs.  The key is that
    // isValid() always returns false.
    virtual QAccessibleInterface *child(int index) const override {return nullptr;}
    virtual QAccessibleInterface *childAt(int x, int y) const override {return nullptr;}
    virtual int childCount() const override {return 0;}
    virtual int indexOfChild(const QAccessibleInterface *child) const override {return -1;}
    virtual void *interface_cast(QAccessible::InterfaceType type) override {return nullptr;}
    virtual bool isValid() const override {return false;}
    // object() is implemented, QAccessible might assume this still returns the
    // object since this interface was created from an object.
    virtual QObject *object() const override {return _pObject;}
    virtual QAccessibleInterface *parent() const override {return nullptr;}
    virtual QRect rect() const override {return {};}
    virtual QVector<QPair<QAccessibleInterface *, QAccessible::Relation>> relations(QAccessible::Relation match) const override {return {};}
    virtual QAccessible::Role role() const override {return QAccessible::Role::NoRole;}
    virtual void setText(QAccessible::Text t, const QString &text) override {}
    virtual QAccessible::State state() const override {return {};}
    virtual QString text(QAccessible::Text t) const override {return {};}
    virtual QWindow *window() const override {return nullptr;}

private:
    QPointer<QObject> _pObject;
};

}

#endif
