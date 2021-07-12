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
import QtQuick.Layouts 1.12
import "../../core"
import "../../theme"
import "../../common"
import "../../daemon"
import PIA.NativeAcc 1.0 as NativeAcc
import PIA.NativeHelpers 1.0
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util

TableBase {
  id: qualityEventsList

  //: Screen reader label for the list of connection events
  label: qualityEvents.length > 0 ? uiTr("Connection Events") : ""

  keyboardRow: ""

  verticalScrollPolicy: ScrollBar.AsNeeded
  contentHeight: qualityEventsLayout.implicitHeight

  keyboardColumnCount: 2  // QualityEventsRow.keyColumnCount

  // Since the keyboard model only exposes "event" and "expose" columns, while
  // the accessibility model has columns for timestamp and all of the details,
  // map accessibility column indices to keyboard column indices
  accColumnForKeyColumn: function(column) {
    switch(column) {
      case 0: // QualityEventsRow.keyColumns.event
        return 0
      case 1: // QualityEventsRow.keyColumns.expand
        return 2
    }
  }

  property string expandedRow: ""

  readonly property var qualityEvents: {
    let events = Daemon.data.qualityEventsSent.concat(Daemon.data.qualityEventsQueued)

    // TableBase really wants to work with items that each have a unique ID,
    // which these items don't have.  (It's easily possible to have more than
    // one identical item if you try to connect more than once with the same
    // settings within the same hour.)
    //
    // A pretty effective simple solution is just to add a sequence counter
    // for TableBase to distinguish events with the same timestamp.  Since
    // this is added from oldest-to-newest, it remains consistent even as new
    // events are added.
    //
    // When old events roll off, it remains consistent as long as all events
    // from a given hour roll off together.  If part of an hour rolls off,
    // the indices for the remaining events in that hour change.  This is
    // pretty unusual though, and the consequences are minor (just might move
    // the cursor to a remaining event instead of the very end, and possibly
    // move the expanded event too).
    let idxInHr = 0
    let lastHr = 0
    for(let i=0; i<events.length; ++i) {
      if(events[i].event_time !== lastHr) {
        idxInHr = 0
        lastHr = events[i].event_time
      }
      else
        ++idxInHr
      events[i].idxInHr = idxInHr
    }

    if(events.length > 0) {
      console.info("Oldest timestamp was: " + NativeHelpers.renderDateTime(events[0].event_time * 1000))
      events.reverse()
      console.info("Newest timestamp is: " + NativeHelpers.renderDateTime(events[0].event_time * 1000))
    }

    return events
  }

  ColumnLayout {
    id: qualityEventsLayout
    spacing: 0
    width: parent.width

    Repeater {
      id: queuedEventsRepeater
      model: qualityEventsList.qualityEvents
      delegate: QualityEventsRow {
        Layout.fillWidth: true
        event: modelData
        expanded: rowId === qualityEventsList.expandedRow
        parentTable: qualityEventsList
        rowId: qualityEventsList.buildRowId("ev",
          "" + modelData.event_time + "-" + modelData.idxInHr)
        onExpand: qualityEventsList.expandedRow = rowId
        onCollapse: qualityEventsList.expandedRow = ""
      }
    }
  }

  property NativeAcc.TableColumn eventColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the "event" column of the Connection Events
    //: table, which displays the event type.
    name: uiTr("Event")
    item: qualityEventsList
  }

  property NativeAcc.TableColumn timeColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the "time" column of the Connection Events
    //: table, which displays the time when the event was generated (both date
    //: and time of day).
    name: uiTr("Time")
    item: qualityEventsList
  }

  property NativeAcc.TableColumn expandColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the "expand" column of the Connection
    //: Events list, which displays the "expand" buttons for each event
    name: uiTr("Expand")
    item: qualityEventsList
  }

  property NativeAcc.TableColumn platformColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the "platform" column of the Connection
    //: Events list, which displays the client platform ("Windows", "macOS", or
    //: "Linux").  The details fields are modeled as columns for screen readers,
    //: although they are not visually organized as a single column.
    name: uiTr("Platform")
    item: qualityEventsList
  }

  property NativeAcc.TableColumn prereleaseColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the "prerelease" column of the Connection
    //: Events list, which indicates whether the client is a prerelease client
    //: or not ("Yes" or "No").
    //: The details fields are modeled as columns for screen readers, although
    //: they are not visually organized as a single column.
    name: uiTr("Prerelease")
    item: qualityEventsList
  }

  property NativeAcc.TableColumn protocolColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the "protocol" column of the Connection
    //: Events list, which indicates the VPN protocol used for that connection
    //: ("OpenVPN" or "WireGuard").
    //: The details fields are modeled as columns for screen readers, although
    //: they are not visually organized as a single column.
    name: uiTr("Protocol")
    item: qualityEventsList
  }

  property NativeAcc.TableColumn sourceColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the "source" column of the Connection
    //: Events list, which indicates whether the connection was started
    //: manually or automatically ("Manual" or "Automatic").
    //: The details fields are modeled as columns for screen readers, although
    //: they are not visually organized as a single column.
    name: uiTr("Source")
    item: qualityEventsList
  }

  property NativeAcc.TableColumn versionColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the "version" column of the Connection
    //: Events list, which displays the client version (such as 2.8.1+06339).
    //: The details fields are modeled as columns for screen readers, although
    //: they are not visually organized as a single column.
    name: uiTr("Version")
    item: qualityEventsList
  }

  property NativeAcc.TableColumn userAgentColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the "user agent" column of the Connection
    //: Events list, which indicates the User Agent string for that client
    //: version (https://en.wikipedia.org/wiki/User_agent - for example,
    //: "PIA/2.8.1+06339 (Linux; x86_64)").
    //: The details fields are modeled as columns for screen readers, although
    //: they are not visually organized as a single column.
    name: uiTr("User Agent")
    item: qualityEventsList
  }

  property NativeAcc.TableColumn aggregationIdColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the "aggregation ID" column of the
    //: Connection Events list, which shows the aggregation ID used for that
    //: event (a UUID).
    //: The details fields are modeled as columns for screen readers, although
    //: they are not visually organized as a single column.
    name: uiTr("Aggregation ID")
    item: qualityEventsList
  }

  property NativeAcc.TableColumn eventIdColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the "event ID ID" column of the
    //: Connection Events list, which shows the event's unique ID (a UUID).
    //: The details fields are modeled as columns for screen readers, although
    //: they are not visually organized as a single column.
    name: uiTr("Event ID")
    item: qualityEventsList
  }

  property NativeAcc.TableColumn productIdColumn: NativeAcc.TableColumn {
    //: Screen reader annotation for the "product ID" column of the Connection
    //: Events list, which indicates the product family that the client belongs
    //: to (a UUID, which represents the product "PIA Desktop production", etc.)
    //: The details fields are modeled as columns for screen readers, although
    //: they are not visually organized as a single column.
    name: uiTr("Product ID")
    item: qualityEventsList
  }

  accColumns: [
    { property: "accEventCell", column: eventColumn },
    { property: "accTimeCell", column: timeColumn },
    { property: "accExpandCell", column: expandColumn },
    { property: "accPlatformCell", column: platformColumn },
    { property: "accPrereleaseCell", column: prereleaseColumn },
    { property: "accProtocolCell", column: protocolColumn },
    { property: "accSourceCell", column: sourceColumn },
    { property: "accVersionCell", column: versionColumn },
    { property: "accUserAgentCell", column: userAgentColumn },
    { property: "accAggregationIdCell", column: aggregationIdColumn },
    { property: "accEventIdCell", column: eventIdColumn },
    { property: "accProductIdCell", column: productIdColumn }
  ]

  accTable: {
    var childrenDep = queuedEventsRepeater.children
    var table = []

    for(var i=0; i<queuedEventsRepeater.count; ++i) {
      var eventItem = queuedEventsRepeater.itemAt(i)
      if(!eventItem)
        continue
      table.push({row: eventItem.rowId, name: eventItem.name,
                  item: eventItem})
    }

    return table
  }
}
