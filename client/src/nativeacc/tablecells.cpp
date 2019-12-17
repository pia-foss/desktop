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
#line SOURCE_FILE("tablecells.cpp")

#include "tablecells.h"
#include "table.h"
#include <typeinfo>

namespace NativeAcc {

/*** Button base ***/

void *TableCellButtonBaseImpl::interface_cast(QAccessible::InterfaceType type)
{
    if(type == QAccessible::InterfaceType::ActionInterface)
    {
        QAccessibleActionInterface *pThisItf = this;
        return reinterpret_cast<void*>(pThisItf);
    }

    return TableCellImpl::interface_cast(type);
}

TableCellButtonBase *TableCellButtonBaseImpl::buttonBaseDef() const
{
    TableCellBase *pBaseDef = definition();
    if(!pBaseDef)
        return nullptr;

    TableCellButtonBase *pBtnDef = dynamic_cast<TableCellButtonBase*>(pBaseDef);
    Q_ASSERT(pBtnDef); // Should be a button
    return pBtnDef;
}

QStringList TableCellButtonBaseImpl::actionNames() const
{
    TableCellButtonBase *pBtnDef = buttonBaseDef();
    // We don't store a copy of the action name, just get it from the
    // definition; it can't be changed.
    if(pBtnDef)
        return {pBtnDef->activateAction()};
    return {};
}

void TableCellButtonBaseImpl::doAction(const QString &actionName)
{
    TableCellButtonBase *pBtnDef = buttonBaseDef();
    if(!pBtnDef)
    {
        qWarning() << "Action applied to button after definition was destroyed:" << actionName;
        return;
    }

    if(actionName == pBtnDef->activateAction())
        emit pBtnDef->activated();
    else
        qWarning() << "Unknown action applied to table button:" << actionName;
}

QStringList TableCellButtonBaseImpl::keyBindingsForAction(const QString &) const
{
    return {};  // Stub, key bindings not supported right now
}

TableCellButtonBase::TableCellButtonBase(QAccessible::Role role,
                                         QString activateAction)
    : TableCellBase{role}, _activateAction{activateAction}
{
}

bool TableCellButtonBase::attachImpl(TableCellImpl &cellImpl)
{
    // Nothing specific to actually reattach, just make sure it's a button
    if(typeid(cellImpl) == typeid(TableCellButtonBaseImpl))
    {
        cellImpl.reattach(*this);
        return true;
    }

    return false;
}

TableCellImpl *TableCellButtonBase::createInterface(TableAttached &table,
                                                    AccessibleElement &accParent)
{
    return new TableCellButtonBaseImpl{role(), table, *this, accParent};
}

/*** Check button ***/

TableCellCheckButtonImpl::TableCellCheckButtonImpl(QAccessible::Role role,
                                                   TableAttached &parentTable,
                                                   TableCellCheckButton &definition,
                                                   AccessibleElement &accParent)
    : TableCellButtonBaseImpl{role, parentTable, definition, accParent}
{
    setState(StateField::checkable, true);
    // Check button cells _do_ report as "editable" on Windows.
    setState(StateField::readOnly, false);
    QObject::connect(&definition, &TableCellCheckButton::checkedChanged, this,
                     &TableCellCheckButtonImpl::onCheckedChanged);
    onCheckedChanged();
}

TableCellCheckButton *TableCellCheckButtonImpl::checkButtonDef() const
{
    TableCellBase *pBaseDef = definition();
    if(!pBaseDef)
        return nullptr;

    TableCellCheckButton *pCheckBtnDef = dynamic_cast<TableCellCheckButton*>(pBaseDef);
    Q_ASSERT(pCheckBtnDef); // Should be a check button
    return pCheckBtnDef;
}

void TableCellCheckButtonImpl::onCheckedChanged()
{
    TableCellCheckButton *pCheckBtnDef = checkButtonDef();
    if(pCheckBtnDef)
        setState(StateField::checked, pCheckBtnDef->checked());
}

QString TableCellCheckButtonImpl::text(QAccessible::Text t) const
{
#ifdef Q_OS_WIN
    if(t == QAccessible::Text::Value)
    {
        if(getState(StateField::checked))
        {
            //: Value indicator for a toggle button in the "checked" state.
            //: Should use normal terminology for a check box or toggle button.
            //: (Screen reader annotation for "Favorite region" table cell
            //: button on Windows.)
            return tr("checked");
        }
        else
        {
            //: Value indicator for a toggle button in the "unchecked" state.
            //: Should use normal terminology for a check box or toggle button.
            //: (Screen reader annotation for "Favorite region" table cell
            //: button on Windows.)
            return tr("unchecked");
        }
    }
#endif
    return TableCellButtonBaseImpl::text(t);
}

void TableCellCheckButtonImpl::reattach(TableCellCheckButton &definition)
{
    // Disconnect the old signal if there was one
    TableCellCheckButton *pOldDef = checkButtonDef();
    if(pOldDef)
        QObject::disconnect(pOldDef, nullptr, this, nullptr);

    // Call base to change to the new definition
    TableCellButtonBaseImpl::reattach(definition);

    // Attach the new signal
    QObject::connect(&definition, &TableCellCheckButton::checkedChanged, this,
                     &TableCellCheckButtonImpl::onCheckedChanged);
    // Check for property changes
    onCheckedChanged();
}

const QString &TableCellCheckButton::activateAction()
{
#ifdef Q_OS_WIN
    // On Windows, we can't represent a toggle button in a table, the role is
    // just "cell".  The UI backend sends cells a "press" action even if they
    // report some other action, so the activate action has to be "press" on
    // Windows.
    return QAccessibleActionInterface::pressAction();
#else
    // On all other platforms, use the correct action, "toggle".
    return QAccessibleActionInterface::toggleAction();
#endif

}

TableCellCheckButton::TableCellCheckButton()
    : TableCellButtonBase{QAccessible::Role::CheckBox, activateAction()},
      _checked{false}
{
}

bool TableCellCheckButton::attachImpl(TableCellImpl &cellImpl)
{
    if(typeid(cellImpl) == typeid(TableCellCheckButtonImpl))
    {
        TableCellCheckButtonImpl &checkBtnImpl = static_cast<TableCellCheckButtonImpl&>(cellImpl);
        checkBtnImpl.reattach(*this);
        return true;
    }

    return false;
}

TableCellImpl *TableCellCheckButton::createInterface(TableAttached &table,
                                                     AccessibleElement &accParent)
{
    return new TableCellCheckButtonImpl{role(), table, *this, accParent};
}

void TableCellCheckButton::setChecked(bool checked)
{
    if(checked != _checked)
    {
        _checked = checked;
        emit checkedChanged();
    }
}

/*** Row ***/

TableRow *TableRowImpl::rowDefinition() const
{
    TableCellBase *pBaseDef = definition();
    if(!pBaseDef)
        return nullptr;

    // The definition should be a TableRow, there shouldn't be a way to attach to
    // some other type of definition.
    TableRow *pRowDef = dynamic_cast<TableRow*>(pBaseDef);
    Q_ASSERT(pRowDef);
    return pRowDef;
}

QList<QAccessibleInterface*> TableRowImpl::getAccChildren() const
{
    return parentTable().getRowCells(rowIndex());
}

QAccessibleInterface *TableRowImpl::child(int index) const
{
    const auto &children = getAccChildren();
    if(index >= 0 && index < children.size())
        return children[index];
    return nullptr;
}

int TableRowImpl::childCount() const
{
    return getAccChildren().size();
}

int TableRowImpl::indexOfChild(const QAccessibleInterface *child) const
{
    return getAccChildren().indexOf(const_cast<QAccessibleInterface*>(child));
}

int TableRowImpl::getOutlineLevel() const
{
    TableRow *pRowDef = rowDefinition();
    return pRowDef ? pRowDef->outlineLevel() : 0;
}

bool TableRowImpl::getExpanded() const
{
    TableRow *pRowDef = rowDefinition();
    return pRowDef ? pRowDef->outlineExpanded() : 0;
}

QAccessibleInterface *TableRowImpl::getOutlineParent() const
{
    return parentTable().getRowOutlineParent(rowIndex());
}

QList<QAccessibleInterface *> TableRowImpl::getOutlineChildren() const
{
    return parentTable().getRowOutlineChildren(rowIndex());
}

TableRow::TableRow()
    : TableCellBase{QAccessible::Role::Row}, _selected{false},
      _outlineExpanded{false}, _outlineLevel{0}
{
}

bool TableRow::attachImpl(TableCellImpl &cellImpl)
{
    // Nothing specific to actually reattach, just make sure it's a TableRowImpl
    if(typeid(cellImpl) == typeid(TableRowImpl))
    {
        cellImpl.reattach(*this);
        return true;
    }

    // TODO - Possibly emit changes for outlineExpanded changing (attach to
    // cellImpl's signal here)
    return false;
}

TableCellImpl *TableRow::createInterface(TableAttached &table,
                                        AccessibleElement &accParent)
{
    return new TableRowImpl{role(), table, *this, accParent};
}

void TableRow::setSelected(bool selected)
{
    if(selected != _selected)
    {
        _selected = selected;
        emit selectedChanged();
    }
}

void TableRow::setOutlineExpanded(bool outlineExpanded)
{
    if(outlineExpanded != _outlineExpanded)
    {
        _outlineExpanded = outlineExpanded;
        emit outlineExpandedChanged();
    }
}

void TableRow::setOutlineLevel(int outlineLevel)
{
    if(outlineLevel != _outlineLevel)
    {
        _outlineLevel = outlineLevel;
        emit outlineLevelChanged();
    }
}

}
