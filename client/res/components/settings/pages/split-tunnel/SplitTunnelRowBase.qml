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
import "../../../common"
import "../../../theme"
import "../"

TableRowBase {
  // Indices of the keyboard navigation columns in this row.
  //
  // The keyboard navigation model has three columns: 'app', 'mode', and 'remove':
  // - The 'app' column has no selectable action, but it seems surprising that
  //   the 'mode' column would be highlighted by default, and it isn't very
  //   consistent with the 'add app' row.
  // - We expect to add more controls in the future (rule types, etc.).  This
  //   model extends naturally to more columns.
  //
  // The accessibility model also has an additional column - 'path'.  The
  // keyboard model visually includes this in the 'app' column, but it's
  // separate for accessibility to keep the annotations to a reasonable length.
  readonly property var keyColumns: ({
    app: 0,
    mode: 1,
    remove: 2
  })

  readonly property var appModeChoices: [
    {name: uiTr("Bypass VPN")},
    {name: uiTr("Only VPN")}
  ]

  readonly property var otherAppModeChoices: [
    {name: uiTr("Bypass VPN")},
    {name: uiTr("Use VPN")}
  ]

  readonly property int dropdownWidth: 160
  readonly property int labelFontSize: 14
  readonly property int iconSize: 26
  readonly property int textLeftMargin: 50
  readonly property int iconLeftMargin: 10

  readonly property var nameServersChoices: [
    //: Indicates that name servers will match app rules - bypass apps will
    //: also bypass the VPN DNS to use the existing name servers, and VPN apps
    //: will use VPN DNS.
    {name: uiTr("Follow App Rules")},
    //: Indicates that all apps will use VPN DNS, regardless of whether the app
    //: is set to bypass or use the VPN.
    {name: uiTr("VPN DNS Only")}
  ]

  property var accAppCell
  property var accPathCell
  property var accModeCell
  property var accRemoveCell

  Rectangle {
    anchors.fill: parent
    color: Theme.settings.inlayRegionColor
    radius: 5
  }
}
