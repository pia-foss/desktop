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
#line HEADER_FILE("textfield.h")

#ifndef NATIVEACC_TEXTFIELD_H
#define NATIVEACC_TEXTFIELD_H

#include "textfieldbase.h"

namespace NativeAcc {

// TextField implements the EditableText role for a normal editable text field.
// In addition to the text interface implemented by TextFieldBase, it provides
// a "show menu" action, and some properties like "read only", "password", and
// "search".
class TextFieldAttached : public TextFieldBase
{
    Q_OBJECT
    // Whether the control is read-only - default is false
    Q_PROPERTY(bool readOnly READ readOnly WRITE setReadOnly NOTIFY readOnlyChanged)
    // Whether the control is a password text field - default is false
    Q_PROPERTY(bool passwordEdit READ passwordEdit WRITE setPasswordEdit NOTIFY passwordEditChanged)
    // Whether the control is a search text field - default is false
    Q_PROPERTY(bool searchEdit READ searchEdit WRITE setSearchEdit NOTIFY searchEditChanged)

public:
    TextFieldAttached(QQuickItem &item);

public:
    bool readOnly() const {return getState(StateField::readOnly);}
    void setReadOnly(bool readOnly);
    bool passwordEdit() const {return getState(StateField::passwordEdit);}
    void setPasswordEdit(bool passwordEdit);
    bool searchEdit() const {return getState(StateField::searchEdit);}
    void setSearchEdit(bool searchEdit);

    // Overrides of QAccessibleActionInterface (from AccessibleItem)
    virtual QStringList actionNames() const override;
    virtual void doAction(const QString &actionName) override;

signals:
    void readOnlyChanged();
    void passwordEditChanged();
    void searchEditChanged();
};

}

NATIVEACC_ATTACHED_PROPERTY_STUB(TextField, TextFieldAttached)

#endif
