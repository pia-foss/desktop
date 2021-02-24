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
#line SOURCE_FILE("apiretry.cpp")

#include "apiretry.h"
#include <QElapsedTimer>
#include <QRegularExpression>

namespace
{
    // Timeout for all API requests using the 'counted' retry strategy
    const std::chrono::seconds countedRequestTimeout{5};

    // Accuracy of the timeout value for the 'timed' retry strategy.
    //
    // If a request ends within this time of the timeout, we won't start
    // another request.  If we do start another request, we only allow it to
    // exceed the timeout by this amount.
    //
    // For example, if the timeout given is 10 seconds, and the accuracy is 1
    // second, then the timeout range will be 9-11 seconds - we won't start
    // another request after 9 seconds, and any request will terminate at 11
    // seconds.
    //
    // Additionally, this determines the minimum request time when we're nearing
    // the timeout - the last request started will is given at least 2*accuracy
    // to complete.
    const std::chrono::seconds timedTimeoutAccuracy{1};
}

void ApiResource::trace(QDebug &dbg) const
{
    // If this resource doesn't contain a query string, just trace it as-is
    if(!contains('?'))
    {
        const QString &thisStr{*this};
        dbg << thisStr;
        return;
    }

    // Otherwise, replace parameter sequences to hide their values.  This is
    // very crude but is good enough for the URIs used by PIA.
    QString redacted{*this};
    redacted.replace(QRegularExpression{"=[^&]*"}, QStringLiteral("=..."));
    dbg << redacted;
}

// Counted retry strategy.  Just retries immediately up to a maximum number of
// attempts.
class COMMON_EXPORT CountedApiRetry : public ApiRetry
{
public:
    CountedApiRetry(unsigned maxAttempts);

public:
    virtual std::chrono::milliseconds beginAttempt(const ApiResource &resource) override;
    virtual nullable_t<std::chrono::milliseconds> attemptFailed(const ApiResource &resource) override;

private:
    unsigned _maxAttempts;
    // Count of failed attempts (incremented by attemptFailed()).
    unsigned _failureCount;
};

CountedApiRetry::CountedApiRetry(unsigned maxAttempts)
    : _maxAttempts{maxAttempts}, _failureCount{0}
{
}

auto CountedApiRetry::beginAttempt(const ApiResource &resource)
    -> std::chrono::milliseconds
{
    // Counted attempts always use a fixed retry interval
    qInfo() << "Begin attempt" << (_failureCount+1) << "for resource" << resource;
    return countedRequestTimeout;
}

auto CountedApiRetry::attemptFailed(const ApiResource &resource)
    -> nullable_t<std::chrono::milliseconds>
{
    ++_failureCount;

    if(_failureCount >= _maxAttempts)
    {
        qWarning() << "Request for resource" << resource
            << "failed after" << _failureCount << "attempts";
        return {};
    }

    qInfo() << "Attempt" << _failureCount << "for resource" << resource << "failed, retry";

    // Counted retries always begin immediately
    return std::chrono::milliseconds{0};
}

// Timed retry strategy; used for the VPN IP and port forward requests.
//
// Uses a backing-off interval between requests (with an upper bound) and an
// overall time limit to terminate retries.
//
// Most of the timing parmeters are fixed.  The parameters that can be
// controlled are:
// - "Fast request time" - How long to use the initial retry interval before
//   starting to back off.  Can be 0 to back off for all failures from the
//   beginning.
// - "Max attempt time" - How long to keep trying to fetch the resource, after
//   this time elapses (and any in-flight request completes) the request fails.
class COMMON_EXPORT TimedApiRetry : public ApiRetry
{
public:
    TimedApiRetry(std::chrono::seconds fastRequestTime,
                  std::chrono::seconds maxAttemptTime);

public:
    virtual std::chrono::milliseconds beginAttempt(const ApiResource &resource) override;
    virtual nullable_t<std::chrono::milliseconds> attemptFailed(const ApiResource &resource) override;

private:
    // Time since the first attempt began; used for the overall timeout
    QElapsedTimer _firstAttemptTime;
    // Time since the most recent attempt began; used for the per-request
    // backoff delay.
    QElapsedTimer _thisAttemptTime;
    // Interval to apply to the next request (if one occurs).  Ensures a minimum
    // interval between the beginning of the prior request and the beginning of
    // the next request.
    std::chrono::milliseconds _nextRequestInterval;
    // How long to keep trying "fast" requests (using the initial interval)
    // before slowing down (can be 0)
    std::chrono::seconds _fastRequestTime;
    // Maximum attempt time; after this long no more attempts will begin.
    std::chrono::seconds _maxAttemptTime;
    // Count of failed attempts.  Used for tracing.
    unsigned _failedAttempts;
};

