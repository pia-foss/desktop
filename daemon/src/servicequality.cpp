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

#include "servicequality.h"
#include <kapps_core/src/uuid.h>
#include <chrono>
#include <array>
#include <QRandomGenerator>

namespace
{
    // How frequently we rotate the aggregation ID
    std::chrono::hours _rotateIdTime{24};
    // Resolution of event timestamps reported to API
    std::chrono::hours _timestampResolution{1};
    // We want to be able to send a partial batch early if we haven't generated
    // any events in a while, since for users that don't connect/disconnect
    // frequently it can take a long time to accumulate 20 events.
    //
    // However:
    // - we only store hour-granularity timestamps (_timestampResolution)
    // - we don't want to hammer the web API with a bunch of requests all
    //   aligned to "0 minutes past the hour" every hour
    //
    // Therefore, a random interval between 90-150 minutes is used for each
    // early-send interval.
    //
    // The timeout must always be measurably greater than _timestampResolution,
    // otherwise we might send an event generated at (say) 3:59 immediately,
    // since it's stored as 3:00 and 59 minutes seem to already have passed.
    //
    // We want to cover the full "minutes past the hour" range so requests are
    // uniformly distributed.  The interval is generated randomly for each
    // batch so that it can't be used to correlate requests from the same
    // client.
    std::chrono::minutes _minEarlySendTime{90};
    std::chrono::minutes _maxEarlySendTime{150};

    enum : size_t
    {
        // Batch size when we'll try to send a group of events.
        EventBatchSize = 20,
        // If a batch can't be sent, we'll try again a few times after the next
        // few events.  We don't do this for every event in case it's failing
        // all the time.
        //
        // Event batches are more prone to failure when in the "connecting"
        // state since we have usually blocked DNS in this state (meta APIs
        // should still work, but the fallback to the web origin won't).  This
        // tolerance avoids correlating effects if event batches consistently
        // fail in a particular state (e.g if every 20th event was an "attempt"
        // event so we were only trying to send in the "connecting" state, we
        // might never be able to send events otherwise.)
        EventBatchRetryCount = 3,
        // Maximum batch size; if we fail to send events for so long that this
        // many events accumulate, we skip sending the oldest events.
        EventBatchMaxSize = 100,
    };

    // We use a Qt::TimerType::VeryCoarseTimer (1-second accuracy) because
    // Qt::TimerType::CoarseTimer is actually coarser for large intervals
    // (fuzzed by up to 5% of the interval).  When a timer elapses, expiration
    // times that we check might still be slightly in the future, use a
    // tolerance so we expire these slightly early rather than setting more
    // timers until they elapse.
    std::chrono::seconds _timerTolerance{2};

    QString genUuidV4Str()
    {
        std::array<std::uint64_t, 2> data{};
        QRandomGenerator::global()->fillRange(data.data(), data.size());
        const auto &uuid = kapps::core::Uuid::buildV4(data[0], data[1]);
        return QString::fromStdString(uuid.toString());
    }
}

ServiceQuality::ServiceQuality(ApiClient &apiClient, Environment &environment,
                               DaemonAccount &account, DaemonData &data,
                               bool enabled,
                               nullable_t<SemVersion> version)
    : _apiClient{apiClient}, _environment{environment}, _account{account},
      _data{data}, _enabled{enabled}, _sendingBatchSize{0}, _consentVersion{version}
{
    // Names used for tracing in startExpireTimer()
    _rotateIdTimer.setObjectName(QStringLiteral("aggregation ID rotate timer"));
    _earlySendTimer.setObjectName(QStringLiteral("early send timer"));
    _rotateIdTimer.setSingleShot(true);
    _earlySendTimer.setSingleShot(true);

    connect(&_rotateIdTimer, &QTimer::timeout, this,
            &ServiceQuality::onRotateIdElapsed);
    connect(&_earlySendTimer, &QTimer::timeout, this,
            &ServiceQuality::onEarlySendElapsed);

    generateEarlySendTime();

    // Whenever the aggregation ID rotate time changes, set our timer so we'll
    // actually rotate it at that time.
    connect(&_data, &DaemonData::qualityAggIdRotateTimeChanged, this,
            &ServiceQuality::onAggIdRotateTimeChanged);
    onAggIdRotateTimeChanged();
    // Whenever the queued events change, update the early-send timer
    connect(&_data, &DaemonData::qualityEventsQueuedChanged, this,
            &ServiceQuality::onQueuedEventsChanged);
    onQueuedEventsChanged();

    // If service quality is enabled, check/rotate ID
    if(_enabled)
    {
        qInfo() << "Service quality events are enabled";
        start();
    }
    // Otherwise, service quality isn't enabled, make sure DaemonData is cleared
    else
    {
        qInfo() << "Service quality events are disabled";
        stop();
    }
}

