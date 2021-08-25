// Copyright (c) 2021 Private Internet Access, Inc.
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
import "../../../common"
import "../../../core"
import "../../../daemon"
import "../../../theme"
import PIA.NativeAcc 1.0 as NativeAcc

MovableModule {
  implicitHeight: 80
  moduleKey: 'usage'

  //: Screen reader annotation for the Usage tile.
  tileName: uiTr("Usage tile")
  NativeAcc.Group.name: tileName

  function formatUsageBytes(bytes) {
      var sizes = ['KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];
      // Break the size down into <mantissa> * 1024^<exponent> to
      // find the most appropriate prefix
      var exponent = (Math.floor(Math.log(bytes) / Math.log(1024)));
      // We don't display "bytes", if we haven't transferred a whole KB
      // yet, say "0 KB"
      if(exponent < 1)
        return "0 KB"
      // Calculate the mantissa and pick a unit with the exponent
      return Math.round(bytes / Math.pow(1024, exponent), 2) + ' ' + sizes[exponent-1];
}

  Text {
    text: uiTr("USAGE")
    color: Theme.dashboard.moduleTitleColor
    font.pixelSize: Theme.dashboard.moduleLabelTextPx
    x: 20
    y: 10
    width: 260
    elide: Text.ElideRight
  }

  LabelText {
    id: downloadLabel
    text: uiTr("Download")
    color: Theme.dashboard.moduleTextColor
    font.pixelSize: Theme.dashboard.moduleSublabelTextPx
    x: 20
    y: 33
    width: 125
    elide: Text.ElideRight
  }

  ValueText {
    text: formatUsageBytes(Daemon.state.bytesReceived)
    label: downloadLabel.text
    color: Theme.dashboard.moduleTextColor
    font.pixelSize: Theme.dashboard.moduleValueTextPx
    x: 20
    y: 50
    width: 125
    elide: Text.ElideRight
  }

  LabelText {
    id: uploadLabel
    text: uiTr("Upload")
    color: Theme.dashboard.moduleTextColor
    font.pixelSize: Theme.dashboard.moduleSublabelTextPx
    x: 150
    y: 33
    width: 130
    elide: Text.ElideRight
  }

  ValueText {
    text: formatUsageBytes(Daemon.state.bytesSent)
    label: uploadLabel.text
    color: Theme.dashboard.moduleTextColor
    font.pixelSize: Theme.dashboard.moduleValueTextPx
    x: 150
    y: 50
    width: 130
    elide: Text.ElideRight
  }
}
