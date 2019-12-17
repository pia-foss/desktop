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
    signal clicked()
    implicitWidth: linkText.width
    implicitHeight: linkText.height
    Text {
        id: linkText
        x: linkItem.horzCenter ? (parent.width/2 - width/2) : 0
        color: linkItem.enabled ? linkMouseArea.containsMouse ? Theme.login.linkHoverColor : Theme.login.linkColor : Theme.settings.inputLabelDisabledColor
        font.pixelSize: Theme.login.linkTextPx
        text: linkItem.text
        font.underline: linkItem.enabled && linkItem.underlined

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
