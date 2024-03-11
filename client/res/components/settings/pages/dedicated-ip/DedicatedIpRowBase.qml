// Copyright (c) 2024 Private Internet Access, Inc.
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
import "../../../common"
import "../../../theme"

TableRowBase {
  // Indices of the keyboard navigation columns in this list.
  //
  // There are three columns - 'region', 'ip', and 'remove'.
  //
  // Like the split tunnel list, the first column ('region') has no selectable
  // action, but it'd be surprising if 'ip' was the first selectable column, and
  // it would be inconsistent with the 'add' row.
  readonly property var keyColumns: ({
    region: 0,
    ip: 1,
    remove: 2
  })

  readonly property int keyColumnCount: 3

  //: Screen reader annotation for the column in the Dedicated IP list that
  //: displays the IP address for that dedicated IP.
  readonly property string ipAddressColumnName: uiTr("IP Address")

  // Rows must define these cell annotations
  property var accRegionCell
  property var accIpCell
  property var accRemoveCell

  Rectangle {
    anchors.fill : parent
    radius: 10
    color: Theme.settings.inlayRegionColor
  }
}
