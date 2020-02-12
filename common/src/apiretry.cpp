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
#line SOURCE_FILE("apiretry.cpp")

#include "apiretry.h"
#include <QElapsedTimer>
#include <QRegularExpression>

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
    virtual nullable_t<std::chrono::milliseconds> beginNextAttempt(const ApiResource &resource) override;

private:
    unsigned _maxAttempts, _attemptCount;
};

CountedApiRetry::CountedApiRetry(unsigned maxAttempts)
    : _maxAttempts{maxAttempts}, _attemptCount{0}
{
}

auto CountedApiRetry::beginNextAttempt(const ApiResource &resource)
    -> nullable_t<std::chrono::milliseconds>
{
    if(_attemptCount >= _maxAttempts)
    {
        qWarning() << "Request for resource" << resource
            << "failed after" << _attemptCount << "attempts";
        return {};
    }

    if(_attemptCount > 0)
        qInfo() << "Attempt" << _attemptCount << "for resource" << resource << "failed, retry";

    ++_attemptCount;
    return std::chrono::milliseconds{0};
}

// VPN IP-style timed retry strategy.
//
// Uses a backing-off interval between requests (with an upper bound) and an
// overall time limit to terminate retries.
//
// The timing parameters are fixed for the VPN IP request because that's the
// only way this is used currently, but it could be generalized for other uses.
class COMMON_EXPORT TimedApiRetry : public ApiRetry
{
public:
    TimedApiRetry(const std::chrono::seconds &maxAttemptTime);

public:
    virtual nullable_t<std::chrono::milliseconds> beginNextAttempt(const ApiResource &resource) override;

private:
    // Time since the first attempt began; used for the overall timeout
    QElapsedTimer _firstAttemptTime;
    // Time since the most recent attempt began; used for the per-request
    // backoff delay.
    QElapsedTimer _thisAttemptTime;
    // Actual delay that we returned for the prior request.
    std::chrono::milliseconds _priorRequestDelay;
    // Interval to apply to the next request (if one occurs).  Ensures a minimum
    // interval between the beginning of the prior request and the beginning of
    // the next request.
    std::chrono::milliseconds _nextRequestInterval;
    // Maximum attempt time; after this long no more attempts will begin.
    std::chrono::seconds _maxAttemptTime;
    // Count of attempts.  Used for tracing.
    unsigned _attemptCount;
};

namespace
{
    // Minimum interval between the first and second attempt.  (Compounded with
    // backoffFactor to determine subsequent intervals.)
    const std::chrono::seconds _initialInterval{1};
    // Backoff factor for later retries.  The per-attempt interval is multiplied
    // by this factor for each attempt (up to _maxInterval).
    const int backoffFactor{2};
    // Maximum interval between attempts.
    const std::chrono::seconds _maxInterval{30};
}

TimedApiRetry::TimedApiRetry(const std::chrono::seconds &maxAttemptTime)
    : _priorRequestDelay{0}, _nextRequestInterval{_initialInterval},
      _maxAttemptTime{maxAttemptTime}, _attemptCount{0}
{
}

auto TimedApiRetry::beginNextAttempt(const ApiResource &resource)
    -> nullable_t<std::chrono::milliseconds>
{
    // First attempt - just start the timers and proceed
    if(!_firstAttemptTime.isValid())
    {
        _firstAttemptTime.start();
        _thisAttemptTime.start();
        _attemptCount = 1;
        return std::chrono::milliseconds{0};
    }

    // If the maximum attempt time has elapsed, we're done.
    qint64 firstAttemptElapsedMs = _firstAttemptTime.elapsed();
    if(firstAttemptElapsedMs >= msec(_maxAttemptTime))
    {
        qWarning() << "Request for resource" << resource << "failed after"
            << traceMsec(firstAttemptElapsedMs)
            << "(" << _attemptCount << "attempts)";
        return {};
    }

    ++_attemptCount;

    // Figure out the delay for this request.  Apply this delay from the
    // _beginning_ of the prior request - if the request took a long time to
    // fail, we might not actually need any delay this time.
    //
    // This method of applying the interval is a tad imprecise, but that's fine
    // for this purpose.
    std::chrono::milliseconds thisRequestDelay{0};
    std::chrono::milliseconds priorElapsed{_thisAttemptTime.restart()};
    // The elapsed time includes the delay before the request was actually
    // started; subtract that to get the (approximate) time spent on this
    // request.
    priorElapsed -= _priorRequestDelay;
    if(_nextRequestInterval > priorElapsed)
    {
        thisRequestDelay = _nextRequestInterval - priorElapsed;
        qInfo() << "Wait" << traceMsec(msec(thisRequestDelay))
            << "before attempt" << _attemptCount << "for" << resource
            << "(" << traceMsec(msec(_nextRequestInterval)) << "total delay)";
    }
    else
    {
        // Otherwise, the remaining delay for this request is 0 because the
        // interval has already elapsed.
        qInfo() << "Interval of" << traceMsec(msec(_nextRequestInterval))
            << "has already elapsed for attempt" << _attemptCount << "for"
            << resource << "(" << traceMsec(msec(priorElapsed)) << "elapsed)";
    }

    // Update the next interval
    _nextRequestInterval *= backoffFactor;
    if(_nextRequestInterval > _maxInterval)
        _nextRequestInterval = _maxInterval;

    _priorRequestDelay = thisRequestDelay;
    return _priorRequestDelay;
}

std::unique_ptr<ApiRetry> ApiRetries::counted(unsigned maxAttempts)
{
    return std::make_unique<CountedApiRetry>(maxAttempts);
}

std::unique_ptr<ApiRetry> ApiRetries::timed(std::chrono::seconds maxAttemptTime)
{
    return std::make_unique<TimedApiRetry>(maxAttemptTime);
}
