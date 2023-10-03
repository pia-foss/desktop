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
#line HEADER_FILE("tablecells.h")

#ifndef NATIVEACC_TABLECELLS_H
#define NATIVEACC_TABLECELLS_H

#include "tablecellbase.h"
#include "tablecellimpl.h"
#include "valuetext.h"  // Shared "copy" action name and description

namespace NativeAcc {

/*** Text ***/

// Plain text cell.  No additional properties, just uses the Text role.
class TableCellText : public TableCellBase
{
    Q_OBJECT
public:
    TableCellText() : TableCellBase{QAccessible::Role::StaticText} {}
};

// Base implementation for the button types.  Just provides a "press" action and
// the action interface.
class TableCellButtonBase;
class TableCellButtonBaseImpl : public TableCellImpl,
                                protected QAccessibleActionInterface
{
    Q_OBJECT

public:
    using TableCellImpl::TableCellImpl;

private:
    TableCellButtonBase *buttonBaseDef() const;

public:
    // Overrides of TableCellImpl
    QAccessibleActionInterface *actionInterface() override {return this;}

    // Implementation of QAccessibleActionInterface
    virtual QStringList actionNames() const override;
    virtual void doAction(const QString &actionName) override;
    virtual QStringList keyBindingsForAction(const QString &actionName) const override;
};

/*** Button base ***/

// Button base - just creates a TableCellButtonBaseImpl as its implementation.
class TableCellButtonBase : public TableCellBase
{
    Q_OBJECT

public:
    TableCellButtonBase(QAccessible::Role role, QString activateAction);

protected:
    virtual bool attachImpl(TableCellImpl &cellImpl) override;

public:
    virtual TableCellImpl *createInterface(TableAttached &table,
                                           AccessibleElement &accParent) override;

    const QString &activateAction() const {return _activateAction;}

signals:
    // The button was activated with its Press/Toggle action.
    void activated();

private:
    const QString _activateAction;
};

/*** Button ***/

// Table push button - just a generic button with the proper role
class TableCellButton : public TableCellButtonBase
{
    Q_OBJECT
public:
    TableCellButton()
        : TableCellButtonBase{QAccessible::Role::Button,
                              QAccessibleActionInterface::pressAction()}
    {}
};

/*** Drop-down button ***/

// Table drop-down button - just a generic button with the proper role
class TableCellDropDownButton : public TableCellButtonBase
{
    Q_OBJECT
public:
    TableCellDropDownButton()
        : TableCellButtonBase{QAccessible::Role::ComboBox,
                              QAccessibleActionInterface::pressAction()}
    {}
};

/*** Check button ***/

// Table cell check button implementation - provides a check state.
//
// Check button cells are somewhat poorly represented on Windows.  Qt's UIA
// backend can't produce an item with a "toggle" interface and a "grid item"
// interface - both of these check the item's role.  The "toggle" interface
// probably should have been based on the presence of a "toggle" action instead
// (and/or the "checkable" state).
//
// The least-bad representation is to represent it as an "editable" cell, and
// represent the checked/unchecked state in the cell value.
class TableCellCheckButton;
class TableCellCheckButtonImpl : public TableCellButtonBaseImpl
{
    Q_OBJECT
public:
    TableCellCheckButtonImpl(QAccessible::Role role,
                             TableAttached &parentTable,
                             TableCellCheckButton &definition,
                             AccessibleElement &accParent);

private:
    TableCellCheckButton *checkButtonDef() const;
    void onCheckedChanged();

    // QAccessibleInterface overrides
    virtual QString text(QAccessible::Text t) const override;

public:
    // Attach to a new definition
    void reattach(TableCellCheckButton &definition);
};

// Table check button - provides a 'checked' property that should be bound to
// the check state of the button.  It's the responsibility of the QML code to
// actually change the state when the button is pressed.
class TableCellCheckButton : public TableCellButtonBase
{
    Q_OBJECT
    Q_PROPERTY(bool checked READ checked WRITE setChecked NOTIFY checkedChanged)

private:
    static const QString &activateAction();

public:
    TableCellCheckButton();

protected:
    virtual bool attachImpl(TableCellImpl &cellImpl) override;

public:
    virtual TableCellImpl *createInterface(TableAttached &table,
                                           AccessibleElement &accParent) override;

