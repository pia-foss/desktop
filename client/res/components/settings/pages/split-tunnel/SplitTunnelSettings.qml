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

import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick.Window 2.3
import "../../../core"
import "../../../theme"
import "../../../common"
import "../../../client"
import "../../../daemon"
import Qt.labs.platform 1.1
import PIA.SplitTunnelManager 1.0
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util
import "../"


// List of existing split tunnel app rules
TableBase {
  id: splitTunnelSettings
  color: "transparent"
  border.color: "transparent"
  hideTableFocusCue: true

  // Shadow the default enabled property; SplitTunnelSettings remains scrollable
  // when disabled.
  property bool enabled: true

  // The label for SplitTunnelList comes from the associated check box; it's
  // needed for accessibility

  // The initial keyboard row is the "add app" row.
  // Add app: "add/"
  // Add subnet: "addIp/"
  // Subnet: "ip/<subnet>"
  // App: "app/<path>"
  // Routed packets: "routed/"
  // All other apps: "other/"
  // DNS: "dns/"
  keyboardRow: buildRowId("add") // TODO: handle default row to point to appropriate row

  keyboardColumnCount: 3

  contentHeight: selectedAppLayout.implicitHeight

  ColumnLayout {
    id: selectedAppLayout
    spacing: 3
    width: parent.width
    opacity: enabled ? 1.0 : 0.6
    // When disabled, disable the contents of the scroll view (but not the
    // scroll view itself)
    enabled: splitTunnelSettings.enabled
    Repeater {
      id: ipRuleRepeater
      model: Daemon.settings.bypassSubnets
      delegate: SplitTunnelIpRow {
        Layout.fillWidth: true
        subnet: modelData.subnet
        parentTable: splitTunnelSettings
        rowId: splitTunnelSettings.buildRowId("ip", subnet)
      }
    }

    Repeater {
      id: appRuleRepeater
      model: Daemon.settings.splitTunnelRules
      delegate: SplitTunnelAppRow {
        Layout.fillWidth: true
        showAppIcons: Qt.platform.os !== 'linux'
        appPath: modelData.path
        linkTarget: modelData.linkTarget
        appMode: modelData.mode
        appName: SplitTunnelManager.getNameFromPath(appPath)
        parentTable: splitTunnelSettings
        rowId: splitTunnelSettings.buildRowId("ip", appPath)
      }
    }

    SplitTunnelRoutedRow {
      id: routedPacketsRow
      Layout.fillWidth: true
      visible: Qt.platform.os == 'linux'
      parentTable: splitTunnelSettings
      rowId: splitTunnelSettings.buildRowId("routed")
    }

    SplitTunnelDefaultRow {
      id: defaultBehaviourRow
      Layout.fillWidth: true
      parentTable: splitTunnelSettings
      rowId: splitTunnelSettings.buildRowId("other")
    }

    SplitTunnelNameServersRow {
      id: nameServersRow
      Layout.fillWidth: true
      // Split tunnel DNS is not supported on Mac yet.
      visible: Qt.platform.os !== 'osx'
      parentTable: splitTunnelSettings
      rowId: splitTunnelSettings.buildRowId("dns")
    }
  }

  // Keyboard nav / accessibility representation.
  // This is an array of objects with the following properties:
  // - type - What type of row this is, either 'add' or 'app'.
  //   (Mainly out of paranoia in case an app path would somehow be 'add',
  //   avoids any possible conflict.)
  // - value - For 'app' rows, the app path.
  // - name - The row's display name
  // - item - The QML item - SplitTunnelAppRow (or the add app row)
  accTable: {
    var childrenDep = appRuleRepeater.children
    var ipRuleChildrenDep = ipRuleRepeater.children
    var table = []

    function pushRow(rowItem, displayName) {
      table.push({row: rowItem.rowId, name: displayName || rowItem.displayName,
                  item: rowItem})
    }

    // Ip rows
    for(var i=0; i<ipRuleRepeater.count; ++i) {
      var ipRowItem = ipRuleRepeater.itemAt(i)
      if(!ipRowItem)
        continue

      pushRow(ipRowItem, ipRowItem.subnet)
    }

    // App rows
    for(i=0; i<appRuleRepeater.count; ++i) {
      var appRowItem = appRuleRepeater.itemAt(i)
      if(!appRowItem)
        continue

      pushRow(appRowItem, appRowItem.appName)
    }

    // 'Routed packets' row
    if(routedPacketsRow.visible)
      pushRow(routedPacketsRow)

    // 'Other apps' row
    pushRow(defaultBehaviourRow)

    // 'Name servers' row
    if(nameServersRow.visible)
      pushRow(nameServersRow)

    return table
  }

  property NativeAcc.TableColumn appColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the column in the split tunnel app list
    //: that displays app names.
    name: uiTr("App")
    item: splitTunnelSettings
  }

  property NativeAcc.TableColumn pathColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the column in the split tunnel app list
    //: that displays app file paths.  (These are visually placed below the
    //: app names, but they're annotated as a separate column.)
    name: uiTr("Path")
    item: splitTunnelSettings
  }

  property NativeAcc.TableColumn modeColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the column in the split tunnel app list
    //: that displays the behavior selected for a specific app.
    name: uiTr("Behavior")
    item: splitTunnelSettings
  }

  property NativeAcc.TableColumn removeColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the column in the split tunnel app list
    //: that removes a selected app.
    name: uiTr("Remove")
    item: splitTunnelSettings
  }

  accColumns: [
    { property: "accAppCell", column: appColumn },
    { property: "accPathCell", column: pathColumn },
    { property: "accModeCell", column: modeColumn },
    { property: "accRemoveCell", column: removeColumn }
  ]

  accColumnForKeyColumn: function(keyCol) {
    switch(keyCol) {
      case 0: // SplitTunnelAppRow.keyColumns.app
        return 0
      case 1: // SplitTunnelAppRow.keyColumns.mode
      case 2: // SplitTunnelAppRow.keyColumns.remove
        return keyCol + 1 // Keyboard model skips 'path' acc column
    }
  }
}
