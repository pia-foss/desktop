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
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick.Window 2.3
import "../../core"
import "../../theme"
import "../../common"
import "../../client"
import "../../daemon"
import Qt.labs.platform 1.1
import PIA.SplitTunnelManager 1.0
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util

// List of existing split tunnel app rules
Rectangle {
  id: splitTunnelSettings

  // Shadow the default enabled property; SplitTunnelSettings remains scrollable
  // when disabled.
  property bool enabled: true

  // The label comes from the associated check box; it's needed for accessibility
  property string label

  color: Theme.settings.hbarBackgroundColor
  border.color: Theme.settings.hbarBottomBorderColor
  border.width: 1
  activeFocusOnTab: true

  // Cell selected by keyboard - identified using the type and path from the
  // accTable rows.
  // There's always a keyboard cell - if at any point the selection becomes
  // invalid, it's moved to a nearby valid selection or defaulted to the 'add'
  // row.
  property var keyboardRow: ({type: 'add', value: ''})
  property int keyboardColumn: 0
  // If there's a highlight cell, this is the highlighted column within the
  // keyboard row.  (There's always a keyboard cell, but there's only a
  // highlighted cell when focus cues are shown.)
  property int highlightColumn: focusCue.show ? keyboardColumn : -1
  readonly property int keyboardColumnCount: 3

  function mouseFocusCell(row, column) {
    keyboardRow = row
    keyboardColumn = column
    splitTunnelSettings.forceActiveFocus(Qt.MouseFocusReason)
  }

  ThemedScrollView {
    id: scrollView
    ScrollBar.vertical.policy: ScrollBar.AlwaysOn
    label: uiTr("Applications")
    anchors.fill: parent
    contentWidth: parent.width
    contentHeight: selectedAppLayout.implicitHeight
    clip: true

    Flickable {
      id: scrollViewFlickable
      boundsBehavior: Flickable.StopAtBounds
      // Never a tab stop, scroll using keyboard nav internal to the list.
      activeFocusOnTab: false

      ColumnLayout {
        id: selectedAppLayout
        spacing: 0
        width: parent.width
        opacity: enabled ? 1.0 : 0.6
        // When disabled, disable the contents of the scroll view (but not the
        // scroll view itself)
        enabled: splitTunnelSettings.enabled
        SplitTunnelAddAppRow {
          id: addApplicationRow
          Layout.fillWidth: true
          // Highlight this row if the highlight is being shown for any cell
          // (this row only has 1 column) and if this is the row that's
          // highlighted.
          showHighlight: splitTunnelSettings.highlightColumn !== -1 &&
                   splitTunnelSettings.keyboardRow.type === 'add' &&
                   splitTunnelSettings.keyboardRow.value === ''
          onFocusCell: mouseFocusCell({type: 'add', value: ''}, column)
        }

        SplitTunnelAddIpRow {
          id: addIpRow
          Layout.fillWidth: true
          // Highlight this row if the highlight is being shown for any cell
          // (this row only has 1 column) and if this is the row that's
          // highlighted.
          showHighlight: splitTunnelSettings.highlightColumn !== -1 &&
                   splitTunnelSettings.keyboardRow.type === 'addIp' &&
                   splitTunnelSettings.keyboardRow.value === ''
          onFocusCell: mouseFocusCell({type: 'addIp', value: ''}, column)
        }

        Repeater {
          id: ipRuleRepeater
          model: Daemon.settings.bypassSubnets
          delegate: SplitTunnelIpRow {
            Layout.fillWidth: true
            showAppIcons: Qt.platform.os !== 'linux'
            subnet: modelData.subnet

            highlightColumn: {
              // If the highlighted row is this row, apply the highlight
              // column
              if(splitTunnelSettings.keyboardRow.type === 'ip' &&
                 splitTunnelSettings.keyboardRow.value === subnet) {
                return splitTunnelSettings.highlightColumn
              }
              return -1
            }
            onFocusCell: mouseFocusCell({type: 'ip', value: subnet}, column)
          }
        }

        Repeater {
          id: appRuleRepeater
          model: Daemon.settings.splitTunnelRules
          delegate: SplitTunnelAppRow {
            Layout.fillWidth: true
            showAppIcons: Qt.platform.os !== 'linux'
            appPath: modelData.path
            appMode: modelData.mode
            appName: SplitTunnelManager.getNameFromPath(appPath)

            highlightColumn: {
              // If the highlighted row is this row, apply the highlight
              // column
              if(splitTunnelSettings.keyboardRow.type === 'app' &&
                 splitTunnelSettings.keyboardRow.value === appPath) {
                return splitTunnelSettings.highlightColumn
              }
              return -1
            }
            onFocusCell: mouseFocusCell({type: 'app', value: appPath}, column)
          }
        }

        SplitTunnelRoutedRow {
          id: routedPacketsRow
          Layout.fillWidth: true
          showAppIcons: false
          visible: Qt.platform.os == 'linux'
          highlightColumn: {
            if(splitTunnelSettings.keyboardRow.type === 'routed')
              return splitTunnelSettings.highlightColumn
            return -1
          }
          onFocusCell: mouseFocusCell({type: 'other', value: ''}, column)
        }

        SplitTunnelDefaultRow {
          id: defaultBehaviourRow
          Layout.fillWidth: true
          showAppIcons: Qt.platform.os !== 'linux'
          highlightColumn: {
            if(splitTunnelSettings.keyboardRow.type === 'other')
              return splitTunnelSettings.highlightColumn
            return -1
          }
          onFocusCell: mouseFocusCell({type: 'other', value: ''}, column)
        }

        SplitTunnelNameServersRow {
          id: nameServersRow
          Layout.fillWidth: true
          showAppIcons: Qt.platform.os !== 'linux'
          // Split tunnel DNS is not supported on Mac yet.
          visible: Qt.platform.os !== 'osx'
          highlightColumn: {
            if(splitTunnelSettings.keyboardRow.type === 'nameservers')
              return splitTunnelSettings.highlightColumn
            return -1
          }
          onFocusCell: mouseFocusCell({type: 'nameservers', value: ''}, column)
        }
      }
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
  property var accTable: {
    var childrenDep = appRuleRepeater.children
    var table = []

    // 'Add' row
    table.push({type: 'add', value: '', name: addApplicationRow.displayName,
                item: addApplicationRow})

    // 'AddIp' row
    table.push({type: 'addIp', value: '', name: addIpRow.displayName,
                item: addIpRow})

    // Ip rows
    for(var i=0; i<ipRuleRepeater.count; ++i) {
      var ipRowItem = ipRuleRepeater.itemAt(i)
      if(!ipRowItem)
        continue

      table.push({type: 'ip',
                  value: ipRowItem.subnet, item: ipRowItem})
    }

    // App rows
    for(var i=0; i<appRuleRepeater.count; ++i) {
      var appRowItem = appRuleRepeater.itemAt(i)
      if(!appRowItem)
        continue

      table.push({type: 'app', value: appRowItem.appPath,
                  name: appRowItem.appName, item: appRowItem})
    }

    // 'Routed packets' row
    if(routedPacketsRow.visible) {
      table.push({type: 'routed', value: '', name: routedPacketsRow.displayName,
                  item: routedPacketsRow})
    }

    // 'Other apps' row
    table.push({type: 'other', value: '', name: defaultBehaviourRow.displayName,
                item: defaultBehaviourRow})

    return table
  }
  function findAccKeyboardIndex(table) {
    return table.findIndex(function(row) {
      return row.type === keyboardRow.type && row.value === keyboardRow.value
    })
  }
  // If the accessibility table changes, and the current keyboard row is
  // removed, select another row.
  property var lastAccTable: []
  onAccTableChanged: {
    var oldKeyboardIdx = findAccKeyboardIndex(lastAccTable)

    // There should always be a selection - if there aren't any apps to
    // select, select the 'add' row by default.
    //
    // If there's no selection now, start looking for a new one from the last
    // selected item.  (If there was no selected item before, then
    // oldKeyboardIdx is -1 and we start looking from the beginning of the
    // list - the 'add' row.)
    //
    // If we can't find an item after the item that was removed (the last
    // item(s) in the list were removed), look for a prior item that exists.
    var nextDir = 1
    var nextKeyboardIdx = oldKeyboardIdx
    while(findAccKeyboardIndex(accTable) < 0) {
      nextKeyboardIdx += nextDir
      // If we're past the end of the list, switch to looking for prior items
      if(nextKeyboardIdx >= lastAccTable.length) {
        nextDir = -1
        nextKeyboardIdx = oldKeyboardIdx-1
      }
      // If we're past the beginning of the list, select the first row.  This
      // doesn't normally happen since the 'add' row should almost always be
      // in both tables.  It can happen at startup when the tables are first
      // built.
      if(nextKeyboardIdx < 0) {
        keyboardRow = {type: 'add', value: ''}
        break
      }
      keyboardRow = {type: lastAccTable[nextKeyboardIdx].type, value: lastAccTable[nextKeyboardIdx].value}
    }

    lastAccTable = accTable
  }

  property var inKeyPress: null

  Keys.onPressed: {
    // Find the index of the current keyboard row (-1 if none)
    var keyboardIdx = findAccKeyboardIndex(accTable)

    var nextIdx = -1
    // Check for an 'accept' key
    if(KeyUtil.handleButtonKeyEvent(event)) {
      inKeyPress = event.key
      // Row might not change, but still reveal it
      nextIdx = keyboardIdx
    }
    // Left and Right just move columns - other navigation keys navigate rows
    else if(event.key === Qt.Key_Left) {
      keyboardColumn = Math.max(0, keyboardColumn-1)
      event.accepted = true
      nextIdx = keyboardIdx  // Row doesn't change, but still reveal it
    }
    else if(event.key === Qt.Key_Right) {
      keyboardColumn = Math.min(keyboardColumnCount-1, keyboardColumn+1)
      event.accepted = true
      nextIdx = keyboardIdx
    }
    else {
      nextIdx = KeyUtil.handleVertKeyEvent(event, accTable, 'name',
                                           keyboardIdx)
    }

    // If this wasn't a key event we understood, don't do anything else.
    if(nextIdx === -1)
      return

    // Find the new row and update keyboardRow
    var newKeyboardRow = accTable[nextIdx]
    keyboardRow = {type: newKeyboardRow.type, value: newKeyboardRow.value}
    console.info("keyboard col " + keyboardColumn + " of " + JSON.stringify(keyboardRow))
    // Reveal the focus cue for the whole list
    focusCue.reveal()
    // Reveal the current row
    var bound = newKeyboardRow.item.mapToItem(scrollViewFlickable.contentItem,
                                              0, 0, newKeyboardRow.item.width,
                                              newKeyboardRow.item.height)
    Util.ensureScrollViewVertVisible(scrollView, scrollView.ScrollBar.vertical,
                                     bound.y, bound.height)
  }

  Keys.onReleased: {
    if(inKeyPress && inKeyPress === event.key) {
      event.accepted = true
      inKeyPress = null
      // Activate the current cell
      var keyboardIdx = findAccKeyboardIndex(accTable)
      var currentRow = accTable[keyboardIdx]
      if(currentRow && currentRow.item) {
        currentRow.item.keyboardSelect(keyboardColumn)
      }
    }
  }

  onActiveFocusChanged: {
    if(!activeFocus)
      inKeyPress = null
  }

  OutlineFocusCue {
    id: focusCue
    anchors.fill: parent
    control: splitTunnelSettings
    // Fade - the rows/cells also show a focus cue
    opacity: 0.6
  }

  NativeAcc.Table.name: label

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

  NativeAcc.Table.columns: [
    { property: "app", column: appColumn },
    { property: "path", column: pathColumn },
    { property: "mode", column: modeColumn },
    { property: "remove", column: removeColumn }
  ]

  NativeAcc.Table.rows: {
    var tblRows = []
    var accRow, rowId
    for(var i=0; i<accTable.length; ++i) {
      accRow = accTable[i]
      rowId = accRow.type + '/' + accRow.value
      tblRows.push({id: rowId,
                    row: accRow.item.accRow,
                    app: accRow.item.accAppCell,
                    value: accRow.item.accPathCell,
                    mode: accRow.item.accModeCell,
                    remove: accRow.item.accRemoveCell})
    }

    return tblRows
  }

  NativeAcc.Table.navigateRow: findAccKeyboardIndex(accTable)
  NativeAcc.Table.navigateCol: {
    var keyboardIdx = findAccKeyboardIndex(accTable)
    var keyboardRow = accTable[keyboardIdx] // undefined if idx=-1
    if(!keyboardRow)
      return -1
    // Map keyboard columns to screen reader columns
    var effectiveKeyCol = keyboardRow.item.effectiveColumnFor(keyboardColumn)
    switch(effectiveKeyCol) {
      case 0: // SplitTunnelAppRow.keyColumns.app
        return 0
      case 1: // SplitTunnelAppRow.keyColumns.mode
      case 2: // SplitTunnelAppRow.keyColumns.remove
        return effectiveKeyCol + 1 // Keyboard model skips 'path' acc column
    }
    return -1
  }
}
