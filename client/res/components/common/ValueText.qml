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
import "../core"
import PIA.NativeAcc 1.0 as NativeAcc

// ValueText is just a Text that has a StaticText accessible role, sets its
// value to the displayed text, and provides a 'label' property.
//
// The 'label' property should be bound to the text displayed in the labeling
// StaticText, or to a text string describing a labeling image, etc.  It must be
// non-empty to have any accessibility annotation.
Text {
  property string label

  NativeAcc.ValueText.name: label
  NativeAcc.ValueText.value: text
}
