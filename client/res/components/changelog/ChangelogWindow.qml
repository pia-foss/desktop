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
import QtQuick.Layouts 1.3
import QtQuick.Window 2.3
import "../common"
import "../core"
import "../daemon"
import "../theme"
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/util.js" as Util
import "qrc:/javascript/keyutil.js" as KeyUtil

DecoratedWindow {
  id: changelog

  title: uiTr("Changelog")
  color: Theme.dashboard.backgroundColor

  readonly property real contentMargin: 20
  readonly property real contentWidth: 680
  windowLogicalWidth: contentWidth
  // Just like SettingsWindow, the content of this window is only loaded when
  // needed, so the window size is set when the content becomes active, or when
  // the content's size changes (possible when changing language).  (Unlike
  // SettingsWindow, this window's width is fixed, only the height changes.)
  windowLogicalHeight: 200

  // Default to the actual changelog since we haven't updated What's New in
  // some time.
  property int activePage: 1
  readonly property int pageCount: 2

  function updateWindowSize() {
    if(contentLoader.item)
      changelog.windowLogicalHeight = contentLoader.item.contentHeight
  }

  Loader {
    id: contentLoader
    anchors.fill: parent
    active: changelog.visible || changelog.positioningForShow
    sourceComponent: contentComponent
    onItemChanged: changelog.updateWindowSize()
  }

  // Have to wrap everything in an Item so we can attach a Keys.onPressed
  // handler that covers both the tabs and the content
  Component {
    id: contentComponent
      Item {
      readonly property int contentHeight: newsContent.height + tabBar.height
      onContentHeightChanged: changelog.updateWindowSize()

      Rectangle {
        id: tabBar
        color: Theme.changelog.tabBackgroundColor
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 40

        // Highlight indicator
        Rectangle {
          readonly property var currentItem: {
            // Depend on tabsRepeater's children (both these are needed to ensure
            // the bindings are reevaluated)
            var dummy = tabsRepeater.children
            var dummy2 = tabsRepeater.count
            return tabsRepeater.itemAt(changelog.activePage)
          }
          x: tabsRow.x + (currentItem ? currentItem.x : 0)
          y: tabBar.height - height
          width: currentItem ? currentItem.width : 10
          height: 2
          color: Theme.changelog.tabHighlightColor

          Behavior on x {
            SmoothedAnimation {
              duration: 200
            }
            enabled: changelog.visible
          }
          Behavior on width {
            SmoothedAnimation {
              duration: 200
            }
            enabled: changelog.visible
          }
        }

        Row {
          id: tabsRow
          x: 23
          y: 12
          height: parent.height - y

          NativeAcc.TabList.name: changelog.title

          spacing: 16
          activeFocusOnTab: true

          Repeater {
            id: tabsRepeater
            model: [
              // Don't actually translate the content of the model; this avoids
              // rebuilding the repeater content when the language changes
              {label: QT_TR_NOOP("What's new")},
              {label: QT_TR_NOOP("Changelog")}
            ]

            Item {
              id: tabBound

              width: tabText.width + tabText.x*2
              height: parent.height
              readonly property int tabIndex: index

              NativeAcc.Tab.name: tabText.text
              NativeAcc.Tab.checked: changelog.activePage === tabIndex
              NativeAcc.Tab.onActivated: mouseClick()

              function mouseClick() {
                tabsRow.forceActiveFocus(Qt.MouseFocusReason)
                changelog.activePage = tabIndex
              }

              Text {
                id: tabText
                x: 3 // sets both horizontal margins due to tabBound.width calculation
                y: (tabBound.height - height)/2
                color: Theme.changelog.tabTextColor
                text: uiTr(modelData.label)
                font.pixelSize: 12
              }
              MouseArea {
                anchors.fill: parent
                onClicked: mouseClick()
              }
            }
          }

          Keys.onPressed: {
            var pagesTranslated = tabsRepeater.model.map(function(page) {
              return uiTr(page.label)
            })
            var nextIndex = KeyUtil.handleHorzKeyEvent(event, pagesTranslated,
                                                       undefined,
                                                       changelog.activePage)
            if(nextIndex !== -1) {
              changelog.activePage = nextIndex
              tabsFocusCue.reveal()
            }
          }
        }

        OutlineFocusCue {
          id: tabsFocusCue
          anchors.fill: tabsRow
          control: tabsRow
        }
      }

      // Handle Ctrl+[Shift]+Tab anywhere in the tabs or the page content
      Keys.onPressed: {
        var nextIndex = KeyUtil.handleSettingsTabKeyEvent(event, changelog.activePage,
                                                          changelog.pageCount)
        if(nextIndex >= 0) {
          changelog.activePage = nextIndex
          // If this _was_ from the list view, show its focus; this has no effect if
          // it isn't focused.
          tabsFocusCue.reveal()
        }
      }

      FocusScope {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: tabBar.bottom
        anchors.bottom: parent.bottom

        StackLayout {
          id: pagesStack
          anchors.fill: parent
          currentIndex: changelog.activePage
          clip: true

          WindowScrollView {
            id: newsScrollView
            contentWidth: newsContent.width
            contentHeight: newsContent.height
            label: uiTr("What's new")

            WhatsNewContent {
              id: newsContent
              width: 680
            }
          }
          WindowScrollView {
            id: changelogScrollView
            contentWidth: changelogText.width
            contentHeight: changelogText.height
            label: uiTr("Changelog")

            MarkdownPage {
              id: changelogText
              width: pagesStack.width
              margins: contentMargin
              fontPixelSize: 14
              text: uiBrand(NativeHelpers.readResourceText("qrc:/CHANGELOG.md"))
              color: Theme.dashboard.textColor
              rtlAlignmentMirror: false // Changelog is English-only
            }
          }
        }
      }
    }
  }

  Connections {
    target: ClientNotifications
    function onShowChangelog() {
      open();
    }
  }
}
