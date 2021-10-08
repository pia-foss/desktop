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

#include "common.h"
#line HEADER_FILE("elapsedtime.h")

#ifndef ELAPSEDTIME_H
#define ELAPSEDTIME_H

#include "timestamp.h"

// ContinuousElapsedTime measures the elapsed time since a given point,
// including time the system has spent asleep.
//
// ContinuousElapsedTime measures from the point when it is constructed.  To
// be able to stop/start/reset the timer, use a nullable_t<ContinuousElapsedTime>
class COMMON_EXPORT ContinuousElapsedTime
{
public:
    ContinuousElapsedTime();

public:
    std::chrono::milliseconds elapsed() const;

private:
    quint64 _startTimestamp;
};

#endif
