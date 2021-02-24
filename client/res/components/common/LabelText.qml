// Copyright (c) 2021 Private Internet Access, Inc.
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

import QtQuick 2.11
import PIA.NativeAcc 1.0 as NativeAcc
import "../core"

// LabelText is a Text that acts as a label for a neighboring control.  It has
// the StaticText accessible role and sets is name to its displayed text.
//
// This should only be used for an element with a neighboring control that is
// itself named with this text.  These elements are hidden on Mac OS, since
// VoiceOver reliably reads control titles (see TextAttached's definition for
// more details).  Other platforms' screen readers do not read control titles
// reliably, but they also skip these elements by default since they are not
// tab stops.
//
// For other uses:
// - Use StaticText for a text element that does not label a nearby control.
// - Use ValueText for a "value indicator" text that changes frequently.
Text {
  NativeAcc.Label.name: text
}
