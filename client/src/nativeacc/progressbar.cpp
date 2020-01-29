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
#line SOURCE_FILE("progressbar.cpp")

#include "progressbar.h"

namespace NativeAcc {

ProgressBarAttached::ProgressBarAttached(QQuickItem &item)
    : AccessibleItem{QAccessible::Role::ProgressBar, item}, _minimum{0.0},
      _maximum{100.0}, _value{0.0}
{
    // Always read-only
    setState(StateField::readOnly, true);
}

void ProgressBarAttached::setMinimum(double minimum)
{
    if(minimum != _minimum)
    {
        _minimum = minimum;
        emit minimumChanged();
        // There doesn't appear to be an accessibility event for the range.
    }
}

void ProgressBarAttached::setMaximum(double maximum)
{
    if(maximum != _maximum)
    {
        _maximum = maximum;
        emit maximumChanged();
        // There doesn't appear to be an accessibility event for the range.
    }
}

void ProgressBarAttached::setValue(double value)
{
    if(value != _value)
    {
        _value = value;
        emit valueChanged();
        if(accExists())
        {
            QAccessibleValueChangeEvent change{item(), value};
            QAccessible::updateAccessibility(&change);
        }
    }
}

}
