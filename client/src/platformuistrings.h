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

#include "common.h"
#line HEADER_FILE("platformuistrings.h")

#ifndef PLATFORMUISTRINGS_H
#define PLATFORMUISTRINGS_H

// This file defines UI strings for platform-specific code.
// These are here so they always end up in *.ts files, even when not building on
// that platform.
//
// TODO: We need to find a better solution for this.  Some possibilities
// include:
// - Don't update the in-tree *.ts files on build, instead collect the updated
//   files as build artifacts and check them in once they're translated.  We
//   could leave translatable strings in platform files; we'd have to combine
//   all the build artifacts for the various platform builds in a
//   post-processing step.
// - Find a way to include all platform source files in the `lupdate` run
//   without compiling them.  It's possible to do this in the QBS script, but
//   `lupdate` seems to have issues parsing the Windows source files (it
//   consistently issues nuisance errors on Mac, it issues them inconsistently
//   on Windows).
// - Find a way to prevent `lupdate` from removing strings from the in-tree
//   files during the build.  Maybe just run `lupdate` to generate the build
//   artifact *.ts files, then integrate the new strings into the in-tree files
//   in a post-processing step.
namespace PlatformUIStrings {

// Tray context menu strings for Mac OS
QString macTrayAccShowMenu();

}

#endif
