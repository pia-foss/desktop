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
import "../../../javascript/keyutil.js" as KeyUtil
import "../stores"
import "../../theme"
import "../../client"
import "../../common"
import "../../core"
import PIA.NativeAcc 1.0 as NativeAcc

FocusScope {
  id: root

  property Setting setting
  property var onValue: true
  property var offValue: false

  // Custom 'enabled' property; don't use Item.enabled so the InfoTip remains
  // enabled
  property bool enabled: true

  property string label: ""
  // Description displayed below the checkbox label (optional).
  property string desc: ""
  // Links associated with description (optional) - see MessageWithLinks.links.
  // Only shown if 'desc' is non-empty.
  property var descLinks: []
  property string info
  property string warning

  // A text link can be displayed to the right of the InfoTip, this has some
  // limitations currently:
  // - It spills out of the right side of the control
  // - Its layout requires a InfoTip to be configured
  property string linkText
  property string linkTarget

  // Beta features are tagged with a "beta" badge.
  property bool isBeta: false

  readonly property var currentValue: setting ? setting.currentValue : undefined

  Layout.fillWidth: true
  implicitHeight: desc ? (descriptionText.y + descriptionText.height) : text.contentHeight

  onCurrentValueChanged: control.checkState = (currentValue === onValue) ? Qt.Checked : (currentValue === offValue) ? Qt.Unchecked : Qt.PartiallyChecked

  function mapValueToState(value) {
    return (value === onValue) ? Qt.Checked : (value === offValue) ? Qt.Unchecked : Qt.PartiallyChecked;
  }
  function mapStateToValue(state) {
    return (state === Qt.Checked) ? onValue : (state === Qt.Unchecked) ? offValue : null;
  }

  CheckBox {
    id: control
    text: label
    enabled: root.enabled
    font.pixelSize: 13
    padding: 0
    anchors.top: parent.top

    // This doesn't account for the InfoTip's width, it's allowed to spill into
    // the right margin
    width: Math.min(root.width, text.implicitWidth)

    NativeAcc.CheckButton.name: label
    NativeAcc.CheckButton.checked: control.checkState === Qt.Checked
    NativeAcc.CheckButton.onActivated: {
      // Advance the state before calling updateState(); this is baked into the
      // button's key/mouse event handler normally.  toggle() isn't exactly the
      // same because it doesn't respect nextCheckState, but we don't set a
      // custom nextCheckState here.
      control.toggle()
      control.updateState()
    }

    function updateState() {
      if (checkState === Qt.Checked) {
        if (setting.currentValue !== parent.onValue) setting.currentValue = parent.onValue;
      } else if (checkState === Qt.Unchecked) {
        if (setting.currentValue !== parent.offValue) setting.currentValue = parent.offValue;
      }
    }

    onClicked: updateState()

    indicator: Image {
      id: indicator
      anchors.left: parent.left
      anchors.verticalCenter: parent.verticalCenter
      height: 14
      width: 14
      source: control.checked ? Theme.settings.inputCheckboxOnImage : Theme.settings.inputCheckboxOffImage
      opacity: root.enabled ? 1.0 : 0.3
    }
    contentItem: Text {
      id: text
      leftPadding: indicator.width + control.spacing
      text: control.text
      wrapMode: Text.WordWrap
      font: control.font
      color: root.enabled ? Theme.settings.inputLabelColor : Theme.settings.inputLabelDisabledColor
      verticalAlignment: Text.AlignVCenter
    }

    // If the user interacts with the check box using the keyboard, show the
    // focus cue (if it isn't already shown).
    Keys.onPressed: {
      if(KeyUtil.handlePartialButtonKeyEvent(event, focusCue)) {
        // Like in the accessibility handler, manually change the check state,
        // this is baked into the button's key event handler normally.
        control.toggle()
        control.updateState()
      }
    }
  }

  OutlineFocusCue {
    id: focusCue
    anchors.fill: control
    anchors.leftMargin: -4
    anchors.rightMargin: -4
    anchors.topMargin: 3
    anchors.bottomMargin: 3
    control: control
  }

  readonly property int textRightX: control.x + text.leftPadding + text.contentWidth

  StaticImage {
    id: betaFeature
    anchors.verticalCenter: control.verticalCenter
    readonly property int leftMargin: 7
    x: root.textRightX + leftMargin
    width: sourceSize.width/2
    height: sourceSize.height/2
    visible: root.isBeta
    label: Messages.betaPrereleaseImg
    source: Theme.settings.inputBetaFeature
  }

  InfoTip {
    id: infoTip
    anchors.verticalCenter: control.verticalCenter
    readonly property int leftMargin: 3
    x: {
      if(betaFeature.visible)
        return betaFeature.x + betaFeature.width + leftMargin
      return root.textRightX + leftMargin
    }
    showBelow: true
    tipText: warning || info
    visible: warning || info
    icon: warning ? icons.warning : icons.settings
  }

  // This link is currently only used with App Exclusions, it is a stopgap
  // solution with some minor limitations documented with the linkText /
  // linkTarget properties.
  TextLink {
    id: checkboxLink
    anchors.leftMargin: 3
    anchors.left: infoTip.right
    anchors.verticalCenter: control.verticalCenter
    visible: root.linkText && root.linkTarget
    text: root.linkText
    link: root.linkTarget
    underlined: true
  }

  MessageWithLinks {
    id: descriptionText
    anchors.top: control.bottom
    x: control.x + text.leftPadding
    message: desc
    links: desc ? descLinks : []
    wrapMode: Text.WordWrap
    color: Theme.settings.inputDescriptionColor
    width: Math.min(parent.width, implicitWidth)

    linkColor: Theme.settings.inputDescLinkColor
    linkHoverColor: Theme.settings.inputDescLinkHoverColor
    linkFocusColor: Theme.settings.backgroundColor
    linkFocusBgColor: Theme.popup.focusCueColor
  }

  Component.onCompleted: {
    control.checkState = mapValueToState(currentValue)
  }
}