namespace
{
    // Minimum interval between the first and second attempt.  (Compounded with
    // backoffFactor to determine subsequent intervals.)
    // If this is too low, it may cause lots of initial requests to time out
    // when connecting to high-latency regions.
    const std::chrono::seconds _initialInterval{2};
    // Backoff factor for later retries.  The per-attempt interval is multiplied
    // by this factor for each attempt (up to _maxInterval).
    const int backoffFactor{2};
    // Maximum interval between attempts.
    const std::chrono::seconds _maxInterval{10};
}

TimedApiRetry::TimedApiRetry(std::chrono::seconds fastRequestTime,
                             std::chrono::seconds timeout)
    : _nextRequestInterval{_initialInterval}, _fastRequestTime{fastRequestTime},
      _maxAttemptTime{timeout - timedTimeoutAccuracy}, _failedAttempts{0}
{
}

auto TimedApiRetry::beginAttempt(const ApiResource &resource)
    -> std::chrono::milliseconds
{
    // Keep track of the first attempt time to terminate retries after the total
    // time elapses
    if(!_firstAttemptTime.isValid())
        _firstAttemptTime.start();
    _thisAttemptTime.start();

    // Only allow overshooting the timeout by timedTimeoutAccuracy
    // (limit to _maxAttemptTime + 2*timedTimeoutAccuracy)
    // Reduce the request's time if it would exceed this limit
    std::chrono::milliseconds attemptTimeout =
        _maxAttemptTime + 2*timedTimeoutAccuracy -
        std::chrono::milliseconds{_firstAttemptTime.elapsed()};
    if(attemptTimeout < _nextRequestInterval)
    {
        qInfo() << "Reduce limit for attempt" << (_failedAttempts+1) << "for"
            << resource << "to" << traceMsec(attemptTimeout) << "from"
            << traceMsec(_nextRequestInterval) << "- would have exceeded limit";
    }
    else
    {
        // The full interval is allowed, use the next interval.
        attemptTimeout = _nextRequestInterval;
    }

    qInfo() << "Starting request" << (_failedAttempts+1) << "for resource"
        << resource << "with timeout" << traceMsec(attemptTimeout);
    return attemptTimeout;
}

auto TimedApiRetry::attemptFailed(const ApiResource &resource)
    -> nullable_t<std::chrono::milliseconds>
{
    ++_failedAttempts;

    // Figure out the delay for this request.  Apply this delay from the
    // _beginning_ of the prior request - if the request took a long time to
    // fail, we might not actually need any delay this time.
    //
    // This method of applying the interval is a tad imprecise, but that's fine
    // for this purpose.
    std::chrono::milliseconds thisRequestDelay{0};
    std::chrono::milliseconds priorElapsed{_thisAttemptTime.elapsed()};
    if(_nextRequestInterval > priorElapsed)
        thisRequestDelay = _nextRequestInterval - priorElapsed;
    // Otherwise, the remaining delay for this request is 0 because the
    // interval has already elapsed.

    // If the maximum attempt time will have elapsed when we would start the
    // next request, we're done.
    std::chrono::milliseconds firstAttemptElapsed{_firstAttemptTime.elapsed()};
    if(firstAttemptElapsed + thisRequestDelay >= _maxAttemptTime)
    {
        qWarning() << "Request for resource" << resource << "failed after"
            << traceMsec(firstAttemptElapsed)
            << "(" << _failedAttempts << "attempts)";
        return {};
    }

    // Update the next interval
    if(firstAttemptElapsed >= _fastRequestTime)
    {
        _nextRequestInterval *= backoffFactor;
        if(_nextRequestInterval > _maxInterval)
            _nextRequestInterval = _maxInterval;
    }

    // Trace the next delay
    if(thisRequestDelay > std::chrono::milliseconds{0})
    {
        qInfo() << "Wait" << traceMsec(thisRequestDelay)
            << "before attempt" << (_failedAttempts+1) << "for" << resource
            << "(" << traceMsec(_nextRequestInterval) << "total delay)";
    }
    else
    {
        qInfo() << "Interval of" << traceMsec(_nextRequestInterval)
            << "has already elapsed for attempt" << (_failedAttempts+1) << "for"
            << resource << "(" << traceMsec(priorElapsed) << "elapsed)";
    }

    return thisRequestDelay;
}

std::unique_ptr<ApiRetry> ApiRetries::counted(unsigned maxAttempts)
{
    return std::make_unique<CountedApiRetry>(maxAttempts);
}

std::unique_ptr<ApiRetry> ApiRetries::timed(std::chrono::seconds fastRequestTime,
                                            std::chrono::seconds maxAttemptTime)
{
    return std::make_unique<TimedApiRetry>(fastRequestTime, maxAttemptTime);
}
