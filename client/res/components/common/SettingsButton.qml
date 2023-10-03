// Copyright (c) 2023 Private Internet Access, Inc.
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
import "../theme"
import "../common"
import "../core"
import "qrc:/javascript/keyutil.js" as KeyUtil
import PIA.NativeAcc 1.0 as NativeAcc

FocusScope {
    id: linkItem

    property string text
    // Hiding the TextLink by setting the 'visible' property disables the
    // MouseArea.
    property string link: ""
    property bool underlined: false
    property bool horzCenter: false
    property int verticalPadding: 8
    property int horizontalPadding: 20
    property string icon: ""
    property int iconSize: 10
    property int iconRightMargin: 8

    signal clicked()
    readonly property bool hasIcon: icon !== ""

    implicitWidth: linkText.width + 2*horizontalPadding + !!hasIcon * (iconSize + iconRightMargin)
    implicitHeight: linkText.height + 2*verticalPadding
    Rectangle {
      height: parent.implicitHeight
      width: parent.implicitWidth
      border.color: linkItem.enabled ? Theme.settings.buttonBorderColor : Theme.settings.disabledButtonBorderColor
      radius: height/2
      color: "transparent"
      opacity: {
        return linkMouseArea.containsMouse ? 0.8 : 1;
      }

      Image {
        visible: hasIcon
        source: hasIcon ? Theme.settings.buttonIcons[icon] : ""
        width: iconSize
        height: iconSize
        x: horizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        opacity: linkItem.enabled ? 1 : 0.6
      }

      Text {
        id: linkText
        color: Theme.dashboard.textColor
        x: horizontalPadding + !!hasIcon * (iconSize + iconRightMargin)
        anchors.verticalCenter: parent.verticalCenter
        font.pixelSize: Theme.login.linkTextPx
        text: linkItem.text
        font.underline: false
        opacity: linkItem.enabled ? 1 : 0.6
      }
      GenericButtonArea {
        id: linkMouseArea
        anchors.fill: parent

        NativeAcc.Link.name: linkItem.text
        NativeAcc.Link.onActivated: mouseClicked()

        enabled: linkItem.visible && linkItem.enabled
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true
        onClicked: {
          if(link.length > 0) {
            Qt.openUrlExternally(linkItem.link)
          }
          linkItem.clicked()
        }
      }
    }
}
