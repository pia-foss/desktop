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

import QtQuick 2.0
import QtQuick.Controls 2.3
import "../../core"
import "../../theme"

Rectangle {
  property bool active

  implicitWidth: 28
  implicitHeight: 28
  color: active ? Theme.settings.pageIconBgActiveColor : Theme.settings.pageIconBgInactiveColor
  radius: width / 2

  Image {
    id: iconImg
    source: Theme.settings.pageImages[modelData.name][active ? 0 : 1]
    anchors.centerIn: parent
    height: sourceSize.width/2
    width: sourceSize.height/2
  }
}
