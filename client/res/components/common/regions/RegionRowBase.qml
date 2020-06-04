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
import "../../daemon"
import "../../theme"
import "../../common"
import "../../core"
import "../../settings/stores"
import PIA.NativeAcc 1.0 as NativeAcc

Item {
  id: regionRowBase

  // Label displayed on the region
  property string label
  // Label text size
  property int labelTextPx
  // Whether to show the latency cell.  (Country groups don't have latencies.)
  property bool showLatency
  // Latency, displayed if >= 0 ms and 'showLatency' is enabled
  property int latency
  // Whether this region is selected
  property bool selected
  // Non-hover background color
  property color backgroundColor
  // Hover background color
  property color hoverBackgroundColor
  // Separator color
  property color separatorColor
  // Whether this region can become a favorite region (controls whether the
  // heart icon is displayed)
  property bool canFavorite
  // The location ID used for the favorite setting
  property string favoriteRegionId
  // Whether to show indicators for regions that don't support port forwarding
  property bool lacksPortForwarding
  // Tip shown for the port forwarding warning (depends on whether this is a
  // single region or a group).
  property string pfWarningTipText
  // Whether to show the "geo" location indicator
  property bool geoLocation

  // When this row is highlighted with the keyboard, highlightColumn is set to
  // the column that is highlighted (an integer in range
  // [0, keyboardColumnCount-1].  Otherwise, -1 indicates that the row is not
  // highlighted.
  //
  // This just shows highlight cues.  If the user presses Space/Enter,
  // keyboardSelect() actually selects a column.
  property int highlightColumn
  // Whether the row is expanded.  (Only on RegionRowBase because it's expressed
  // in the row's screen reader annotation.)
  property bool expanded
  // The row's outline level (for the screen reader annotation)
  property int outlineLevel

  // Symbolic constants for the "keyboard nav columns" in the regions list.  The
  // count of columns is specified in RegionListView.keyboardColumnCount.
  // This only includes "interactive" columns, "static" columns are only
  // represented in screen reader annotations (keyboard nav doesn't need to
  // reach them).
  readonly property var keyColumns: ({
    region: 0,
    favorite: 1
  })

  // Children of the region row go into the opacity wrapper, so they're dimmed
  // if the region is dimmed
  default property alias contents: opacityWrapper.data

  // The regions list should take focus due to a mouse event on a cell
  // Emitted for any click on the row (region or favorite parts), includes the
  // index of the column that was selected
  signal focusCell(int keyColumn)
  // The row was clicked
  signal clicked()

  // Get the effective column for a specified highlight or selection column.
  // (If a row doesn't have a favorite icon, highlighting/selecting the favorite
  // column applies to the region column instead.)
  function effectiveColumnFor(column) {
    if(column < 0)
      return -1
    if(column === keyColumns.favorite && canFavorite)
      return keyColumns.favorite
    return keyColumns.region
  }

  // Select the row with the keyboard.  'column' is the column that is selected.
  function keyboardSelect(column) {
    switch(effectiveColumnFor(column)) {
      case keyColumns.region:
        clicked()
        break
      case keyColumns.favorite:
        toggleFavorite()
        break
    }
  }

  // Screen reader row annotation
  // RegionAuto has a corresponding property.
  readonly property NativeAcc.TableRow accRow: NativeAcc.TableRow {
    name: regionRowBase.label
    item: regionRowBase
    selected: regionRowBase.selected
    outlineExpanded: regionRowBase.expanded
    outlineLevel: regionRowBase.outlineLevel
  }
  // Screen reader table cell annotations.  RegionAuto has properties
  // corresponding to these.
  // Some of these are null if the row doesn't have that cell.  (As an
  // implementation detail, these cells are defined in an 'impl' property, and
  // the "public" property either provides that value or is null.)
  readonly property NativeAcc.TableCellButton accRegionCell: NativeAcc.TableCellButton {
    // The port forward warning is added into the region cell when visible
    // instead of giving it its own column.
    // - The PF elements aren't vertically aligned, they wouldn't make a
    //   well-defined column.
    // - The PF elements are only shown when PF is not supported, the empty
    //   cells would have to have text that describes them, which is awkward.
    // - The PF warning really isn't very important information, it's not worth
    //   changing the navigational structure of the table to indicate, which
    //   would be pretty disruptive.
    name: {
      if(geoTip.visible && pfWarningTip.visible) {
        //: Screen reader annotation used for a region that is a geo-only region
        //: and does not support port forwarding.  Corresponds to the two badges
        //: shown in the regions list.  %1 is a translated region name.  The
        //: region name should come first so the screen reader reads it first.
        return uiTr("%1, geo-located region, does not support port forwarding").arg(regionRowBase.label)
      }
      else if(geoTip.visible) {
        //: Screen reader annotation used for a region that is a geo-only
        //: region.  Corresponds to the globe badge shown in the regions list.
        //: %1 is a translated region name.  The region name should come first
        // so the screen reader reads it first.
        return uiTr("%1, geo-located region").arg(regionRowBase.label)
      }
      else if(pfWarningTip.visible) {
        //: Screen reader annotation used for a region that does not support
        //: port forwarding when the feature is enabled.  Corresponds to the
        //: "slashed-arrow" indicator and "Port forwarding is not supported by
        //: this region/country." tips.  %1 is a translated region name.  The
        //: region name should come first so the screen reader reads it first.
        return uiTr("%1, does not support port forwarding").arg(regionRowBase.label)
      }
      return regionRowBase.label
    }
    item: regionCellBound
    onActivated: regionRowBase.clicked()
  }
  readonly property NativeAcc.TableCellText _accLatencyCellImpl: NativeAcc.TableCellText {
    name: latencyText.text
    item: latencyText
  }
  // The latency cell is provided if the region is supposed to show latency,
  // even if its latency isn't known yet.  The cell value is blank if the
  // latency isn't known yet.
  readonly property NativeAcc.TableCellText accLatencyCell: regionRowBase.showLatency ? _accLatencyCellImpl : null
  readonly property NativeAcc.TableCellCheckButton _accFavoriteCellImpl: NativeAcc.TableCellCheckButton {
    //: Screen reader annotation for the "favorite" button (heart icon) next to
    //: regions in the regions list.  (The screen reader will indicate whether
    //: the button is "on" or "off".)
    name: uiTr("Favorite region")
    item: favoriteContainer
    checked: favoriteSetting.currentValue
    onActivated: {console.info('favorite toggle'); regionRowBase.toggleFavorite()}
  }
  readonly property NativeAcc.TableCellCheckButton accFavoriteCell: regionRowBase.canFavorite ? _accFavoriteCellImpl : null

  property ClientSetValueSetting favoriteSetting: ClientSetValueSetting {
    name: 'favoriteLocations'
    settingValue: favoriteRegionId
  }

  function toggleFavorite() {
    favoriteSetting.currentValue = !favoriteSetting.currentValue
  }

  // Background (not dimmed if the region dims)
  Rectangle {
    anchors.fill: parent
    color: {
      // The InfoTip propagates press/release events, but not ordinary hover due
      // to limitations in MouseArea.
      if(regionBaseMouseArea.containsMouse || pfWarningTip.containsMouse ||
         effectiveColumnFor(highlightColumn) === keyColumns.region) {
        return hoverBackgroundColor
      }
      return backgroundColor
    }
  }

  MouseArea {
    id: regionBaseMouseArea

    anchors.fill: parent
    hoverEnabled: true
    onClicked: {
      focusCell(keyColumns.region)
      regionRowBase.clicked()
    }
  }

  Item {
    id: opacityWrapper
    anchors.fill: parent

    // Dim the row when the port forward notice is shown (the region probably
    // shouldn't be selected, but it still could be)
    opacity: pfWarningTip.visible ? 0.4 : 1.0

    // Label text
    Text {
      id: labelText

      font.pixelSize: labelTextPx
      color: selected ? Theme.regions.itemSelectedTextColor : Theme.regions.itemTextColor
      text: label
      anchors.verticalCenter: parent.verticalCenter
      anchors.left: parent.left
      anchors.leftMargin: 56
      anchors.right: badges.left
      elide: Text.ElideRight
    }

    Item {
      id: badges
      // Follow the label text, unless it's too long to fit everything before
      // the latency text, in which case align to the latency text (which causes
      // the label to truncate).
      x: Math.min(latencyText.x - width, labelText.x + labelText.implicitWidth)
      anchors.verticalCenter: latencyText.verticalCenter

      readonly property real horzMargins: 6

      width: {
        var w = 0
        if(pfWarningTip.show)
          w += pfWarningTip.implicitWidth + horzMargins
        if(geoTip.show)
          w += geoTip.implicitWidth + horzMargins
        // If anything was visible, add another margin for the right side
        if(w > 0)
          w += horzMargins
        return w
      }
      height: Math.max(geoTip.implicitHeight, pfWarningTip.implicitHeight)
      visible: width > 0

      GeoTip {
        id: geoTip
        anchors.verticalCenter: parent.verticalCenter
        x: parent.horzMargins
        readonly property bool show: regionRowBase.geoLocation
        visible: show
        selected: regionRowBase.selected

        // Like the PF warning, don't provide accessibility for tips, include
        // that in the region cell.
        accessible: false
      }

      // Slashed arrow - shown when port forwarding is enabled, but this region
      // doesn't support it
      InfoTip {
        id: pfWarningTip

        anchors.verticalCenter: parent.verticalCenter
        x: geoTip.visible ? (geoTip.x + geoTip.implicitWidth + parent.horzMargins) : parent.horzMargins
        icon: regionRowBase.selected ? Theme.dashboard.ipPortForwardSlashSelectedImage : Theme.dashboard.ipPortForwardSlashImage
        // 'badges.width' can't depend on pfWarningTip.visible, because reading
        // pfWarningTip.visible doesn't actually return the value assigned to it
        // - it returns the combined result including the parent items'
        // visibilities.  Put the desired 'visible' value in an intermediate
        // property and use that instead.
        readonly property bool show: regionRowBase.lacksPortForwarding
        visible: show

        tipText: pfWarningTipText

        // Don't provide accessibility for these tips, there are way too many of
        // them and they don't have much useful information.
        // Tab focus is just ignored, they don't show enough additional info to
        // be worth the navigation.
        // Screen readers see the warnings as part of the Regions table model.
        accessible: false
      }
    }

    // This invisible Item just defines the bounds for the "region" cell used by
    // screen reader annotations.  It includes the region name and the PF
    // warning arrow (if shown).
    Item {
      id: regionCellBound
      anchors.left: labelText.left
      anchors.right: badges.right
      y: Math.min(labelText.y, badges.y)
      height: {
        var bottom = Math.max(labelText.y + labelText.height, badges.y + badges.height)
        return bottom - y
      }
    }

    // Latency text
    Text {
      id: latencyText

      readonly property real highLatencyThreshold: 150

      font.pixelSize: Theme.regions.latencyTextPx
      color: latency >= highLatencyThreshold ? Theme.regions.itemLatencyHighColor : Theme.regions.itemLatencyLowColor
      anchors.right: favoriteContainer.left
      anchors.rightMargin: 20
      anchors.verticalCenter: parent.verticalCenter
      horizontalAlignment: Text.AlignRight
      text: {
        if(regionRowBase.latency >= 0 && regionRowBase.showLatency)
          return uiTr("%1 ms").arg(latency)
        return ""
      }
    }

    // Separator
    Rectangle {
      color: separatorColor
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.bottom: parent.bottom
      height: 1
    }

    Item {
      id: favoriteContainer
      anchors.rightMargin: 20
      anchors.right: parent.right
      anchors.verticalCenter: parent.verticalCenter
      visible: canFavorite
      width: 24
      height: 24

      Image {
        id: favoriteUnselectedHeart
        anchors.centerIn: parent
        width: sourceSize.width / 2
        height: sourceSize.height / 2
        source: favoriteMouseArea.containsMouse ? Theme.regions.favoriteHoverImage : Theme.regions.favoriteUnselectedImage
      }

      // The 'selected' (red) heart is used to indicate a selected region and for
      // 'press' feedback.  When pressed, it's blended into the 'hover' image
      // with an opacity less than 1.  When selected (and not pressed), it
      // overlays that image with opacity 1.0.  When neither selected nor pressed,
      // it's not visible at all.
      Image {
        id: favoriteSelectedHeart
        anchors.centerIn: parent
        width: sourceSize.width / 2
        height: sourceSize.height / 2
        source: Theme.regions.favoriteSelectedImage
        visible: favoriteMouseArea.containsPress || favoriteSetting.currentValue
        opacity: favoriteMouseArea.containsPress ? 0.6 : 1.0
      }

      MouseArea {
        id: favoriteMouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
          focusCell(keyColumns.favorite)
          toggleFavorite()
        }
      }

      // Highlight for the favorite icon
      HighlightCue {
        anchors.fill: parent
        visible: effectiveColumnFor(highlightColumn) === keyColumns.favorite
      }
    }
  }

  // Keyboard highlight cue for the region itself
  HighlightCue {
    anchors.fill: parent
    visible: effectiveColumnFor(highlightColumn) === keyColumns.region
    inside: true
  }
}

// Group:
// - expander
// - flag
