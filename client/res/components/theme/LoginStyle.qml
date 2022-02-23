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

import QtQuick 2.0
QtObject {
    readonly property string buttonDisabledImage: Theme.imagePath + "/dashboard/login/login-button-disabled.png"
    readonly property string buttonNormalImage: Theme.imagePathCommon + "/dashboard/login/login-button-primary.png"
    readonly property string buttonSpinnerImage: Theme.imagePathCommon + "/dashboard/login/login-button-spinner.png"
    readonly property string buttonWorkingImage: Theme.imagePathCommon + "/dashboard/login/login-button-press.png"
    readonly property string buttonHoverImage: Theme.imagePathCommon + "/dashboard/login/login-button-hover.png"
    readonly property string upgradeRocketImage: Theme.imagePathCommon + "/dashboard/login/upgrade-rocket.png"
    readonly property color buttonDisabledTextColor: Theme.dark ? "#323642" : "#ffffff"
    readonly property color buttonEnabledTextColor: "#ffffff"
    readonly property int buttonTextPx: 14

    readonly property color errorTextColor: "#f24458"
    readonly property color errorLineColor: "#f24458"
    readonly property int errorTextPx: 12

    readonly property string mapImage: "qrc:/img/map.png"
    readonly property string mapOpacity: Theme.dark ? 1.0 : 0.15

    readonly property color linkColor: "#889099"
    readonly property color linkHoverColor: Theme.dark ? "#D7D8D9" : "#5C6370"
    readonly property int linkTextPx: 12

    readonly property color inputDefaultAccentColor: Theme.dark ? "#22252e" : "#d7d8d9"
    readonly property color inputFocusAccentColor: "#889099"
    readonly property color inputTextColor: Theme.dark ? "#ffffff" : "#323642"
    readonly property color inputErrorAccentColor: "#f24458"
}
