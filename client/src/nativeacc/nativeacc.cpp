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
#line SOURCE_FILE("nativeacc.cpp")

#include "nativeacc.h"
#include "accessibleimpl.h"
#include "tablecells.h"
#include "controls.h"
#include "buttons.h"
#include "table.h"
#include "textfield.h"
#include "progressbar.h"
#include "valuetext.h"
#include "window.h"
#include <QQmlEngine>
#include <QAccessible>

#ifdef Q_OS_MAC
#include "mac/mac_accessibility.h"
#endif

namespace NativeAcc {

void init()
{
#ifdef Q_OS_MAC
    // Mac specific initialization
    macAccInit();
#endif

    // Generic controls (controls.h)
    qmlRegisterType<Label>("PIA.NativeAcc", 1, 0, "Label");
    qmlRegisterType<Text>("PIA.NativeAcc", 1, 0, "Text");
    qmlRegisterType<Group>("PIA.NativeAcc", 1, 0, "Group");
    qmlRegisterType<Graphic>("PIA.NativeAcc", 1, 0, "Graphic");
    qmlRegisterType<Chart>("PIA.NativeAcc", 1, 0, "Chart");
    qmlRegisterType<TabList>("PIA.NativeAcc", 1, 0, "TabList");
    qmlRegisterType<PopupMenu>("PIA.NativeAcc", 1, 0, "PopupMenu");
    qmlRegisterType<Dialog>("PIA.NativeAcc", 1, 0, "Dialog");

    // Buttons (buttons.h)
    qmlRegisterType<Button>("PIA.NativeAcc", 1, 0, "Button");
    qmlRegisterType<Link>("PIA.NativeAcc", 1, 0, "Link");
    qmlRegisterType<MenuButton>("PIA.NativeAcc", 1, 0, "MenuButton");
    qmlRegisterType<DropDownButton>("PIA.NativeAcc", 1, 0, "DropDownButton");
    qmlRegisterType<CheckButton>("PIA.NativeAcc", 1, 0, "CheckButton");
    qmlRegisterType<RadioButton>("PIA.NativeAcc", 1, 0, "RadioButton");
    qmlRegisterType<Tab>("PIA.NativeAcc", 1, 0, "Tab");
    qmlRegisterType<ActionMenuItem>("PIA.NativeAcc", 1, 0, "ActionMenuItem");
    qmlRegisterType<DropDownMenuItem>("PIA.NativeAcc", 1, 0, "DropDownMenuItem");
    qmlRegisterType<MoveButton>("PIA.NativeAcc", 1, 0, "MoveButton");

    // Complex controls
    qmlRegisterType<Table>("PIA.NativeAcc", 1, 0, "Table");
    qmlRegisterType<TextField>("PIA.NativeAcc", 1, 0, "TextField");
    qmlRegisterType<ProgressBar>("PIA.NativeAcc", 1, 0, "ProgressBar");
    qmlRegisterType<ValueText>("PIA.NativeAcc", 1, 0, "ValueText");

    // Cell definitions for Table
    qmlRegisterType<TableCellText>("PIA.NativeAcc", 1, 0, "TableCellText");
    qmlRegisterType<TableCellButton>("PIA.NativeAcc", 1, 0, "TableCellButton");
    qmlRegisterType<TableCellCheckButton>("PIA.NativeAcc", 1, 0, "TableCellCheckButton");
    qmlRegisterType<TableColumn>("PIA.NativeAcc", 1, 0, "TableColumn");
    qmlRegisterType<TableRow>("PIA.NativeAcc", 1, 0, "TableRow");

    // Interface factories
    QAccessible::installFactory(&WindowAccImpl::interfaceFactory);
    QAccessible::installFactory(&AccessibleImpl::interfaceFactory);
}

}
