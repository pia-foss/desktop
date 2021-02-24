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
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick.Window 2.3
import "../../core"
import "../../theme"
import "../../common"
import "../../client"
import "../../daemon"
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util

TableBase {
  id: dedicatedIpList

  //: Screen reader label for the list of Dedicated IPs.
  label: uiTr("Dedicated IPs")

  readonly property bool showAddRow: {
    // Both clients and Web are going to limit to one configured DIP in order to
    // support renewals, we want this change to occur simultaneously with the
    // Web changes
    if(!Daemon.data.flags.includes("limit_one_dip"))
      return true // Not limiting to one DIP yet

    // It seems that dedicatedIpRepeater.count doesn't always report changes
    // correctly - hack in a dependency on the children.
    var dep = dedicatedIpRepeater.children

    // Only one DIP can be added, show the add row when no DIPs have been added
    // yet.
    return dedicatedIpRepeater.count === 0
  }

  // The initial keyboard row is the "add" row.
  // Add row: "add/" (no value)
  // DIP row: "dip/<dip-region-id>"
  keyboardRow: {
    if(dedicatedIpList.showAddRow)
      return buildRowId("add")
    else
      return buildRowId("dip", Daemon.state.dedicatedIpLocations[0].id)
  }

  verticalScrollPolicy: ScrollBar.AsNeeded
  contentHeight: dedicatedIpsLayout.implicitHeight

  keyboardColumnCount: addDedicatedIpRow.keyColumnCount

  ColumnLayout {
    id: dedicatedIpsLayout
    spacing: 0
    width: parent.width

    DedicatedIpAddRow {
      visible: dedicatedIpList.showAddRow
      id: addDedicatedIpRow
      Layout.fillWidth: true
      parentTable: dedicatedIpList
      rowId: dedicatedIpList.buildRowId("add")
    }

    Repeater {
      id: dedicatedIpRepeater
      model: Daemon.state.dedicatedIpLocations
      delegate: DedicatedIpRow {
        Layout.fillWidth: true
        regionCountry: modelData.country
        regionName: Daemon.getLocationName(modelData)
        regionIp: modelData.dedicatedIp
        regionId: modelData.id

        parentTable: dedicatedIpList
        rowId: dedicatedIpList.buildRowId("dip", regionId)
      }
    }
  }

  property NativeAcc.TableColumn regionColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the column in the Dedicated IP list that
    //: displays region names.
    name: uiTr("Region")
    item: dedicatedIpList
  }

  property NativeAcc.TableColumn ipColumn: NativeAcc.TableColumn {
    // Constant from DedicatedIpRowBase (must use a specific object to access it
    // though, fortunately we can just use the "add" row)
    name: addDedicatedIpRow.ipAddressColumnName
    item: dedicatedIpList
  }

  property NativeAcc.TableColumn removeColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the column in the Dedicated IP list that
    //: removes a dedicated IP.
    name: uiTr("Remove")
    item: dedicatedIpList
  }

  accColumns: [
    { property: "accRegionCell", column: regionColumn },
    { property: "accIpCell", column: ipColumn },
    { property: "accRemoveCell", column: removeColumn },
  ]

  // Keyboard nav / accessibility representation.
  // Array of objects with the following properties:
  // - type - 'add' or 'dip'
  // - id - for 'dip' rows, the DIP region ID
  // - name - display name of the row (the region's display name), used for
  //   key navigation
  // - item - the row item for this entry - used to scroll the entry into view
  //   when needed and to get accessibility cell definitions
  accTable: {
    var childrenDep = dedicatedIpRepeater.children
    var table = []

    // 'Add' row
    if(dedicatedIpList.showAddRow)
      table.push({row: buildRowId("add"), name: addDedicatedIpRow.displayName,
                  item: addDedicatedIpRow})

    // DIP rows
    for(var i=0; i<dedicatedIpRepeater.count; ++i) {
      var dipRowItem = dedicatedIpRepeater.itemAt(i)
      if(!dipRowItem)
        continue

      table.push({row: buildRowId("dip", dipRowItem.regionId),
                  name: dipRowItem.regionName, item: dipRowItem})
    }

    return table
  }
}
