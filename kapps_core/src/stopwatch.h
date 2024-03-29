// Copyright (c) 2024 Private Internet Access, Inc.
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

#pragma once
#include <chrono>

namespace kapps { namespace core {

// Stopwatch measures the elapsed time from a given starting point.
class KAPPS_CORE_EXPORT Stopwatch
{
private:
    using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
public:
    Stopwatch() : _start{std::chrono::steady_clock::duration{-1}} {}

public:
    bool running() const {return _start >= TimePoint{};}
    explicit operator bool() const {return running();}
    bool operator!() const {return !running();}

    void reset() { *this = {}; }
    void start() { _start = std::chrono::steady_clock::now(); }

    std::chrono::milliseconds elapsed() const
    {
        auto clockElapsed = std::chrono::steady_clock::now() - _start;
        return std::chrono::duration_cast<std::chrono::milliseconds>(clockElapsed);
    }

private:
    TimePoint _start;
};

}}
