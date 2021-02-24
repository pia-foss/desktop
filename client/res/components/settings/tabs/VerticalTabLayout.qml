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

import QtQuick 2.11
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick.Window 2.10
import "../pages"
import "../../theme"
import "../../common"
import "../../core"
import "qrc:/javascript/keyutil.js" as KeyUtil
import PIA.NativeAcc 1.0 as NativeAcc
import PIA.NativeHelpers 1.0

FocusScope {
  id: tabLayout

  property var pages
  // Function used to translate page titles (called with page's ID)
  property var pageTitleFunc
  // Function used to translate page headings (called with page's ID)
  property var pageHeadingFunc
  anchors.fill: parent
  implicitWidth: Theme.settings.vbarWidth + Theme.settings.vbarContentLeftMargin + Theme.settings.contentWidth + Theme.settings.vbarContentRightMargin
  implicitHeight: Theme.settings.headingHeight + Theme.settings.vbarContentTopMargin + Theme.settings.contentHeight + Theme.settings.vbarContentBottomMargin

  function selectPage(index) {
    listView.currentIndex = index
  }

  TabLayoutCommon {
    id: common
  }

  Rectangle {
    x: 0; y: 0; width: Theme.settings.vbarWidth; height: parent.height
    color: Theme.settings.vbarBackgroundColor
  }

  StaticImage {
    source: Theme.settings.vbarHeaderImage
    label: NativeHelpers.productName
    x: (Theme.settings.vbarWidth - width) / 2
    y: 40
    height: 24
    width: 160
  }

  // Highlight indicator
  Rectangle {
    x: 0
    y: {
      var currentItem = pagesRepeater.itemAt(listView.currentIndex)
      return listView.y + (currentItem ? currentItem.y : 0)
    }
    width: Theme.settings.vbarWidth
    height: Theme.settings.vbarItemHeight
    color: Theme.settings.vbarActiveBackgroundColor
    Rectangle {
      width: 3
      height: parent.height
      color: Theme.settings.vbarHiglightBarColor
    }

    Behavior on y {
      SmoothedAnimation {
        duration: 200
      }
      enabled: tabLayout.Window.window.visible
    }
  }

  Column {
    id: listView
    x: 0
    y: 100
    width: Theme.settings.vbarWidth
    height: pages.length * Theme.settings.vbarItemHeight

    NativeAcc.TabList.name: common.settingsTabsName

    activeFocusOnTab: true

    property int currentIndex: 0

    Repeater {
      id: pagesRepeater
      model: pages

      Item {
        width: listView.width
        height: Theme.settings.vbarItemHeight

        NativeAcc.Tab.name: tabText.text
        NativeAcc.Tab.checked: tabIcon.active
        NativeAcc.Tab.onActivated: mouseClick()

        function mouseClick() {
          listView.forceActiveFocus(Qt.MouseFocusReason)
          listView.currentIndex = index
        }

        TabIcon {
          id: tabIcon
          x: 20
          anchors.verticalCenter: parent.verticalCenter
          active: listView.currentIndex == index
        }
        Text {
          id: tabText
          color: Theme.settings.vbarTextColor
          anchors.verticalCenter: parent.verticalCenter
          text: pageTitleFunc(modelData.name)
          x: 58
          font.pixelSize: Theme.settings.vbarTextPx
        }
        MouseArea {
          anchors.fill: parent
          onClicked: mouseClick()
        }
      }
    }

    Keys.onPressed: {
      var pagesTranslated = tabLayout.pages.map(function(page) {
        return pageTitleFunc(page.name)
      })
      var nextIndex = KeyUtil.handleVertKeyEvent(event, pagesTranslated,
                                                 undefined,
                                                 listView.currentIndex)
      if(nextIndex !== -1) {
        listView.currentIndex = nextIndex
        focusCue.reveal()
      }
    }
  }

  Rectangle {
    anchors.fill: parent
    anchors.leftMargin: Theme.settings.vbarWidth
    color: Theme.settings.backgroundColor

    StaticText {
      x: Theme.settings.vbarContentLeftMargin
      y: Theme.settings.headingHeight - 40
      color: Theme.settings.headingTextColor
      font.weight: Font.Light
      font.pixelSize: Theme.settings.headingTextPx
      text: pageHeadingFunc(pages[listView.currentIndex].name)
    }
    Rectangle {
      x: Theme.settings.vbarContentLeftMargin
      y: Theme.settings.headingHeight - 1
      width: parent.width - Theme.settings.vbarContentLeftMargin - Theme.settings.vbarContentRightMargin
      height: 1
      color: Theme.settings.headingLineColor
    }

    // As in the horizontal tabs...we need a focus scope around the StackLayout
    FocusScope {
      anchors.fill: parent
      StackLayout {
        id: stack
        anchors.fill: parent
        anchors.topMargin: Theme.settings.vbarContentTopMargin + Theme.settings.headingHeight
        anchors.leftMargin: Theme.settings.vbarContentLeftMargin
        anchors.rightMargin: Theme.settings.vbarContentRightMargin
        anchors.bottomMargin: Theme.settings.vbarContentBottomMargin
        currentIndex: listView.currentIndex
        Repeater {
          id: pageContentRepeater
          model: pages
          delegate: Component {
            Loader {
              source: "../pages/" + modelData.component
              width: parent.width
              height: parent.height
            }
          }
          Layout.fillWidth: true
          Layout.fillHeight: true
        }
        // When the current index changes, focus the new page to ensure that the
        // old page loses focus
        onCurrentIndexChanged: pageContentRepeater.itemAt(currentIndex).focus = true
      }
    }
  }

  OutlineFocusCue {
    id: focusCue
    anchors.fill: listView
    // The left edge shows up inside the window
    anchors.leftMargin: borderSize
    control: listView
  }

  // Handle Ctrl+[Shift]+Tab anywhere in the tabs or the page content
  Keys.onPressed: {
    var nextIndex = KeyUtil.handleSettingsTabKeyEvent(event, listView.currentIndex,
                                                      pagesRepeater.count)
    if(nextIndex >= 0) {
      listView.currentIndex = nextIndex
      // If this _was_ from the list view, show its focus; this has no effect if
      // it isn't focused.
      focusCue.reveal()
    }
  }
}
