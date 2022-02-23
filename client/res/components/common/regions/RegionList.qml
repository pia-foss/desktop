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
import QtQuick.Layouts 1.3
import "../../daemon"
import "../../theme"
import "../../core"
import "../../common"
import "../../vpnconnection"
import "qrc:/javascript/keyutil.js" as KeyUtil
import "qrc:/javascript/util.js" as Util
import PIA.NativeAcc 1.0 as NativeAcc

Item {
  id: regionList
  clip: true

  //: Screen reader annotation for the "Name / Latency" heading above the
  //: region list, which sorts by either name or latency.  The screen
  //: reader will indicate that this is a group of controls.
  property string sortGroupName: uiTranslate("RegionPage", "Region list sort")

  //: Screen reader annotation for the region list on the regions page, where
  //: users can choose a region and mark regions as favorites.  (Also used to
  //: describe the scroll bar for the region list.)
  property string regionListLabel: uiTranslate("RegionListView", "Region list")

  property alias regionFilter: regionListView.regionFilter
  property alias serviceLocations: regionListView.serviceLocations
  property alias portForwardEnabled: regionListView.portForwardEnabled
  property alias canFavorite: regionListView.canFavorite
  property alias collapsedCountriesSettingName: regionListView.collapsedCountriesSettingName
  property alias scrollStateGroupName: regionListView.scrollStateGroupName
  property alias scrollStatePropertyName: regionListView.scrollStatePropertyName
  signal regionSelected(string locationId)

  function clearSearch() {
    regionListView.searchTerm = ""
    regionSearch.text = ""
  }

  // For some reason, the region list in the Shadowsocks modal doesn't show the
  // "Search..." placeholder unless the binding is re-evaluated at some point.
  // This seems like a bug in the edit control, work around it by forcing a
  // re-evaluation with a dummy dependency
  property int dummySearchPlaceholderDep: 0
  function reevalSearchPlaceholder() {
    // Toggle the value and toggle it back, this generates a change in the
    // placeholder text binding
    dummySearchPlaceholderDep = 1
    dummySearchPlaceholderDep = 0
  }

  ColumnLayout {
    anchors.fill: parent
    spacing: 0
    Rectangle {
      Layout.preferredHeight: 32 + 24
      Layout.fillWidth: true
      color: Theme.dashboard.backgroundColor

      ThemedTextField {
        id: regionSearch
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.topMargin: 12
        anchors.bottomMargin: 12

        background: Rectangle {
          radius: 16
          color: Theme.regions.searchBarBackground
          implicitHeight: 32
        }
        palette.text: Theme.regions.searchBarTextColor
        leftPadding: 10
        placeholderText: {
          // Dummy dependency, see reevalSearchPlaceholder().  The resulting
          // text actually has to change when this is fiddled in order to get
          // the text field to reapply it
          if(dummySearchPlaceholderDep)
            return ""
          return uiTranslate("RegionPage", "Search...")
        }
        label: placeholderText
        searchEdit: true
        onTextEdited: {
          regionListView.searchTerm = regionSearch.text;
        }
      }
    }
    Rectangle {
      id: regionColumnDisplay
      Layout.preferredHeight: 32
      Layout.fillWidth: true
      color: Theme.regions.regionListHeaderBackground
      activeFocusOnTab: true

      NativeAcc.Group.name: regionList.sortGroupName

      // The header acts as a horizontal list of 2 items for keyboard
      // navigation.
      readonly property var choices: [
        {
          id: 'name',
          display: uiTranslate("RegionPage", "Name")
        },
        {
          id: 'latency',
          display: uiTranslate("RegionPage", "Latency")
        }
      ]

      Repeater {
        model: regionColumnDisplay.choices
        Text {
          anchors.verticalCenter: parent.verticalCenter

          readonly property bool checked: regionListView.sortKey.currentValue === modelData.id

          color: checked ? Theme.regions.regionListHeaderTextActive : Theme.regions.regionListHeaderTextInactive
          text: modelData.display
          x: {
            // The choices are positioned manually with explicit X positions
            if(modelData.id === 'name')
              return 20

            if(modelData.id === 'latency') {
              // Normally, this is 200, but scoot to the left if necessary to
              // ensure the right margin is no smaller than Name's left
              // margin.  (This affects Polish slightly.)
              var maxX = parent.width - width - 20
              return Math.min(200, maxX)
            }

            console.warn('No positioning coded for heading ' + modelData.id)
          }
          font.pixelSize: Theme.regions.headerTextPx

          NativeAcc.RadioButton.name: text
          NativeAcc.RadioButton.checked: checked
          NativeAcc.RadioButton.onActivated: mouseClicked()

          function mouseClicked() {
            regionColumnDisplay.forceActiveFocus(Qt.MouseFocusReason)
            regionListView.sortKey.currentValue = modelData.id
          }

          MouseArea {
            anchors.fill: parent
            onClicked: mouseClicked()
          }
        }
      }

      OutlineFocusCue {
        id: focusCue
        anchors.fill: parent
        control: regionColumnDisplay
        inside: true
      }

      Keys.onPressed: {
        var currentIndex = choices.findIndex(function(c){return c.id === regionListView.sortKey.currentValue})
        var nextIndex = KeyUtil.handleHorzKeyEvent(event, choices, 'display',
                                                   currentIndex)
        if(nextIndex !== -1) {
          if(nextIndex !== currentIndex)
            regionListView.sortKey.currentValue = choices[nextIndex].id
          focusCue.reveal()
        }
      }
    }

    RegionListView {
      id: regionListView
      Layout.fillHeight: true
      Layout.fillWidth: true
      regionListLabel: regionList.regionListLabel
      onRegionSelected: regionList.regionSelected(locationId)
    }
  }
}
