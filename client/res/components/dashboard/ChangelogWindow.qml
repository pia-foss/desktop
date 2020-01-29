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
import "../common"
import "../core"
import "../daemon"
import "../theme"
import PIA.NativeHelpers 1.0
import PIA.NativeAcc 1.0 as NativeAcc
import "qrc:/javascript/util.js" as Util

SecondaryWindow {
  id: changelog

  title: uiTr("Changelog")
  color: Theme.dashboard.backgroundColor

  readonly property real contentMargin: 20
  windowLogicalWidth: 500
  windowLogicalHeight: 650
  resizeable: true
  // The changelog is resizeable; the content width is the actual window width.
  contentLogicalWidth: actualLogicalWidth
  contentLogicalHeight: {
    var h = changelogText.height
    console.info(title + ' height: ' + h)
    return h
  }

  MarkdownPage {
    id: changelogText
    width: parent.width
    margins: contentMargin
    fontPixelSize: 14
    text: uiBrand(NativeHelpers.readResourceText("qrc:/CHANGELOG.md"))
    color: Theme.dashboard.textColor
  }

  Connections {
    target: ClientNotifications
    onShowChangelog: {
      open();
    }
  }
}
