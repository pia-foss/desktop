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
import qbs.Environment

Module {
  // PiaProject reads SOURCE_DATE_EPOCH to detect the effective timestamp if it
  // is set.  However, we also need to set QT_RCC_SOURCE_DATE_OVERRIDE for rcc,
  // and we set SOURCE_DATE_EPOCH if it wasn't already set to the detected time.
  //
  // This is complex in qbs because we need to detect the timestamp in the
  // Project (we need it in project properties, and Projects can't depend on
  // Modules), but we can't set build environment variables there - only a
  // Module can do that.
  //
  // To do this, PiaProject detects the current timestamp using
  // SOURCE_DATE_EPOCH if it's already set, and piabuildenv uses that value to
  // set QT_RCC_SOURCE_DATE_OVERRIDE/SOURCE_DATE_EPOCH.
  //
  // Products that depend on these environment variables must depend on this
  // module.

  setupBuildEnvironment: {
    // rcc doesn't support SOURCE_DATE_EPOCH in Qt <= 5.12, set this environment
    // variable that it does use.
    Environment.putEnv("QT_RCC_SOURCE_DATE_OVERRIDE", project.timestamp)

    // If SOURCE_DATE_EPOCH wasn't set, set it too.  If it was set, this should
    // have no effect, because PiaProject should have used this value to set
    // project.timestamp.
    Environment.putEnv("SOURCE_DATE_EPOCH", project.timestamp)
  }
}