std::chrono::milliseconds ServiceQuality::timeToExpire(
    const sysTimeMs &timestamp,
    const std::chrono::milliseconds &retention,
    const sysTimeMs &now) const
{
    return (timestamp + retention) - now;
}

bool ServiceQuality::hasExpired(const sysTimeMs &timestamp,
                                const std::chrono::milliseconds &retention,
                                const sysTimeMs &now) const
{
    return timeToExpire(timestamp, retention, now) <= _timerTolerance;
}

void ServiceQuality::startExpireTimer(QTimer &timer, const sysTimeMs &timestamp,
                                      const std::chrono::milliseconds &retention,
                                      const sysTimeMs &now) const
{
    auto expireTime = msec(timeToExpire(timestamp, retention, now));
    if(expireTime < 1)
    {
        qWarning() << "Next expiration for" << timer.objectName()
            << "seems to have already passed";
        expireTime = 1;
    }
    qInfo() << "Starting" << timer.objectName() << "with expiration in"
        << traceMsec(expireTime);
    timer.start(expireTime);
}

void ServiceQuality::generateId(const sysTimeMs &now)
{
    _data.qualityAggId(genUuidV4Str());
    // Set the new rotate time.  This will reset our timer since we detect when
    // this changes in DaemonData.
    _data.qualityAggIdRotateTime(msec((now + _rotateIdTime).time_since_epoch()));
    qInfo() << "Generated a new aggregation ID";

    // Since we generated a new ID, check if we need to roll off any events that
    // used the prior ID.
    rollOffEvents();
}

void ServiceQuality::generateEarlySendTime()
{
    _earlySendTime = std::chrono::milliseconds{QRandomGenerator::system()->bounded(
        static_cast<int>(msec(_minEarlySendTime)),
        static_cast<int>(msec(_maxEarlySendTime)))};
}

void ServiceQuality::rollOffEvents()
{
    const auto &currentAggId = _data.qualityAggId();
    auto sent{_data.qualityEventsSent()};
    std::size_t oldSentCount{sent.size()};
    // Roll off events that have been sent and no longer match the current
    // aggregation ID.  (Stop checking if we find the current aggregation ID,
    // because all subsequent events would have that ID too.)
    while(!sent.empty() && sent.front().aggregated_id() != currentAggId)
    {
        sent.pop_front();
    }

    // Don't check queued - queued events do not roll off until they're sent or
    // the batch overflows the limit.

    if(oldSentCount != sent.size())
    {
        qInfo() << "Rolled off" << (oldSentCount - sent.size())
            << "events from history";
        _data.qualityEventsSent(std::move(sent));
    }
}

void ServiceQuality::start()
{
    const sysTimeMs now{nowMs()};

    // Check if we have an aggregation ID or if it has expired
    if(_data.qualityAggId().isEmpty() ||
       hasExpired(timeFromMsec(_data.qualityAggIdRotateTime()),
                  std::chrono::milliseconds{0}, now))
    {
        generateId(now);
    }
}

void ServiceQuality::stop()
{
    // Discard a last attempt if there is one.  This means we won't send an
    // event indicating the result of that attempt, but events are now turned
    // off so we will just have to leave it unresolved.
    _pLastAttempt.clear();

    // Clear everything in DaemonData.  The changes observed here cause us to
    // stop the timers.
    _data.qualityAggId({});
    _data.qualityAggIdRotateTime(0);
    _data.qualityEventsQueued({});
    _data.qualityEventsSent({});
}

void ServiceQuality::onAggIdRotateTimeChanged()
{
    if(_data.qualityAggIdRotateTime())
    {
        // Set a new timer for the new expiration time
        startExpireTimer(_rotateIdTimer, timeFromMsec(_data.qualityAggIdRotateTime()),
                         std::chrono::milliseconds{0});
    }
    else
    {
        // The time was cleared (the ID would also have been cleared), just stop
        // the timer.
        _rotateIdTimer.stop();
        qInfo() << "Stopped" << _rotateIdTimer.objectName();
    }
}

void ServiceQuality::onQueuedEventsChanged()
{
    sysTimeMs newestQueuedEventTime;
    const auto &queued{_data.qualityEventsQueued()};
    if(!queued.empty())
        newestQueuedEventTime = timeFromSec(queued.back().event_time());

    // If there are any events, start the expire timer based on the newest one.
    // If we don't generate any more events before this timer elapses, we'll go
    // ahead and send a partial batch.
    if(newestQueuedEventTime != sysTimeMs{})
    {
        startExpireTimer(_earlySendTimer, newestQueuedEventTime,
                         _earlySendTime);
    }
    else
    {
        _earlySendTimer.stop();  // No events to send
        qInfo() << "Stopped" << _earlySendTimer.objectName();
    }
}

