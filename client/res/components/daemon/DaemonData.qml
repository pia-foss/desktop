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
import PIA.NativeDaemon 1.0

QtObject {
  // The release versions/URIs shouldn't be used by the client, these are just
  // the persistent data for UpdateDownloader.  They're only provided so they
  // show up in dev tools.
  readonly property string gaChannelVersion: NativeDaemon.data.gaChannelVersion
  readonly property string gaChannelVersionUri: NativeDaemon.data.gaChannelVersionUri
  readonly property string betaChannelVersion: NativeDaemon.data.betaChannelVersion
  readonly property string betaChannelVersionUri: NativeDaemon.data.betaChannelVersionUri
}
