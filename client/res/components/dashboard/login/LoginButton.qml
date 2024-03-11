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
import "qrc:/javascript/keyutil.js" as KeyUtil
import "../../common"
import "../../core"
import "../../theme"

FocusScope {
  property bool loginEnabled
  property bool loginWorking
  property string buttonText: uiTr("LOG IN")

  width: 260
  height: 40
  id: lb
  enabled: loginEnabled

  signal triggered;

  state: {
    if(loginWorking)
      return 'working'
    else if(!loginEnabled)
      return 'disabled'
    else if(loginMouseArea.containsPress)
      return 'pressed'
    else if(loginMouseArea.containsMouse)
      return 'hovered'
    else
      return 'normal'
  }

  states: [
    State {
      name: "disabled"
      PropertyChanges {
        target: loginImage
        source: Theme.login.buttonDisabledImage
      }
      PropertyChanges {
        target: loginLabel
        color: Theme.login.buttonDisabledTextColor
      }
      PropertyChanges {
        target: loginLabel
        opacity: 1
      }
      PropertyChanges {
        target: spinnerImage
        opacity: 0
      }
    },
    State {
      name: "normal"
      PropertyChanges {
        target: loginImage
        source: Theme.login.buttonNormalImage
      }
      PropertyChanges {
        target: loginLabel
        color: Theme.login.buttonEnabledTextColor
      }
      PropertyChanges {
        target: loginLabel
        opacity: 1
      }
      PropertyChanges {
        target: spinnerImage
        opacity: 0
      }
    },
    State {
      name: "hovered"
      PropertyChanges {
        target: loginImage
        source: Theme.login.buttonHoverImage
      }
      PropertyChanges {
        target: loginLabel
        color: Theme.login.buttonEnabledTextColor
      }
      PropertyChanges {
        target: loginLabel
        opacity: 1
      }
      PropertyChanges {
        target: spinnerImage
        opacity: 0
      }
    },
    State {
      name: "pressed"
      PropertyChanges {
        target: loginImage
        source: Theme.login.buttonWorkingImage
      }
      PropertyChanges {
        target: loginLabel
        color: Theme.login.buttonEnabledTextColor
      }
      PropertyChanges {
        target: loginLabel
        opacity: 1
      }
      PropertyChanges {
        target: spinnerImage
        opacity: 0
      }
    },
    State {
      name: "working"
      PropertyChanges {
        target: loginImage
        source: Theme.login.buttonWorkingImage
      }
      PropertyChanges {
        target: loginLabel
        color: Theme.login.buttonEnabledTextColor
      }
      PropertyChanges {
        target: loginLabel
        opacity: 0
      }
      PropertyChanges {
        target: spinnerImage
        opacity: 1
      }
    }

  ]


  Image {
    id: loginImage
    anchors.fill: parent
    source: Theme.login.buttonNormalImage
  }

  Image {
    id: spinnerImage
    height: 20
    width: 20
    source: Theme.login.buttonSpinnerImage
    anchors.horizontalCenter: loginImage.horizontalCenter
    anchors.verticalCenter: loginImage.verticalCenter


    RotationAnimator {
      id: ra
      target: spinnerImage
      running: loginWorking
      from: 0;
      to: 360;
      duration: 1000
      loops: Animation.Infinite
    }
  }

  Text {
    id: loginLabel
    anchors.fill: parent
    anchors.leftMargin: 15
    anchors.rightMargin: 15
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
    text: buttonText
    font.pixelSize: Theme.login.buttonTextPx
  }

  ButtonArea {
    id: loginMouseArea
    anchors.fill: loginImage

    name: loginLabel.text

    hoverEnabled: true
    cursorShape: loginEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    onClicked: {
      if(loginEnabled && !loginWorking) {
        triggered();
      }
    }
  }
}
