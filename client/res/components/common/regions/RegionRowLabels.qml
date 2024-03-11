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
import QtQuick.Layouts 1.3
import "../../theme"
import "../../common"
import "../../core"
import "../../settings/stores"
import PIA.NativeAcc 1.0 as NativeAcc

Item {
  id: regionRowLabels

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
  // Whether this region can become a favorite region (controls whether the
  // heart icon is displayed)
  property bool canFavorite
  // Whether to show indicators for regions that don't support port forwarding
  property bool lacksPortForwarding
  // Tip shown for the port forwarding warning (depends on whether this is a
  // single region or a group).
  property string pfWarningTipText
  // Whether to show the "geo" location indicator
  property bool geoLocation
  // Whether the region is unavailable (offline)
  property bool offline
  // Whether this region is currently a favorite
  property bool isFavorite
  // Whether to show the keyboard focus highlight for the favorite cell
  property bool highlightFavorite

  // Tells the parent to show the region row highlight when an InfoTip in the
  // row is pointed - MouseArea doesn't propagate hover events.
  readonly property bool showRowHighlight: pfWarningTip.containsMouse || geoTip.containsMouse

  // Although the region's MouseArea is actually part of the background, the
  // accessibility cell is defined by RegionRowLabels
  signal regionClicked()
  signal favoriteClicked()

  default property alias contents: opacityWrapper.data

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
        return uiTranslate("RegionRowBase", "%1, geo-located region, does not support port forwarding").arg(regionRowLabels.label)
      }
      else if(geoTip.visible) {
        //: Screen reader annotation used for a region that is a geo-only
        //: region.  Corresponds to the globe badge shown in the regions list.
        //: %1 is a translated region name.  The region name should come first
        // so the screen reader reads it first.
        return uiTranslate("RegionRowBase", "%1, geo-located region").arg(regionRowLabels.label)
      }
      else if(pfWarningTip.visible) {
        //: Screen reader annotation used for a region that does not support
        //: port forwarding when the feature is enabled.  Corresponds to the
        //: "slashed-arrow" indicator and "Port forwarding is not supported by
        //: this region/country." tips.  %1 is a translated region name.  The
        //: region name should come first so the screen reader reads it first.
        return uiTranslate("RegionRowBase", "%1, does not support port forwarding").arg(regionRowLabels.label)
      }
      return regionRowLabels.label
    }
    item: regionCellBound
    onActivated: regionRowLabels.regionClicked()
  }
  readonly property NativeAcc.TableCellText _accLatencyCellImpl: NativeAcc.TableCellText {
    name: latencyText.text
    item: latencyText
  }
  // When the region is offline, the latency cell is replaced with an "offline"
  // graphic.
  readonly property NativeAcc.TableCellText _accOfflineCellImpl: NativeAcc.TableCellText {
    //: Screen reader annotation for the "offline" image displayed when a region
    //: is temporarily unavailable
    name: uiTr("offline")
    item: offlineRegionIcon
  }
  // The latency cell is provided if the region is supposed to show latency,
  // even if its latency isn't known yet.  The cell value is blank if the
  // latency isn't known yet.
  readonly property NativeAcc.TableCellText accLatencyCell: {
    if(offlineRegionIcon.visible)
      return _accOfflineCellImpl
    if(regionRowLabels.showLatency)
      return _accLatencyCellImpl
    return null
  }
  readonly property NativeAcc.TableCellCheckButton _accFavoriteCellImpl: NativeAcc.TableCellCheckButton {
    //: Screen reader annotation for the "favorite" button (heart icon) next to
    //: regions in the regions list.  (The screen reader will indicate whether
    //: the button is "on" or "off".)
    name: uiTranslate("RegionRowBase", "Favorite region")
    item: favoriteContainer
    checked: regionRowLabels.isFavorite
    onActivated: regionRowLabels.favoriteClicked()
  }
  readonly property NativeAcc.TableCellCheckButton accFavoriteCell: regionRowLabels.canFavorite ? _accFavoriteCellImpl : null

  Item {
    id: opacityWrapper
    anchors.fill: parent

    // Dim the row when the region is offline or the port forward notice is
    // shown (the region probably shouldn't be selected, but it still could be)
    opacity: (regionRowLabels.offline || pfWarningTip.visible) ? 0.4 : 1.0

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
        readonly property bool show: regionRowLabels.geoLocation
        visible: show
        selected: regionRowLabels.selected

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
        icon: regionRowLabels.selected ? Theme.dashboard.ipPortForwardSlashSelectedImage : Theme.dashboard.ipPortForwardSlashImage
        // 'badges.width' can't depend on pfWarningTip.visible, because reading
        // pfWarningTip.visible doesn't actually return the value assigned to it
        // - it returns the combined result including the parent items'
        // visibilities.  Put the desired 'visible' value in an intermediate
        // property and use that instead.
        readonly property bool show: regionRowLabels.lacksPortForwarding
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

    Image {
      id: offlineRegionIcon
      anchors.right: favoriteContainer.left
      anchors.rightMargin: 20
      anchors.verticalCenter: parent.verticalCenter

      width: sourceSize.width / 4
      height: sourceSize.height / 4
      source:  Theme.regions.offlineRegionImage
      visible: offline
    }

    // Latency text
    Text {
      id: latencyText

      readonly property real highLatencyThreshold: 150

      font.pixelSize: Theme.regions.latencyTextPx
      color: latency >= highLatencyThreshold ? Theme.regions.itemLatencyHighColor : Theme.regions.itemLatencyLowColor
      anchors.right: favoriteContainer.left
      anchors.rightMargin: 5
      anchors.verticalCenter: parent.verticalCenter
      horizontalAlignment: Text.AlignRight
      text: {
        if(regionRowLabels.latency >= 0 && regionRowLabels.showLatency)
          return uiTranslate("RegionRowBase", "%1 ms").arg(latency)
        return ""
      }
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
        visible: favoriteMouseArea.containsPress || regionRowLabels.isFavorite
        opacity: favoriteMouseArea.containsPress ? 0.6 : 1.0
      }

      MouseArea {
        id: favoriteMouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: regionRowLabels.favoriteClicked()
      }

      // Highlight for the favorite icon
      HighlightCue {
        anchors.fill: parent
        visible: regionRowLabels.highlightFavorite
      }
    }
  }
}
