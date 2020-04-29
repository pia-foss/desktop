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

pragma Singleton
import QtQuick 2.0
import "../client"
import "../daemon"
import PIA.BrandHelper 1.0

QtObject {
    readonly property bool light: Client.settings.themeName === "light"
    readonly property bool dark: !light

    readonly property string name: dark ? "dark" : "light"

    readonly property string imagePath: "qrc:/img/" + name
    readonly property string imagePathCommon: "qrc:/img/common"

    readonly property DashboardStyle dashboard: DashboardStyle {}
    readonly property LoginStyle login: LoginStyle {}
    readonly property AnimationStyle animation: AnimationStyle {}
    readonly property SettingsStyle settings: SettingsStyle {}
    readonly property RegionStyle regions: RegionStyle {}
    readonly property PopupStyle popup: PopupStyle {}
    readonly property SplashStyle splash: SplashStyle {}
    readonly property OnboardingStyle onboarding: OnboardingStyle{}
}
