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
#line HEADER_FILE("accutil.h")

#ifndef NATIVEACC_ACCUTIL_H
#define NATIVEACC_ACCUTIL_H

#include <QObject>
#include <QQuickItem>
#include <QList>
#include <QAccessibleObject>

// Declare a stub type that just has attached properties with
// NATIVEACC_ATTACHED_PROPERTY_STUB().
// Due to QML macro limitations, this has to be used at global scope.  'Type' is
// declared in the NativeAcc namespace.
#define NATIVEACC_ATTACHED_PROPERTY_STUB(Type, AttachedType) \
    namespace NativeAcc \
    { \
        class Type : public QObject, public AttachedPropertyStub<AttachedType> \
        { \
            Q_OBJECT \
        }; \
    } \
    QML_DECLARE_TYPEINFO(NativeAcc::Type, QML_HAS_ATTACHED_PROPERTIES)

namespace NativeAcc {

// Cast a QObject to a QQuickItem for AttachedPropertyStub::qmlAttachedProperties()
// with appropriate tracing.
QQuickItem *castObjectToItem(QObject *pObject);

// Append pStartItem's accessibility interface if it is accessible, or its
// direct accessible children if it isn't accessible itsef.
// (Used instead of appendAccChildren() when the parent is a non-Item like
// Window that examines each of its children directly.)
void appendAccElementOrChildren(QQuickItem *pStartItem, QList<QAccessibleInterface *> &accChildren);

// Append the accessible children of pItem to accChildren.  (Does not include
// pItem itself.)
void appendAccChildren(QQuickItem *pItem, QList<QAccessibleInterface *> &accChildren);

// Get the accessible children of a QQuickItem to 'accChildren' as their
// accessibility interfaces.
// The children returned are the first accessible descendants from item, they
// may not be direct children of it.
// If pItem is nullptr, an empty list is returned.
QList<QAccessibleInterface*> getAccChildren(QQuickItem *pItem);

QRect itemScreenRect(const QQuickItem &item);

// Implementation of NATIVEACC_ATTACHED_PROPERTY_STUB().
// QObject doesn't support template classes, but at least we can keep the macro
// part somewhat thin.
template<class AttachedType>
class AttachedPropertyStub
{
public:
    static AttachedType *qmlAttachedProperties(QObject *object)
    {
        QQuickItem *pItem = castObjectToItem(object);
        return pItem ? new AttachedType{*pItem} : nullptr;
    }
};

// Dynamic property shim.  Takes a property name from a QObject, and exposes its
// value getter/setter and signal as regular methods/signals.
//
// (The getter and setter are straightforward, but the signal is more complex.)
// There's also a typed dynamic property below which maps the QVariants to a
// specific type.
class DynProp : public QObject
{
    Q_OBJECT

public:
    // Create DynProp with an object and property name.
    // DynProp holds a reference to object, the owner must guarantee that object
    // outlives it.  (This is used by NativeAcc attached types that are parented
    // to object.)
    DynProp(QObject &object, const char *propertyName);

public:
    // Get the property value.
    QVariant get() const;
    // Set the property value.
    void set(const QVariant &value);

signals:
    // Emitted when the property value changes.
    void changed();

private:
    QObject &_object;
    QMetaProperty _property;
};

// Typed dynamic property shim; just replaces get() and set() with typed
// accessors.
template<class PropType>
class TypedDynProp : public DynProp
{
public:
    using DynProp::DynProp;

public:
    // The variant is converted with QVariant::value<PropType>(); this can apply
    // conversions if the value is not actually this type.  If it can't be
    // converted, a default PropType is returned.
    PropType get() const {return DynProp::get().template value<PropType>();}
    void set(const PropType &value) {DynProp::set(QVariant::fromValue(value));}
};

// For some unknown reason, QAccessible::State is a big struct of ~40 one-bit
// bit fields, rather than actual flag bits used with QFlags or something.
//
// We frequently need to identify a member of a State to get it, set it, and/or
// emit a change for it, and we can't use a pointer-to-member since they're bit
// fields.
//
// So, here we define a huge enum that matches the fields of State, and two
// functions containing huge switches to get or set the appropriate field.
enum class StateField
{
    disabled,
    selected,
    focusable,
    focused,
    pressed,
    checkable,
    checked,
    checkStateMixed,
    readOnly,
    hotTracked,
    defaultButton,
    expanded,
    collapsed,
    busy,
    expandable,
    marqueed,
    animated,
    invisible,
    offscreen,
    sizeable,
    movable,
    selfVoicing,
    selectable,
    linked,
    traversed,
    multiSelectable,
    extSelectable,
    passwordEdit,
    hasPopup,
    modal,
    active,
    invalid,
    editable,
    multiLine,
    selectableText,
    supportsAutoCompletion,
    searchEdit,
};
bool getStateField(const QAccessible::State &state, StateField field);
void setStateField(QAccessible::State &state, StateField field, bool value);

}

#endif
