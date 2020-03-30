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
import "pia_util.js" as PiaUtil

Project {
  minimumQbsVersion: "1.10"

  property string productName: brandName
  property string productVersion: "2.0.0"
  property string productPrerelease: ""
  property string brandCode: "pia"
  id: piaProject

  // Override this to 'true' to run macdeployqt on mac builds.  This only works
  // if the output directory is clean, because macdeployqt can't be re-run on
  // a bundle that was previously deployed.
  // (Override on the qbs command line or in Qt Creator build configuration).
  property bool macdeployqt: false

  readonly property var productVersionFields: {
    var fields = productVersion.split('.').map(function (v) { return parseInt(v, 10); });
    if (fields.length !== 3) throw new Error("Invalid version");
    return fields;
  }
  property string packageName: {
    var parts = [ brandCode ];
    parts.push(qbs.targetOS.contains('macos') ? "macos" : qbs.targetOS.contains('windows') ? "windows" : qbs.targetOS.contains('linux') ? "linux" : "unknown");
    if (qbs.targetOS.contains('windows')) {
      parts.push(qbs.architecture === 'x86_64' ? "x64" : "x86");
    }
    parts.push(productVersion.replace(/\.0$/, ''));
    if (productPrerelease) {
      parts.push(productPrerelease.replace(' ', '.').replace(/[^A-Za-z0-9-.]+/g, '-'));
    }
    if (buildSuffix) {
      parts.push(buildSuffix.replace(/[^A-Za-z0-9-]+/g, '-'));
    }
    return parts.join("-");
  }

  property bool builtByQtCreator: profile.startsWith("qtc_")
  property bool combineSources: !builtByQtCreator // Qt Creator's code model doesn't handle amalgamation

  // The product name has to be found with a probe since it is used throughout
  // the Qbs scripts; probes are never automatically re-run by Qt though, so it
  // won't pick up changes if the name is changed in brandinfo.json.
  Probe {
    id: brand

    property string brandCode: piaProject.brandCode
    property string sourceDir: piaProject.sourceDirectory

    property string brandName
    property string brandIdentifier

    configure: {
      var filePath = sourceDir + "/brands/" + brandCode + "/brandinfo.json"
      if(!File.exists(filePath)) {
        console.warn("Cannot find brand json path: " + filePath);
        return;
      }

      var fileContent = new TextFile(filePath).readAll();
      var brandParams = JSON.parse(fileContent);
      // Just read the brand name and identifier; the rule to generate
      // brand.txt/brand.h reads the other fields so they are updated if
      // brandinfo.json is changed.
      brandName = brandParams.brandName;
      brandIdentifier = brandParams.brandIdentifier;
    }
  }

  Probe {
    id: git

    readonly property string gitCommand: "git"
    readonly property string filePath: FileInfo.joinPaths(path, ".git/logs/HEAD")

    property string revision: ""
    property string shortRevision: ""
    property string branch: "release"
    property int commitCount: 0
    property bool mergedIntoMaster: false
    property bool clean: true
    property int lastCommitTime: Date.now()
    property bool found: false

    configure: {
      function runGitCommand(args, throwOnError) {
        if (throwOnError === undefined) throwOnError = true;
        var proc = new Process();
        try {
          proc.setWorkingDirectory(path);
          var error = proc.exec(gitCommand, args, throwOnError);
          return { error: error, stdout: proc.readStdOut().trim(), stderr: proc.readStdErr().trim() };
        } finally {
          proc.close();
        }
      }

      if (!File.exists(filePath))
        return; // No commits yet.
      revision = runGitCommand(["rev-parse", "HEAD"], true).stdout.trim();
      shortRevision = runGitCommand(["describe", "--always", "--match=nosuchtagpattern"]).stdout.trim();
      branch = runGitCommand(["rev-parse", "--symbolic-full-name", "--abbrev-ref", "HEAD"]).stdout.trim();
      commitCount = parseInt(runGitCommand(["rev-list", "--count", "HEAD"]).stdout.trim());
      mergedIntoMaster = !runGitCommand(["merge-base", "--is-ancestor", "HEAD", "origin/master"], false).error;
      lastCommitTime = parseInt(runGitCommand(["log", "-1", "--format=%at"]).stdout.trim());
      found = true;
    }
  }
  // Git properties forwarded as project properties
  property string revision: git.revision
  property string branch: Environment.getEnv("PIA_BRANCH_BUILD") || git.branch
  property int lastCommitTime: git.lastCommitTime
  // Effective project timestamp - also see piabuildenv.qbs
  property int timestamp: {
    var srcDateEpoch = Environment.getEnv("SOURCE_DATE_EPOCH")
    var effectiveTime = lastCommitTime
    if (srcDateEpoch)
      effectiveTime = parseInt(srcDateEpoch, 10)
    return effectiveTime
  }
  property string datetime: {
    return new Date(timestamp * 1000).toISOString().replace(/[^0-9]/g, '').slice(0, 14)
  }

  property string buildNumber: ('00000' + git.commitCount).replace(/0*(.{5,})$/, '$1')
  property string buildSuffix: {
    var debug = qbs.buildVariant !== "release" ? "." + qbs.buildVariant : ""
    if(!git.found)
      return debug;
    // Use release-style release numbers for distributable builds.
    // For any branch build (even master), use a branch-style version number.
    if (Environment.getEnv("PIA_BRANCH_BUILD") === undefined)
      return buildNumber + debug;
    else
      return (branch.replace(/[^A-Za-z0-9]+/g, '-') + '.' + datetime + '.' + git.shortRevision) + debug;
  }
  property string semanticVersion: productVersion + (productPrerelease ? '-' + productPrerelease.replace(' ', '.') : '') + (buildSuffix ? '+' + buildSuffix : '')

  property string brandName: brand.brandName
  property string brandIdentifier: brand.brandIdentifier
}
