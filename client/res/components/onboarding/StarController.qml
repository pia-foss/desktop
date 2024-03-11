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


Item {
  property int min_x: 0
  property int max_x: 500

  property int min_y: 0
  property int max_y: 200

  property int min_length: 150
  property int max_length: 350

  property int min_duration: 1000
  property int max_duration: 1500

  property int min_wait: 1000
  property int max_wait: 1500

  AnimatedStar {id: star0}
  AnimatedStar {id: star1}
  AnimatedStar {id: star2}
  AnimatedStar {id: star3}
  property var stars: [star0, star1, star2, star3]

  // the x coordinate of last launch. This is used to prevent
  // two stars launching too close to each other
  property int lastlaunch_x: 0

  // the threshold to determine whether a star is too close
  property int too_close_threshold: 60

  // dispatch a star
  function dispatch (stackSize) {
      // find one instance amongst the 4 available which is ready for dispatch
      var star = findAvailableStar()

      if(star === null)
        return;

      star.x = randBetween(min_x, max_x)

      if(Math.abs(star.x - lastlaunch_x) < too_close_threshold) {
        // dispatch another star and hope that one gets better luck
        if(stackSize > 0)
          dispatch(stackSize - 1);
        return;
      }

      star.y = randBetween(min_y, max_y)
      star.final_length = randBetween(min_length, max_length);
      star.fly_duration = randBetween(min_duration, max_duration);
      lastlaunch_x = star.x;

      star.start();
  }

  Timer {
    interval: 4000
    repeat: true
    running: true
    onTriggered: {
      dispatch(10)
    }
  }
  Timer {
    interval: 3200
    repeat: true
    running: true
    onTriggered: {
      dispatch()
    }
  }

  function randBetween(a, b) {
    return a + Math.random()*(b - a);

  }

  function findAvailableStar () {
    // find a star instance which is ready and can accept new parameters
    for(var i = 0; i < stars.length; i++) {
      if(stars[i].ready)
        return stars[i];
    }
    console.warn("No available stars found");
    return null;
  }

  Timer {
    // Timer to dispatch the second star a few ms after the very first
    running: true
    interval: 800
    onTriggered: dispatch()
  }

  Component.onCompleted:  {
    dispatch()
  }

}
