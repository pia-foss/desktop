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
import qbs.Utilities

CppApplication {
  Depends { name: "brand-files" }

  name: "installer"
  targetName: "pia-windows-installer"
  condition: qbs.targetOS.contains("windows")

  property bool uninstaller: false

  consoleApplication: false
  cpp.cxxLanguageVersion: "c++17"
  cpp.minimumWindowsVersion: "6.1" // Windows 7
  cpp.windowsApiCharacterSet: "unicode"
  cpp.windowsApiFamily: "desktop"
  cpp.runtimeLibrary: "static"
  cpp.defines: [
    "UNICODE", "_UNICODE",
    "NTDDI_VERSION=NTDDI_WIN7",
    "_STATIC_CPPLIB",
    "PIA_PRODUCT_NAME=" + Utilities.cStringQuote(project.productName),
    "PIA_VERSION=" + Utilities.cStringQuote(project.semanticVersion)
  ]
  cpp.linkerFlags: [
    "/INCREMENTAL:NO",
    "/MANIFEST:EMBED",
    "/MANIFESTINPUT:" + path + "/common/res/manifest.xml",
    "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'",
    // Specify these as delay-loaded since they're not "known DLLs" (i.e. can be
    // subject to executable path lookup rules)
    "delayimp.lib",
    "/DELAYLOAD:newdev.dll",
    "/DELAYLOAD:userenv.dll",
  ]
  cpp.includePaths: [ path + "/extras/installer/win", path + "/deps/lzma/src", product.buildDirectory + "/brand" ]
  cpp.cFlags: [ "/wd4005" ]
  cpp.combineCSources: true
  cpp.combineCxxSources: true
  Properties {
    condition: qbs.buildVariant == "debug"
    cpp.defines: outer.concat([ "DEBUG", "_DEBUG" ])
  }
  Properties {
    condition: qbs.buildVariant == "release"
    cpp.defines: outer.concat([ "NDEBUG", "_NDEBUG", "RELEASE", "_RELEASE" ])
    cpp.cFlags: outer.concat([ "/GL", "/Gw", "/Gs-" ])
    cpp.cppFlags: outer.concat([ "/GL", "/Gw", "/Gs-" ])
    cpp.linkerFlags: outer.concat([ "/LTCG", "/OPT:ICF", "notelemetry.obj" ])
    cpp.discardUnusedData: true
    //cpp.optimization: "small"
    optimization: "small"
  }
  files: [
    "extras/installer/win/*.rc",
    "extras/installer/win/*.cpp",
    "extras/installer/win/*.h",
    "extras/installer/win/*.inl",
    "extras/installer/win/tasks/*.cpp",
    "extras/installer/win/tasks/*.h",
    "extras/installer/win/translations/*.rc",
    "brands/" + project.brandCode + "/brand_installer.rc"
  ]
  // Exclude ro.rc and ps.rc on non-debug builds, they're pseudotranslations
  excludeFiles: {
    if(qbs.buildVariant !== "debug")
      return ["extras/installer/win/translations/ro.rc",
              "extras/installer/win/translations/ps.rc"]
    return []
  }
  Group {
    name: "resources"
    files: [
      "extras/installer/win/close.bmp",
      "extras/installer/win/minimize.bmp",
      "brands/" + project.brandCode + "/installer/win/logo.bmp",
      "brands/" + project.brandCode + "/icons/win_installer.ico"
    ]
  }
}
