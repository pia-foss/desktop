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
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick.Shapes 1.11
import "../theme"

Item {
  x: 0
  y: 0
  width: 200
  height: 200
  state: 'start'
  id: starItem

  property int final_length: 350
  property int final_radius: 3
  property double star_angle: 1
  property int fly_duration: 1500
  property int wait_duration: 1500
  opacity: 1

  // if this star is ready to take on a new instance
  property bool ready: true


  property int trail_length: 0
  property int star_radius: 0

  SequentialAnimation {
    id: mainAnimation
    ScriptAction {
      script: {
        ready = false
      }
    }

    ParallelAnimation {
      SmoothedAnimation{
        target: starItem
        property: 'trail_length'
        to: final_length
        duration: fly_duration
      }
      SmoothedAnimation{
        target: starItem
        property: 'star_radius'
        to: final_radius
        duration: fly_duration
      }
    }

    PauseAnimation {
      duration: wait_duration
    }
      NumberAnimation {
          target: starItem
          property: 'opacity'
          to: 0
          duration: 1000
      }
    ScriptAction {
      script: {
        // allow this star to be reassigned in a new position
       starItem.ready = true
      }
    }
  }

  Shape {
    width: 200
    height: 150
    anchors.centerIn: parent
    ShapePath {
      id: starShape
      strokeWidth: 0
      strokeColor: "transparent"
      fillGradient: LinearGradient {
        x1: 0
        y1: 0
        x2: final_length * Math.sin(star_angle)
        y2: final_length * Math.cos(star_angle)
        GradientStop {
          position: 0
          color: Theme.onboarding.starGradientStartColor
        }
        GradientStop {
          position: 0.4
          color: Theme.onboarding.starGradientStartColor
        }
        GradientStop {
          position: 0.7
          color: Theme.onboarding.starGradientMidColor
        }
        GradientStop {
          position: 1
          color: Theme.onboarding.starGradientEndColor
        }
      }

      startX: 0
      startY: 0

      property int tipX: Math.sin(star_angle) * trail_length
      property int tipY: Math.cos(star_angle) * trail_length

      property int arcStartX: tipX + Math.cos(star_angle) * star_radius
      property int arcStartY: tipY - Math.sin(star_angle) * star_radius
      property int arcEndX: tipX - Math.cos(star_angle) * star_radius
      property int arcEndY: tipY + Math.sin(star_angle) * star_radius

      PathLine {
        x: starShape.arcStartX
        y: starShape.arcStartY
      }
      PathArc {
        x: starShape.arcEndX
        y: starShape.arcEndY
        radiusX: star_radius
        radiusY: star_radius
      }
      PathLine {
        x: 0
        y: 0
      }
    }
  }
  function start() {
    if(ready) {
      // reset starting values
      trail_length = 0
      star_radius = 0
      opacity = 1
      mainAnimation.start();
    }
  }
}
