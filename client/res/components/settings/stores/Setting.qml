// Copyright (c) 2022 Private Internet Access, Inc.
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

import QtQuick 2.0
QtObject {
  // Anything assigned to sourceValue will also be assigned to currentValue, but
  // sourceValue does not get overwritten by user interactions, i.e. any binding
  // is preserved.
  property var sourceValue: undefined

  // Read/write property holding the current value either received from the
  // source or selected by the user. Will be overwritten with non-bound values.
  property var currentValue: sourceValue

  // Explicitly bind so we keep receiving source updates even after currentValue
  // has been overwritten.
  onSourceValueChanged: {
    if (currentValue !== sourceValue) {
      currentValue = sourceValue;
    }
  }
}
