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
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import "../../../common"
import "../../../core"
import "../../../theme"
import PIA.FocusCue 1.0
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/util.js" as Util
import "qrc:/javascript/keyutil.js" as KeyUtil

OverlayDialog {
  id: betaAgreementDialog
  title: uiTr("Agreement")
  buttons: [
    //: "Accept" button for accepting the Beta agreement, should use the
    //: typical terminology for accepting a legal agreement.
    { text: uiTr("Accept"), role: DialogButtonBox.AcceptRole },
    //: "Decline" button for declining the Beta agreement, should use the
    //: typical terminology for declining a legal agreement.
    { text: uiTr("Decline"), role: DialogButtonBox.RejectRole },
  ]
  contentWidth: 450
  ColumnLayout {
    width: parent.width
    StaticText {
      Layout.fillWidth: true
      // Legal text - not translated
      text: "Please read the PIA® Software Program Agreement carefully. It represents a legal agreement between you and PIA® regarding PIA® proprietary and confidential information. Please keep in mind that you should not install beta software on a business critical device and we strongly recommend installing beta software on a secondary system or device."
      // Always in English, never flip this text.
      rtlAlignmentMirror: false
      wrapMode: Text.WordWrap
      color: Theme.settings.inputLabelColor
    }

    // The agreement text itself is styled like a read-only text box.
    Rectangle {
      id: agreementLayoutItem
      Layout.fillWidth: true
      height: 200

      color: Theme.settings.inputTextboxBackgroundColor
      border.width: 1
      border.color: Theme.settings.inputTextboxBorderColor

      // Clip the contents of the scroll view (but not the focus cue)
      // For some reason, putting clip: true on the scroll view itself creates
      // weird margins on Windows only, so use a wrapper.  (It's really strange,
      // the heading rows seem fine but the regular paragraphs cut off the left
      // side.)
      Item {
        id: agreementClip
        anchors.fill: parent
        clip: true

        // Put an accessibility group around the text.  Otherwise, VoiceOver
        // interleaves the text and Decline/Accept buttons, since it ignores
        // control order.
        //: Screen reader annotation for the beta license agreement content (a
        //: text element containing the license agreement).
        NativeAcc.Group.name: uiTr("Agreement text")

        ThemedScrollView {
          id: scrollWrapper
          anchors.fill: parent
          label: betaAgreementDialog.title
          contentWidth: width
          contentHeight: agreementText.implicitHeight

          Flickable {
            id: scrollWrapperFlickable
            boundsBehavior: Flickable.StopAtBounds

            // Always a tab stop, the legal text is loooooong.
            activeFocusOnTab: true

            MarkdownPage {
              id: agreementText
              width: agreementLayoutItem.width
              margins: 10
              text: NativeHelpers.readResourceText("qrc:/BETA_AGREEMENT.md")
              color: Theme.settings.inputTextboxTextColor
              rtlAlignmentMirror: false   // This is English-only
            }

            Keys.onPressed: {
              KeyUtil.handleVertScrollKeyEvent(event, scrollWrapper,
                                               scrollWrapper.ScrollBar.vertical,
                                               scrollFocusCue)
            }

            FocusCue.onChildCueRevealed: {
              var cueBound = focusCue.mapToItem(scrollWrapperFlickable.contentItem,
                                                0, 0, focusCue.width,
                                                focusCue.height)
              Util.ensureScrollViewBoundVisible(scrollWrapper,
                                                scrollWrapper.ScrollBar.horizontal,
                                                scrollWrapper.ScrollBar.vertical,
                                                cueBound)
            }
          }
        }
      }

      // The focus cue is a sibling to the scroll view, not inside it
      OutlineFocusCue {
        id: scrollFocusCue
        anchors.fill: parent
        control: scrollWrapperFlickable
      }
    }
  }

  function setInitialFocus() {
    scrollWrapperFlickable.forceActiveFocus(Qt.MouseFocusReason)
  }
}
