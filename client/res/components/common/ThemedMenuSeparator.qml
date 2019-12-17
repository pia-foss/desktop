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
import QtQuick.Controls 2.4
import "../theme"

// A themed MenuSeparator intended for use with ThemedMenu.
MenuSeparator {
  implicitWidth: parent.width
  implicitHeight: visible ? 9 : 0
  topPadding: 4
  bottomPadding: 4
  contentItem: Rectangle {
    color: Theme.dashboard.menuSeparatorColor
  }
}
