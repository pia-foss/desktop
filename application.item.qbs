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
import qbs.FileInfo
import qbs.TextFile
import qbs.Utilities
import "pia_util.js" as PiaUtil

CppApplication {
  Depends { name: "version-files" }
  Depends { name: "brand-files" }
  Depends { name: "Qt.core" }
  Depends { name: "Qt.network" }
  Depends { name: "bundle" }
  Depends { name: "piabuildenv" }

  property stringList macosFrameworks: [
    "AppKit",
    "Foundation",
    "Security",
    "ServiceManagement",
  ]

  // Whether to compile 'common' and 'common/builtin' directly into this module.
  property bool includeCommon: true
  // Whether to include crash reporting support.  This requires including common
  // in this module.
  property bool includeCrashReporting: false

  // Crash reporting requires common
  readonly property bool includeBreakpad: {
    if(includeCrashReporting && !includeCommon)
      console.warn(name + ': ignoring crash reporting, requires common')
    return includeCrashReporting && includeCommon
  }

  readonly property stringList commonSourceDirectories: ["common/src", "common/src/builtin"]

  // Base class properties, only used as a common point of definition for common sources
  property stringList sourceDirectories: includeCommon ? commonSourceDirectories : []
  // Include paths derived from the source directories
  property stringList sourceIncludePaths: sourceDirectories.map(function (p) { return path + "/" + p; })

  property stringList sources: PiaUtil.pathCombine(sourceDirectories, "/", ["*.cpp", "*.h"])
  property stringList windowsSources: PiaUtil.pathCombine(sourceDirectories, "/win/", ["*.cpp", "*.h", "*.rc"]).concat("common/res/manifest.xml")
  property stringList posixSources: PiaUtil.pathCombine(sourceDirectories, "/posix/", ["*.cpp", "*.h"])
  property stringList macosSources: PiaUtil.pathCombine(sourceDirectories, "/mac/", ["*.cpp", "*.h", "*.mm"])
  property stringList linuxSources: PiaUtil.pathCombine(sourceDirectories, "/linux/", ["*.cpp", "*.h"])

  version: project.productVersion

  // We still only use C++14 on Linux, because our release builds are made on
  // Ubuntu 16.04 with clang 3.8.
  cpp.cxxLanguageVersion: qbs.targetOS.contains('linux') ? "c++14" : "c++17"

  cpp.includePaths: {
    var builtIncludeDirs = [product.buildDirectory + "/brand", product.buildDirectory + "/version"]
    return sourceIncludePaths.concat(builtIncludeDirs)
  }

  cpp.defines: {
    var defs = [
      // The following define makes your compiler emit warnings if you use
      // any feature of Qt which as been marked deprecated (the exact warnings
      // depend on your compiler). Please consult the documentation of the
      // deprecated API in order to know how to port your code away from it.
      "QT_DEPRECATED_WARNINGS",
      "SOURCE_ROOT=" + Utilities.cStringQuote(project.sourceDirectory),
    ]

    if(includeCommon)
      defs.push("BUILD_COMMON")

    // Macro definitions for property groups below (other than toolchain)
    // (Can't put cpp.defines in more than one property group, even when they
    // all concat, qbs seems to ignore all but one.)
    if(includeBreakpad)
      defs.push("PIA_CRASH_REPORTING")

    return defs
  }

  cpp.combineCSources: project.combineSources
  cpp.combineCxxSources: project.combineSources

  // The default installPrefix changed from "" to "/usr/local" (for Unix) in
  // qbs 1.13: https://github.com/qbs/qbs/commit/17059ccd06e56010fa0d44ebcaf0be7b85d69703
  qbs.installPrefix: ""

  files: sources

  Group {
    name: "windows"
    condition: qbs.targetOS.contains("windows")
    files: product.windowsSources
  }
  Group {
    name: "posix"
    condition: !qbs.targetOS.contains("windows")
    files: product.posixSources
  }
  Group {
    name: "macos"
    condition: qbs.targetOS.contains("macos")
    files: product.macosSources
  }
  Group {
    name: "linux"
    condition: qbs.targetOS.contains("linux")
    files: product.linuxSources
  }

  Properties {
    condition: qbs.targetOS.contains('windows')
    cpp.minimumWindowsVersion: "6.1" // Windows 7
    cpp.windowsApiCharacterSet: "unicode"
    cpp.windowsApiFamily: "desktop"
  }
  Properties {
    condition: qbs.targetOS.contains('macos')
    cpp.rpaths: typeof Qt === "undefined" ? [] : [Qt.core.libPath] // workaround for macdeployqt bug (only handles absolute paths)
    cpp.minimumMacosVersion: "10.10"
    cpp.automaticReferenceCounting: true
    cpp.frameworks: outer.concat(macosFrameworks)
    bundle.identifier: project.brandIdentifier
  }
  Properties {
    condition: qbs.targetOS.contains('linux')
    cpp.dynamicLibraries: ["dl"]
  }

  // Note: as these Properties blocks touch the same properties, only
  // one block's condition should hold true on any target platform
  Properties {
    condition: qbs.toolchain.contains('msvc')
    cpp.cxxFlags: outer.concat([ "/Zc:rvalueCast", "/utf-8", "/we4834" ]).concat(qbs.buildVariant == "release" ? [ "/GL" ] : [])
    cpp.linkerFlags: outer.concat([ "/INCREMENTAL:NO", "/MANIFEST:EMBED", "/MANIFESTINPUT:" + path + "/common/res/manifest.xml" ]).concat(qbs.buildVariant == "release" ? [ "/LTCG" ] : [])
    cpp.defines: outer.concat([ "NTDDI_VERSION=NTDDI_WIN7", "UNICODE", "_UNICODE" ])
  }
  Properties {
    condition: qbs.toolchain.contains('gcc') && !qbs.toolchain.contains("clang")
    cpp.commonCompilerFlags: outer.concat([ "-fvisibility=hidden" ])
    cpp.cxxFlags: outer.concat([ "-Wno-unused-parameter", "-Wno-attributes", "-Werror=unused-result" ])
  }
  Properties {
    condition: qbs.toolchain.contains('clang')
    cpp.commonCompilerFlags: outer.concat([ "-fvisibility=hidden" ])
    cpp.cxxFlags: outer.concat([ "-Wno-unused-parameter", "-Wno-dangling-else", "-Werror=unused-result" ])
  }

  Properties {
    condition: qbs.buildVariant == "debug"
  }
  Properties {
    condition: qbs.buildVariant == "release"
    cpp.discardUnusedData: true
    optimization: "small"
  }

  property string bpPath: "deps/breakpad"

  // always generate debug information, but store separately for release
  // builds. This is required for getting symbols. By default this is stored separately
  // from the actual binaries
  cpp.debugInformation: true

  // When building crash reporting support, export the PIA_CRASH_REPORTING
  // macro.  (Client and daemon code are built without crash reporting for unit
  // tests.)
  Export {
    Depends { name: "cpp" }
    cpp.defines: (product.includeCommon && product.includeCrashReporting) ? ["PIA_CRASH_REPORTING"] : []
  }
  Properties {
    condition: includeBreakpad
    sourceDirectories: outer.concat([ bpPath ])
    windowsSources: outer.uniqueConcat([
      bpPath + "/client/windows/handler/exception_handler.cc",
      bpPath + "/client/windows/crash_generation/crash_generation_client.cc",
      bpPath + "/common/windows/guid_string.cc",
    ])
    posixSources: outer.uniqueConcat([
      bpPath + "/client/minidump_file_writer.cc",
      bpPath + "/common/string_conversion.cc",
      bpPath + "/common/convert_UTF.c",
      bpPath + "/common/md5.cc",
    ]);
    linuxSources: outer.uniqueConcat([
      bpPath + "/client/linux/log/log.cc",
      bpPath + "/client/linux/crash_generation/crash_generation_client.cc",
      bpPath + "/client/linux/dump_writer_common/thread_info.cc",
      bpPath + "/client/linux/dump_writer_common/ucontext_reader.cc",
      bpPath + "/client/linux/microdump_writer/microdump_writer.cc",
      bpPath + "/client/linux/minidump_writer/linux_dumper.cc",
      bpPath + "/client/linux/minidump_writer/linux_core_dumper.cc",
      bpPath + "/client/linux/minidump_writer/linux_ptrace_dumper.cc",
      bpPath + "/client/linux/minidump_writer/minidump_writer.cc",
      bpPath + "/client/linux/handler/minidump_descriptor.cc",
      bpPath + "/client/linux/handler/exception_handler.cc",
      bpPath + "/common/linux/guid_creator.cc",
      bpPath + "/common/linux/file_id.cc",
      bpPath + "/common/linux/elfutils.cc",
      bpPath + "/common/linux/elf_core_dump.cc",
      bpPath + "/common/linux/memory_mapped_file.cc",
      bpPath + "/common/linux/safe_readlink.cc",
      bpPath + "/common/linux/linux_libc_support.cc"
    ])
    macosSources: outer.uniqueConcat([
      bpPath + "/client/mac/handler/exception_handler.cc",
      bpPath + "/client/mac/crash_generation/crash_generation_client.cc",
      bpPath + "/client/mac/handler/minidump_generator.cc",
      bpPath + "/client/mac/handler/dynamic_images.cc",
      bpPath + "/client/mac/handler/breakpad_nlist_64.cc",
      bpPath + "/common/mac/string_utilities.cc",
      bpPath + "/common/mac/file_id.cc",
      bpPath + "/common/mac/macho_id.cc",
      bpPath + "/common/mac/macho_utilities.cc",
      bpPath + "/common/mac/macho_walker.cc",
      bpPath + "/common/mac/MachIPC.mm",
      bpPath + "/common/mac/bootstrap_compat.cc",
    ])
  }
}
