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

import QtQuick 2.9
import PIA.NativeAcc 1.0 as NativeAcc

// This is the common base for all rows in the regions list, which includes:
// - RegionAuto (the "auto" row)
// - DedicatedIpRegion (dedicated IP regions)
// - RegionRowBase (country group rows and individual region rows)
//
// (RegionRowBase's name is mostly historical, as originally those were the only
// type of rows.)
//
// This is mainly used for keyboard navigation and accessibility in
// RegionListView.  Each row in the accessibility model is associated with row
// item, which it uses to take actions such as selecting a cell.
Item {
  // Symbolic constants for the "keyboard nav columns" in the regions list.  The
  // count of columns is specified in RegionListView.keyboardColumnCount.
  // This only includes "interactive" columns, "static" columns are only
  // represented in screen reader annotations (keyboard nav doesn't need to
  // reach them).
  readonly property var keyColumns: ({
    region: 0,
    favorite: 1
  })

  readonly property string singleRegionPfWarning: uiTranslate("RegionDelegate", "Port forwarding is not available for this location.")
  readonly property string regionGroupPfWarning: uiTranslate("RegionDelegate", "Port forwarding is not available for this country.")

  // All region rows have a highlightColumn property, which highlights a
  // keyboard cell in the row (the effective cell for the given value, if the
  // region doesn't have that cell).
  //
  // When this row is highlighted with the keyboard, highlightColumn is set to
  // the column that is highlighted (an integer in range
  // [0, keyboardColumnCount-1].  Otherwise, -1 indicates that the row is not
  // highlighted.
  //
  // This just shows highlight cues.  If the user presses Space/Enter,
  // keyboardSelect() actually selects a column.
  property int highlightColumn

  // The regions list should take focus due to a mouse event on a cell
  // Emitted for any click on the row (region or favorite parts), includes the
  // index of the column that was selected
  signal focusCell(int keyColumn)
  // The row was clicked
  signal clicked()

  // Screen reader row and cell annotations
  property NativeAcc.TableRow accRow
  property var accRegionCell
  property var accDetailCell
  property var accLatencyCell
  property var accFavoriteCell
}
