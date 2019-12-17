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
  name: "cli"
  targetName: project.brandCode + "ctl"

  sourceDirectories: base.concat(["cli/src"])

  type: ["application"]
  consoleApplication: true
  includeCommon: false  // common is linked in via clientlib

  Depends { name: "Qt.core" }
  Depends { name: "clientlib" }

  Group {
    fileTagsFilter: ["application"]
    // The CLI doesn't get application.windeploy, for some reason this prevents
    // windeployqt from deploying any QML dependencies.  The CLI doesn't have
    // any unique dependencies of its own, so this is a reasonable workaround.
    fileTags: ["application.macdeploy"]
    qbs.install: true
    qbs.installDir: {
      if(qbs.targetOS.contains("linux"))
        return "bin"
      if(qbs.targetOS.contains("macos"))
        return project.productName + ".app/Contents/MacOS/"
      return ""
    }
    qbs.installPrefix: ""
  }
}
