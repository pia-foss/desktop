// Copyright (c) 2022 Private Internet Access, Inc.
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
import QtQuick.Window 2.11
import "../daemon"
import "../theme"
import "../client"
import "../common"
import "../core"
import "../helpers"

Item {
  ColumnLayout {
    anchors.margins: 100
    anchors.fill: parent

    Row {
      Layout.alignment: Qt.AlignCenter
      Label {
        id: heading
        text: "In-App Communication Demo Message"
        font.pixelSize: 30
      }
    }

    Row {
      Layout.alignment: Qt.AlignCenter
      TextField {
        id: messagetext
        placeholderText: "Message text"
        selectByMouse: true
      }
    }

    Row {
      Layout.alignment: Qt.AlignCenter
      TextField {
        id: linktext
        placeholderText: "Link text"
        selectByMouse: true
      }
    }

    Row {
      Layout.alignment: Qt.AlignCenter
      TextField {
        id: viewAction
        placeholderText: "View action"
        selectByMouse: true
      }
    }

    Row {
      Layout.alignment: Qt.AlignCenter
      TextField {
        id: uriAction
        placeholderText: "URI action"
        selectByMouse: true
      }
    }

    Row {
      Layout.alignment: Qt.AlignCenter
      TextField {
        id: settingsAction
        placeholderText: "Settings action"
        selectByMouse: true
      }
    }

    Row {
      Layout.alignment: Qt.AlignCenter
      spacing: 5
      Button {
        id: createbutton
        width: 170
        text: ClientNotifications.testMessageExists() ? "Update Message" : "Create Message"
        onClicked: {
          ClientNotifications.updateTestMessage({
            id: 9999999,
            hasLink: linktext.text != "",
            messageTranslations: {"en-US": messagetext.text},
            linkTranslations: {"en-US": linktext.text},
            viewAction: viewAction.text,
            settingsAction: parseJson(settingsAction.text),
            uriAction: uriAction.text
          })

          function parseJson(json) {
            try {
              return JSON.parse(json)
            } catch (e) {
              return null
            }
          }
        }
      }

      Button {
        id: deletebutton
        width: 170
        text: "Delete Message"
        onClicked: {
          linktext.text = ""
          messagetext.text = ""
          viewAction.text = ""
          uriAction.text = ""
          settingsAction.text = ""
          ClientNotifications.deleteTestMessage()
        }
      }
    }
  }
}
