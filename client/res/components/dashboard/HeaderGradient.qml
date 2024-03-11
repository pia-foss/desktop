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
// HeaderGradient is just a vertical gradient with (optionally) rounded top
// corners.  It's used to render the gradient background in the header bar.
Item {
  id: headerGradient

  // Radius of the top corners (0 results in square corners)
  property real topRadius
  property color topColor
  property color bottomColor

  // Clip off the bottom of the gradient rectangle, so only the top corners are
  // rounded
  clip: true

  Rectangle {
    id: gradientRect

    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: parent.top

    // Extend the rounded rectangle so we can clip off the bottom rounded
    // corners.  Add a fudge factor of 1 to ensure there's no fringing where the
    // rounded corner would meet the straight edge.
    height: parent.height + topRadius + 1
    radius: headerGradient.topRadius

    gradient: Gradient {
      // Top
      GradientStop {
        color: topColor
        position: 0
      }
      // Bottom visible edge.  We reach the bottom color slightly before 1.0
      // since we clip off the bottom of the rectangle.
      GradientStop {
        color: bottomColor
        position: gradientRect.height / headerGradient.height
      }
      // Bottom
      GradientStop {
        color: bottomColor
        position: 1
      }
    }
  }
}
