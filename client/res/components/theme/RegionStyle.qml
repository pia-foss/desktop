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

import QtQuick 2.0
QtObject {
    readonly property color itemBackgroundColor: Theme.dashboard.backgroundColor
    readonly property color itemHighlightBackgroundColor: Theme.dark ? "#2b2e39" : "#e2e2e3"
    readonly property color itemTextColor: Theme.dark ? "#ffffff" : "#323642"
    readonly property color itemSelectedTextColor: "#4cb649"
    readonly property color itemLatencyLowColor: Theme.dark ? "#037900" : "#4cb649"
    readonly property color itemLatencyHighColor: Theme.dark ? "#8e6706": "#d5a900"
    readonly property color itemSeparatorColor: Theme.dark ? "#22252e" : "#d7d8d9"
    readonly property color subRegionBackground: Theme.dark ? "#2f333f" : "#e9e9e9"
    readonly property color subRegionHighlightBackgroundColor: Theme.dark ? "#2c2e39" : "#e2e2e3"
    readonly property color subRegionSeparatorColor: Theme.dark ? "#22252E" : "#D7D8D9"
    readonly property string regionExpander: Theme.imagePath + "/dashboard/regions/region-expander.png"
    readonly property color regionListHeaderBackground: Theme.dark ?"#22252e" : "#d7d8d9"
    readonly property color regionListHeaderTextInactive: Theme.dark ?"#889099" : "#889099"
    readonly property color regionListHeaderTextActive: Theme.dark ?"#FFFFFF" : "#323642"
    readonly property color searchBarBackground : Theme.dark ? "#22252e" : "#d7d8d9"
    readonly property color searchBarTextColor: Theme.dark ? "#fff" : "#323642"
    readonly property color autoRegionSublabelColor: "#889099"
    readonly property color dipRegionSublabelColor: "#889099"
    readonly property string favoriteUnselectedImage: Theme.imagePath + "/dashboard/regions/heart-unselected.png"
    readonly property string favoriteHoverImage: Theme.imagePath + "/dashboard/regions/heart-hover.png"
    readonly property string favoriteSelectedImage: Theme.imagePathCommon + "/dashboard/regions/heart-selected.png"
    readonly property string offlineRegionImage: Theme.imagePathCommon + "/dashboard/regions/offline-icon.png"

    readonly property int headerTextPx: 12
    readonly property int labelTextPx: 13 // Group or standalone region
    readonly property int sublabelTextPx: 12
    readonly property int latencyTextPx: 12 // Nested region in group
    readonly property int tagTextPx: 9 // Tags, like "Dedicated IP"
}
