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
#line HEADER_FILE("valuetext.h")

#ifndef NATIVEACC_VALUETEXT_H
#define NATIVEACC_VALUETEXT_H

#include "textfieldbase.h"

namespace NativeAcc {

// ValueText implements the EditableText role for a non-editable "value
// indicator" text element.  These elements optionally have a "Copy" action.
//
// The TextField role is used for "indicator" texts mainly because there isn't a
// better alternative (in particular, Qt does not allow a StaticText to have a
// value and label on Mac OS).  This isn't too far from the truth though, some
// of these have a "copy" action, so they behave more like a read-only text
// field.
//
// ValueText provides QAccessibleTextInterface because that's the only way to
// provide a value on Linux for the text field role.  On Mac and Windows, this
// isn't used because the field isn't actually editable; it just uses
// textValue() to get the value.  (This is normal for Qt on Windows, on Mac it's
// fixed by the fixup layer.)
class ValueTextAttached : public TextFieldBase
{
    Q_OBJECT

public:
    // "Copy" action name and localized description - shared by
    // TableCellValueText
    static const QString copyActionName;
    static QString copyActionLocalizedDescription();
    static QString copyActionLocalizedName();

    // Whether the control is copiable.  Enables the "copy" action, which
    // becomes the default.  The copy() signal is emitted if the user takes the
    // "copy" action.
    Q_PROPERTY(bool copiable READ copiable WRITE setCopiable NOTIFY copiableChanged)

public:
    ValueTextAttached(QQuickItem &item);

public:
    bool copiable() const {return _copiable;}
    void setCopiable(bool copiable);

    // Overrides of QAccessibleActionInterface (from AccessibleItem)
    virtual QStringList actionNames() const override;
    virtual void doAction(const QString &actionName) override;
    virtual QString localizedActionDescription(const QString &actionName) const override;
    virtual QString localizedActionName(const QString &actionName) const override;

signals:
    void valueChanged();
    void copiableChanged();
    void copy();

private:
    QString _value;
    bool _copiable;
};

}

NATIVEACC_ATTACHED_PROPERTY_STUB(ValueText, ValueTextAttached)

#endif
