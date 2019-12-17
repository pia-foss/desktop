// Copyright (c) 2019 London Trust Media Incorporated
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
import "../core"
import "../theme"

Rectangle {
  id: pushButton

  // Minimum width for the button as long as the text fits (otherwise, extended
  // to fit the length of the longest label)
  property int minWidth: 80
  // Minimum end margin around each label
  property int minEndMargin: 10
  // Size of the border
  property int borderSize: 2

  // The button can have multiple labels.  Specifying all the labels (rather
  // than binding just a single label to multiple values) allows the button to
  // size itself such that any label will fit (the button will not resize when
  // changing state).
  //
  // Set 'labels' to an array of strings.
  //
  // If the sizing behavior isn't needed, 'labels' can be dynamically bound like
  // a single text label (just return that label as an array of one element, and
  // set currentLabel to 0).
  property var labels: []
  // currentLabel is the index of the label to display from 'labels'
  property int currentLabel: 0

  signal clicked()

  // Color styles for the button - set 'style' to one of these
  readonly property var dashboardStyle: ({
    borderColor: Theme.dashboard.pushButtonBackgroundColor,
    backgroundColor: Theme.dashboard.pushButtonBackgroundColor,
    // Use dashboard background color to show an outline only button when
    // disabled
    disabledBackgroundColor: Theme.dashboard.backgroundColor,
    hoverBackgroundColor: Theme.dashboard.pushButtonBackgroundHover,
    textColor: Theme.dashboard.textColor,
    disabledTextColor: Theme.dashboard.disabledTextColor,
    hoverTextColor: Theme.dashboard.textColor
  })
  readonly property var settingsStyle: ({
    borderColor: Theme.settings.inputPushButtonHoverBackgroundColor,
    backgroundColor: Theme.settings.backgroundColor,
    disabledBackgroundColor: Theme.settings.backgroundColor,
    hoverBackgroundColor: Theme.settings.inputPushButtonHoverBackgroundColor,
    textColor: Theme.settings.inputPushButtonTextColor,
    disabledTextColor: Theme.settings.inputPushButtonTextColor,
    hoverTextColor: Theme.settings.inputPushButtonHoverTextColor
  })

  property var style: dashboardStyle

  // Normally the button is minWidth, but in some languages the text is very
  // long - in that case, expand to fit the longest label with appropriate
  // margins
  width: Math.max(minWidth, buttonTextStack.width + 2*minEndMargin)
  radius: height / 2
  color: pushButton.style.borderColor

  Rectangle {
    id: snooozeButtonHover
    x: pushButton.borderSize
    y: pushButton.borderSize
    height: parent.height - 2 * pushButton.borderSize
    width: parent.width - 2 * pushButton.borderSize
    color: {
      if(!pushButton.enabled)
        return pushButton.style.disabledBackgroundColor

      return pushButtonArea.containsMouse ? pushButton.style.hoverBackgroundColor : pushButton.style.backgroundColor
    }
    radius: height / 2

    StackLayout {
      id: buttonTextStack
      anchors.centerIn: parent

      function calcMaxLabelSize(propName) {
        // Find the height/width of the largest label
        var childrenDep = buttonTextRepeater.children
        var maxTextWidth = 1
        for(var i=0; i<buttonTextRepeater.count; ++i) {
          var buttonText = buttonTextRepeater.itemAt(i)
          if(buttonText)
            maxTextWidth = Math.max(maxTextWidth, buttonText[propName])
        }
        return maxTextWidth
      }

      height: calcMaxLabelSize('contentHeight')
      width: calcMaxLabelSize('contentWidth')
      currentIndex: pushButton.currentLabel

      Repeater {
        id: buttonTextRepeater
        model: pushButton.labels
        Text {
          horizontalAlignment: Text.Center
          color: {
            if(!pushButton.enabled)
              return pushButton.style.disabledTextColor
            return pushButtonArea.containsMouse ? pushButton.style.hoverTextColor : pushButton.style.textColor
          }
          text: pushButton.labels[index]
        }
      }
    }
  }

  ButtonArea {
    id: pushButtonArea
    anchors.fill: parent
    name: pushButton.labels[pushButton.currentLabel]
    enabled: pushButton.enabled
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor
    onClicked: pushButton.clicked()
  }
}
