// Copyright (c) 2022 Private Internet Access, Inc.
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
#line HEADER_FILE("textfieldbase.h")

#ifndef NATIVEACC_TEXTFIELDBASE_H
#define NATIVEACC_TEXTFIELDBASE_H

#include <QQuickItem>
#include <QAccessibleTextInterface>
#include "accessibleitem.h"
#include "accutil.h"

namespace NativeAcc {

// TextFieldBase is a general model of the EditableText role.  It provides the
// QAccessibleTextInterface implementation for text fields (both editable text
// fields and value texts).
//
// Typically, for a TextField, the name is the text shown in the label (or a
// descriptive string if the label is a graphic).  The value is usually the
// displayed string.
class TextFieldBase : public AccessibleItem, private QAccessibleTextInterface
{
    Q_OBJECT

    // The text value in the control
    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)
    // The current cursor position (index into the value string - range
    // [0, value().count()], can be count() exactly when the cursor is at the
    // end of the text)
    Q_PROPERTY(int cursorPos READ cursorPos WRITE setCursorPos NOTIFY cursorPosChanged)

public:
    TextFieldBase(QAccessible::Role role, QQuickItem &item);

private:
    // For password edits, masking the value is our job, not the screen
    // reader's.  Do this when the passwordEdit state bit is enabled.
    bool isPasswordEdit() const {return getState(StateField::passwordEdit);}

    // Get a substring from a string (QString::mid()), or a masked placeholder
    // if this is a password edit.
    // n can be -1 to return the entire string after pos.
    QString maskedValueMid(const QString &value, int pos, int n = -1) const;

public:
    QString value() const {return _value;}
    void setValue(const QString &value);
    int cursorPos() const {return _cursorPos;}
    void setCursorPos(int cursorPos);

    // Overrides of AccessibleItem
    virtual QString textValue() const override;
    virtual QAccessibleTextInterface *textInterface() override {return this;}

    // Implementation of QAccessibleTextInterface
    virtual void addSelection(int startOffset, int endOffset) override;
    virtual QString attributes(int offset, int *startOffset, int *endOffset) const override;
    virtual int characterCount() const override;
    virtual QRect characterRect(int offset) const override;
    virtual int cursorPosition() const override;
    virtual int offsetAtPoint(const QPoint &point) const override;
    virtual void removeSelection(int selectionIndex) override;
    virtual void scrollToSubstring(int startIndex, int endIndex) override;
    virtual void selection(int selectionIndex, int *startOffset, int *endOffset) const override;
    virtual int selectionCount() const override;
    virtual void setCursorPosition(int position) override;
    virtual void setSelection(int selectionIndex, int startOffset, int endOffset) override;
    virtual QString text(int startOffset, int endOffset) const override;
    // text[After|At|Before]Offset() provided by QAccessibleTextInterface

signals:
    void valueChanged();
    void cursorPosChanged();

    // The showMenu action was taken, show the context menu.  This action is
    // provided for focusable TextFields.
    void showMenu();

    // The screen reader wants to set the cursor position, set it.
    // This should cause cursorPos to change to the new value.
    void changeCursorPos(int newPos);

private:
    QString _value;
    int _cursorPos;
};

}

#endif
