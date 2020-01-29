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
import qbs.TextFile
import "application.item.qbs" as PiaApplication
import "pia_util.js" as PiaUtil

PiaApplication {
  name: "daemon"
  targetName: qbs.targetOS.contains("windows") ? project.brandCode + "-service" : project.brandCode + "-daemon"

  sourceDirectories: base.concat(["daemon/src"])

  files: base.uniqueConcat([ "daemon/res/daemon.qrc" ])

  consoleApplication: true
  includeCrashReporting: true

  // Changes to dependencies also need to be reflected in all-tests-lib
  // and the Linux build script (addQtLib lines)
  Depends { name: "Qt.xml"; condition: qbs.targetOS.contains("windows") }

  cpp.defines: base.concat(["PIA_DAEMON"])
  type: base.concat(["dep_bins_branded"]).concat(qbs.targetOS.contains("macos") ? ["macos_pf_processed"] : [])
  Group {
    name: "macos/pf"
    files: "daemon/res/pf/*"
    fileTags: "macos_pf"
  }
  Rule {
    inputs: ["macos_pf"]
    Artifact {
      filePath: product.destinationDirectory + "/" + project.brandIdentifier + "." + input.fileName
      fileTags: "macos_pf_processed"
    }
    prepare: {
      var edit = new JavaScriptCommand()
      edit.description = "generating " + output.fileName
      edit.highlight = "filegen"
      edit.sourceCode = function() {
        PiaUtil.generateBranded(input.filePath, output.filePath, project)
      }
      return [edit]
    }
  }
  Group {
    name: "macos/pf-processed"
    condition: qbs.targetOS.contains("macos")
    fileTagsFilter: "macos_pf_processed"
    qbs.install: true
    qbs.installDir: project.productName + ".app/Contents/Resources/pf/"
    qbs.installPrefix: ""
  }
  // Dependency binaries from external build projects
  Group {
    name: "dep_bins"
    files: {
      var depPaths = ["deps/openvpn/", "deps/hnsd/", "deps/shadowsocks/"];
      var platformDir
      if (qbs.targetOS.contains("windows")) {
        platformDir = "win/";
      } else if (qbs.targetOS.contains("macos")) {
        platformDir = "mac/";
      } else if (qbs.targetOS.contains("linux")) {
        platformDir = "linux/";
      }
      platformDir += qbs.architecture + "/*";
      return PiaUtil.pathCombine(depPaths, platformDir)
    }
    fileTags: "dep_files"
  }
  Rule {
    inputs: ["dep_files"]
    outputArtifacts: {
      var brandedName = input.fileName.replace("pia", project.brandCode)
      // On Linux, we need to install .so files to lib/
      var tag = input.fileName.contains(".so") ? "dep_libs_branded" : "dep_bins_branded"
      return [{filePath: product.destinationDirectory + "/" + brandedName, fileTags: [tag]}]
    }
    outputFileTags: ["dep_bins_branded", "dep_libs_branded"]
    prepare: {
      return [PiaUtil.makeCopyCommand(input.filePath, output.filePath)]
    }
  }
  Group {
    name: "dep_bins_branded"
    fileTagsFilter: "dep_bins_branded"
    qbs.install: true
    qbs.installDir: {
      if(qbs.targetOS.contains("macos"))
        return project.productName + ".app/Contents/MacOS/"
      if(qbs.targetOS.contains("linux"))
        return "bin/"
      return ""
    }
    qbs.installPrefix: ""
  }
  Group {
    name: "dep_libs_branded"
    fileTagsFilter: "dep_libs_branded"
    qbs.install: true
    qbs.installDir: {
      if(qbs.targetOS.contains("macos"))
        return project.productName + ".app/Contents/MacOS/"
      if(qbs.targetOS.contains("linux"))
        return "lib/"
      return ""
    }
    qbs.installPrefix: ""
  }
  Group {
    name: "tap"
    condition: qbs.targetOS.contains("windows")
    qbs.install: true;
    files: "deps/tap/win/" + qbs.architecture + "/win*/**"
    qbs.installSourceBase: "deps/tap/win/" + qbs.architecture + "/"
    qbs.installDir: "tap/"
  }
  Group {
    name: "wfp_callout"
    condition: qbs.targetOS.contains("windows")
    qbs.install: true
    files: "deps/wfp_callout/win/" + qbs.architecture + "/**"
    qbs.installSourceBase: "deps/wfp_callout/win/" + qbs.architecture + "/"
    qbs.installDir: "wfp_callout/"
  }
  Group {
    name: "openvpn-updown"
    condition: qbs.targetOS.contains("windows")
    qbs.install: true
    files: ["extras/openvpn/win/openvpn_updown.bat"]
    qbs.installSourceBase: "extras/openvpn/win"
    qbs.installDir: ""
  }
  Group {
    name: "downloaded"
    files: ["daemon/res/json/*.json"]
    qbs.install: true
    qbs.installDir: qbs.targetOS.contains("windows") ? "" : qbs.targetOS.contains("macos") ? project.productName + ".app/Contents/Resources/" : "share/"
  }

  Group {
    fileTagsFilter: "application"
    fileTags: ["application.windeploy", "application.macdeploy"]
    qbs.install: true
    qbs.installDir: qbs.targetOS.contains("macos") ? project.productName + ".app/Contents/MacOS/" : qbs.targetOS.contains("windows") ? "" : "bin/"
  }

  Properties {
    condition: qbs.toolchain.contains('msvc')
    cpp.linkerFlags: outer.concat([ "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'" ]) // require root to run
  }
}
