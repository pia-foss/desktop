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

#include "common.h"
#line HEADER_FILE("apiretry.h")

#ifndef APIRETRY_H
#define APIRETRY_H

#include <memory>
#include <chrono>

// ApiResource is just a string that may contain sensitive query parameters.
// When traced, any query parameters are redacted.
// It may be a complete URI, just a query string, etc.; tracing just looks for
// ?val=<....>&foo=<....>.
class COMMON_EXPORT ApiResource : public QString
{
public:
    using QString::QString;
    ApiResource(const ApiResource &) = default;
    ApiResource(ApiResource &&) = default;
    ApiResource(const QString &val) : QString{val} {}
    ApiResource(QString &&val) : QString{std::move(val)} {}

public:
    void trace(std::ostream &os) const;
};

inline std::ostream &operator<<(std::ostream &os, const ApiResource &val)
{
    val.trace(os);
    return os;
}

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
    // Begin an attempt.  Called immediately after construction for the first
    // attempt, or following a call to attemptFailed() (with a successful
    // result) for subsequent attempts.
    //
    // Return the timeout for this attempt - how long we are able to wait for a
    // response.
    //
    // The resource path is provided just for tracing, it shouldn't affect the
    // attempt behavior.
    virtual std::chrono::milliseconds beginAttempt(const ApiResource &resource) = 0;

    // An attempt failed.  This can be called before or after the timeout
    // returned by beginAttempt() (the request could fail faster, or there could
    // be a delay handling an elapsed timeout interval).
    //
    // If another attempt is allowed, return the delay time before we should
    // start that attempt (which can be 0 to start immediately).  beginAttempt()
    // will be called after this interval elapses.
    //
    // If all attempts have been exhausted, return an empty nullable_t to
    // terminate the retry sequence.
    //
    // The resource path is provided just for tracing, it shouldn't affect the
    // attempt behavior.
    virtual nullable_t<std::chrono::milliseconds> attemptFailed(const ApiResource &resource) = 0;
};

namespace ApiRetries
{
    // Create a counted retry strategy with the given number of attempts.  There
    // is no delay between each attempt.
    std::unique_ptr<ApiRetry> COMMON_EXPORT counted(unsigned maxAttempts);

    // Create a timed retry strategy with timing factors tuned for the VPN IP
    // address request.
    std::unique_ptr<ApiRetry> COMMON_EXPORT timed(std::chrono::seconds fastRequestTime,
                                                  std::chrono::seconds maxAttemptTime);
};

#endif
