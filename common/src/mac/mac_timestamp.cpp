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
#line SOURCE_FILE("mac_timestamp.cpp")

#include <mach/mach.h>
#include <mach/mach_time.h>

namespace
{
    class MachTimebaseInfoValue
    {
    public:
        MachTimebaseInfoValue()
        {
            mach_timebase_info(&_value);
        }
    public:
        mach_timebase_info_data_t _value;
    } machTimebaseInfo;
}

quint64 getContinuousTimestamp()
{
    return mach_continuous_time();
}

std::chrono::milliseconds elapsedBetweenTimestamps(quint64 start, quint64 end)
{
    // Use floating point arithmetic to avoid possible overflow
    double nsec = static_cast<double>(end - start);
    nsec *= machTimebaseInfo._value.numer;
    nsec /= machTimebaseInfo._value.denom;
    std::chrono::nanoseconds rounded{static_cast<std::uint64_t>(nsec)};
    return std::chrono::duration_cast<std::chrono::milliseconds>(rounded);
}
