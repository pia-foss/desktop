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

import QtQuick 2.9
import PIA.NativeAcc 1.0 as NativeAcc

// Base for rows used in Table models.  Defines common accessibility model
// elements for list rows.
Item {
  // Parent TableBase containing this TableRowBase.  The keyboardRow and
  // highlightColumn properties are used to derive highlightColumn, and
  // focusCell is conneted to the the parent table's mouseFocusCell().
  property TableBase parentTable

  // This row's ID string used in the accessibility table, see TableBase.
  property string rowId

  // Functor to get the effective column for a keyboard column in this row.
  // (Some rows do not have all columns.)
  // - function effectiveColumnFor(column)
  property var effectiveColumnFor

  // If this row is higlighted (focus cue is displayed), the index of the cell
  // to highlight (in the keyboard model if the keyboard/accessibility columns
  // differ).
  //
  // If the row does not actually have the column specified, highlight the
  // effective column for that column instead.
  //
  // If no cell is highlighted in this row, set to -1.
  readonly property int highlightColumn: {
    return (parentTable && parentTable.keyboardRow === rowId) ? parentTable.highlightColumn : -1
  }

  // Functor to select a cell in this row using the keyboard.  This may do
  // nothing for some columns, like name/path labels, etc.  If the row does
  // not have all columns, it should activate the effective column for the
  // specified column.
  //
  // This does not need to emit focusCell().
  // - function keyboardSelect(keyboardColumn)
  property var keyboardSelect

  // Signal emitted if a cell in this row is focused by the mouse.  Causes the
  // list control to take focus.
  signal focusCell(int column)
  onFocusCell: parentTable && parentTable.mouseFocusCell(rowId, column)

  // Screen reader annotation for the whole row
  property NativeAcc.TableRow accRow

  // Additionally, table rows must define NativeAcc TableCell models using
  // properties defined by the table.
}