    bool checked() const {return _checked;}
    void setChecked(bool checked);

signals:
    void checkedChanged();

private:
    bool _checked;
};

// Table value text.  Because this provides the "copy" action, it's implemented
// with TableCellButtonBase in order to handle the action.  It's not a button
// in any other way, it uses the EditableText role.
//
// This has some limitations on the Windows UIA backend like
// TableCellCheckButton; in particular we have to use the "press" action instead
// of the custom "copy" action.
class TableCellValueText;
class TableCellValueTextImpl : public TableCellButtonBaseImpl,
                               public QAccessibleTextInterface
{
    Q_OBJECT

public:
    TableCellValueTextImpl(QAccessible::Role role,
                           TableAttached &parentTable,
                           TableCellValueText &definition,
                           AccessibleElement &accParent);

private:
    TableCellValueText *valueTextDef() const;
    void onValueChanged();
    void onCopiableChanged();

public:
    // QAccessibleInterface overrides
    virtual QString text(QAccessible::Text t) const override;
    virtual QAccessibleTextInterface *textInterface() override {return this;}

    // QAccessibleActionInterface overrides
    virtual QStringList actionNames() const override;
    virtual QString localizedActionDescription(const QString &actionName) const override;
    virtual QString localizedActionName(const QString &actionName) const override;

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

public:
    void reattach(TableCellValueText &definition);

private:
    // The last value is needed to report "text update" events.
    QString _value;
    // Needed to report actions and check for a change when a new definition is
    // reattached
    bool _copiable;
};

class TableCellValueText : public TableCellButtonBase
{
    Q_OBJECT
    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(bool copiable READ copiable WRITE setCopiable NOTIFY copiableChanged)

private:
    static const QString &activateAction();

public:
    TableCellValueText();

protected:
    virtual bool attachImpl(TableCellImpl &cellImpl) override;

public:
    virtual TableCellImpl *createInterface(TableAttached &table,
                                           AccessibleElement &accParent) override;

    QString value() const {return _value;}
    void setValue(const QString &value);
    bool copiable() const {return _copiable;}
    void setCopiable(bool copiable);

signals:
    void valueChanged();
    void copiableChanged();

private:
    QString _value;
    bool _copiable;
};

/*** Column ***/

// Column and Row are implemented with cells because they are basically the
// same.  The column and row accesssibility elements are only used on Mac OS
// (Table creates them on all platforms, but they're only reported as parts of
// the table on Mac OS.)

// Column is just an accessible element with the column type.  Columns don't
// have children on any platform (on Mac, the cells are the children of the
// row).
class TableColumn : public TableCellBase
{
    Q_OBJECT
public:
    TableColumn() : TableCellBase{QAccessible::Role::Column} {}
};

/*** Row ***/

// Accessibility element implementation for TableRow.
// Rows have additional functionality:
// - They return the cells as their children
// - The provide outlining information (what level row this is, its "outline
//   child" rows, its "outline parent" row, and whether this row is expanded/
//   collapsed.
class TableRow;
class TableRowImpl : public TableCellImpl, private AccessibleRowFiller
{
    Q_OBJECT

public:
    using TableCellImpl::TableCellImpl;
    QList<QAccessibleInterface*> getAccChildren() const;

private:
    TableRow *rowDefinition() const;

    // Overrides of QAccessibleInterface
    QAccessibleInterface *child(int index) const override;
    int childCount() const override;
    int indexOfChild(const QAccessibleInterface *child) const override;

    // Overrides of AccessibleElement
    virtual AccessibleRowFiller *rowFillerInterface() override {return this;}

    // Implementation of AccessibleRowFiller
    virtual int getOutlineLevel() const override;
    virtual bool getExpanded() const override;
    virtual QAccessibleInterface *getOutlineParent() const override;
    virtual QList<QAccessibleInterface *> getOutlineChildren() const override;
};

// TableRow definition.  Creates a TableRowImpl implementation as its accessibility
// element.  Also defines outlineExpanded and outlineLevel properties.
class TableRow : public TableCellBase
{
    Q_OBJECT
    Q_PROPERTY(bool selected READ selected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(bool outlineExpanded READ outlineExpanded WRITE setOutlineExpanded NOTIFY outlineExpandedChanged)
    Q_PROPERTY(int outlineLevel READ outlineLevel WRITE setOutlineLevel NOTIFY outlineLevelChanged)

public:
    TableRow();

protected:
    virtual bool attachImpl(TableCellImpl &cellImpl) override;

public:
    virtual TableCellImpl *createInterface(TableAttached &table,
                                           AccessibleElement &accParent) override;

    bool selected() const {return _selected;}
    void setSelected(bool selected);
    bool outlineExpanded() const {return _outlineExpanded;}
    void setOutlineExpanded(bool outlineExpanded);
    int outlineLevel() const {return _outlineLevel;}
    void setOutlineLevel(int outlineLevel);

signals:
    void selectedChanged();
    void outlineExpandedChanged();
    void outlineLevelChanged();

private:
    bool _selected;
    bool _outlineExpanded;
    int _outlineLevel;
};

}

#endif
