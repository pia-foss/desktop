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
import QtQuick.Layouts 1.12
import "../../../core"
import "../../../common"
import "../../../theme"
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc

TableRowBase {
  id: qualityEventsRow

  // Since this list just displays information, there isn't any reason to allow
  // keyboard nav through all of the "columns" in the screen reader model (which
  // aren't really even true columns).  Just navigate through the "event" and
  // "expand button".
  readonly property var keyColumns: ({
    event: 0,
    expand: 1
  })

  readonly property int keyColumnCount: 2

  implicitHeight: expanded ? details.y + details.height : summary.y + summary.height

  // The event from DaemonData
  property var event
  // Whether this row is expanded
  property bool expanded: false

  // The event type text is used as the row name for keyboard nav
  readonly property string name: {
    switch(event.event_name) {
      case "VPN_CONNECTION_ATTEMPT":
        return uiTr("Connection attempt")
      case "VPN_CONNECTION_ESTABLISHED":
        return uiTr("Connection established")
      case "VPN_CONNECTION_CANCELED":
        return uiTr("Connection canceled")
    }
  }

  signal expand()
  signal collapse()

  // This row has all columns
  effectiveColumnFor: function(column){return column}
  keyboardSelect: function(column) {
    if(column === keyColumns.expand)
      qualityEventsRow.expandClicked()
  }

  accRow: NativeAcc.TableRow {
    name: qualityEventsRow.name
    item: qualityEventsRow
    selected: false
    outlineExpanded: false
    outlineLevel: 0
  }

  readonly property var accEventCell: NativeAcc.TableCellText {
    name: eventText.text
    item: eventText
  }

  readonly property var accTimeCell: NativeAcc.TableCellText {
    name: timestampText.text
    item: timestampText
  }

  readonly property var accExpandCell: NativeAcc.TableCellButton {
    //: Screen reader annotation for the "expand" button shown on a
    //: connection event row that is not expanded
    readonly property string expandText: uiTr("Expand")
    //: Screen reader annotation for the "collapse" button shown on a connection
    //: event row that is expanded
    readonly property string collapseText: uiTr("Collapse")
    name: qualityEventsRow.expanded ? collapseText : expandText
    item: expandButton
    onActivated: qualityEventsRow.expandClicked()
  }

  readonly property var accPlatformCell: NativeAcc.TableCellText {
    name: platformValue.text
    item: platformValue
  }

  readonly property var accPrereleaseCell: NativeAcc.TableCellText {
    name: prereleaseValue.text
    item: prereleaseValue
  }

  readonly property var accProtocolCell: NativeAcc.TableCellText {
    name: protocolValue.text
    item: protocolValue
  }

  readonly property var accSourceCell: NativeAcc.TableCellText {
    name: sourceValue.text
    item: sourceValue
  }

  readonly property var accVersionCell: NativeAcc.TableCellText {
    name: versionValue.text
    item: versionValue
  }

  readonly property var accUserAgentCell: NativeAcc.TableCellText {
    name: userAgentValue.text
    item: userAgentValue
  }

  readonly property var accAggregationIdCell: NativeAcc.TableCellText {
    name: aggregationIdValue.text
    item: aggregationIdValue
  }

  readonly property var accEventIdCell: NativeAcc.TableCellText {
    name: eventIdValue.text
    item: eventIdValue
  }

  readonly property var accProductIdCell: NativeAcc.TableCellText {
    name: productIdValue.text
    item: productIdValue
  }

  function expandClicked() {
    qualityEventsRow.focusCell(keyColumns.expand)
    if(qualityEventsRow.expanded)
      qualityEventsRow.collapse()
    else
      qualityEventsRow.expand()
  }

  // Highlight cue for the entire row, used for the event column
  HighlightCue {
    anchors.fill: parent
    visible: highlightColumn === keyColumns.event
    inside: true
  }

  // Type/time summary - always visible
  Item {
    id: summary
    width: parent.width
    height: 35

    Image {
      id: eventIcon
      anchors.verticalCenter: parent.verticalCenter
      x: 6
      width: 10
      height: 10
      source: {
        switch(event.event_name) {
          case "VPN_CONNECTION_ATTEMPT":
            return Theme.settings.connEventAttemptImage
          case "VPN_CONNECTION_ESTABLISHED":
            return Theme.settings.connEventEstablishedImage
          case "VPN_CONNECTION_CANCELED":
            return Theme.settings.connEventCanceledImage
        }
      }
    }

    Text {
      id: eventText
      anchors.verticalCenter: parent.verticalCenter
      anchors.left: eventIcon.right
      anchors.leftMargin: 10

      text: qualityEventsRow.name

      color: Theme.settings.inputListItemPrimaryTextColor
      font.pixelSize: 12
    }

    Text {
      id: timestampText

      anchors.verticalCenter: parent.verticalCenter
      anchors.right: expandButton.left
      anchors.rightMargin: 10

      // event_time is in seconds, not milliseconds like we normally use
      text: NativeHelpers.renderDateTime(event.event_time * 1000)
      color: Theme.settings.inputListItemSecondaryTextColor
      font.pixelSize: 12
    }

    Image {
      id: expandButton
      height: sourceSize.height/2
      width: sourceSize.width/2
      anchors.verticalCenter: parent.verticalCenter
      anchors.right: parent.right
      anchors.rightMargin: 20
      source: expandMouseArea.containsMouse ?
        Theme.settings.connEventExpandButtonHover :
        Theme.settings.connEventExpandButton
      // When expanded, flip the image so the triangle points up
      transform: [
        Scale {
          origin.y: expandButton.height/2
          yScale: qualityEventsRow.expanded ? -1 : 1
        }
      ]
    }

    MouseArea {
      id: expandMouseArea
      anchors.fill: expandButton
      hoverEnabled: true
      cursorShape: Qt.PointingHandCursor
      onClicked: qualityEventsRow.expandClicked()
    }

    // Highlight cue for the expand cell
    HighlightCue {
      anchors.fill: expandButton
      visible: highlightColumn === keyColumns.expand
    }
  }

  // Details (visible when expanded)
  Item {
    id: details
    anchors.top: summary.bottom
    visible: qualityEventsRow.expanded

    readonly property int margin: 8
    // Find the largest label's size and add spacing
    readonly property int labelWidth: {
      let maxLabelWidth = 0
      let labels = [platformLabel, prereleaseLabel, protocolLabel, sourceLabel,
        versionLabel, userAgentLabel, aggregationIdLabel, eventIdLabel,
        productIdLabel]
      for(let i=0; i<labels.length; ++i) {
        if(labels[i])
          maxLabelWidth = Math.max(maxLabelWidth, labels[i].implicitWidth)
      }
      return maxLabelWidth + 10
    }

    // Can't anchor to non-siblings; this assumes that summary is at 0,0
    x: eventText.x
    width: timestampText.x + timestampText.width - x
    height: longDetailsGrid.y + longDetailsGrid.height + margin

    GridLayout {
      id: leftGrid
      anchors.top: details.top
      anchors.left: details.left
      width: details.width/2 - details.margin/2

      columns: 2

      Text {
        id: platformLabel
        Layout.preferredWidth: details.labelWidth
        text: uiTr("Platform:")
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: platformValue
        Layout.fillWidth: true
        text: event.event_properties.platform
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: prereleaseLabel
        Layout.preferredWidth: details.labelWidth
        text: uiTr("Prerelease:")
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: prereleaseValue
        Layout.fillWidth: true
        text: event.event_properties.prerelease ? uiTr("Yes") : uiTr("No")
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }
    }

    GridLayout {
      id: rightGrid
      anchors.top: details.top
      anchors.right: details.right
      width: details.width/2 - details.margin/2

      columns: 2

      Text {
        id: protocolLabel
        Layout.preferredWidth: details.labelWidth
        text: uiTr("Protocol:")
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: protocolValue
        Layout.fillWidth: true
        text: event.event_properties.vpn_protocol
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: sourceLabel
        Layout.preferredWidth: details.labelWidth
        text: uiTr("Source:")
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: sourceValue
        Layout.fillWidth: true
        text: event.event_properties.connection_source === "Manual" ? uiTr("Manual") : uiTr("Automatic")
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }
    }

    GridLayout {
      id: longDetailsGrid
      y: Math.max(leftGrid.y+leftGrid.height, rightGrid.y+rightGrid.height) + rowSpacing

      anchors.left: details.left
      anchors.right: details.right

      columns: 2

      Text {
        id: versionLabel
        Layout.preferredWidth: details.labelWidth
        text: uiTr("Version:")
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: versionValue
        Layout.fillWidth: true
        text: event.event_properties.version
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: userAgentLabel
        Layout.preferredWidth: details.labelWidth
        text: uiTr("User Agent:")
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: userAgentValue
        Layout.fillWidth: true
        text: event.event_properties.user_agent
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: aggregationIdLabel
        Layout.preferredWidth: details.labelWidth
        text: uiTr("Aggregation ID:")
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: aggregationIdValue
        Layout.fillWidth: true
        text: event.aggregated_id
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: eventIdLabel
        Layout.preferredWidth: details.labelWidth
        text: uiTr("Event ID:")
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: eventIdValue
        Layout.fillWidth: true
        text: event.event_unique_id
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: productIdLabel
        Layout.preferredWidth: details.labelWidth
        // Though called an "event_token" internally, this is just an
        // identifier for the product sending the event.
        text: uiTr("Product ID:")
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }

      Text {
        id: productIdValue
        Layout.fillWidth: true
        text: event.event_token
        color: Theme.settings.inputListItemSecondaryTextColor
        font.pixelSize: 12
      }
    }
  }


  Rectangle {
    anchors.bottom: parent.bottom
    height: 1
    color: Theme.settings.splitTunnelItemSeparatorColor
    opacity: 0.5
    anchors.left: parent.left
    anchors.right: parent.right
  }
}
