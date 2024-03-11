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

import QtQuick 2.0
QtObject {
  readonly property color tipBackgroundColor: Theme.dark ? "#ffffff" : "#22252e"
  readonly property color tipTextColor: Theme.dark ? "#323642" : "#ffffff"
  readonly property int tipBalloonRadius: 4
  readonly property int tipTextSizePx: 13
  readonly property int tipTextMargin: 13
  readonly property int tipArrowSize: 8
  readonly property int tipWindowMargin: 10
  readonly property string tipInfoImg: Theme.imagePath + "/dashboard/connect/notification-info.png"

  // Focus cues aren't implemented as popups, but they do behave somewhat like
  // them in that they're overlaid over the normal window content, and they are
  // used in all windows
  readonly property int focusCueWidth: 2
  readonly property color focusCueColor: "#889099"
  // The dark focus cue color is used in the header bar over colored
  // backgrounds, even in the dark theme.
  readonly property color focusCueDarkColor: "#323642"

  readonly property color dialogBackgroundColor: Theme.settings.backgroundColor
  readonly property color dialogFrameColor: Theme.dark ? "#22252e" : "#c2c5c8"
  readonly property color dialogTitleTextColor: Theme.dark ? "#889099" : "#5c6370"
  readonly property color dialogTextColor: Theme.dark ? "#ffffff" : "#323642"

  readonly property color acceptBackgroundColor: "#52c44e"
  readonly property color acceptDownColor: "#45a643"
  readonly property color acceptDisabledColor: "#005c1c"
}
