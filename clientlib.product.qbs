// Copyright (c) 2019 London Trust Media Incorporated
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
import "application.item.qbs" as PiaApplication
import "pia_util.js" as PiaUtil

PiaApplication {
  name: "clientlib"
  targetName: project.brandCode + "-clientlib"

  sourceDirectories: base.concat(["clientlib/src"])

  cpp.defines: base.concat(["DYNAMIC_CLIENTLIB", "BUILD_CLIENTLIB", "DYNAMIC_COMMON", "PIA_CLIENT"])
  type: ["dynamiclibrary", "dynamiclibrary_symlink"]
  bundle.isBundle: false

  // On Mac OS, link to this library using a path relative to the executable
  // (required by Gatekeeper).  (On Linux this is handled with cpp.rpaths, which
  // is set globally by the build script for installer builds.)
  cpp.sonamePrefix: qbs.targetOS.contains("macos") ? "@executable_path" : undefined

  includeCrashReporting: true

  Export {
    Depends { name: "cpp" }
    cpp.includePaths: product.sourceIncludePaths
    cpp.defines: base.concat(["DYNAMIC_CLIENTLIB", "DYNAMIC_COMMON", "PIA_CLIENT"])
  }

  readonly property string libInstallDir: {
    if(qbs.targetOS.contains("linux"))
      return "lib"
    if(qbs.targetOS.contains("macos"))
      return project.productName + ".app/Contents/MacOS/"
    return ""
  }

  Group {
    fileTagsFilter: ["dynamiclibrary", "dynamiclibrary_symlink"]
    fileTags: ["dynamiclibrary.macdeploy", "dynamiclibrary.windeploy"]
    qbs.install: true
    qbs.installDir: product.libInstallDir
    qbs.installPrefix: ""
  }

  Group {
    fileTagsFilter: ["dynamiclibrary_symlink"]
    qbs.install: true
    qbs.installDir: product.libInstallDir
    qbs.installPrefix: ""
  }
}
