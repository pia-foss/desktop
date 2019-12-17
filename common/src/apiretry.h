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

#include "common.h"
#line HEADER_FILE("apiretry.h")

#ifndef APIRETRY_H
#define APIRETRY_H

#include <memory>
#include <chrono>

// ApiRetry - interface to a particular retry strategy.
//
// Retry strategies can choose when to stop retrying and/or delay successive
// retries.  For example:
// - A "counted" retry strategy just limits to a fixed number of retries.
//   (With limit=1, there will be no retries.)
// - A "timed" retry strategy limits to a maximum time since the first request
//   and uses a backing-off delay for successive requests.
//
// These are used by NetworkTaskWithRetry.
class COMMON_EXPORT ApiRetry
{
public:
    virtual ~ApiRetry() = default;

public:
    // Begin the next attempt, if possible.  (Called immediately after
    // construction for the first attempt).  If another attempt is allowed,
    // return the amount of time to wait before making the request - 0 for no
    // delay.  If no more attempts are allowed, return an empty nullable.
    //
    // The immplementation should normally log for attempts after the first to
    // indicate that a retry is allowed or that all attempts have been
    // exhausted and the request will fail (and in either case, why).
    //
    // The resource path is provided just for tracing, it shouldn't affect the
    // attempt behavior.
    virtual nullable_t<std::chrono::milliseconds> beginNextAttempt(const QString &resource) = 0;
};

namespace ApiRetries
{
    // Create a counted retry strategy with the given number of attempts.  There
    // is no delay between each attempt.
    std::unique_ptr<ApiRetry> COMMON_EXPORT counted(unsigned maxAttempts);

    // Create a timed retry strategy with timing factors tuned for the VPN IP
    // address request.
    std::unique_ptr<ApiRetry> COMMON_EXPORT vpnIpTimed();
};

#endif
