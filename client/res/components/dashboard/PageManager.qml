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
import "../../javascript/app.js" as App
import "./login"
import "./connect"
import "./region"
import "../daemon"
import "../core"

Item {
  id: manager

  // Current layout height for the pages.  Used by ConnectPage.
  property int layoutHeight
  // PageManager determines the overall height of the dashboard based on
  // the height of the active page.  Each page provides a 'pageHeight' property
  // that indicates the height that it currently wants.  (Some pages
  // always return a constant value, but others update it on the fly.)
  property int pageHeight: getCurrentPage().pageHeight
  // This is the likely maximum possible height of any page.  (Note the
  // absolute paranoid maximum; see ConnectPage.)  DashboardWindowPlacement
  // uses this to choose where to place the dashboard, so it will still fit on
  // the screen even if it grows to this height after it's shown.  It's OK if
  // the dashboard grows past this height in some circumstances; the dashboard
  // will scroll if needed.
  //
  // The likely max. height is just determined by the ConnectPage.  Although
  // the other pages in principle could contribute to this, they are unloaded
  // when not needed, and in practice the connect page always determines the
  // max. height.
  property int maxPageHeight: connectPage.maxPageHeight
  readonly property string backButtonDescription: getCurrentPage().backButtonDescription || ""
  readonly property var backButtonFunction: getCurrentPage().backButtonFunction

  // Provide ConnectPage.clipRightExtend when it's the active page
  readonly property real clipRightExtend: getCurrentPage().clipRightExtend || 0

  readonly property int offScreenOffset: parent.width + 10

  readonly property var pageIndices: ({
    login: 0,
    connect: 1,
    region: 2
  })

  property int currentPageIdx: pageIndices.login

  function pageXOffset(pageIndex) {
    // Page is to the left of the current page
    if(pageIndex < currentPageIdx)
      return -offScreenOffset
    // Page is to the right of the current page
    if(pageIndex > currentPageIdx)
      return offScreenOffset
    // Page is the current page
    return 0
  }

  function setPage(pageIndex) {
    var currentPage = getCurrentPage()
    if(currentPage.beforeExit) {
      currentPage.beforeExit()
    }

    console.info("Change page from " + currentPageIdx + " to " + pageIndex)
    currentPageIdx = pageIndex
    // Focus the new page - although the old page will be hidden, apparently Qt
    // would still allow it to have focus.
    // Pages are FocusScopes, so this preserves the specific focused control
    // within each page, etc.
    var nextPage = getCurrentPage()
    nextPage.focus = true
    if(nextPage.onEnter) {
      nextPage.onEnter()
    }
  }

  // Get the current page - used to hook up some properties that always refer
  // to the "current page" (back button, page sizing, etc.).
  //
  // Since some pages are only loaded when active, we can't provide a general
  // accessor to get the page for any index.  We can always get the current
  // page, because dynamic pages are always loaded when they're current.
  function getCurrentPage() {
    switch(currentPageIdx) {
      case pageIndices.login:
        return loginPageLoader.item || connectPage
      case pageIndices.connect:
        return connectPage
      case pageIndices.region:
        return regionPageLoader.item || connectPage
    }
  }

  // A page is visible when it's within one page-width of the viewable area.
  // This is used to actually hide the pages in the QML heirarchy when they're
  // invisible, which is important for keyboard navigation and accessibility.
  function isPageVisible(pageX) {
    return pageX > -offScreenOffset && pageX < offScreenOffset
  }

  // The login page is loaded dynamically, like the region page.  Although this
  // page doesn't consume significant resources like the region page, it's
  // only used when logged out.
  Loader {
    id: loginPageLoader
    width: parent.width
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    
    x: pageXOffset(pageIndices.login)
    Behavior on x {
      SmoothedAnimation {
        duration: 400
      }
      enabled: manager.Window.window && manager.Window.window.visible
    }

    visible: isPageVisible(x)
    active: visible || manager.currentPageIdx === manager.pageIndices.login

    sourceComponent: Component { LoginPage {} }
  }

  // The connect page is always loaded.  The sizing characteristics of the 
  // connect page are always needed in order for the dashboard to position
  // itself (see maxPageHeight above), and the connect page is active most of
  // the time.
  ConnectPage {
    id: connectPage
    width: parent.width
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    layoutHeight: manager.layoutHeight
    
    x: pageXOffset(pageIndices.connect)
    Behavior on x {
      SmoothedAnimation {
        duration: 400
      }
      enabled: manager.Window.window && manager.Window.window.visible
    }

    visible: isPageVisible(x)
  }

  // The region page is loaded only when active.  This page consumes a lot of
  // memory and also can cause a lot of heap thrashing as updates to regions
  // come in from the daemon (which can happen a lot as latencies are updated).
  //
  // This page also isn't active very often, usually it's opened and then
  // quickly used/closed.
  //
  // Loaders always ensure that the loader and its loaded item are the same
  // size (if the loader is positioned, it sizes the item to the loader;
  // otherwise the loader takes the size of the loaded item).  Because of this,
  // we can do all layout with the Loader itself treating it as a "proxy" for
  // for the real region page whether or not it is actually loaded.
  Loader {
    id: regionPageLoader
    width: parent.width
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    
    x: pageXOffset(pageIndices.region)
    Behavior on x {
      SmoothedAnimation {
        duration: 400
      }
      enabled: manager.Window.window && manager.Window.window.visible
    }

    visible: isPageVisible(x)
    active: visible || manager.currentPageIdx === manager.pageIndices.region

    sourceComponent: Component {
      RegionPage {}
    }
  }

  Component.onCompleted: {
    // If we're already logged in, go to the Connect page instead of the login
    // page
    if (Daemon.account.loggedIn) {
      console.log('login onEnter - logged in')
      // Go directly to this page (don't do the transitions in setPage()) since
      // we haven't done the initial page's onEnter yet
      currentPageIdx = pageIndices.connect
    }

    // Call the initial page's onEnter
    var currentPage = getCurrentPage()
    if(currentPage.onEnter) {
      currentPage.onEnter()
    }
  }
}
