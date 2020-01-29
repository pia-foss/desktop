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
import QtQuick.Window 2.3
import "../../javascript/app.js" as App
import "./login"
import "./connect"
import "./region"
import "../daemon"

Item {
  id: manager

  // Current layout height for the pages.  Used by ConnectPage.
  property int layoutHeight
  // PageManager determines the overall height of the dashboard based on
  // the height of the active page.  Each page provides a 'pageHeight' property
  // that indicates the height that it currently wants.  (Some pages
  // always return a constant value, but others update it on the fly.)
  property int pageHeight: getPage(state).pageHeight
  // This is the maximum possible height of any page.  DashboardWindowPlacement
  // uses this to choose where to place the dashboard, which ensures that it
  // will still fit on the screen even if it grows to this height after it's
  // shown.
  property int maxPageHeight: {
    var max = 0
    max = Math.max(max, loginPage.maxPageHeight)
    max = Math.max(max, connectPage.maxPageHeight)
    max = Math.max(max, regionPage.maxPageHeight)
    return max
  }
  readonly property string backButtonDescription: getPage(state).backButtonDescription || ""
  readonly property var backButtonFunction: getPage(state).backButtonFunction

  // Provide ConnectPage.clipRightExtend when it's the active page
  readonly property real clipRightExtend: getPage(state).clipRightExtend || 0

  readonly property int offScreenOffset: parent.width + 10

  transitions: [
    Transition {
      from: "*"
      to: "*"

      SmoothedAnimation {
        targets: [loginPage, connectPage, regionPage]
        properties: "x"
        duration: 400
      }
      enabled: manager.Window.window.visible
    }
  ]

  state: 'login'

  states: [
    State {
      name: "login"
      PropertyChanges {
        target: loginPage
        x: 0
      }
      PropertyChanges {
        target: connectPage
        x: offScreenOffset
      }
      PropertyChanges {
        target: regionPage
        x: offScreenOffset
      }
    },
    State {
      name: "connect"
      PropertyChanges {
        target: loginPage
        x: -1 * offScreenOffset
      }
      PropertyChanges {
        target: connectPage
        x: 0
      }
      PropertyChanges {
        target: regionPage
        x: offScreenOffset
      }
    },
    State {
      name: "region"
      PropertyChanges {
        target: loginPage
        x: -1 * offScreenOffset
      }
      PropertyChanges {
        target: connectPage
        x: -1 * offScreenOffset
      }
      PropertyChanges {
        target: regionPage
        x: 0
      }
    }
  ]


  signal pageChange()

  function setPage (pageId) {
    var currentPage = getPage(state)
    var nextPage = getPage(pageId)
    if(currentPage.beforeExit) {
      currentPage.beforeExit()
    }

    state = pageId
    // Focus the new page - although the old page will be hidden, apparently Qt
    // would still allow it to have focus.
    // Pages are FocusScopes, so this preserves the specific focused control
    // within each page, etc.
    nextPage.focus = true
    pageChange()
    if(nextPage.onEnter) {
      nextPage.onEnter()
    }
  }

  function getPage(pageId) {
    switch(pageId) {
    case 'login':
      return loginPage;
    case 'connect':
      return connectPage;
    case 'region':
      return regionPage
    }
  }

  // A page is visible when it's within one page-width of the viewable area.
  // This is used to actually hide the pages in the QML heirarchy when they're
  // invisible, which is important for keyboard navigation and accessibility.
  function isPageVisible(pageX) {
    return pageX > -offScreenOffset && pageX < offScreenOffset
  }

  LoginPage {
    id: loginPage
    width: parent.width
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    visible: isPageVisible(x)
  }

  ConnectPage {
    id: connectPage
    width: parent.width
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    layoutHeight: manager.layoutHeight
    visible: isPageVisible(x)
  }

  RegionPage {
    id: regionPage
    width: parent.width
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    visible: isPageVisible(x)
  }

  Timer {
    running: true
    interval: 1000
    onTriggered: {
//      pageManager.state = "connect"
    }
  }

  Component.onCompleted: {
    // If we're already logged in, go to the Connect page instead of the login
    // page
    if (Daemon.account.loggedIn) {
      console.log('login onEnter - logged in')
      // Go directly to this page (don't do the transitions in setPage()) since
      // we haven't done the initial page's onEnter yet
      state = 'connect'
    }

    // Call the initial page's onEnter
    var currentPage = getPage(state)
    if(currentPage.onEnter) {
      currentPage.onEnter()
    }
  }
}
