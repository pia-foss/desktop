// Copyright (c) 2023 Private Internet Access, Inc.
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

import QtQuick 2.10
QtObject {
  readonly property int height: 570
  readonly property int width: 700
  readonly property string obImagePath: imagePath + "/onboarding"

  // color palette
  readonly property color primaryColor: "#4CB649"
  readonly property color defaultTextColor: dark ? "#fff" : "#323642"

  // common
  readonly property string buttonTextColor: "#fff"
  readonly property int pageMoveDistance: 50
  readonly property color navDotInactive: "#5C6370" // same for dark and light
  readonly property color navDotActive: "#5DDF5A" // same for dark and light

  // Backdrop
  readonly property string backdropImage: obImagePath + "/backdrop.png"
  readonly property int backdropWidth: 1500
  readonly property int backdropImageWidth: 1500

  // Animated stars
  // Prevent stars from 'starting' beyond the padding
  readonly property int starStartLeftPadding: 50
  readonly property int starStartRightPadding: 150

  readonly property color starGradientStartColor: dark ? "#323642" : "#eeeeee"
  readonly property color starGradientMidColor: dark ? "#3f6949" : "#b7e8b6"
  readonly property color starGradientEndColor: "#5ddf5a"

  // Page content
  readonly property color sectionTitleColor: primaryColor
  readonly property color sectionDividerColor: "#889099"
  readonly property color sectionHeadlineColor: dark ? "#fff" : "#22252E"
  readonly property color sectionParagraphColor: sectionDividerColor

  readonly property string logoImage: imagePath + "/onboarding/logo.png"
  readonly property string spacemanImage: imagePath + "/onboarding/graphic.png"
  readonly property string primaryButtonImage: Theme.imagePathCommon + "/onboarding/btn/primary.png"
  readonly property string secondaryButtonImage: Theme.imagePathCommon + "/onboarding/btn/secondary.png"

  readonly property string themeButtonSelectedImage: obImagePath + "/theme-selected.png"
  readonly property string themeButtonDeselectedImage: obImagePath + "/theme-deselected.png"
  readonly property string themeButtonTextColor: dark ? "#fff" : "#323642"
  readonly property string themePreviewImage: obImagePath + "/preview.png"

  readonly property string customizeImage: obImagePath + "/preview_customization.png"
  readonly property string customizeShadowImage: obImagePath + "/shadow-customize.png"
  readonly property string platformsImage: obImagePath + "/content-platforms.png"


  readonly property color primaryButtonTextColor: "#fff"
  readonly property color secondaryButtonTextColor: "#4CB649"

  readonly property color dropShadowColor: dark ? Qt.rgba(0,0,0,0.5) : "#22252E40"
  readonly property color themeSelectorDropShadow: dark ? Qt.rgba(0,0,0,0.4) : "#22252E20"
  readonly property string themeSelectorDropShadowImage: obImagePath + "/shadow-theme.png"

  // Footer/navigation
  readonly property color skipTourColor: primaryColor
  readonly property string nextButtonImage: Theme.imagePathCommon + "/onboarding/btn/next.png"

  readonly property string helpUsImproveImage: Theme.imagePathCommon + "/onboarding/checklist-icon.png"
}
