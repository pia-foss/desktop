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

import QtQuick 2.11
import PIA.NativeAcc 1.0 as NativeAcc
import "../core"

// StaticText is a Text that has a StaticText accessible role, and sets its name
// to the displayed text.
//
// This should only be used with plain text - don't use HTML in accessibility
// annotations.  (StaticHtml can be used if the markup is simple and removable.)
//
// This control is for fixed text that rarely changes and is not associated with
// a nearby control (such as the message in MessageWithLinks, the changelog
// text, etc.)  For other uses:
// - Use LabelText for a fixed text that labels a nearby control (that has
//   the label text as its own name)
// - Use ValueText for a "value indicator" text that changes frequently
Text {
  NativeAcc.Text.name: text
}
