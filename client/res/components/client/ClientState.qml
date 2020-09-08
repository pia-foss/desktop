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

import QtQuick 2.0
import PIA.NativeClient 1.0

QtObject {
  readonly property var languages: NativeClient.state.languages
  readonly property var activeLanguage: NativeClient.state.activeLanguage
  readonly property bool clientHasBeenUpdated: NativeClient.state.clientHasBeenUpdated
  readonly property bool showWhatsNew: NativeClient.state.showWhatsNew
  readonly property bool firstRunFlag: NativeClient.state.firstRunFlag
  readonly property bool quietLaunch: NativeClient.state.quietLaunch
  readonly property bool usingSafeGraphics: NativeClient.state.usingSafeGraphics
  readonly property bool winIsElevated: NativeClient.state.winIsElevated
}
