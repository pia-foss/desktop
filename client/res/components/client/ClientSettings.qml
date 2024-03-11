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
import PIA.NativeClient 1.0

// Definition of client settings
// Do not use ClientSettings directly. If you want to read ClientSettings.themeName,
// use Client.settings.themeName instead to access the value.
QtObject {
  readonly property bool migrateDaemonSettings: NativeClient.settings.migrateDaemonSettings
  readonly property bool connectOnLaunch: NativeClient.settings.connectOnLaunch
  readonly property bool desktopNotifications: NativeClient.settings.desktopNotifications
  readonly property string themeName: NativeClient.settings.themeName
  readonly property string regionSortKey: NativeClient.settings.regionSortKey
  readonly property var favoriteLocations: NativeClient.settings.favoriteLocations
  readonly property var recentLocations: NativeClient.settings.recentLocations
  readonly property var vpnCollapsedCountries: NativeClient.settings.vpnCollapsedCountries
  readonly property var shadowsocksCollapsedCountries: NativeClient.settings.shadowsocksCollapsedCountries
  readonly property var primaryModules: NativeClient.settings.primaryModules
  readonly property var secondaryModules: NativeClient.settings.secondaryModules
  readonly property string iconSet: NativeClient.settings.iconSet
  readonly property string language: NativeClient.settings.language
  readonly property string dashboardFrame: NativeClient.settings.dashboardFrame
  readonly property string lastUsedVersion: NativeClient.settings.lastUsedVersion
  readonly property bool disableHardwareGraphics: NativeClient.settings.disableHardwareGraphics
  readonly property int snoozeDuration: NativeClient.settings.snoozeDuration
}
