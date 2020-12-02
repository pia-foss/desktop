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
import "../theme"
import "../common"
import "../core"
import "qrc:/javascript/util.js" as Util
import PIA.NativeAcc 1.0 as NativeAcc

RowLayout {
  id: root

  property alias text: content.text
  property alias color: content.color
  property alias wrapMode: content.wrapMode
  property string icon: 'info'
  // Amount the text is indented relative to the left edge of DialogMessage,
  // used to align additional controls to the text
  property alias textIndent: content.x

  implicitWidth: (source ? image.sourceSize.width / 2 + 10 : 0) + content.implicitWidth
  implicitHeight: Math.max(source ? image.sourceSize.height / 2 : 0, content.contentHeight)

  readonly property var iconImages: ({
    'info': Theme.dashboard.notificationInfoAlert,
    'warning': Theme.dashboard.notificationWarningAlert,
    'error': Theme.dashboard.notificationErrorAlert,
  })
  readonly property var iconNames: ({
    //: Screen reader annotation for the "info" icon in dialog messages
    'info': uiTr("Information"),
    //: Screen reader annotation for the "warning" icon in dialog messages
    'warning': uiTr("Warning"),
    //: Screen reader annotation for the "error" icon in dialog messages
    'error': uiTr("Error")
  })
  readonly property string source: {
    if (!icon) return '';
    return iconImages[icon]
  }

  Layout.fillWidth: true
  Layout.topMargin: 5
  Layout.bottomMargin: -10

  spacing: 10
  StaticImage {
    id: image
    label: root.iconNames[icon]
    visible: !!root.source
    source: root.source
    Layout.preferredWidth: sourceSize.width / 2
    Layout.preferredHeight: sourceSize.height / 2
    Layout.alignment: Qt.AlignTop
  }
  StaticHtml {
    id: content
    Layout.fillWidth: true
    color: Theme.popup.dialogTextColor
    wrapMode: Text.Wrap
  }
}
