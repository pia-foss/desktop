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
import qbs.Process
import qbs.TextFile
import "project.item.qbs" as PiaProject
import "application.item.qbs" as PiaApplication
import "test.item.qbs" as Test
import "pia_util.js" as PiaUtil

PiaProject {
  name: "tests"

  readonly property bool useLlvmProf: qbs.toolchain.contains('clang')

  AutotestRunner {
    name: "all-tests"
    Depends { name: "Qt.core" }
    Depends {
      productTypes: "autotest"
      limitToSubProject: product.limitToSubProject
    }

    type: base.concat(useLlvmProf ? ["llvm_profdata_raw"] : [])
    Properties {
      condition: qbs.targetOS.contains("linux")
      environment: outer.concat([ "LD_LIBRARY_PATH=" + Qt.core.libPath ])
    }
    Properties {
      condition: qbs.targetOS.contains("windows")
      environment: outer.concat([ "PATH=" + Qt.core.binPath + ";" + Environment.getEnv("PATH") ])
    }

    // These artifacts are actually created by running the tests themselves, but
    // we create a pretend rule to "generate" them so Qbs knows about them
    // (there doesn't seem to be a better way)
    Rule {
      inputsFromDependencies: ['application']
      // This auxiliaryInputs tag causes this rule to depend on the rule that
      // actually runs the autotests, and it seems to work even though that rule
      // does not actually produce any artifacts.
      auxiliaryInputs: "autotest-result"
      Artifact {
        filePath: FileInfo.cleanPath(FileInfo.joinPaths(input.filePath, "../default.profraw"))
        fileTags: "llvm_profdata_raw"
      }
      prepare: {
        // Just update the mtime - the file should already have been created by
        // running the tests
        var touch = new Command("touch", [output.filePath])
        touch.description = "Generated " + output.filePath
        touch.highlight = "filegen"
        return [touch]
      }
    }
  }

  PiaApplication {
    name: "all-tests-lib"
    type: ["staticlibrary"]
    builtByDefault: false
    Depends { name: "Qt.xml"; condition: qbs.targetOS.contains("windows") }
    Depends { name: "Qt.quick" }
    Depends {
      condition: qbs.targetOS.contains("linux")
      name: "Qt.widgets"
    }
    Depends {
      condition: qbs.targetOS.contains("windows")
      name: "Qt.winextras"
    }
    Depends {
      condition: qbs.targetOS.contains("macos")
      name: "Qt.macextras"
    }
    sourceDirectories: base.concat(["clientlib/src", "client/src", "client/src/nativeacc", "daemon/src", "deps/embeddable-wg-library/src", "tests/src"])
    cpp.defines: base.concat(["PIA_CLIENT", "PIA_DAEMON", "UNIT_TEST"])
    Properties {
      condition: qbs.toolchain.contains('msvc')
      cpp.linkerFlags: outer.concat(["/IGNORE:4099"])
    }
    Properties {
      condition: qbs.targetOS.contains("macos")
      cpp.includePaths: outer.concat([path + "/extras/installer/mac/helper/"])
    }
    Properties {
      condition: useLlvmProf
      cpp.driverFlags: outer.concat(["-fprofile-instr-generate", "-fcoverage-mapping"])
    }
    Export {
      Depends {name: "cpp"}
      Properties {
        condition: useLlvmProf
        // These are set as driver flags because they also have to be passed to
        // Clang when linking.
        cpp.driverFlags: base.concat(["-fprofile-instr-generate", "-fcoverage-mapping"])
      }
    }
  }

  Test { testName: "apiclient" }
  Test { testName: "check" }
  Test { testName: "json" }
  Test { testName: "jsonrefresher" }
  Test { testName: "jsonrpc" }
  Test { testName: "latencytracker" }
  Test { testName: "localsockets" }
  Test { testName: "nodelist" }
  Test { testName: "nullable_t" }
  Test { testName: "path" }
  Test { testName: "portforwarder" }
  Test { testName: "raii" }
  Test { testName: "semversion" }
  Test { testName: "settings" }
  Test { testName: "tasks" }
  Test { testName: "updatedownloader" }
  Test { testName: "wireguarduapi" }

  // Platform-specific tests - only built and run on relevant platforms.
  PiaProject {
    name: "tests-win"
    condition: qbs.targetOS.contains("windows")

    Test { testName: "wfp_filters" }
  }

  // Test analysis results
  Product {
    name: "llvm-code-coverage"
    // Require clang 6.0 to process the coverage output.
    //
    // We can't generate coverage data on Ubuntu 16.04 - its LLVM is too old
    // (llvm-cov does not support "export", and it seems to fail to load the
    // merged output for some reason).
    //
    // It's possible this might actually work with some versions of clang
    // between 3.8 and 6.0, but they haven't been tested.
    condition: useLlvmProf && cpp.compilerVersionMajor >= 6

    Depends {name: "all-tests"}
    Depends {name: "cpp"} // Need cpp compiler path to find llvm bin path on Linux

    destinationDirectory: "llvm-code-coverage"
    type: ["llvm_profdata_merged", "llvm_codecov_listing", "llvm_codecov_report", "pia_codecov_summary"]
    builtByDefault: false

    // Any single unit test binary - see the llvm-cov rule
    readonly property string anyTestBuildDir: "test--check"
    readonly property string anyTestBinPath: qbs.targetOS.contains('macos') ? "test-check.app/Contents/MacOS/test-check" : "test-check"

    Probe {
      id: llvmToolPathProbe
      property string path
      readonly property var targetOS: product.qbs.targetOS
      readonly property string compilerPath: product.cpp.compilerPath

      configure: {
        if(targetOS.contains('macos')) {
          path = "/Library/Developer/CommandLineTools/usr/bin"
          found = true
        }
        else if(targetOS.contains('linux')) {
          // Use the llvm-profdata and llvm-cov from the same LLVM version as the
          // compiler
          function readlink(path) {
            var proc = new Process()
            proc.exec("readlink", ["-en", path], true)
            return proc.readStdOut()
          }

          // Get the canonical path to clang; typically /usr/lib/llvm-X.Y/bin/clang
          var canonicalClang = readlink(compilerPath)
          path = FileInfo.path(canonicalClang)
          found = true
        }
      }
    }

    readonly property string llvmToolPath: llvmToolPathProbe.path

    // Process and merge the profile data with "llvm-profdata merge"
    Rule {
      multiplex: true
      inputsFromDependencies: ['llvm_profdata_raw']
      requiresInputs: false
      Artifact {
        filePath: FileInfo.joinPaths(product.destinationDirectory, "unittest.profdata")
        fileTags: "llvm_profdata_merged"
      }
      prepare: {
        var rawData = inputs.llvm_profdata_raw.map(function(f){return FileInfo.joinPaths(FileInfo.path(f.filePath), "default.profraw")})
        var merge = new Command(FileInfo.joinPaths(product.llvmToolPath, "llvm-profdata"),
                                ["merge"].concat(rawData, ["-o", output.filePath]))
        merge.description = "Merging code coverage data"
        merge.highlight = "filegen"
        return [merge]
      }
    }

    // Generate the detailed coverage listing and report
    Rule {
      multiplex: true
      inputs: ['llvm_profdata_merged']
      Artifact {
        filePath: FileInfo.joinPaths(product.destinationDirectory, "unittest_listing.txt")
        fileTags: "llvm_codecov_listing"
      }
      Artifact {
        filePath: FileInfo.joinPaths(product.destinationDirectory, "unittest_report.txt")
        fileTags: "llvm_codecov_report"
      }
      Artifact {
        filePath: FileInfo.joinPaths(product.destinationDirectory, "unittest_coverage.json")
        fileTags: "llvm_codecov_json"
      }
      prepare: {
        // This is a bit of a hack too...
        //
        // Ideally we'd depend on all-tests-lib and tell llvm-cov to generate a
        // report for that here (we do not actually want coverage for
        // test-specific code).
        //
        // However, this doesn't seem to work for whatever reason.  Instead,
        // pass any arbitrary unit test executable.  The coverage report does
        // include coverage for all tests over all-tests-lib; it'll also include
        // this unit test's specific files though, which we ignore later.  It
        // also will generate a warning about conflicting data due to each
        // test's main() functions appearing in the profile data.
        //var buildDir = FileInfo.joinPaths(product.destinationDirectory, "..")
        var outputDirs = File.directoryEntries(project.buildDirectory, File.Dirs)

        var anyUnitTestDirIdx = 0
        while(anyUnitTestDirIdx < outputDirs.length) {
          if(outputDirs[anyUnitTestDirIdx].startsWith(product.anyTestBuildDir))
            break
          ++anyUnitTestDirIdx
        }
        var anyUnitTest = FileInfo.joinPaths(project.buildDirectory,
                                             outputDirs[anyUnitTestDirIdx],
                                             product.anyTestBinPath)

        var llvmCov = FileInfo.joinPaths(product.llvmToolPath, "llvm-cov")
        var list = new Command(llvmCov, ["show", anyUnitTest, "-instr-profile=" + input.filePath])
        list.stdoutFilePath = outputs.llvm_codecov_listing[0].filePath
        list.description = "Generating code coverage listing"
        list.highlight = "filegen"

        var report = new Command(llvmCov, ["report", anyUnitTest, "-instr-profile=" + input.filePath])
        report.stdoutFilePath = outputs.llvm_codecov_report[0].filePath
        report.description = "Generating code coverage report"
        report.highlight = "filegen"

        var exportJson = new Command(llvmCov, ["export", "-format=text", anyUnitTest, "-instr-profile=" + input.filePath])
        exportJson.stdoutFilePath = outputs.llvm_codecov_json[0].filePath
        exportJson.description = "Generating code coverage JSON export"
        exportJson.highlight = "filegen"

        return [list, report, exportJson]
      }
    }

    // Read the exported JSON and create the PIA-specific code coverage summary
    Rule {
      inputs: ['llvm_codecov_json']
      Artifact {
        filePath: FileInfo.joinPaths(product.destinationDirectory, "pia_unittest_summary.json")
        fileTags: "pia_codecov_summary"
      }
      prepare: {
        var summary = new JavaScriptCommand();

        summary.description = "Generating code coverage summary"
        summary.highlight = "filegen"
        summary.sourceCode = function() {
          var covObj = JSON.parse(PiaUtil.readTextFile(input.filePath))

          if(covObj.data.length !== 1) {
            var msg = "Expected one coverage data object, found " + covObj.data.length
            console.error()
            throw new Error(msg)
          }

          var srcDirs = [
            FileInfo.joinPaths(product.sourceDirectory, "client/src"),
            FileInfo.joinPaths(product.sourceDirectory, "common/src"),
            FileInfo.joinPaths(product.sourceDirectory, "daemon/src"),
          ]
          function genPlatformSrcDirs(platform) {
            // Include a trailing '/' in the directory name so file names like
            // "../src/windowscaler.cpp" don't look like 'win' sources
            return srcDirs.map(function(f){return FileInfo.joinPaths(f, platform) + "/"})
          }

          var platformSrcDirs = {
            'mac': genPlatformSrcDirs('mac'),
            'linux': genPlatformSrcDirs('linux'),
            'win': genPlatformSrcDirs('win'),
            'posix': genPlatformSrcDirs('posix')
          }

          var lineCounts = {
            'mac': {},
            'linux': {},
            'win': {},
            'posix': {},
            'common': {}
          }
          for(var p in lineCounts) {
            lineCounts[p].total = 0
            lineCounts[p].covered = 0
          }

          function startsWithAny(string, prefixes) {
            for(var i=0; i<prefixes.length; ++i) {
              if(string.startsWith(prefixes[i]))
                return true
            }
            return false
          }

          // Determine which platform a file path corresponds to (including
          // 'common' if it's PIA code but not platform-specific), or return
          // undefined if it's not code that we cover for unit tests (Qt, moc,
          // or test code)
          function findPlatform(filePath) {
            for(var p in platformSrcDirs) {
              if(startsWithAny(filePath, platformSrcDirs[p]))
                return p
            }
            if(startsWithAny(filePath, srcDirs))
              return 'common'
            // otherwise, undefined
          }

          var fileData = covObj.data[0].files
          for(var i=0; i<fileData.length; ++i) {
            var filePlatform = findPlatform(fileData[i].filename)
            if(filePlatform) {
              lineCounts[filePlatform].total += fileData[i].summary.lines.count
              lineCounts[filePlatform].covered += fileData[i].summary.lines.covered
            }
          }

          console.info("Unit test code coverage:")
          for(var linePlatform in lineCounts) {
            var percent = 0
            var covered = lineCounts[linePlatform].covered
            var total = lineCounts[linePlatform].total
            if(total > 0)
              percent = covered / total * 100
            var pctRound = Math.round(percent * 10) / 10;
            console.info("--> " + linePlatform + ": " + pctRound + "% (" + covered + "/" + total + ")")
          }

          PiaUtil.writeTextFile(output.filePath, JSON.stringify(lineCounts))
        }

        return [summary]
      }
    }
  }
}
