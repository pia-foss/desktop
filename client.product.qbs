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

import qbs
import qbs.Environment
import qbs.File
import qbs.FileInfo
import qbs.TextFile
import "application.item.qbs" as PiaApplication
import "pia_util.js" as PiaUtil

PiaApplication {
  name: "client"
  targetName: qbs.targetOS.contains("macos") ? productName : project.brandCode + "-client"

  sourceDirectories: base.concat(["client/src", "client/src/nativeacc"])

  type: ["application", "translations.rcc"]
  consoleApplication: false
  includeCommon: false  // common is linked in via clientlib

  Depends { name: "translations" }
  Depends { name: "clientlib" }
  Depends { name: "Qt.core" }
  Depends { name: "Qt.quick" }
  Depends { name: "Qt.gui" }
  // Qt.widgets is needed only for Qt tray icon implementation; we have
  // native implementations for Mac and Windows
  Depends {
    condition: qbs.targetOS.contains("linux")
    name: "Qt.widgets"
  }
  // Qt.*extras dependencies contain platform-specific utility functions
  Depends {
    condition: qbs.targetOS.contains("windows")
    name: "Qt.winextras"
  }
  Depends {
    condition: qbs.targetOS.contains("macos")
    name: "Qt.macextras"
  }
  Depends { name: "Qt.quickcontrols2" }

  bundle.isBundle: qbs.targetOS.contains("macos")

  // Enable LSUIElement in the Info.plist for the client.  This does several
  // things:
  // - Doesn't give the app a dock icon
  // - Allows the app to appear on top of a fullscreen window.  (Without this,
  //   revealing the menu bar over a fullscreen app and clicking the tray icon
  //   causes a space switch away from the fullscreen app, even with the
  //   CanJoinAllSpaces flag set on the dashboard.)
  // - Allows the app to activate itself on a monitor that isn't the active
  //   space.  (Without this, clicking the tray icon on such a monitor will
  //   place the dashboard correctly and try to raise it, but might not be
  //   able to raise it above the active app on that dashboard.  This seems to
  //   be impacted by the presence of the dev tools window - if it's not on
  //   the monitor that the dashboard ends up on, OS X may activate its space
  //   instead, which prevents the dashboard from being the top window on its
  //   space.)
  bundle.infoPlist: {
    // SMPrivilegedExecutables is built dynamically; key value depends on brand
    var smPrivExec = {}
    var certCN = Environment.getEnv("PIA_CODESIGN_CERT")
    // Signing requirements for install helper:
    // - corresponding bundle identifier
    // - signed using cert with same common name
    // - same build version
    // This corresponds to similar requirements in the helper's Info.plist and
    // in the signing requirements enforced by the helper at runtime.
    smPrivExec[project.brandIdentifier + ".installhelper"] =
      "identifier " + project.brandIdentifier + ".installhelper " +
      "and certificate leaf[subject.CN] = \"" + certCN + "\" " +
      "and info [" + project.brandIdentifier + ".version] = \"" + project.semanticVersion + "\""

    var props = {
      "LSUIElement": true,
      "CFBundleIconFile": "app.icns",
      "NSSupportsAutomaticGraphicsSwitching" : true,
      "SMPrivilegedExecutables": smPrivExec
    }

    // Include the complete version in a custom key, used by install helper.
    // Apple recommends using a product-specific prefix to avoid collisions with
    // standard properties.
    props[project.brandIdentifier + ".version"] = project.semanticVersion

    return props
  }
  // Copy over extra resources for the macOS bundle. The `icns file is used for
  // application icon
  bundle.resources: [
    "brands/" + project.brandCode + "/icons/app.icns"
  ]

  // Additional import path used to resolve QML modules in Qt Creator's code model
  property pathList qmlImportPaths: []

  Qt.core.resourceSourceBase: "client/res"
  windowsSources:base.concat("brands/" + project.brandCode + "/brand_client.rc")

  files: sources

  Group {
    name: "resources"
    fileTags: ["qt.core.resource_data"]
    files: [
      "client/res/**/*",
      "brands/" + project.brandCode + "/img/**/*",
      "brands/" + project.brandCode + "/gen_res/img/**/*"
    ]
    excludeFiles: [
      "**/.DS_Store",
      "**/*.qrc",
      "**/*.svg",
      "**/*.sh",
      "**/*.autosave",
      "**/*.otf",
      "**/RobotoCondensed-*.ttf",
      "**/Roboto-*Italic.ttf",
      "**/Roboto-Black.ttf",
      "**/Roboto-Medium.ttf",
      "**/Roboto-Thin.ttf",
    ]
  }

  Group {
    fileTagsFilter: ["bundle.content", "application"]
    // The client executable doesn't get "application.macdeploy" because the
    // bundle's startup executable is automatically included by macdeployqt.
    fileTags: ["application.windeploy"]
    qbs.install: true
    qbs.installDir: qbs.targetOS.contains("linux") ? "bin" : ""
    qbs.installSourceBase: product.buildDirectory
  }

  Properties {
    condition: qbs.targetOS.contains("windows")
    cpp.linkerFlags: outer.concat([
      "/MANIFESTINPUT:" + path + "/client/src/win/res/dpiManifest.xml"
    ])
    cpp.generateManifestFile: false
  }
  Properties {
    condition: qbs.targetOS.contains("macos")
    cpp.includePaths: outer.concat([path + "/extras/installer/mac/helper/"])
  }

  FileTagger {
    fileTags: ["required-symbols"]
    patterns: ["*.pdb"]
  }
  Group {
    fileTagsFilter: ['required-symbols']
    qbs.install: qbs.targetOS.contains("windows")
    qbs.installDir: ""
  }

  Group {
    name: "markdown"
    fileTags: ["qt.core.resource_data"]
    files: ["CHANGELOG.md", "BETA_AGREEMENT.md"]
    Qt.core.resourceSourceBase: ""
  }
}
