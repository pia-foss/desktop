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
import "../../client"
import "qrc:/javascript/keyutil.js" as KeyUtil
import PIA.NativeAcc 1.0 as NativeAcc

FocusScope {
  id: tabLayout

  property var pages
  // Function used to translate page titles (called with page's ID)
  property var pageTitleFunc
  anchors.fill: parent

  // Some translations are so long that we just can't fit them in the normal
  // width, but enlarging the width in general leaves an excess of empty space.
  // Enlarge the width for very long languages to accommodate the tab bar.
  // These languages typically have long translations for the content too, so
  // the extra width is beneficial there also.
  property int minimumWidth: Theme.settings.hbarContentLeftMargin + Theme.settings.contentWidth + Theme.settings.hbarContentRightMargin
  property int minimumTabEndMargins: 10
  // Widen the window whenever the tabs would exceed the normal width.  Note
  // that the way the tab list accommodates very long text does not actually
  // include that text's bounds in the list's bound (it just adjusts the item
  // spacing), but fortunately the first and last tabs are reasonably short in
  // all languages.
  implicitWidth: Math.max(minimumWidth, listView.width + 2*minimumTabEndMargins)
  implicitHeight: Theme.settings.hbarHeight + Theme.settings.hbarContentTopMargin + stack.pageImplicitHeight + Theme.settings.hbarContentBottomMargin

  TabLayoutCommon {
    id: common
  }

  Rectangle {
    x: 0; y: 0; width: parent.width; height: Theme.settings.hbarHeight
    color: Theme.settings.hbarBackgroundColor
  }

  // Highlight indicator
  Rectangle {
    x: {
      // These dummy dependencies are needed to recalculate x when
      // pagesRepeater.itemAt() changes while the tabs are loaded
      var tabsDep = pagesRepeater.children
      var tabsDep2 = pagesRepeater.count
      var currentItem = pagesRepeater.itemAt(Client.uiState.settings.currentPage)
      return listView.x + (currentItem ? currentItem.x : 0)
    }
    y: listView.height - height
    width: Theme.settings.hbarItemWidth
    height: 3
    color: Theme.settings.hbarHiglightBarColor

    Behavior on x {
      SmoothedAnimation {
        duration: 200
      }
      enabled: tabLayout.Window.window.visible
    }
  }

  Row {
    id: listView
    anchors.horizontalCenter: parent.horizontalCenter
    y: 0
    height: Theme.settings.hbarHeight - 1

    NativeAcc.TabList.name: common.settingsTabsName

    spacing: {
      // Add enough spacing such that there is always
      // Theme.settings.hbarItemSpacing between the items' text.
      //
      // Don't just size based on the max text width, several languages have one
      // relatively-wide tab name that's next to two short tab names, which is
      // fine.  (Dumb padding based on max text width would be too aggressive
      // and spaces these out too much.)
      //
      // Instead, actually consider the adjacent texts' width and figure out how
      // close they are.
      //
      // By default, start with hbarItemSpacing.  We always use at least this
      // much spacing, even if the texts are small.
      var actualSpacing = Theme.settings.hbarItemSpacing
      for(var i=0; i<pagesRepeater.count-1; ++i) {
        var thisItem = pagesRepeater.itemAt(i)
        var nextItem = pagesRepeater.itemAt(i+1)
        if(!thisItem || !nextItem)
          continue

        // If the center of thisItem is x=0, thisItem's text ends at
        // thisItem.textWidth/2.
        //
        // If there was 0 spacing, nextItem's text would start at
        // Theme.settings.hbarItemWidth - nextItem.textWidth/2.
        //
        // Therefore, the gap at 0 spacing would be:
        var nospaceGap = Theme.settings.hbarItemWidth - nextItem.textWidth/2 - thisItem.textWidth/2
        // A positive nospaceOverlap means the texts do not overlap (it's the
        // gap width), a negative value means they would overlap by that amount.
        //
        // We want the overlap to be -Theme.settings.hbarItemMinLabelGap - that
        // is, at least hbarItemMinLabelGap space between the actual text.
        //
        // The spacing value to get exactly hbarItemMinLabelGap gap is:
        var thisPairSpacing = Theme.settings.hbarItemMinLabelGap - nospaceGap
        // This value is negative if the texts did not overlap - that's fine,
        // we initialized actualSpacing with hbarItemSpacing above, so that's
        // the minimum spacing.
        // The actual spacing will be the max of any pair's computed spacing.
        actualSpacing = Math.max(thisPairSpacing, actualSpacing)
      }
      return actualSpacing
    }
    activeFocusOnTab: true

    readonly property int currentIndex: Client.uiState.settings.currentPage

    Repeater {
      id: pagesRepeater
      model: pages
      Item {
        width: Theme.settings.hbarItemWidth
        height: listView.height
        readonly property int textWidth: labelText.implicitWidth

        NativeAcc.Tab.name: labelText.text
        NativeAcc.Tab.checked: tabIcon.active
        NativeAcc.Tab.onActivated: mouseClick()

        function mouseClick() {
          listView.forceActiveFocus(Qt.MouseFocusReason)
          Client.uiState.settings.currentPage = index
        }

        TabIcon {
          id: tabIcon
          y: 10
          anchors.horizontalCenter: parent.horizontalCenter
          active: listView.currentIndex == index
        }
        Text {
          id: labelText
          color: Theme.settings.hbarTextColor
          anchors.horizontalCenter: parent.horizontalCenter
          text: pageTitleFunc(modelData.name)
          y: 42
          font.pixelSize: Theme.settings.hbarTextPx
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
      var nextIndex = KeyUtil.handleHorzKeyEvent(event, pagesTranslated,
                                                 undefined,
                                                 listView.currentIndex)
      if(nextIndex !== -1) {
        Client.uiState.settings.currentPage = nextIndex
        focusCue.reveal()
      }
    }
  }

  Rectangle {
    x: 0
    y: Theme.settings.hbarHeight - height
    width: parent.width
    height: 1
    color: Theme.settings.hbarBottomBorderColor
  }

  Rectangle {
    anchors.fill: parent
    anchors.topMargin: Theme.settings.hbarHeight
    color: Theme.settings.backgroundColor

    // We really just want the stack layout to be a focus scope, but this isn't
    // exposed as a QML property (even though that's how it's implemented in
    // QQuickItem -_-)
    FocusScope {
      anchors.fill: parent
      StackLayout {
        id: stack
        anchors.fill: parent
        anchors.topMargin: Theme.settings.hbarContentTopMargin
        anchors.leftMargin: Theme.settings.hbarContentLeftMargin
        anchors.rightMargin: Theme.settings.hbarContentRightMargin
        anchors.bottomMargin: Theme.settings.hbarContentBottomMargin
        currentIndex: listView.currentIndex
        readonly property int pageImplicitHeight: {
          var page = pageContentRepeater.itemAt(currentIndex)
          return page ? page.implicitHeight : Theme.settings.contentHeight
        }
        Repeater {
          id: pageContentRepeater
          model: pages
          delegate: Component {
            Loader {
              source: "../pages/" + modelData.component
              width: parent.width
              height: parent.height
              active: visible
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
    // The top edge shows up inside the window
    anchors.topMargin: borderSize
    control: listView
  }

  // Handle Ctrl+[Shift]+Tab anywhere in the tabs or the page content
  Keys.onPressed: {
    var nextIndex = KeyUtil.handleSettingsTabKeyEvent(event, listView.currentIndex,
                                                      pagesRepeater.count)
    if(nextIndex >= 0) {
      Client.uiState.settings.currentPage = nextIndex
      // If this _was_ from the list view, show its focus; this has no effect if
      // it isn't focused.
      focusCue.reveal()
    }
  }
}
