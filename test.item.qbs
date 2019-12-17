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
import qbs.File
import qbs.FileInfo
import "application.item.qbs" as PiaApplication

PiaApplication {
  Depends { name: "Qt.core" }
  Depends { name: "Qt.network" }
  Depends { name: "Qt.testlib" }
  Depends { name: "Qt.xml"; condition: qbs.targetOS.contains("windows") }
  Depends { name: "all-tests-lib" }

  property string testName;

  name: "test: " + testName;
  targetName: "test-" + testName;
  type: ["application", "autotest"]

  // Exclude all normal sources as these are pulled in from all-tests-lib
  sourceDirectories: []
  // Manually specify common include directories instead
  cpp.includePaths: base.concat([ path, path + "/common/src", path + "/common/src/builtin" ])

  cpp.defines: base.concat([ "UNIT_TEST", 'TEST_MOC="tst_' + testName + '.moc"' ])
  cpp.combineCxxSources: false

  files: [ "tests/tst_" + testName + ".cpp" ]

  // Override the main test file with a FileTagger so it doesn't get combined,
  // as the auto-moc compiler isn't compatible with combined sources when using
  // classes defined in source files
  FileTagger {
    patterns: "tst_" + testName + ".cpp"
    fileTags: ["cpp"]
    priority: 1000
  }

  // Override so we don't install the test binary
  Group {
    fileTagsFilter: "application"
    qbs.install: false
  }
}
