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
import "../../../javascript/app.js" as App
import "qrc:/javascript/util.js" as Util
import "../../client"
import "../../common"
import "../../daemon"
import "../../helpers"
import "../../theme"
import "../../vpnconnection"
import PIA.FocusCue 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/keyutil.js" as KeyUtil

FocusScope {
  id: cp

  // This is the current layout height for the page.  The only difference
  // between this height and cp.height is that layoutHeight isn't animated.
  // We use this to size the page content, so we avoid showing a transient
  // scroll bar during height animations.
  property int layoutHeight

  // ConnectPage's expanded state determines whether this page is expanded.
  // This is hooked up to the positions of some widgets, and it's hooked up to
  // the state of the ExpandButton, which ensures that it displays the
  // appropriate state.
  // This persists even if we transition to another page - the ConnectPage will
  // be in the same state when we return to it (except for a logout, which
  // collapses the page).
  property bool pageExpanded: false
  // Collapsed - shows connect region, primary modules, and expand button
  readonly property int collapsedHeight: connectRegion.height + moduleSorter.aboveFoldHeight + expandButton.height
  // Expanded - shows primary modules, secondary modules, and expand button
  readonly property int expandedHeight: moduleSorter.height + expandButton.height
  readonly property int pageHeight: pageExpanded ? expandedHeight : collapsedHeight
  // This "maximum" height is actually a "likely maximum" not a "paranoid
  // absolute maximum" - see DashboardPositioner.
  //
  // The likely maximum that the user will actually see is the height of either
  // the collapsed or expanded view.
  //
  // Technically, the true (paranoid) maximum is the height of the collapsed
  // state with all possible modules favorited and all error notifications
  // active.  This is extreme though; it causes the placement to be overly
  // paranoid in most cases.  (In particular, on Windows, with the taskbar at
  // the bottom and the tray icon in a popup, we would often pop to the left
  // side instead of above.  This is surprising, and this is the default
  // placement users on Windows will get.)
  //
  // The side effect of not using the true paranoid maximum is that if the user
  // does favorite most/all modules, the dashboard might start to scroll before
  // it would otherwise have needed to.  That's OK though, because the next time
  // the dashboard is shown, it will show to the side in order to avoid or
  // reduce scrolling.
  readonly property int maxPageHeight: Math.max(collapsedHeight, expandedHeight)

  readonly property alias clipRightExtend: moduleSorter.clipRightExtend

  // The scroll view is wrapped in an Item, because we want the ScrollView's
  // height to always be the layout height (so it doesn't show transient scroll
  // bars during an animation), but we want to clip it to the actual space it
  // fits in.
  Item {
    id: scrollClipper
    anchors.top: parent.top
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: expandButton.top
    // Extend the scroll clipper to the right when a module pops out to drag.
    anchors.rightMargin: -moduleSorter.clipRightExtend
    clip: true

    ThemedScrollView {
      id: scrollWrapper
      ScrollBar.vertical.policy: ScrollBar.AsNeeded
      //: Screen reader annotation for Connect page.  This describes the entire
      //: page that contains the Connect button and tiles.
      label: uiTr("Connect page")
      anchors.top: parent.top
      anchors.left: parent.left
      width: cp.width
      // Size the page content to the layout height, not the page's current
      // size.  The effect is that the content ignores the transient height
      // animations of the dashboard, which avoids showing a transient
      // scrollbar.
      //
      // The layout height includes the expand button (it's part of this page),
      // so subtract that.
      //
      // As a result of this mechanism, the scroll wrapper's height _cannot_
      // influence the pageHeight expressed to the parent layout.  (This would
      // create a binding loop.  Instead, the page height is computed from the
      // content of the page, passed up to the top-level window as pageHeight,
      // then back down as layoutHeight, which might not be the same if
      // pageHeight was too large.)
      height: layoutHeight - expandButton.height

      contentWidth: pageContent.implicitWidth
      contentHeight: pageContent.implicitHeight

      // The Flickable inside this ScrollView is customized to stop at its
      // bounds; the default behavior of overscrolling and springing back is
      // pretty strange on non-touch devices, which are the most common case for
      // this app.
      //
      // (ScrollView always decorates a Flickable, but it will create one for
      // you if you put children in the ScrollView directly.  If you put a
      // Flickable in it instead, it uses that one and does not make one on its
      // own.)
      Flickable {
        id: scrollWrapperFlickable
        boundsBehavior: Flickable.StopAtBounds

        // Tab stop only when scrolling is actually needed - most of the time,
        // it isn't.
        activeFocusOnTab: contentHeight > height

        NumberAnimation {
          id: expandScrollToAnimation
          target: scrollWrapper.ScrollBar.vertical
          property: "position"
          duration: Theme.animation.normalDuration
          easing.type: Easing.InOutQuad
        }

        // This item wraps the content of the page that we want to display.  This is
        // what prevents the user from scrolling down to the secondary modules when
        // collapsed, or to the connect region when expanded.
        Item {
          id: pageContent

          // The slide offset when fully expanded
          readonly property int expandedSlideOffset: -connectRegion.height
          // The slide offset in the current state
          property int slideOffset: pageExpanded ? expandedSlideOffset : 0

          x: 0
          y: slideOffset
          implicitWidth: cp.width
          implicitHeight: pageHeight - expandButton.height
          // We don't animate pageContent.y for all changes, because it can change
          // while expanded if the connect region resizes (due to a notification
          // triggering or ending), and this change should not cause any movement.
          NumberAnimation {
            id: expandContentSlide
            target: pageContent
            property: "y"
            duration: Theme.animation.normalDuration
            easing.type: Easing.InOutQuad
          }

          ConnectRegion {
            id: connectRegion
            x: 0
            y: 0
            width: parent.width
            // Hide when not visible, so tab stops work correctly.
            visible: !pageExpanded || pageContent.y > pageContent.expandedSlideOffset
          }

          ModuleSorter {
            id: moduleSorter
            anchors.top: connectRegion.bottom
            x: 0
            width: parent.width

            showBookmarks: pageExpanded
            // Show the below-fold section only if the page is expanded or it is
            // (even slightly) slid into view (so it's not visible during a
            // transition to the regions list while collapsed).
            // - pageExpanded is important for Linux, the windowed dashboard
            //   does not animate its height, so the below-fold section becomes
            //   visible immediately upon expansion (before the page content
            //   slides)
            // - pageContent.y < 0 can apply even when pageExpanded is false
            //   since the collapse slide is animated
            showBelowFold: pageExpanded || pageContent.y < 0
          }

          // Whenever a child shows its focus cue, scroll to make sure it's visible.
          // Note that this is here on the page content instead of on ConnectPage
          // itself so it excludes the ExpandButton.
          FocusCue.onChildCueRevealed: {
            // If we're animating, don't scroll, it'd be jarring to jump out of
            // the animation or change it.  Tabbing while the animation is
            // occurring makes little sense anyway and this is a pretty
            // reasonable result.
            if(expandScrollToAnimation.running || expandContentSlide.running)
              return

            // Get the focus cue's bounds relative to the content of the scroll
            // view's Flickable
            // Note that we scroll the focus cue into view instead of the
            // control, many of the focus cues extend outside of the control
            // slightly.
            var cueBound = focusCue.mapToItem(scrollWrapperFlickable.contentItem,
                                              0, 0, focusCue.width,
                                              focusCue.height)

            Util.ensureScrollViewVertVisible(scrollWrapper,
                                             scrollWrapper.ScrollBar.vertical,
                                             cueBound.y, cueBound.height)
          }
        }

        Keys.onPressed: {
          KeyUtil.handleVertScrollKeyEvent(event, scrollWrapper,
                                           scrollWrapper.ScrollBar.vertical,
                                           scrollFocusCue)
        }
      }
    }

    OutlineFocusCue {
      id: scrollFocusCue
      anchors.fill: parent
      control: scrollWrapperFlickable
      inside: true
    }
  }

  ExpandButton {
    id: expandButton
    // Note that the expand button is anchored to the bottom of the page (not
    // positioned using the layout height), so it animates as the page height
    // animates.
    anchors.bottom: parent.bottom
    anchors.left: parent.left
    anchors.right: parent.right
    height: 40
    expanded: pageExpanded
    onToggleExpand: {
      // If we were already animating, stop that animation and start another.
      expandScrollToAnimation.stop()
      expandContentSlide.stop()

      // Get the current scroll position in pixels
      var oldScrollPos = scrollWrapper.ScrollBar.vertical.position * scrollWrapper.contentHeight
      // Get the old slide offset.  If we interrupted an animation, this could
      // be a value in between the expanded and closed positions.
      var oldSlideOffset = pageContent.y

      // Toggle the expansion state
      pageExpanded = !pageExpanded

      // Restore the scroll position to a position that matches visually - the
      // scroll bar resets it back to 0 when the layout changes.  This position
      // might be outside of the scroll bar's normal range.
      var matchingScrollPos = oldScrollPos / scrollWrapper.contentHeight
      scrollWrapper.ScrollBar.vertical.position = matchingScrollPos

      // Then, find the position where we ultimately want to end.
      // When collapsing, end at the top; when expanding, aim for the top of the
      // below-fold content ("the fold"!).
      var finalScrollPos = pageExpanded ? moduleSorter.aboveFoldHeight + pageContent.slideOffset : 0
      finalScrollPos = Math.max(finalScrollPos, 0)
      finalScrollPos = Math.min(finalScrollPos, scrollWrapper.contentHeight - scrollWrapper.height)
      finalScrollPos /= scrollWrapper.contentHeight

      // Scroll and slide simultaneously, so we smoothly shift to the new view
      expandScrollToAnimation.to = finalScrollPos
      expandScrollToAnimation.start()
      expandContentSlide.from = oldSlideOffset
      expandContentSlide.to = pageContent.slideOffset
      expandContentSlide.start()
    }
  }

  function beforeExit() {

  }
  function onEnter () {
    headerBar.logoCentered = true
    headerBar.needsBottomLine = true
  }

  Connections {
    target: Daemon.account
    // Collapse the connect page when a logout occurs.  It's strange to log in
    // and then be presented with an already-expanded connect page.  However,
    // going to the regions list from an expanded connect page should preserve
    // the expanded state, so we don't do this on a page transition.
    onLoggedInChanged: {
      if (!Daemon.account.loggedIn)
        pageExpanded = false;
    }
  }

  Component.onCompleted: {
    // If auto-connect is enabled, the daemon is disconnected, and we have user
    // credentials, connect at startup.
    // We don't know right now whether the user credentials are valid, the
    // connection attempt will fail if they're incorrect.
    if(Client.settings.connectOnLaunch &&
       Daemon.state.connectionState === 'Disconnected' &&
       Daemon.account.loggedIn) {
       console.info('Connecting automatically, connectOnLaunch is enabled');
       VpnConnection.connectCurrentLocation();
    }
  }
}
