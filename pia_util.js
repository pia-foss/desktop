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

var FileInfo = require("qbs.FileInfo");
var Process = require("qbs.Process");
var File = require("qbs.File");
var TextFile = require("qbs.TextFile");
var Environment = require("qbs.Environment");

// Takes a number of arguments which can be either strings or arrays,
// and generates every possible left-to-right concatenation of items
// by picking one item from every array argument. (String arguments
// are treated as single-item arrays.)
function pathCombine() {
  if (arguments.length === 0) {
    return [];
  }
  var result = Array.isArray(arguments[0]) ? arguments[0] : [arguments[0]];
  for (var i = 1; i < arguments.length; i++) {
    var arg = arguments[i];
    if (Array.isArray(arguments[i])) {
      result = result.reduce(function (acc, e) { return acc.concat(arg.map(function (f) { return e + f; })); }, []);
    } else {
      result = result.map(function (e) { return e + arg; });
    }
  }
  return result;
}

// Invoke the signtool executable for signing binaries on Windows
// certInfo contains params used to identify the signing cert:
// - certInfo.path - path to a certificate file
// - certInfo.password - when using a certificate file, optional password to decrypt that file
// - certInfo.thumbprint - cert thumbprint used to identify a cert from the cert store
function signtool(filePaths, certInfo, useTimestamping, fileDescription) {
  if (!Array.isArray(filePaths)) {
    filePaths = [filePaths];
  }
  var certArgs = []
  if(certInfo.path) {
    certArgs = certArgs.concat([ "/f", certInfo.path ])
    if (certInfo.password)
      certArgs = certArgs.concat([ "/p", certInfo.password ])
  }
  else if(certInfo.thumbprint)
    certArgs = certArgs.concat([ "/sha1", certInfo.thumbprint ])
  else {
    console.error('Cannot sign, no certificate information provided')
    return
  }
  var fileArgs = filePaths.map(function(s) { return FileInfo.toWindowsSeparators(s); });
  // Sign twice, first with SHA1 and second with SHA256
  var first = true;
  [ "sha1", "sha256" ].forEach(function (hash) {
    var process;
    try {
      process = new Process();
      process.exec("signtool", [ "sign" ]
                   .concat(first ? [] : [ "/as" ])
                   .concat([ "/fd", hash ])
                   .concat(useTimestamping ? [ "/tr", "http://timestamp.digicert.com", "/td", hash ] : [])
                   .concat(certArgs)
                   .concat(fileDescription ? [ "/d", fileDescription ] : [])
                   .concat(fileArgs),
                   true);
      if (useTimestamping) {
        // Insert a two-second pause to try to avoid flooding the timestamping server.
        (new Process()).exec("ping", ["127.0.0.1", "-n", "3"]);
      }
    } finally {
      if (process) process.close();
    }
    first = false;
  })
}

// Invoke windeployqt on Windows.  Writes a text file artifact containing the
// paths of the deployed files.
function winDeploy(qmlDirs, binaryFilePaths, product, inputFilePaths, outputFilePath, installRoot) {
  var out;
  var process;
  try {
    process = new Process();
    var args = ["--json"]
    for(var i=0; i<qmlDirs.length; ++i) {
      args.push("--qmldir")
      args.push(qmlDirs[i])
    }
    args = args.concat(["--no-webkit2", "--no-angle", "--no-compiler-runtime", "--no-translations"], binaryFilePaths)
    console.info('windeployqt args: ' + args.join(' '))
    process.exec(FileInfo.joinPaths(product.moduleProperty("Qt.core", "binPath"), "windeployqt"), args, true)
    out = process.readStdOut();
  } finally {
    if (process) process.close();
  }
  var tf;
  try {
    var deployFilePaths = inputFilePaths.uniqueConcat(JSON.parse(out).files.map(function(obj) {
      return FileInfo.joinPaths(
            FileInfo.fromWindowsSeparators(obj.target),
            FileInfo.fileName(FileInfo.fromWindowsSeparators(obj.source)));
    }));
    deployFilePaths.sort();
    tf = new TextFile(outputFilePath, TextFile.WriteOnly);
    deployFilePaths.forEach(function(p) {
      tf.writeLine(FileInfo.relativePath(installRoot, p));
    });
  } finally {
    if (tf) tf.close();
  }
}

// Create a command to run macdeployqt on Mac
function makeMacDeployCommand(product, appBundle, deployBins, qmlDirs) {
  var deployArgs = [appBundle]
  deployArgs = deployArgs.concat(deployBins.map(function(bin){return "-executable=" + bin}))
  deployArgs = deployArgs.concat(qmlDirs.map(function(qmldir){return "-qmldir=" + qmldir}))
  deployArgs.push("-no-strip")

  console.info('deploy args: ' + JSON.stringify(deployArgs))

  var macdeployqt = Environment.getEnv("MACDEPLOYQT") ||
    FileInfo.joinPaths(product.moduleProperty("Qt.core", "binPath"), "macdeployqt")

  var deploy = new Command(macdeployqt, deployArgs)
  deploy.description = "running macdeployqt"
  return deploy
}

// Read a text file
function readTextFile(path) {
  var file = new TextFile(path, TextFile.ReadOnly)
  var text = file.readAll()
  file.close()
  return text
}

// Write a text file
function writeTextFile(path, text) {
  var file = new TextFile(path, TextFile.WriteOnly)
  file.write(text)
  file.close()
}

// Do brand substitutions on the content of a file
// brandInfo is optional, since it requires the rule to have a dependency on
// brand.json
function brandSubstitute(project, brandInfo, content) {
  content = content.replace(/\{\{BRAND_CODE\}\}/g, project.brandCode)
  content = content.replace(/\{\{BRAND_IDENTIFIER\}\}/g, project.brandIdentifier)
  content = content.replace(/\{\{PIA_PRODUCT_NAME\}\}/g, project.productName)
  if(brandInfo)
    content = content.replace(/\{\{BRAND_SHORT\}\}/g, brandInfo.shortName)
  return content
}

// Generate a branded file by applying substitutions
// Like brandSubstitute(), brandInfo is optional
function generateBranded(inputFile, outputFile, project, brandInfo) {
  var content = readTextFile(inputFile)
  content = brandSubstitute(project, brandInfo, content)
  writeTextFile(outputFile, content)
}

// Make a JavaScriptCommand to copy one file
function makeCopyCommand(sourcePath, destPath) {
  var copy = new JavaScriptCommand();
  copy.silent = true;
  copy.source = sourcePath;
  copy.destination = destPath;
  copy.sourceCode = function() { File.copy(source, destination); };
  return copy
}

// Make commands to copy one file while dereferencing symlinks on Unix platforms
function makeCopyDerefCommands(targetOS, sourcePath, destPath) {
  if(targetOS.contains("unix")) {
    // File.copy() makes parent directories, do that here too
    var mkdir = new Command("mkdir", ["-p", FileInfo.path(destPath)])
    mkdir.silent = true
    var copy = new Command("cp", ["-H", sourcePath, destPath])
    copy.silent = true
    return [mkdir, copy]
  }
  else {
    // On Windows, we don't necessarily have a cp command, but we don't have
    // symlinks either, so just use a normal File.copy.
    return [makeCopyCommand(sourcePath, destPath)]
  }
}
