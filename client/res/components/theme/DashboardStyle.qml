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
QtObject {
    // Font sizes are all intended to be used as 'font.pixelSize' (not
    // pointSize).  pixelSize tends to be more consistent between Windows and
    // Mac OS - pointSize varies more because Windows generally has (or pretends
    // to have) 96 DPI, while Mac OS uses 72 DPI.
    //
    // 13px is the go-to standard since this looks pretty 'normal' on both Mac
    // OS and Windows.
    //
    // Also, since Mac OS usually uses 72 DPI, this means pixelSize and
    // pointSize will be the same on Mac OS, so be sure to use the correct one.

    readonly property int width: 300
    readonly property int windowPadding: 10
    readonly property int shadowRadius: 10
    readonly property int windowRadius: 12

    readonly property color backgroundColor: Theme.dark ? '#323642' : '#eeeeee'
    readonly property color textColor: Theme.dark ? '#ffffff' : '#323642'
    readonly property color disabledTextColor: "#889099"
    readonly property string shadowImage: Theme.imagePathCommon + "/dashboard/shadow-dashboard.png"

    readonly property color scrollBarColor: Theme.dark ? '#5c6370' : '#889099'
    readonly property color scrollBarPressedColor: Theme.dark ? '#889099' : '#5c6370'

    readonly property string connectButtonOutlineImage: Theme.imagePath + "/dashboard/connect/connect-button-outline.png"
    readonly property string connectButtonDisconnectedImage: Theme.imagePathCommon + "/dashboard/connect/connect-button-disconnected.png"
    readonly property string connectButtonDisconnectedHoverImage: Theme.imagePathCommon + "/dashboard/connect/connect-button-disconnected-hover.png"
    readonly property string connectButtonConnectedImage: Theme.imagePath + "/dashboard/connect/connect-button-connected.png"
    readonly property string connectButtonConnectedHoverImage: Theme.imagePath + "/dashboard/connect/connect-button-connected-hover.png"
    readonly property string connectButtonConnectingImage: Theme.imagePathCommon + "/dashboard/connect/connect-button-connecting.png"
    readonly property string connectButtonSnoozedImage: Theme.imagePathCommon + "/dashboard/connect/connect-button-snoozed.png"
    readonly property string connectButtonSnoozedHoverImage: Theme.imagePathCommon + "/dashboard/connect/connect-button-snoozed.png" // TODO: Use separate hover image
    readonly property string connectButtonDisconnectingImage: Theme.imagePathCommon + "/dashboard/connect/connect-button-disconnecting.png"
    readonly property string connectButtonErrorImage: Theme.imagePathCommon + "/dashboard/connect/connect-button-error.png"
    readonly property string connectButtonErrorHoverImage: Theme.imagePath + "/dashboard/connect/connect-button-error-hover.png"
    readonly property string clipboardImage: Theme.imagePath + "/dashboard/connect/copy-content.png"
    readonly property string subnetImage: Theme.imagePath + "/dashboard/connect/computer-screen-3.png"
    readonly property int connectButtonSizePx: 156
    readonly property int connectButtonVMarginPx: 22

    readonly property string headerLogoImage: Theme.imagePath + "/header-logo.png"
    readonly property string headerMenuLightImage: Theme.imagePathCommon + "/dashboard/header/menu1.png"
    readonly property string headerMenuDarkImage: Theme.imagePathCommon + "/dashboard/header/menu2.png"
    readonly property string headerMenuUpdateLightImage: Theme.imagePathCommon + "/dashboard/header/menu-update-light.png"
    readonly property string headerMenuUpdateDarkImage: Theme.imagePathCommon + "/dashboard/header/menu-update-dark.png"
    readonly property string headerBackImage: Theme.imagePathCommon + "/dashboard/header/back.png"
    readonly property string headerBack2Image: Theme.imagePathCommon + "/dashboard/header/back2.png"
    readonly property color headerDefaultTextColor: Theme.dark ? "#ffffff" : "#323642"
    readonly property color headerYellowTextColor: "#323642"
    readonly property color headerGreenTextColor: "#ffffff"
    readonly property color headerRedTextColor: "#ffffff"
    readonly property int headerTextPx: 14
    readonly property color headerYellowColor: '#f9cf01'
    readonly property color headerYellowBottomColor: '#e6b400'
    readonly property color headerGreenColor: '#5ddf5a'
    readonly property color headerGreenBottomColor: '#4cb649'
    readonly property color headerRedColor: '#f5515f'
    readonly property color headerRedBottomColor: '#c8283b'
    readonly property color headerBottomLineColor: Theme.dark ? "#22252e" : "#c2c5c8"
    readonly property int headerAlphaTextPx: 13
    readonly property string headerAlphaImage: Theme.imagePathCommon + "/dashboard/header/alpha.png"
    readonly property string headerBetaImage: Theme.imagePathCommon + "/dashboard/header/beta.png"

    readonly property color notificationTextColor: Theme.dark ? "#ffffff" : "#323642"
    readonly property int notificationTextPx: 13
    readonly property int notificationTextLinePx: 18
    readonly property int notificationVertMarginPx: 10
    readonly property int notificationHorzMarginPx: 15
    readonly property int notificationImgTextGapPx: 8
    readonly property color notificationBackgroundColor: Theme.dark ? "#22252e" : "#d7d8d9"
    readonly property color notificationHoverBackgroundColor: Theme.dark ? "#08ffffff" : "#10000000"
    readonly property string notificationErrorAlert: Theme.imagePathCommon + "/dashboard/connect/red-alert.png"
    readonly property string notificationWarningAlert: Theme.imagePathCommon + "/dashboard/connect/yellow-alert.png"
    readonly property string notificationInfoAlert: Theme.imagePath + "/dashboard/connect/green-alert.png"
    readonly property string notificationCloseImage: Theme.imagePath + "/dashboard/connect/close.png"
    readonly property color notificationInfoLinkColor: Theme.dark ? '#5ddf5a' : '#4cb649'
    readonly property color notificationInfoLinkHoverColor: Theme.dark ? '#7afa78' : '#5ddf5a'
    readonly property color notificationWarningLinkColor: '#e6b400'
    readonly property color notificationWarningLinkHoverColor: Theme.dark ? '#f9cf01' : '#ffc800'
    readonly property color notificationErrorLinkColor: '#f24458'
    readonly property color notificationErrorLinkHoverColor: Theme.dark ? '#f56d7d' : '#f799a4'
    readonly property int notificationProgressMarginPx: 4
    readonly property int notificationProgressHeightPx: 4
    readonly property color notificationProgressBackgroundColor: Theme.dark ? "#889099" : "#889099"
    readonly property color notificationProgressColor: Theme.dark ? "#d7d8d9" : "#323642"
    readonly property int notificationProgressStopSizePx: 10
    readonly property color notificationStopColor: Theme.dark ? "#889099" : "#889099"
    readonly property color notificationStopHoverColor: Theme.dark ? "#d7d8d9" : "#323642"
    readonly property color notificationStopPressColor: Theme.dark ? "#eeeeee" : "#22252e"

    readonly property color menuBackgroundColor: "#ffffff"
    readonly property color menuBorderColor: "#b6b6b6"
    readonly property color menuTextColor: "#000000"
    readonly property int menuTextPx: 13
    readonly property color menuHighlightColor: "#eeeeee"
    readonly property color menuSeparatorColor: "#dddddd"

    readonly property string moduleBookmarkOffImage: Theme.imagePath + "/dashboard/connect/bookmark-off.png"
    readonly property string moduleBookmarkOnImage: Theme.imagePathCommon + "/dashboard/connect/bookmark-on.png"
    readonly property string moduleGrabberImage: Theme.imagePath + "/dashboard/connect/grabber.png"
    readonly property string moduleGrabberFavoriteImage: Theme.imagePathCommon + "/dashboard/connect/grabber-favorite.png"
    readonly property color moduleDragOverlayColor: "#20889099"
    readonly property color moduleTitleColor: "#889099"
    readonly property color moduleTextColor: Theme.dark ? "#ffffff" : "#323642"
    readonly property color moduleSecondaryTextColor: "#889099"
    readonly property int moduleLabelTextPx: 12
    readonly property int moduleSublabelTextPx: 10
    readonly property int moduleValueTextPx: 14
    readonly property int moduleBorderPx: 1
    readonly property color moduleBorderColor: Theme.dark ? "#22252e" : "#c2c5c8"
    readonly property string moduleRightArrowImage: Theme.imagePath + "/dashboard/connect/arrow-right.png"
    readonly property string moduleRegionMapImage: "qrc:/img/map.png"
    readonly property string moduleRegionMapOpacity: Theme.dark ? 1.0 : 0.15
    readonly property string moduleExpandImage: Theme.imagePath + "/dashboard/connect/expand.png"
    readonly property string moduleExpandActiveImage: Theme.imagePath + "/dashboard/connect/expand-active.png"
    readonly property string moduleRightChevronImage: Theme.imagePath + "/dashboard/connect/chevron-right.png"
    readonly property int moduleSeparatorTextPx: 10
    // The drag offset should be less than the window padding for the proper
    // "pop out" effect.
    readonly property int moduleDragOffset: 8

    readonly property string ipPortForwardImage: Theme.imagePathCommon + "/dashboard/connect/port-forward.png"
    readonly property string ipPortForwardSlashImage: Theme.imagePathCommon + "/dashboard/connect/port-forward-slash.png"
    readonly property string ipPortForwardSlashSelectedImage: Theme.imagePathCommon + "/dashboard/connect/port-forward-slash-green.png"

    readonly property string geoImage: Theme.imagePathCommon + "/dashboard/connect/geo-gray.png"
    readonly property string geoSelectedImage: Theme.imagePathCommon + "/dashboard/connect/geo-green.png"

    readonly property string quickConnectingImage: Theme.imagePath + "/dashboard/connect/quick-connect-connecting.png"
    readonly property string quickConnectedImage: Theme.imagePath + "/dashboard/connect/quick-connect-connected.png"
    readonly property string quickConnectFavoriteImage: Theme.imagePath + "/dashboard/connect/quick-connect-favorite.png"

    readonly property color settingOnHoverBorderColor: Theme.dark ? "#4cb649" : "#5ddf5a"
    readonly property color settingOnPressBorderColor: Theme.dark ? "#5ddf5a" : "#5ddf5a"
    readonly property color settingOnPressFillColor: Theme.dark ? "#4cb649" : "#5ddf5a"
    readonly property color settingOffHoverBorderColor: Theme.dark ? "#5c6370" : "#d7d8d9"
    readonly property color settingOffPressBorderColor: Theme.dark ? "#889099" : "#c2c5c8"
    readonly property color settingOffPressFillColor: Theme.dark ? "#5c6370" : "#d7d8d9"

    // There are a *lot* of images for the quick settings buttons, so they're
    // defined as a set of types and methods to get each image for a given type.
    // The valid types are determined by the embedded image resources, such as:
    // - notifications
    // - mace
    // - port-forwarding
    // - debug
    // - theme
    //
    // Disabled assets are not shipped for those types; currently 'lan' has a
    // disabled asset (only) since the setting is not implemented yet.
    //
    // The original SVG source and script used to render the PNGs can be found
    // in the `graphics/settings_icons` directory in this repo, see that
    // README.md for notes
    function settingImage(type, suffix) { return Theme.imagePathCommon + '/dashboard/connect/settings/' + type + '-' + suffix + '.png' }
    function settingOffImage(type) { return settingImage(type, Theme.dark ? 'grey40' : 'grey55') }
    function settingOffHoverImage(type) { return settingImage(type, Theme.dark ? 'grey85' : 'grey40') }
    function settingOffPressImage(type) { return settingImage(type, Theme.dark ? 'white' : 'grey40') }
    function settingOnImage(type) {return settingImage(type, 'green-dark20') }
    function settingOnHoverImage(type) {return settingImage(type, 'green') }
    function settingOnPressImage(type) {return settingImage(type, 'white') }

    readonly property string performanceDownloadImage: Theme.imagePath + "/dashboard/connect/performance-download.png"
    readonly property string performanceUploadImage: Theme.imagePath + "/dashboard/connect/performance-upload.png"
    readonly property string performanceDurationImage: Theme.imagePath + "/dashboard/connect/performance-duration.png"
    readonly property string performanceChartBackgroundColor: Theme.dark ? "#22252E" : "#ffffff"
    readonly property string performanceChartBorderColor: Theme.dark ? "transparent" : "#D7D8D9"
    readonly property string performanceChartBarActive : Theme.dark ? "#5DDF5A" : "#5DDF5A"
    readonly property string performanceChartBarInactive : Theme.dark ? "#2B2E39" : "#EEEEEE"
    readonly property string performanceChartText : Theme.dark ? "#ffffff" : "#323642"

    readonly property color locationMarkerCenterColor : Theme.dark ? "#5DDF5A" : "#4CB649"
    readonly property color locationMarkerOuterColor : Theme.dark ? "#3D56B654" : "#664CB649"

    readonly property color pushButtonBackgroundColor: Theme.dark ? "#3c4352" : "#cfcfd1"
    readonly property color pushButtonBackgroundHover: Theme.dark ? "#4b576b" : "#a8a9aa"

    readonly property color snoozeTimeDisplayColor: Theme.dark ? "#21252f" : "#fdfdfd"

    readonly property string connectionModuleAuthenticationImage: Theme.imagePathCommon + "/dashboard/connect/connection-tile/icon-authentication.png"
    readonly property string connectionModuleConnectionImage: Theme.imagePathCommon + "/dashboard/connect/connection-tile/icon-connection.png"
    readonly property string connectionModuleEncryptionImage: Theme.imagePathCommon + "/dashboard/connect/connection-tile/icon-encryption.png"
    readonly property string connectionModuleHandshakeImage: Theme.imagePathCommon + "/dashboard/connect/connection-tile/icon-handshake.png"
    readonly property string connectionModulePortImage: Theme.imagePathCommon + "/dashboard/connect/connection-tile/icon-port.png"
    readonly property string connectionModuleSocketImage: Theme.imagePathCommon + "/dashboard/connect/connection-tile/icon-socket.png"

    readonly property string ratingStarFilled: Theme.imagePath + "/dashboard/connect/rating-star-filled.png"
    readonly property string ratingStarEmpty: Theme.imagePath + "/dashboard/connect/rating-star-empty.png"
    readonly property int ratingImageHeightPx: 25
}
