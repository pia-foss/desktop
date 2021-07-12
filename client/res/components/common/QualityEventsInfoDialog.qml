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

import QtQuick.Window 2.3
import QtQuick.Controls 2.4
import "../core"
import "../theme"
import PIA.NativeAcc 1.0 as NativeAcc

OverlayDialog {
  id: qualityEventsInfoDialog
  buttons: [ Dialog.Ok ]

  title: uiTr("About Connection Events")
  contentWidth: 500
  contentHeight: explanationText.height
  visible: false

  MarkdownPage {
    id: explanationText
    width: parent.width
    color: Theme.onboarding.defaultTextColor
    text: {
      var t = ""

      t += uiTr("This minimal information assists us in identifying and fixing potential connection issues. Note that sharing this information requires consent and manual activation as it is turned off by default.") + "\n"

      t += uiTr("We will collect information about the following events:") + "\n"

      t += "* " + uiTr("Connection Attempt") + "\n"
      t += "* " + uiTr("Connection Canceled") + "\n"
      t += "* " + uiTr("Connection Established") + "\n"

      t += uiTr("For all of these events, we will collect the following information:") + "\n"

      t += "* " + uiTr("Platform") + "\n"
      t += "* " + uiTr("App version") + "\n"
      t += "* " + uiTr("App type (pre-release or not)") + "\n"
      t += "* " + uiTr("Protocol used") + "\n"
      t += "* " + uiTr("Connection source (manual or using automation)") + "\n"

      t += uiTr("All events will contain a unique ID, which is randomly generated. This ID is not associated with your user account. This unique ID is re-generated daily for privacy purposes.") + "\n"

      t += uiTr("You will always be in control. You can see what data we’ve collected from Settings, and you can turn it off at any time.")

      return t
    }
  }
}
