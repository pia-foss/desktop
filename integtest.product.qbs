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
import qbs.File
import qbs.FileInfo
import qbs.TextFile
import "application.item.qbs" as PiaApplication
import "project.item.qbs" as PiaProject
import "pia_util.js" as PiaUtil

PiaProject {
  name: project.brandCode + "-integtest"
  // qbs doesn't have any way to describe multiple installable targets (the
  // install properties are global).  Do something similar manually by staging
  // in a directory for integration tests.
  readonly property string stageRoot: FileInfo.joinPaths(project.buildDirectory, "integtest-root")
  readonly property string archiveDirName: qbs.targetOS.contains("macos") ? "" : name

  // Integration test executable
  // Integration tests are run on an installed PIA client.  The test executable is
  // not deployed with the PIA client, so the procedure to run the tests is:
  // * install PIA
  // * copy the integration test executable artifact to the PIA installation manually
  // * start the PIA graphical client and log in
  // * run the integration test executable
  PiaApplication {
    name: "integtest-executable"
    targetName: project.name

    sourceDirectories: base.concat(["integtest/src"])
    consoleApplication: true
    includeCommon: false // linked in via clientlib

    Depends { name: "Qt.core" }
    Depends { name: "Qt.testlib" }
    Depends { name: "clientlib" }

    bundle.isBundle: qbs.targetOS.contains("macos")

    // TODO - combined sources cause linkage issues with some names in anonymous
    // namespaces, see if this actually saves much build time overall
    cpp.combineCSources: false
    cpp.combineCxxSources: false

    Group {
      fileTagsFilter: ["application"]
      // Include in windeployqt, but not macdeployqt (includes default
      // executable automatically)
      fileTags: ["integtest-stage", "application.windeploy"]
    }

    // OpenSSL dependencies
    Group {
      property string platformDir: {
        if(qbs.targetOS.contains("windows"))
          return "win"
        if(qbs.targetOS.contains("macos"))
          return "mac"
        if(qbs.targetOS.contains("linux"))
          return "linux"
      }
      files: "deps/openvpn/" + platformDir + "/" + qbs.architecture + "/lib*"
      fileTags: ["integtest-stage", "dynamiclibrary"]
    }

    // Linux Qt library dependencies (there's no deploy tool for Linux, so the
    // dependencies are hard-coded)
    Group {
      condition: qbs.targetOS.contains("linux")
      files: [
        "libicudata.so.56",
        "libicui18n.so.56",
        "libicuuc.so.56",
        "libQt5Core.so.5",
        "libQt5Network.so.5",
        "libQt5Test.so.5"
      ].map(function(path){return FileInfo.joinPaths(Qt.core.libPath, path)})
      fileTags: ["integtest-stage", "dynamiclibrary"]
    }
  }

  // Stage artifacts for integtest deployment - similar to the copy that qbs
  // would do for installable artifacts
  Product {
    name: "integtest-stage"
    type: ["integtest-staged"]

    Depends { name: "integtest-executable" }
    Depends { name: "clientlib" }
    // Ship the MSVC/UCRT runtime dependencies for Windows
    Depends { name: "runtime"; condition: qbs.toolchain.contains("msvc") }

    Rule {
      // Include installable artifacts from clientlib dependency as well as the
      // integtest-specific artifacts
      inputsFromDependencies: ["installable", "integtest-stage"]
      outputFileTags: ["integtest-staged", "integtest-staged.windeploy", "integtest-staged.macdeploy"]
      // We have to use the outputArtifacts property with a closure rather than
      // Artifact objects, because the output file tags depend on the input
      // file tags.
      outputArtifacts: {
        // Determine file tags
        var tags = ["integtest-staged"]
        // If the source artifact was tagged for windeployqt or macdeployqt,
        // tag the staged artifact
        function propDeployTag(suffix) {
          for(var i=0; i<input.fileTags.length; ++i) {
            if(input.fileTags[i].endsWith(suffix)) {
              tags.push("integtest-staged" + suffix)
              break
            }
          }
        }
        propDeployTag(".windeploy")
        propDeployTag(".macdeploy")

        // Figure out where this file goes.  Using something like
        // installDir/installSourceBase would be more robust than figuring it
        // out based on artifact type, but we can't attach arbitrary
        // properties to the artifacts
        var stageSubdir = project.archiveDirName
        // On Mac, preserve the bundle structure for anything in the integtest
        // bundle, otherwise put bins in Contents/MacOS
        if(product.qbs.targetOS.contains("macos")) {
          var pathComps = input.filePath.split('/')
          var bundleIdx = pathComps.indexOf(project.name + ".app")
          if(bundleIdx >= 0) {
            // Get just the relative path starting from the bundle root and
            // excluding the file name
            stageSubdir += "/" + pathComps.slice(bundleIdx, -1).join('/')
          }
          else {
            // Otherwise assume it goes into Contents/MacOS
            stageSubdir += "/" + project.name + ".app/Contents/MacOS"
          }
        }
        // On Linux, stage bins and libs to appropriate directories
        else if(product.qbs.targetOS.contains("linux")) {
          if(input.fileTags.contains("application"))
            stageSubdir += "/bin"
          else if(input.fileTags.contains("dynamiclibrary") || input.fileTags.contains("dynamiclibrary_symlink"))
            stageSubdir += "/lib"
          else
            console.warn("Unexpected stage artifact: " + input.filePath + "->" + JSON.stringify(input.fileTags))
        }
        // (For Windows everything just goes into the stage root)
        var path = FileInfo.joinPaths(project.stageRoot, stageSubdir, input.fileName)

        return [{fileTags: tags, filePath: path}]
      }
      prepare: {
        // Dereference symlinks on Linux only (for the Qt library dependencies)
        if(product.qbs.targetOS.contains("linux"))
          return PiaUtil.makeCopyDerefCommands(product.qbs.targetOS, input.filePath, output.filePath)
        else
          return [PiaUtil.makeCopyCommand(input.filePath, output.filePath)]
      }
    }
  }

  Product {
    name: "integtest-dist"

    type: ["archiver.archive"]

    Depends { name: "integtest-stage" }
    Depends { name: "archiver" }
    Depends { name: "Qt.core" }

    Properties {
      condition: qbs.targetOS.contains("windows")
      archiver.flags: project.builtByQtCreator ? [ '-bso0', '-bsp0' ] : []
    }

    archiver.archiveBaseName: project.name
    archiver.type: "zip"
    archiver.compressionLevel: "9"
    archiver.workingDirectory: project.stageRoot

    Rule {
      condition: qbs.targetOS.contains("windows")
      multiplex: true
      inputsFromDependencies: ["integtest-staged"]
      Artifact {
        filePath: "integtest-payload.txt"
        fileTags: ["archiver.input-list"]
      }
      prepare: {
        var deploy = new JavaScriptCommand()
        deploy.description = "windeployqt";
        deploy.inputFilePaths = inputs["integtest-staged"].map(function(input){return input.filePath})
        deploy.outputFilePath = output.filePath
        deploy.installRoot = project.stageRoot
        // Deploy binaries marked for deployment (just excludes MSVC runtime
        // DLLs in this case)
        deploy.binaryFilePaths = inputs["integtest-staged"]
          .filter(function(input){return input.fileTags.contains("integtest-staged.windeploy")})
          .map(function(input){return input.filePath})
        deploy.sourceCode = function() {
          PiaUtil.winDeploy([], binaryFilePaths, product,
                            inputFilePaths, outputFilePath, installRoot)
        }

        return [deploy]
      }
    }

    Rule {
      condition: qbs.targetOS.contains("macos")
      multiplex: true
      inputsFromDependencies: ["integtest-staged"]
      Artifact {
        filePath: "integtest-payload.txt"
        fileTags: ["archiver.input-list"]
      }
      prepare: {
        var commands = []
        var appBundle = FileInfo.joinPaths(project.stageRoot, project.name + ".app")
        // Run macdeployqt if enabled
        if(project.macdeployqt) {
          var deployBins = []
          for(var i=0; i<inputs["integtest-staged"].length; ++i) {
            var input = inputs["integtest-staged"][i]
            if(input.fileTags.contains("integtest-staged.macdeploy"))
              deployBins.push(FileInfo.joinPaths(appBundle, "Contents/MacOS", input.fileName))
          }

          commands.push(PiaUtil.makeMacDeployCommand(product, appBundle, deployBins, []))
        }

        // Recursively list files in the stage root (include any files added by
        // macdeployqt) and write the archiver input list
        var listFiles = new JavaScriptCommand()
        listFiles.silent = true
        listFiles.outputFilePath = output.filePath
        listFiles.stageRoot = project.stageRoot
        listFiles.bundlePath = FileInfo.joinPaths(project.stageRoot, project.name + ".app")
        listFiles.sourceCode = function(){
          var archiveList = new TextFile(outputFilePath, TextFile.WriteOnly)
          function listDir(dirPath) {
            // List the files and add them to the output
            File.directoryEntries(dirPath, File.Files).forEach(function(path){
              var completeFilePath = FileInfo.joinPaths(dirPath, path)
              archiveList.writeLine(FileInfo.relativePath(stageRoot, completeFilePath))
            })
            // List the directories and recurse
            File.directoryEntries(dirPath, File.Dirs|File.NoDotAndDotDot).forEach(function(path){
              listDir(FileInfo.joinPaths(dirPath, path))
            })
          }
          listDir(bundlePath)
        }
        commands.push(listFiles)
        return commands
      }
    }

    Rule {
      condition: qbs.targetOS.contains("linux")
      multiplex: true
      inputsFromDependencies: ["integtest-staged"]
      Artifact {
        filePath: "integtest-payload.txt"
        fileTags: ["archiver.input-list"]
      }
      prepare: {
        var list = new JavaScriptCommand()
        list.silent = true
        list.inputFilePaths = inputs["integtest-staged"].map(function(input){return input.filePath})
        list.outputFilePath = output.filePath
        list.installRoot = project.stageRoot
        list.sourceCode = function() {
          // On Linux, the dependencies are hard-coded, so all we do is list the
          // files to archive.
          var inputsRel = inputFilePaths.map(function(path) {return FileInfo.relativePath(installRoot, path)})
          PiaUtil.writeTextFile(outputFilePath, inputsRel.join('\n'))
        }
        return [list]
      }
    }
  }
}
