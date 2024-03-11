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
import PIA.NativeHelpers 1.0

// Client feature toggles.
QtObject {
  readonly property string version: NativeHelpers.getClientVersion()
  // Prelease version tags
  // Don't use this directly to test for feature toggles, provide a named toggle
  // below.
  readonly property string versionPrerelease: version.split('+')[0].split('-').slice(1).join('-')
  // Whether this is a prerelease
  readonly property bool prerelease: versionPrerelease !== ''
  // Whether this is specifically a "beta" prerelease
  readonly property bool beta: versionPrerelease.split('.')[0] === 'beta'
}
