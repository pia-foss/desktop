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
import "qrc:/javascript/keyutil.js" as KeyUtil
import "../../daemon"
import "../../theme"
import "../../common"
import "../../common/regions"
import "../../core"
import "../../vpnconnection"
import PIA.NativeAcc 1.0 as NativeAcc

FocusScope {
  readonly property int pageHeight: 710
  readonly property int maxPageHeight: pageHeight
  //: Screen reader description of the "Back" button in the header when the
  //: user is on the Region page.  This is a slightly longer description of the
  //: button's action, which in this case returns to the Connect page.
  readonly property string backButtonDescription: uiTr("Back to Connect page")
  readonly property var backButtonFunction: backButton

  RegionList {
    id: regionList
    anchors.fill: parent
    anchors.bottomMargin: Theme.dashboard.windowRadius
    //: Screen reader annotation for the "Name / Latency" heading above the
    //: region list, which sorts by either name or latency.  The screen
    //: reader will indicate that this is a group of controls.
    sortGroupName: uiTr("Region list sort")

    //: Screen reader annotation for the region list on the regions page, where
    //: users can choose a region and mark regions as favorites.  (Also used to
    //: describe the scroll bar for the region list.)
    regionListLabel: uiTranslate("RegionListView", "Region list")

    serviceLocations: Daemon.state.vpnLocations
    portForwardEnabled: Daemon.settings.portForward && (Daemon.settings.method === "openvpn" || Daemon.settings.infrastructure !== "current")
    canFavorite: true
    collapsedCountriesSettingName: "vpnCollapsedCountries"
    onRegionSelected: {
      // Choose this location, and reconnect if we were connected to a
      // different one
      VpnConnection.selectLocation(locationId)
      pageManager.setPage('connect')
    }
  }

  function beforeExit() {
  }
  function backButton() {
    pageManager.setPage('connect')
  }
  function onEnter() {
    headerBar.logoCentered = true
    headerBar.needsBottomLine = true

    regionList.clearSearch()
  }
}
