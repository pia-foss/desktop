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
import "../../common"
import "../../theme"

TableRowBase {
  // Indices of the keyboard navigation columns in this list.
  readonly property var keyColumns: ({
    condition: 0,
    action: 1,
    remove: 2
  })

  readonly property int keyColumnCount: 3

  // Rows must define these cell annotations
  property var accConditionCell
  property var accActionCell
  property var accRemoveCell

  Rectangle {
    anchors.bottom: parent.bottom
    height: 1
    color: Theme.settings.splitTunnelItemSeparatorColor
    opacity: 0.5
    anchors.left: parent.left
    anchors.right: parent.right
  }
}