void ServiceQuality::onRotateIdElapsed()
{
    generateId(nowMs());
}

void ServiceQuality::onEarlySendElapsed()
{
    // The early-send timer has elapsed.  If this attempt fails, no more
    // early-send attempts will occur until a new event is queued (which changes
    // the queued events, causing us to reset the timer).
    //
    // (If this succeeds without any new events being queued, we also detect a
    // change in the queued events, but then the queued events are empty so we
    // still don't restart the timer.)
    qInfo() << "Sending partial event batch, no events have been generated recently";
    // Pick a new early-send interval so the next early-send timer will use a
    // different interval.  We don't need to update the timer since it isn't
    // running at this point anyway.
    generateEarlySendTime();
    sendBatch();
}

void ServiceQuality::moveQueuedToSent(std::size_t count)
{
    auto queued{_data.qualityEventsQueued()};
    auto sent{_data.qualityEventsSent()};
    while(count && !queued.empty())
    {
        sent.push_back(std::move(queued.front()));
        queued.pop_front();
        --count;
    }
    _data.qualityEventsQueued(std::move(queued));
    _data.qualityEventsSent(std::move(sent));

    // It's unlikely but possible that some of the events we just moved to
    // sent should actually roll off.  This could happen if events stay in
    // 'queued' so long that the ID has already rotated by the time they are
    // sent.
    rollOffEvents();
}

void ServiceQuality::sendBatch()
{
    if(_sendingBatchSize)
    {
        qInfo() << "A batch is already being sent, not sending another now";
        return; // There's currently a request in flight
    }

    const auto &origQueued{_data.qualityEventsQueued()};

    if(origQueued.empty())
    {
        qInfo() << "There are no events to send";
        return; // No events to send
    }

    // If too many events have accumulated, discard the oldest ones.
    if(origQueued.size() > EventBatchMaxSize)
    {
        std::size_t skipCount{origQueued.size() - EventBatchMaxSize};
        qWarning() << "Skipping" << skipCount
            << "events that could not be sent";
        moveQueuedToSent(skipCount);
    }

    // The queued events changed if we skipped some above
    const auto &queued{_data.qualityEventsQueued()};
    _sendingBatchSize = queued.size();

    qInfo() << "Sending" << _sendingBatchSize << "events now";

    QJsonArray queuedJson{};
    for(const auto &queuedEvent : queued)
        queuedJson.push_back(queuedEvent.toJsonObject());

    QJsonObject serviceQualityPayload;
    serviceQualityPayload.insert(QStringLiteral("events"), queuedJson);

    _apiClient.postRetry(*_environment.getApiv2(), "service-quality",
                         QJsonDocument{serviceQualityPayload},
                         ApiClient::autoAuth(_account.username(), _account.password(),
                                             _account.token()))
        ->notify(this, [this](const Error &err, const QJsonDocument &)
        {
            if(err)
            {
                qWarning() << "Failed to post" << _sendingBatchSize << "events:" << err;
            }
            else
            {
                qInfo() << "Successfully posted" << _sendingBatchSize << "events";
                // Events were posted, move them from queued to sent
                moveQueuedToSent(_sendingBatchSize);
            }
            _sendingBatchSize = 0;  // The request completed
        });
}

