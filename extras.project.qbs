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
import qbs.ModUtils
import qbs.Process
import qbs.TextFile
import qbs.Utilities
import "winstaller.item.qbs" as WindowsInstaller
import "project.item.qbs" as PiaProject
import "application.item.qbs" as PiaApplication
import "pia_util.js" as PiaUtil
import "pia_onesky.js" as PiaOneSky

PiaProject {
  name: "extras"

  Product {
    name: "copyables"
    Group {
      name: "license"
      files: ["LICENSE.txt"]
      qbs.install: true
      qbs.installPrefix: ""
      qbs.installDir: qbs.targetOS.contains("macos") ? project.productName + ".app/Contents/Resources/" : qbs.targetOS.contains("linux") ? "share/" : ""
    }
    Group {
      name: "mac-kext-2"
      qbs.install: true;
      condition: qbs.targetOS.contains("macos")
      files: ["deps/split_tunnel/mac/PiaKext.kext/**/**"]
      qbs.installSourceBase: "deps/split_tunnel/mac/"
      qbs.installDir: project.productName + ".app/Contents/Resources/"
      qbs.installPrefix: ""
    }
  }

  Product {
    name: "version-files"
    destinationDirectory: "version"
    type: ["txt", "hpp"]

    // Generate version.txt for build scripts
    Rule {
      multiplex: true
      requiresInputs: false
      Artifact {
        filePath: product.destinationDirectory + "/version.txt"
        fileTags: ["txt"]
      }
      prepare: {
        var txt = new JavaScriptCommand();
        txt.description = "generating " + output.fileName;
        txt.highlight = "filegen";
        txt.sourceCode = function() {
          var f = new TextFile(output.filePath, TextFile.WriteOnly);
          try {
            f.writeLine(project.semanticVersion);
            f.writeLine(project.productName);
            f.writeLine(project.packageName);
            f.writeLine(project.timestamp);
          } finally {
            f.close();
          }
        };
        return [txt];
      }
    }

    // Generate version.h for source code
    Rule {
      multiplex: true
      requiresInputs: false
      Artifact {
        filePath: product.destinationDirectory + "/version.h"
        fileTags: ["hpp"]
      }
      prepare: {
        var hpp = new JavaScriptCommand();
        hpp.description = "generating " + output.fileName
        hpp.highlight = "codegen";
        hpp.sourceCode = function() {
          File.makePath(FileInfo.path(output.filePath));
          var f = new TextFile(output.filePath, TextFile.WriteOnly);
          try {
            f.writeLine("#ifndef VERSION_H");
            f.writeLine("#define VERSION_H");
            f.writeLine("#pragma once");
            f.writeLine("#define PIA_PRODUCT_NAME " + Utilities.cStringQuote(project.productName));
            f.writeLine("#define PIA_VERSION " + Utilities.cStringQuote(project.semanticVersion));
            var cert = Environment.getEnv("PIA_CODESIGN_CERT") ? Environment.getEnv("PIA_CODESIGN_CERT") : "Unknown"
            f.writeLine("#define PIA_CODESIGN_CERT " + Utilities.cStringQuote(cert));
            f.writeLine("#define RUBY_MIGRATION R\"(" + (Environment.getEnv("RUBY_MIGRATION") || "") + ")\"")

            // Windows-style four-part version number used in VERSIONINFO.
            // This can't completely encode a semantic version, so we use the
            // fourth field to express alpha/beta/GA:
            // * 0 - all alphas (or any non-beta prerelease)
            // * 1-99 - beta.1 through beta.99
            // * 100 - GA
            var winVerLast = "0"
            var betaVerMatch
            if(project.productPrerelease == "")
              winVerLast = "100"
            else if(betaVerMatch = project.productPrerelease.match(/^beta\.([0-9]?[0-9])$/))
              winVerLast = betaVerMatch[1]
            var winVer = project.productVersion.replace(/\./g, ',') + ',' + winVerLast
            // Write it both literally and as a string, the RC preprocessor
            // doesn't seem to support the stringification operator.
            // Note that Qbs doesn't seem to create a dependency on version.h
            // for the RC script, so changes in version require a full rebuild.
            f.writeLine("#define PIA_WIN_VERSION " + winVer)
            f.writeLine("#define PIA_WIN_VERSION_STR " + Utilities.cStringQuote(winVer))
            f.writeLine("#endif");
          } finally {
            f.close();
          }
        };
        return [hpp];
      }
    }

    // Export item to set up include directory etc.
    Export {
      Depends { name: "cpp" }
      cpp.includePaths: [project.buildDirectory + '/version']
    }
  }

  // Generate brand.txt and brand.h
  Product {
    name: "brand-files"
    destinationDirectory: "brand"
    type: ["txt", "hpp"]

    Group {
      fileTags: ["brand_json"]
      files: ["brands/" + project.brandCode + "/brandinfo.json"]
    }

    // Generate brand.txt for build scripts and brand.h for source code
    Rule {
      multiplex: true
      inputs: ["brand_json"]
      Artifact {
        filePath: product.destinationDirectory + "/brand.txt"
        fileTags: ["txt"]
      }
      Artifact {
        filePath: product.destinationDirectory + "/brand.h"
        fileTags: ["hpp"]
      }
      prepare: {
        var txt = new JavaScriptCommand();

        txt.description = "generating brand.txt and brand.h from " + inputs.brand_json[0].filePath;
        txt.highlight = "filegen";
        txt.sourceCode = function() {
          var filePath = inputs.brand_json[0].filePath;
          if(!File.exists(filePath)) {
            console.warn("Cannot find brand json path: " + filePath);
            return;
          }

          var fileContent = new TextFile(filePath).readAll();
          var brandParams = JSON.parse(fileContent);

          var f = new TextFile(outputs.txt[0].filePath, TextFile.WriteOnly);
          try {
            f.writeLine(project.brandName);
            f.writeLine(project.brandCode);
            f.writeLine(project.brandIdentifier);
          } finally {
            f.close();
          }

          File.makePath(FileInfo.path(outputs.hpp[0].filePath));
          var f = new TextFile(outputs.hpp[0].filePath, TextFile.WriteOnly);

          function writeStrDef(name, value) {
            f.writeLine("#define " + name + " " + Utilities.cStringQuote(value))
          }

          try {
            f.writeLine("#ifndef BRAND_H");
            f.writeLine("#define BRAND_H");
            f.writeLine("#pragma once");
            writeStrDef("BRAND_NAME", project.brandName)
            writeStrDef("BRAND_CODE", project.brandCode)
            writeStrDef("BRAND_SHORT_NAME", brandParams.shortName)
            writeStrDef("BRAND_IDENTIFIER", project.brandIdentifier)
            f.writeLine("#define BRAND_HAS_CLASSIC_TRAY " + (brandParams.brandHasClassicTray ? "1" : "0"))
            f.writeLine("#define BRAND_MIGRATES_LEGACY " + (project.brandCode === "pia" ? "1" : "0"))
            writeStrDef("BRAND_RELEASE_CHANNEL_GA", brandParams.brandReleaseChannelGA)
            writeStrDef("BRAND_RELEASE_CHANNEL_BETA", brandParams.brandReleaseChannelBeta)
            writeStrDef("BRAND_WINDOWS_PRODUCT_GUID", brandParams.windowsProductGuid)
            writeStrDef("BRAND_WINDOWS_SERVICE_NAME", brandParams.windowsServiceName)
            f.writeLine("#define BRAND_WINDOWS_WFP_PROVIDER " + brandParams.windowsWfpProvider)
            writeStrDef("BRAND_WINDOWS_WFP_PROVIDER_GUID", brandParams.windowsWfpProviderGuid)
            f.writeLine("#define BRAND_WINDOWS_WFP_SUBLAYER " + brandParams.windowsWfpSublayer)
            writeStrDef("BRAND_WINDOWS_WFP_SUBLAYER_GUID", brandParams.windowsWfpSublayerGuid)
            writeStrDef("BRAND_UPDATE_JSON_KEY_NAME", brandParams.updateJsonKeyName)
            writeStrDef("BRAND_LINUX_APP_NAME", brandParams.linuxAppName)
            writeStrDef("BRAND_PARAMS", JSON.stringify(brandParams))

            var updateApis = brandParams.brandReleaseUpdateUris
            // This was added to the brand kit, default to the old defaults
            if(!Array.isArray(updateApis) || updateApis.length < 1) {
              updateApis = [
                "https://www.privateinternetaccess.com/clients/desktop",
                "https://piaproxy.net/clients/desktop"
              ]
            }
            f.writeLine('#define BRAND_UPDATE_APIS QStringLiteral(R"(' + updateApis.join(')"), QStringLiteral(R\"(') + ')")')
            f.writeLine("#endif");
          } finally {
            f.close();
          }
        };

        return [txt];
      }
    }

    // Export item to set up include directory etc.
    Export {
      Depends { name: "cpp" }
      cpp.includePaths: [project.buildDirectory + '/brand']
    }
  }

  Product {
    name: "translations"
    type: ["translations.rcc", "translations.export"].concat(project.builtByQtCreator ? [] : ["archiver.archive"])
    destinationDirectory: project.buildDirectory + "/translations"

    Depends { name: "archiver" }
    Depends { name: "Qt.core" }
    Depends { name: "piabuildenv" }

    property stringList folders: ['common', 'client', 'daemon', 'extras/installer/win']
    property stringList extensions: ['cpp','h','inl','js','mm','qml','rc']

    archiver.type: "zip"
    archiver.workingDirectory: destinationDirectory

    // Probe the file system for all .ts files so we get an inspectable list
    Probe {
      id: ts
      property stringList files
      property string directory: tsInputPath
      property var lastModified: File.lastModified(directory)
      property bool excludeRoTs: qbs.buildVariant !== "debug"
      configure: {
        files = File.directoryEntries(directory, File.Files).filter(function(f) { return f.endsWith('.ts'); });
        // Exclude 'ro.ts' and 'ps.ts' from non-debug builds, these are pseudotranslations
        if(excludeRoTs)
          files = files.filter(function(f){return !f.endsWith('ro.ts') && !f.endsWith('ps.ts')})
        found = true;
      }
    }
    property string tsInputPath: path + '/client/ts'
    property stringList tsInputFiles: ts.files

    // Expose and tag the .ts files for our own rules
    Group {
      name: "translations"
      files: { return product.tsInputFiles.map(function(f) { return FileInfo.joinPaths(product.tsInputPath, f); }); }
      fileTags: ['ts.input']
    }
    Group {
      name: "sources"
      files: folders.reduce(function(a,f) { return a.concat(extensions.map(function(e) { return path + "/" + f + "/**/*." + e; })); }, [])
      fileTags: ['translatable']
      overrideTags: true
    }

    // Import and lupdate on the .ts files, saving the result out-of-source
    // (this way we can get compile warnings about new strings)
    // The ts files in source control come from OneSky, so we undo the export
    // process in this step.
    Rule {
      multiplex: true
      // All these inputs are just for dependency checking;
      // the rule itself runs on the entire source tree
      inputs: ['ts.input', 'translatable']
      outputFileTags: ['ts.output']
      outputArtifacts: {
        return product.tsInputFiles.map(function(f) {
          return {
            fileTags: ['ts.output'],
            filePath: FileInfo.joinPaths(product.destinationDirectory, f),
          };
        });
      }
      prepare: {
        var tsImport = new JavaScriptCommand();
        tsImport.silent = true;
        tsImport.sourceCode = function() {
          File.makePath(product.destinationDirectory);
          outputs['ts.output'].forEach(function(o) {
            var inputTsXml = PiaUtil.readTextFile(FileInfo.joinPaths(product.tsInputPath, o.fileName))
            PiaUtil.writeTextFile(o.filePath, PiaOneSky.tsImport(inputTsXml))
          });
        };
        var sourcePaths = product.folders.map(function (p) { return FileInfo.relativePath(product.destinationDirectory, FileInfo.joinPaths(product.sourceDirectory, p)); });
        // Use uiTr instead of qsTr - see ClientQmlContext::retranslate().
        // We _only_ accept uiTr() and ignore qsTr().  This ensures that using
        // qsTr() by mistake results in a string that does not translate at all
        // (instead of one that translates at startup but does not retranslate,
        // which is much harder to catch in testing).
        var fixedArgs = [ '-locations', 'absolute', '-extensions', product.extensions.join(','), '-tr-function-alias', 'qsTr=uiTr,qsTranslate=uiTranslate', '-disable-heuristic', 'sametext', '-disable-heuristic', 'similartext', '-disable-heuristic', 'number' ]
        var args = fixedArgs.concat(sourcePaths).concat([ '-ts' ]).concat(outputs['ts.output'].map(function(o) { return o.fileName; }));
        var lupdate = new Command(product.Qt.core.binPath + "/lupdate", args);
        lupdate.description = "lupdate *.ts";
        lupdate.highlight = "codegen";
        lupdate.workingDirectory = product.destinationDirectory;
        lupdate.stdoutFilterFunction = function(s) {
          // Trim uninteresting output
          s = s.replace(/Scanning directory '[^']*'...\r?\n?/g, '');
          // Suppress status updates unless string counts have changed
          return s.replace(/Updating '(?:[^/']*\/)*([^']*)'...\r?\n    Found (\d+) source text\(s\) \(0 new and \2 already existing\)\r?\n?/g, '');
        };
        // lupdate has a bug and doesn't support absolute paths, but regardless
        // we want the actual paths to be relative to the client/ts directory.
        var fix = new JavaScriptCommand();
        fix.silent = true;
        fix.sourceCode = function() {
          outputs['ts.output'].forEach(function(o) {
            var file = new TextFile(o.filePath, TextFile.ReadWrite);
            var text = file.readAll();
            text = text.replace(/<location filename="([^"]*)"/g, function(m,f) {
              var absolutePath = FileInfo.isAbsolutePath(f) ? f : FileInfo.cleanPath(FileInfo.joinPaths(FileInfo.path(o.filePath), f));
              return '<location filename="' + FileInfo.relativePath(product.destinationDirectory, absolutePath) + '"';
            });
            file.truncate();
            file.write(text);
            file.close();
          });
        };
        return [tsImport, lupdate, fix];
      }
    }
    // Run lrelease on the produced .ts files
    Rule {
      inputs: ['ts.output']
      Artifact {
        fileTags: ['qm']
        filePath: FileInfo.joinPaths(product.destinationDirectory, input.baseName + ".qm")
      }
      prepare: {
        var args = ['-silent', '-removeidentical', '-compress', input.filePath, '-qm', output.filePath];
        var lrelease = new Command(product.Qt.core.binPath + "/lrelease", args);
        lrelease.description = "lrelease " + input.fileName;
        lrelease.highlight = "compiler";
        return lrelease;
      }
    }
    // Embed all .qm files in a .qrc file
    Rule {
      multiplex: true
      inputs: ['qm']
      Artifact {
        fileTags: ['translations.qrc']
        filePath: FileInfo.joinPaths(product.destinationDirectory, "translations.qrc")
      }
    }

    // Compile translations into its own rcc file (can revisit and try to
    // compile this into the main executable if we can figure out a way
    // to avoid circular dependencies)
    Rule {
      multiplex: true
      inputs: ['qm']
      Artifact {
        fileTags: ['translations.rcc']
        filePath: FileInfo.joinPaths(product.destinationDirectory, "translations.rcc")
      }
      prepare: {
        var qrc = new JavaScriptCommand();
        qrc.description = "creating translations.qrc";
        qrc.sourceCode = function() {
          var f = new TextFile(output.filePath.replace(/\.rcc$/, '.qrc'), TextFile.WriteOnly);
          f.write('<RCC><qresource prefix="/translations">\n' +
                  inputs['qm'].map(function(i) { return '<file alias="client.' + i.fileName + '">' + i.fileName + '</file>\n'; }).join('') +
                  '</qresource></RCC>');
          f.close();
        };
        var rcc = new Command(product.Qt.core.binPath + "/rcc", ['-binary', output.filePath.replace(/\.rcc$/, '.qrc'), '-o', output.filePath]);
        rcc.description = "creating translations.rcc";
        rcc.highlight = "linker";
        return [qrc, rcc];
      }
    }
    // Archive all .ts files into a zip to simply translations later
    Rule {
      multiplex: true
      inputs: ['ts.output']
      Artifact {
        fileTags: ['archiver.input-list']
        filePath: FileInfo.joinPaths(product.destinationDirectory, "tsfiles.txt")
      }
      prepare: {
        var cmd = new JavaScriptCommand();
        cmd.silent = true;
        cmd.sourceCode = function() {
          var file = new TextFile(output.filePath, TextFile.WriteOnly);
          file.write(inputs['ts.output'].map(function (i) { return i.fileName; }).join('\n'));
          file.close();
        };
        return [cmd];
      }
    }
    // Install the translations.rcc file directly
    Group {
      fileTagsFilter: ['translations.rcc']
      qbs.install: true
      qbs.installDir: qbs.targetOS.contains("macos") ? project.productName + ".app/Contents/Resources/" : qbs.targetOS.contains("linux") ? "etc/" : ""
      qbs.installPrefix: ""
    }

    // Export en_US.ts for use in OneSky.  We have to apply transformations to
    // deal with nuances that OneSky doesn't support.
    Rule {
      multiplex: true
      // We depend on ts.output since that's where the en_US.ts comes from
      inputs: ['ts.output']
      Artifact {
        fileTags: ["translations.export"]
        filePath: FileInfo.joinPaths(product.destinationDirectory, "en_US.onesky.ts")
      }
      prepare: {
        var cmd = new JavaScriptCommand()
        cmd.silent = true
        cmd.sourceCode = function() {
          var inputTsXml = PiaUtil.readTextFile(FileInfo.joinPaths(product.destinationDirectory, "en_US.ts"))
          PiaUtil.writeTextFile(output.filePath, PiaOneSky.tsExport(inputTsXml))
        }
        return [cmd]
      }
    }
  }

  PiaApplication {
    name: "support-tool"
    targetName: project.brandCode + '-' + name
    Depends { name: "Qt.core" }
    Depends { name: "Qt.network" }
    Depends { name: "Qt.quick" }
    Depends { name: "Qt.gui" }
    Depends { name: "Qt.quickcontrols2" }

    type: base.concat(qbs.targetOS.contains('macos') ? ["support-tool-symlink", "macos_support_bundle_plist_processed"] : [])

    files: [
      "extras/support-tool/*.cpp",
      "extras/support-tool/*.h",
    ].uniqueConcat(sources)

    Qt.core.resourceSourceBase: "extras/support-tool"
    Group {
      name: "resources"
      fileTags: ["qt.core.resource_data"]
      files: [
        "extras/support-tool/components/**/*",
        "extras/support-tool/qtquickcontrols2.conf",
      ]
      excludeFiles: [".DS_Store", "*.qrc", "*.svg", "*.sh", "*.autosave"]
    }

    Rule {
      multiplex: true
      requiresInputs: false
      Artifact {
        filePath: project.buildDirectory + "/install-root/" + project.productName + ".app/Contents/Resources/"+ project.brandCode + "-support-tool.app/Contents/MacOS/" + project.brandCode + "-support-tool"
        fileTags: ["support-tool-symlink"]
      }
      prepare: {
        var ln = new Command("ln", [ "-s", "../../../../MacOS/" + project.brandCode + "-support-tool", output.filePath ]);
        ln.description = "symlinking " + output.fileName;
        ln.highlight = "filegen";
        return [ln];
      }
    }

    Group {
      fileTagsFilter: ["application"]
      fileTags: ["application.windeploy", "application.macdeploy"]
      qbs.install: true
      qbs.installDir: qbs.targetOS.contains("macos") ? project.productName + ".app/Contents/MacOS/" : qbs.targetOS.contains("windows") ? "" : "bin/"
    }

    Group {
      name: "zip-tool"
      condition: qbs.targetOS.contains("windows")
      qbs.install: true;
      files: ["deps/zip/zip.exe"]
      qbs.installDir: ""
    }


    // Install an Info.plist and setup the bundle
    Group {
      name: "macos/support-bundle-plist"
      condition: qbs.targetOS.contains("macos")
      files: [
        "extras/support-tool/mac-bundle/Info.plist",
      ]
      fileTags: "macos_support_bundle_plist"
    }
    Group {
      fileTags: ["brand_json"]
      files: ["brands/" + project.brandCode + "/brandinfo.json"]
    }
    Rule {
      inputs: ["macos_support_bundle_plist", "brand_json"]
      multiplex: true
      Artifact {
        filePath: FileInfo.joinPaths(product.destinationDirectory, "Info.plist")
        fileTags: "macos_support_bundle_plist_processed"
      }
      prepare: {
        var edit = new JavaScriptCommand()
        edit.description = "Generating " + output.filePath
        edit.highlight = "filegen"
        edit.sourceCode = function() {
          var brandInfo = JSON.parse(PiaUtil.readTextFile(inputs.brand_json[0].filePath))
          PiaUtil.generateBranded(inputs.macos_support_bundle_plist[0].filePath,
                                  output.filePath, project, brandInfo)
        }
        return [edit]
      }
    }
    Group {
      name: "macos/support-bundle-plist-processed"
      condition: qbs.targetOS.contains("macos")
      fileTagsFilter: "macos_support_bundle_plist_processed"
      qbs.install: true
      qbs.installDir: project.productName + ".app/Contents/Resources/"+project.brandCode+"-support-tool.app/Contents/"
      qbs.installPrefix: ""
    }

    Group {
      name: "macos/support-bundle-resources"
      condition: qbs.targetOS.contains("macos")
      files: [
        "brands/" + project.brandCode + "/icons/app.icns",
        "extras/support-tool/mac-bundle/qt.conf"
      ]
      qbs.install: true
      qbs.installDir: project.productName + ".app/Contents/Resources/"+project.brandCode+"-support-tool.app/Contents/Resources/"
    }
  }

  Project {
    name: "windows"
    condition: qbs.targetOS.contains('windows')

    property var codesigningCertInfo: ({
      path: Environment.getEnv("PIA_SIGNTOOL_CERTFILE"),
      password: Environment.getEnv("PIA_SIGNTOOL_PASSWORD"),
      thumbprint: Environment.getEnv("PIA_SIGNTOOL_THUMBPRINT")
    })
    property bool codesigningEnabled: qbs.buildVariant == "release" && (codesigningCertInfo.path || codesigningCertInfo.thumbprint)

    // This module is used by the client to indirectly access Windows Runtime
    // APIs.  The client remains compatible with Windows 7 by only loading this
    // module on 8+.  The Windows Runtime APIs themselves are spread among
    // various modules, so this level of indirection avoids introducing a hard
    // dependency on any of those modules from the client itself.
    DynamicLibrary {
      name: "winrtsupport"
      targetName: project.brandCode + "-" + name

      Depends { name: "cpp" }
      Depends { name: "Qt.core" }

      files: [
        "common/src/builtin/common.h",
        "common/src/win/win_com.*",
        "common/src/win/win_util.*",
        "extras/winrtsupport/src/*.cpp",
        "extras/winrtsupport/src/*.h"
      ]

      cpp.includePaths: base.concat(["common/src/builtin/", "common/src/win/"])

      cpp.cxxLanguageVersion: "c++17"
      cpp.debugInformation: true

      cpp.defines: [
        "QT_DEPRECATED_WARNINGS"
      ]

      cpp.combineCSources: project.combineSources
      cpp.combineCxxSources: project.combineSources

      Group {
        fileTagsFilter: ["dynamiclibrary"]
        qbs.install: true
      }
    }

    Product {
      name: "runtime"
      Depends { name: "Qt.core" }

      Probe {
        id: sdk
        property string arch: qbs.architecture == 'x86_64' ? 'x64' : 'x86'
        property string vctoolsRedistDir: Environment.getEnv('VCToolsRedistDir')
        property string windowsSdkBinPath: Environment.getEnv('WindowsSdkBinPath')
        property string windowsSdkDir: Environment.getEnv('WindowsSdkDir')
        property bool debug: qbs.buildVariant == "debug"
        property stringList msvcLibraries: [ "msvcp140", "vcruntime140" ]
        property stringList msvcFiles
        property stringList ucrtFiles
        configure: {
          var msvcPath = vctoolsRedistDir + (debug ? "/debug_nonredist/" : "/") + arch + (debug ? "/Microsoft.VC141.DebugCRT" : "/Microsoft.VC141.CRT")
          msvcFiles = msvcLibraries.map(function(l) { return msvcPath + "/" + l + (debug ? "d" : "") + ".dll"; });
          if (debug) {
            var lastSdkVersion = File.directoryEntries(windowsSdkBinPath, File.Dirs).filter(function(f) { return f.startsWith("10."); }).reduce(function(a,v) { return v; }, null);
            ucrtFiles = lastSdkVersion ? [ FileInfo.joinPaths(windowsSdkBinPath, lastSdkVersion, arch, "ucrt", "*.dll") ] : [];
          } else {
            ucrtFiles = [ FileInfo.joinPaths(windowsSdkDir, "Redist", "ucrt", "DLLs", arch, "*.dll") ];
          }
          found = true;
        }
      }

      Group {
        name: "msvc"
        qbs.install: true
        condition: qbs.toolchain.contains('msvc')
        files: sdk.msvcFiles
      }
      Group {
        name: "ucrt"
        qbs.install: true
        condition: qbs.toolchain.contains('msvc')
        files: sdk.ucrtFiles
      }
    }

    // This product gathers up any installable files (i.e. with qbs.install
    // set to true) from its dependencies and produces a 7z archive with
    // the tag "archiver.archive".
    Product {
      name: "payload"
      condition: qbs.targetOS.contains("windows")
      type: ["archiver.archive"]

      // While these pull in a bunch of cpp files, they don't get compiled as
      // the project target type is an archive
      Depends { name: "clientlib" }
      Depends { name: "client" }
      Depends { name: "cli" }
      Depends { name: "daemon" }
      Depends { name: "winrtsupport" }
      // Generated translations resource file
      Depends { name: "translations" }
      Depends { name: "support-tool" }
      // Pull in additional install sources
      Depends { name: "runtime" }
      Depends { name: "uninstaller" }
      // Pull in dependencies for functionality
      Depends { name: "archiver" }
      Depends { name: "Qt.core" }

      archiver.type: "7zip"
      archiver.compressionLevel: qbs.buildVariant == "release" ? "9" : "3"
      archiver.workingDirectory: qbs.installRoot
      archiver.flags: project.builtByQtCreator ? [ '-bso0', '-bsp0' ] : []

      Rule {
        multiplex: true
        inputsFromDependencies: ["installable"]
        Artifact {
          filePath: "payload.input.txt"
          fileTags: ["signtool.input-list"]
        }
        prepare: {
          var cmd = new JavaScriptCommand();
          cmd.description = "windeployqt";
          cmd.inputFilePaths = inputs.installable.map(ModUtils.artifactInstalledFilePath);
          cmd.outputFilePath = output.filePath;
          cmd.installRoot = product.moduleProperty("qbs", "installRoot");
          // For some reason, including pia-cli.exe in the windeployqt list
          // prevents windeployqt from deploying any QtQuick dependencies.  It
          // doesn't have any specific dependencies though, so the other modules
          // that do need to be deployed are tagged with "application.windeploy".
          cmd.binaryFilePaths = inputs.installable.filter(function(artifact) {
            return artifact.fileTags.contains("application.windeploy")
          }).map(ModUtils.artifactInstalledFilePath);
          cmd.qmlPath = product.sourceDirectory + "/client/res/components";
          cmd.qmlPathExtra = product.sourceDirectory + "/extras/support-tool/components";
          cmd.sourceCode = function() {
            PiaUtil.winDeploy([qmlPath, qmlPathExtra],
                              binaryFilePaths, product, inputFilePaths,
                              outputFilePath, installRoot)
          };
          return [cmd];
        }
      }
      Rule {
        multiplex: true
        inputs: ["signtool.input-list"]
        Artifact {
          filePath: "payload.txt"
          fileTags: ["archiver.input-list"]
        }
        prepare: {
          var namedFiles = {};
          namedFiles[project.brandCode + "-client.exe"] = project.productName;
          namedFiles[project.brandCode + "-service.exe"] = project.productName + " Service";
          namedFiles["uninstall.exe"] = project.productName + " Uninstaller";
          var cmds = [];
          if (project.codesigningEnabled) {
            var tf = new TextFile(input.filePath, TextFile.ReadOnly);
            var namedFileArgs = []
            var exeArgs = []
            var dllArgs = []
            var line;
            while (line = tf.readLine()) {
              if (namedFiles[line])
                namedFileArgs.push(line)
              else if(line.endsWith(".exe"))
                exeArgs.push(line)
              else if(line.endsWith(".dll"))
                dllArgs.push(line)
            }

            function makeCodesignCmd(fileArgs, useTimestamping, fileDescription) {
              var cmd = new JavaScriptCommand();
              cmd.description = "signing " + fileArgs.length + " files: " + fileArgs[0] + ", ...";
              cmd.filePaths = fileArgs.map(function(file){return FileInfo.joinPaths(product.moduleProperty("qbs", "installRoot"), file)})
              cmd.certInfo = project.codesigningCertInfo;
              cmd.useTimestamping = useTimestamping;
              cmd.fileDescription = fileDescription;
              cmd.sourceCode = function() { PiaUtil.signtool(filePaths, certInfo, useTimestamping, fileDescription); };
              return cmd
            }

            for(var i=0; i<namedFileArgs.length; ++i)
              cmds.push(makeCodesignCmd([namedFileArgs[i]], true, namedFiles[namedFileArgs[i]]))
            if(exeArgs.length > 0)
              cmds.push(makeCodesignCmd(exeArgs, true, undefined))
            if(dllArgs.length > 0)
              cmds.push(makeCodesignCmd(dllArgs, false, undefined))
          }
          cmds.push(PiaUtil.makeCopyCommand(input.filePath, output.filePath));
          return cmds;
        }
      }
    }

    WindowsInstaller {
      name: "installer"
      targetName: project.packageName
      cpp.defines: base.concat(["INSTALLER"])
      type: ["application", "application.signed"]
      Group {
        name: "lzma"
        files: [
          "deps/lzma/src/*.c",
          "deps/lzma/src/*.h",
        ]
      }
      Depends { name: "payload" }
      Rule {
        inputsFromDependencies: ["archiver.archive"]
        Artifact {
          filePath: "payload.rc"
          fileTags: ["rc"]
        }
        prepare: {
          var cmd = new JavaScriptCommand();
          cmd.silent = true;
          cmd.inputFilePath = input.filePath;
          cmd.outputFilePath = output.filePath;
          cmd.sourceCode = function() {
            var tf;
            try {
              tf = new TextFile(outputFilePath, TextFile.WriteOnly);
              tf.writeLine('1337 RCDATA "' + inputFilePath + '"');
            } finally {
              if (tf) tf.close();
            }
          };
          return [cmd];
        }
      }
      Rule {
        inputs: ["application"]
        Artifact {
          // Put the .exe in a predictable location for our build scripts
          filePath: project.buildDirectory + "/installer/" + input.fileName
          fileTags: ["application.signed"]
        }
        prepare: {
          var copy = PiaUtil.makeCopyCommand(input.filePath, output.filePath)
          if (project.codesigningEnabled) {
            var sign = new JavaScriptCommand();
            sign.description = "signing " + output.fileName;
            sign.outputPath = output.filePath;
            sign.certInfo = project.codesigningCertInfo;
            sign.sourceCode = function() { PiaUtil.signtool(outputPath, certInfo, true, project.productName + " Installer"); };
            return [copy, sign];
          }
          return [copy];
        }
      }
    }
    WindowsInstaller {
      name: "uninstaller"
      targetName: "uninstall"
      cpp.defines: base.concat(["UNINSTALLER"])
      Group {
        fileTagsFilter: ["application"]
        qbs.install: true
      }
    }
  }

  Project {
    name: "macos"
    condition: qbs.targetOS.contains("macos")

    Product {
      name: "installer-script"
      type: ["shell-processed"], ["shell-processed-header"]
      files: ["extras/installer/mac/*.sh"]
      FileTagger {
        patterns: ["*.sh"]
        fileTags: ["shell"]
      }
      Rule {
        inputs: ["shell"]
        Artifact {
          filePath: {
            // install.sh becomes vpn-installer.sh for historical reasons
            if(input.fileName === "install.sh")
              return "vpn-installer.sh"
            return input.fileName
          }
          fileTags: ["shell-processed"]
        }
        prepare: {
          var edit = new JavaScriptCommand()
          edit.description = "generating " + output.fileName;
          edit.highlight = "filegen"
          edit.sourceCode = function() {
            PiaUtil.generateBranded(input.filePath, output.filePath, project)
          }
          var chmod = new Command("chmod", [ "+x", output.filePath ])
          chmod.silent = true;
          return [edit, chmod];
        }
      }

      // Copy of the install script embedded in a header file, for inclusion
      // into the install helper (so the install helper doesn't have to shell
      // out to a possibly-modified script on uninstall)
      Rule {
        inputs: ["shell-processed"]
        Artifact {
          filePath: product.destinationDirectory + "/" + input.fileName.replace(/\./g, "_") + ".h"
          fileTags: ["shell-processed-header"]
        }
        prepare: {
          var xxd = new Command("xxd", ["-i", input.fileName, output.filePath])
          xxd.workingDirectory = FileInfo.cleanPath(input.filePath + "/..")
          xxd.description = "creating " + output.fileName
          xxd.highlight = "filegen"
          return [xxd]
        }
      }

      Group {
        fileTagsFilter: ["shell-processed"]
        qbs.install: true
        qbs.installDir: project.productName + ".app/Contents/Resources/"
        qbs.installPrefix: ""
      }

      Export {
        Depends { name: "cpp" }
        cpp.includePaths: [product.destinationDirectory]
      }
    }

    // Helper installed with SMJobBless to install/repair/update the application
    CppApplication {
      name: "installer"
      Depends { name: "brand-files" }
      Depends { name: "version-files" }
      Depends { name: "installer-script" }
      targetName: project.brandIdentifier + ".installhelper"
      cpp.minimumMacosVersion: "10.10"
      files: [
        "extras/installer/mac/helper/*.cpp",
        "extras/installer/mac/helper/*.mm",
        "extras/installer/mac/helper/*.h"
      ]
      cpp.cxxLanguageVersion: "c++17"
      cpp.automaticReferenceCounting: true
      cpp.frameworks: ["Foundation", "Security"]
      bundle.isBundle: false

      // Embed an Info.plist and Launchd.plist into the helper binary.  These
      // need to be preprocessed for branding.
      //
      // Normally these are linked in with clang's `-sectcreate` option, but in
      // this case we have to generate an assembler file to link them in, so the
      // linker rule has a dependency on the generated plists.
      //
      // (We could have added linker options manually, but there's no way to add
      // auxiliary inputs to the cpp module's linker rule to ensure the files
      // are generated before linking.)
      Group {
        fileTags: ["plist_template"]
        // These plist files have to have an extension other than "plist" to
        // prevent Qbs from trying to include them as regular plists
        files: ["extras/installer/mac/helper/*.plist.template"]
      }
      Rule {
        multiplex: true
        inputs: ["plist_template"]
        outputFileTags: ["asm"]
        outputArtifacts: [
          {
            fileTags: ["asm"],
            filePath: FileInfo.joinPaths(product.destinationDirectory, "plist.s")
          }
        ]
        prepare: {
          var generate = new JavaScriptCommand()
          generate.description = "generating branded plists and " + outputs.asm[0].fileName
          generate.highlight = "filegen"
          generate.sourceCode = function() {
            var plistAsm = ""
            for(var i=0; i<inputs['plist_template'].length; ++i) {
              var inputPlist = inputs['plist_template'][i]
              var outputFile = FileInfo.joinPaths(product.destinationDirectory, inputPlist.fileName)

              var content = PiaUtil.readTextFile(inputPlist.filePath)
              content = PiaUtil.brandSubstitute(project, undefined, content)
              // The certificate's common name and the product version are
              // needed in the info.plist
              content = content.replace(/\{\{PIA_CODESIGN_CERT\}\}/g,
                                        Environment.getEnv("PIA_CODESIGN_CERT"))
              // This ignores prerelease tags in productPrerelease
              content = content.replace(/\{\{PIA_PRODUCT_VERSION\}\}/g,
                                        project.productVersion)
              content = content.replace(/\{\{PIA_SEMANTIC_VERSION\}\}/g,
                                        project.semanticVersion)
              PiaUtil.writeTextFile(outputFile, content)

              var sectName = inputPlist.fileName
              sectName = sectName.replace('.template', '')
              sectName = '__' + sectName.replace('.', '_')
              plistAsm += ".section __TEXT," + sectName + "\n"
              plistAsm += ".incbin \"" + outputFile + "\"\n"
              plistAsm += "\n"
            }

            PiaUtil.writeTextFile(outputs["asm"][0].filePath, plistAsm)
          }
          return [generate]
        }
      }

      Group {
        fileTagsFilter: ["application"]
        qbs.install: true
        qbs.installDir: project.productName + ".app/Contents/Library/LaunchServices/"
        qbs.installPrefix: ""
      }
    }
    CppApplication {
      name: "unquarantine"
      targetName: project.brandCode + "-unquarantine"
      cpp.minimumMacosVersion: "10.10"
      type: ["application"]
      files: [
        "extras/installer/mac/unquarantine.cpp",
      ]
      Group {
        fileTagsFilter: ["application"]
        qbs.install: true
        qbs.installDir: project.productName + ".app/Contents/MacOS/"
        qbs.installPrefix: ""
      }
    }
    PiaApplication {
      name: "openvpn-helper"
      targetName: project.brandCode + "-" + name
      consoleApplication: true
      files: [
        "common/src/builtin/common.h",
        "common/src/builtin/util.cpp",
        "common/src/builtin/util.h",
        "extras/openvpn/mac_openvpn_helper.cpp",
      ]
      Group {
        fileTagsFilter: ["application"]
        fileTags: ["application.macdeploy"]
        qbs.install: true
        qbs.installDir: project.productName + ".app/Contents/MacOS/"
      }
    }

    // Deploy and sign the Mac app bundle
    Product {
      name: "mac-bundle"
      type: ["mac-app-bundle"]

      // This depends on all artifacts that go into the installer
      Depends { name: "clientlib" }
      Depends { name: "cli" }
      Depends { name: "client" }
      Depends { name: "daemon" }
      Depends { name: "translations" }
      Depends { name: "support-tool" }
      Depends { name: "installer-script" }
      Depends { name: "installer" }
      Depends { name: "unquarantine" }
      Depends { name: "openvpn-helper" }

      Depends { name: "Qt.core" }

      Rule {
        multiplex: true
        inputsFromDependencies: ["installable"]
        outputFileTags: ["mac-app-bundle"]

        prepare: {
          var appBundle = FileInfo.joinPaths(product.qbs.installRoot, project.productName + ".app")
          var commands = []

          var codesignOptArgs = []

          // Run macdeployqt (only if product.macdeployqt is set)
          if(project.macdeployqt) {
            var deployBins = []
            for(var i=0; i<inputs.installable.length; ++i) {
              var artifact = inputs.installable[i]
              if(artifact.fileTags.contains("application.macdeploy") || artifact.fileTags.contains("dynamiclibrary.macdeploy"))
                deployBins.push(FileInfo.joinPaths(appBundle, "Contents/MacOS", artifact.fileName))
            }

            var qmlDirs = [
              FileInfo.joinPaths(project.sourceDirectory, "client/res/components"),
              FileInfo.joinPaths(project.sourceDirectory, "extras/support-tool/components")
            ]

            commands.push(PiaUtil.makeMacDeployCommand(product, appBundle, deployBins, qmlDirs))

          }

          // If we're going to notarize the build (done by build-macos.sh),
          // sign with the hardened runtime (required for notarization).
          // - Don't do this when not deploying, we obviously have to load the
          //   unsigned Qt libs
          // - Don't do this when not notarizing, so devs can still sign with
          //   a self-signed cert (hardened runtime requires a team ID)
          var env = Environment.currentEnv();
          if(!env.hasOwnProperty("PIA_BRANCH_BUILD")
              || (env.hasOwnProperty("PIA_BRANCH_BUILD") && env["PIA_BRANCH_BUILD"] === "master")
              || (env.hasOwnProperty("PIA_ALWAYS_NOTARIZE") && env["PIA_ALWAYS_NOTARIZE"] === "1")){
            if(project.macdeployqt)
              codesignOptArgs = ["--options=runtime"]
            else
              console.error("Notarization will fail - requires deployment with macdeployqt")
          }

          // Sign if a certificate was specified
          var cert = Environment.getEnv("PIA_CODESIGN_CERT")
          if(cert) {
            var codesign = Environment.getEnv("CODESIGN") || "codesign"

            // codesign annoyingly prints its output to stderr, which Qt Creator
            // interprets as errors.  Run it indirectly with bash to send
            // stderr to stdout
            function codesignBashArgs(codesignArgs) {
              var codesignWithArgs = [codesign].concat(codesignArgs)
              var cmd = "\"" + codesignWithArgs.join("\" \"") + "\" 2>&1"
              return ["-c", cmd];
            }

            // codesign --deep does not find the helper, probably because it's
            // in Library/LaunchServices
            var installHelper = FileInfo.joinPaths(appBundle, "Contents/Library/LaunchServices", project.brandIdentifier + ".installhelper")
            var signHelper = new Command("bash", codesignBashArgs(codesignOptArgs.concat(["--sign", cert, "--verbose", "--force", installHelper])))
            signHelper.description = "signing install helper"
            commands.push(signHelper)

            var signDeep = new Command("bash", codesignBashArgs(codesignOptArgs.concat(["--deep", "--sign", cert, "--verbose", "--force", appBundle])))
            signDeep.description = "signing app bundle"
            commands.push(signDeep)
          }
          else {
            var signMsg = new JavaScriptCommand()
            signMsg.sourceCode = function() {}
            signMsg.description = "Not signing, build will have to be manually installed.\n" +
              "Set PIA_CODESIGN_CERT to enable code signing, and see README.md for more information"
            commands.push(signMsg)
          }

          return commands;
        }
      }
    }
  }
  Project {
    name: "linux"
    condition: qbs.targetOS.contains("linux")

    CppApplication {
      name: "support-tool-launcher"
      cpp.cxxLanguageVersion: "c++14"
      Depends {
        name: "brand-files"
      }
      files:[
        "extras/support-tool/launcher/linux-launcher.cpp"
      ]
      type:"application"
      cpp.includePaths: [product.buildDirectory + "/brand"]
      qbs.installPrefix: ""
      Group {
        fileTagsFilter: ["application"]
        qbs.install: true
        // Only installl for linux, so set the linux install dir
        qbs.installDir: "bin/"
        qbs.installPrefix: ""
      }
    }

    Product {
      name: "linux-openvpn-updown"
      type: ["openvpn-shell-processed"]
      files: [
              "extras/openvpn/linux/updown.sh"
      ]
      FileTagger {
        fileTags: ["openvpn-shell"]
          patterns: [
            "*.sh"
          ]
      }
      Rule {
        inputs: ["openvpn-shell"]
        multiplex: true
        Artifact {
          filePath: "openvpn-updown.sh"
          fileTags: ["openvpn-shell-processed"]
          alwaysUpdated: true
        }
        prepare: {
          var edit = new JavaScriptCommand()
          edit.description = "generating " + output.fileName
          edit.highlight = "filegen"
          edit.sourceCode = function() {
            PiaUtil.generateBranded(input.filePath, output.filePath, project)
          }
          var chmod = new Command("chmod", [ "+x", output.filePath ])
          chmod.silent = true;
          return [edit, chmod];
        }
      }
      Group {
        fileTagsFilter: ["openvpn-shell-processed"]
        qbs.install: true
        qbs.installDir: "bin/"
        qbs.installPrefix: ""
      }
    }
  }
}
