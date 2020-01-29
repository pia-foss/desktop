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
import '../theme'

Item {
  property int pageIndex
  readonly property int current: parent.currentPage

  x: {
    if(pageIndex < current)
      return -1 * Theme.onboarding.pageMoveDistance
    if(pageIndex > current)
      return Theme.onboarding.pageMoveDistance
    return 0
  }

  opacity: pageIndex == current ? 1 : 0

  Behavior on x {
    NumberAnimation {
      easing.type: Easing.InOutQuad
      duration: Theme.animation.normalDuration
    }
  }
  Behavior on opacity {
    NumberAnimation {
      duration: Theme.animation.normalDuration
      easing.type: Easing.InOutQuad
    }
  }

  visible: pageIndex == current
}