bool ServiceQuality::storeEvent(EventType type, VpnProtocol protocol,
                                ConnectionSource source)
{
    if(!_enabled)
        return false; // Ignore the event
    // ServiceQuality always ensures that qualityAggId() is set if and only if
    // service events are enabled, but Daemon can also alter DaemonData
    if(_data.qualityAggId().isEmpty())
    {
        qWarning() << "Service quality events are enabled, but aggregation ID is not set, ignoring event";
        return false; // Never generate events with a blank aggregation ID
    }

    qInfo() << "Event:" << traceEnum(type) << traceEnum(protocol)
        << traceEnum(source);

    ServiceQualityEvent newEvent;
    newEvent.aggregated_id(_data.qualityAggId());
    newEvent.event_unique_id(genUuidV4Str());
    switch(type)
    {
        case EventType::ConnectionAttempt:
            newEvent.event_name(QStringLiteral("VPN_CONNECTION_ATTEMPT"));
            break;
        case EventType::ConnectionEstablished:
            newEvent.event_name(QStringLiteral("VPN_CONNECTION_ESTABLISHED"));

            // If the user provides consent for service quality metrics after v3.3.0
            // include the time to connect metric, otherwise do not include it.
            if (_consentVersion && *_consentVersion >= SemVersion(3, 3))
                newEvent.event_properties().time_to_connect((float)_connTimer.elapsed() / 1000);
            break;
        case EventType::ConnectionCanceled:
            newEvent.event_name(QStringLiteral("VPN_CONNECTION_CANCELED"));
            break;
    }
    qint64 eventTimeMs{nowMs().time_since_epoch().count()};
    // Report hourly granularity only
    eventTimeMs -= eventTimeMs % msec(_timestampResolution);
    // event_time is in seconds, unlike most of our other timestamps, which use
    // milliseconds.
    newEvent.event_time(eventTimeMs / 1000);
    switch(protocol)
    {
        case VpnProtocol::OpenVPN:
            newEvent.event_properties().vpn_protocol(QStringLiteral("OpenVPN"));
            break;
        case VpnProtocol::WireGuard:
            newEvent.event_properties().vpn_protocol(QStringLiteral("WireGuard"));
            break;
    }
    switch(source)
    {
        case ConnectionSource::Manual:
            newEvent.event_properties().connection_source(QStringLiteral("Manual"));
            break;
        case ConnectionSource::Automatic:
            newEvent.event_properties().connection_source(QStringLiteral("Automatic"));
            break;
    }

    // Add the event to the queued events
    auto queued{_data.qualityEventsQueued()};
    queued.push_back(std::move(newEvent));
    _data.qualityEventsQueued(std::move(queued));


    // Try to send whenever we reach a multiple of the batch size, and also for
    // the next few events as retries (see EventBatchRetryCount).  If all
    // retries fail, we stop trying for a while until another batch has
    // accumulated.
    if(_data.qualityEventsQueued().size() >= EventBatchSize &&
        _data.qualityEventsQueued().size() % EventBatchSize <= EventBatchRetryCount)
    {
        sendBatch();
    }

    // An event was stored (even if we didn't need to send a batch now)
    return true;
}

void ServiceQuality::enable(bool enabled, nullable_t<SemVersion> version)
{
    _consentVersion = version;
    if(enabled == _enabled)
    {
        qInfo() << "Service quality events already"
            << (enabled ? "enabled" : "disabled");
        return;
    }

    _enabled = enabled;
    if(_enabled)
    {
        qInfo() << "Enabling service quality events";
        start();
    }
    else
    {
        qInfo() << "Disabling service quality events";
        stop();
    }
}

void ServiceQuality::sendEventsNow()
{
    qInfo() << "Send events early due to explicit request";
    sendBatch();
}

void ServiceQuality::vpnEnabled(VpnProtocol protocol, ConnectionSource source)
{
    // Though unusual, there might already be an unresolved attempt at this
    // point, which happens if a reconnect-to-apply-settings occurs during a
    // connection before it is established or canceled.  In that case, the
    // previous attempt is left unresolved.
    if(_pLastAttempt)
    {
        qWarning() << "Discarding last attempt, reconnect occurred before the connection was established or canceled.";
        _pLastAttempt.clear();  // In case storeEvent() does not store an event
    }

    // If an event is actually stored (events are enabled), indicate that we
    // have an unresolved connection attempt.
    //
    // Don't do this if events are disabled, so we don't generate a spurious
    // "established/canceled" event on the off chance that events are enabled
    // before this occurs.  (This also avoids misleading traces about events in
    // a few cases when events aren't being stored.)
    if(storeEvent(EventType::ConnectionAttempt, protocol, source))
        _pLastAttempt.emplace(protocol, source);
    _connTimer.restart();
}

void ServiceQuality::vpnDisabled(ConnectionSource source)
{
    // If there's no unresolved attempt, there's nothing to do
    if(!_pLastAttempt)
        return;

    // If the VPN was disabled manually, report a "canceled" event.
    if(source == ConnectionSource::Manual)
    {
        storeEvent(EventType::ConnectionCanceled, _pLastAttempt->first,
                   _pLastAttempt->second);
    }
    else
    {
        qInfo() << "VPN was disabled automatically, not generating an event.";
    }

    // Either way, this attempt is resolved
    _pLastAttempt.clear();
}

void ServiceQuality::vpnConnected()
{
    // If we still have an unresolved attempt, generate an "established" event
    if(_pLastAttempt)
    {
        storeEvent(EventType::ConnectionEstablished, _pLastAttempt->first,
                   _pLastAttempt->second);
        _pLastAttempt.clear();
    }
}

void ServiceQuality::vpnDisconnected()
{
    // If we still have an unresolved attempt, discard it.  This can happen due
    // to a hard error, in this case we reach the "disconnected" state with the
    // VPN still enabled.
    if(_pLastAttempt)
    {
        qInfo() << "VPN reached disconnected state, not generating an event.";
        _pLastAttempt.clear();
    }
}
