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

#ifndef SERVICEQUALITY_H
#define SERVICEQUALITY_H

#include <common/src/common.h>
#include <common/src/settings/daemonaccount.h>
#include <common/src/settings/daemondata.h>
#include <common/src/semversion.h>
#include "apiclient.h"
#include "environment.h"

// ServiceQuality keeps track of service quality information that's sent back to
// help us improve our service.  This only occurs when the user has opted in to
// sending this information - see DaemonSettings::sendServiceQualityEvents.
class ServiceQuality : public QObject
{
    Q_OBJECT

public:
    // Event types - corresponds to ServiceQualityEvent::event_name
    enum class EventType
    {
        ConnectionAttempt,
        ConnectionEstablished,
        ConnectionCanceled,
    };
    Q_ENUM(EventType);
    // VPN protocols - corresponds to ServiceQualityEvent::vpn_protocol
    enum class VpnProtocol
    {
        OpenVPN,
        WireGuard,
    };
    Q_ENUM(VpnProtocol);
    // Connection sources - corresponds to ServiceQualityEvent::connection_source
    enum class ConnectionSource
    {
        Manual,
        Automatic,
    };
    Q_ENUM(ConnectionSource);

    using sysTimeMs = std::chrono::time_point<std::chrono::system_clock,
                                              std::chrono::milliseconds>;

public:
    // Create ServiceQuality with a reference to the DaemonData where service
    // quality data are stored (which must outlive ServiceQuality).  It
    // initially reloads the persisted data from DaemonData, and stores new
    // events there so they can be persisted in case the daemon is shut down
    // before they are sent.
    //
    // apiClient, environment, and account are all needed to make the API
    // request to send a batch of events.
    ServiceQuality(ApiClient &apiClient, Environment &environment,
                   DaemonAccount &account, DaemonData &data, bool enabled, nullable_t<SemVersion> ver);

private:
    static sysTimeMs nowMs()
    {
        // This needs an explicit cast if system_clock::now() has greater
        // precision than milliseconds.
        return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    }

    // Get a system_clock time point from a duration in msec since the epoch.
    // This is just really wordy otherwise due to explicit constructors of
    // time_point and duration.
    sysTimeMs timeFromMsec(quint64 msecSinceEpoch)
    {
        return sysTimeMs{std::chrono::milliseconds{msecSinceEpoch}};
    }
    sysTimeMs timeFromSec(quint64 secSinceEpoch)
    {
        return sysTimeMs{std::chrono::seconds{secSinceEpoch}};
    }

    // Calculate the time until a timestamp has passed, optionally with a
    // retention time (such as for events retained for 24 hr).  The current time
    // defaulst to nowMs() but can be specified (to use a consistent time if
    // multiple events are being checked, etc.)
    //
    // The result is 0 or negative if the timestamp has passed (including
    // retention time, if any).
    std::chrono::milliseconds timeToExpire(const sysTimeMs &timestamp,
                                           const std::chrono::milliseconds &retention,
                                           const sysTimeMs &now = nowMs()) const;

    // Check if a timestamp has passed, optionally with a retention time (such
    // as for events retained for 24 hr).  The current time defaults to
    // nowMs() but can be specified (to use a consistent time if multiple
    // events are being checked. etc.)  Note that this includes the timer
    // tolerance, so times slightly in the future are considered "expired"
    // (see _timerTolerance).
    bool hasExpired(const sysTimeMs &timestamp,
                    const std::chrono::milliseconds &retention,
                    const sysTimeMs &now = nowMs()) const;

    // Set a timer to the expiration time for a timestamp with an optional
    // retention.  Uses timeToExpire() with a minimum timeout of 1ms if the
    // time has already passed.
    void startExpireTimer(QTimer &timer, const sysTimeMs &timestamp,
                          const std::chrono::milliseconds &retention,
                          const sysTimeMs &now = nowMs()) const;

    // Generate a new aggregation ID.
    void generateId(const sysTimeMs &now);

    // Generate a new early-send timeout interval (_earlySendTime)
    void generateEarlySendTime();

    // Roll off old events (sent events with aggregation IDs that have rotated
    // out).
    void rollOffEvents();

    // Start storing and sending events - check/rotate the ID and start timers.
    // (Used to implement the constructor and enable().)
    void start();

    // Stop storing and sending events - wipe out data and clear timers.
    // (Used to implement the constructor and enable().)
    void stop();

    void onAggIdRotateTimeChanged();
    void onQueuedEventsChanged();
    void onRotateIdElapsed();
    void onEarlySendElapsed();

    // Move events from the beginning of DaemonData::qualityEventsQueued() to
    // the end of DaemonData::qualityEventsSent()
    void moveQueuedToSent(std::size_t count);

    // If no request is already in flight, try to send a batch of events now.
    // If more events thatn EventBatchMaxSize have accumulated, this skips the
    // oldest events to limit the batch size.
    void sendBatch();

    // Store an event, if service quality events are enabled.  (Does nothing if
    // they are not enabled.)  The current time is captured as the event time.
    // This may trigger a batch submission if enough events have accumulated.
    //
    // The result indicates whether an event was stored (used for "attempt"
    // events so we'll generate a subsequent established/canceled event).
    bool storeEvent(EventType type, VpnProtocol protocol, ConnectionSource source);

public:
    // Enable or disable service quality events
    void enable(bool enabled, nullable_t<SemVersion> version);

    // Send any queued events now (used by a dev tool to send events early)
    void sendEventsNow();

    // ServiceQuality generates three events:
    //
    // - Connection Attempt - Indicates that we have enabled the VPN connection,
    //   either due to a manual or automatic source.
    // - Connection Established - The connection was established successfully
    //   after sending an "attempt" event.
    // - Connection Canceled - The user manually canceled the connection before
    //   it was established.  This does not include automatic disconnects, such
    //   as an automation rule causing a disconnect while still connecting.
    //
    // An "attempt" is almost always followed by exactly one
    // "established"/"canceled" event - this gives us an idea of the success
    // rate of connections, and lets us compare it across releases and over
    // time.
    //
    // In some cases we do not send any event after an "attempt":
    // - in the event of a hard error that prevents retries (mostly just auth
    //   errors, or hypothetically a daemon crash)
    // - if the connection is canceled _automatically_ before being established,
    //   such as by an automation rule causing a disconnect while still trying
    //   to connect
    // - if the user reconnects (such as to apply settings) while still
    //   connecting (this generates another attempt, leaving the earlier
    //   attempt unresolved).
    //
    // Those events are probably rare but will be apparent as the residual
    // (attempts - (established + canceled)), if they do happen a lot then we
    // might add specific event(s) to see what is causing that.

    // The VPN connection has been enabled (StateModel::vpnEnabled() just
    // became 'true').  Indicate whether this was a manual or automatic change.
    //
    // This generates a "connection attempt" event.
    void vpnEnabled(VpnProtocol protocol, ConnectionSource source);

    // The VPN connection has been disabled (StateModel::vpnEnabled() just
    // became 'false').  If the VPN state had not reached 'connected' since the
    // last call to vpnEnabled(), and the VPN was disabled manually, this
    // generates a "connection canceled" event.
    //
    // Note that the 'source' here indicates the source of the "disconnect"
    // request.  It is not the source actually sent with the event, it
    // determines whether an event is sent at all.  (Even if no event is sent,
    // we still need to discard an unresolved attempt.)
    void vpnDisabled(ConnectionSource source);

    // The VPN has reached the "Connected" state.  If this is the first time
    // this has happened since the last call to vpnEnabled(), this generates a
    // "connection established" event.
    void vpnConnected();

    // The VPN has reached the "Disconnected" state.  If this is the first time
    // this has happened since the last call to vpnEnabled(), we forget about
    // the "attempt" event that was sent and do not send an established or
    // canceled event.  ("Cancel" specifically means that the user canceled,
    // in this case we reached Disconnected for some other reason, such as a
    // hard error.)
    void vpnDisconnected();

private:
    ApiClient &_apiClient;
    Environment &_environment;
    DaemonAccount &_account;
    DaemonData &_data;
    bool _enabled;
    // If the last event generated was a "connection attempt", this is set to
    // the protocol and connection source for that event.  This indicates to
    // some of the other VPN state methods that we should generate an event
    // indicating the result of the attempt, and they use the protocol/source
    // for the new event.
    nullable_t<std::pair<VpnProtocol, ConnectionSource>> _pLastAttempt;
    // Timer used to rotate the aggregation ID
    QTimer _rotateIdTimer;
    // Timer used to send an early batch if we don't generate any events for a
    // while
    QTimer _earlySendTimer;
    // Timer used to get time for connection establishment
    // while
    QElapsedTimer _connTimer;
    // The interval we're currently using for the early batch time.  Changed
    // whenever we send an early batch to ensure that it can't be used to
    // correlate requests.
    //
    // Since we only store hourly event timestamps, this is between 90 and 150
    // minutes, which ensures that we always wait a minimum of 30-90 minutes
    // before sending an early batch, and also means that we spread requests
    // over the full hour.  See _minEarlySendTime/_maxEarlySendTime.
    std::chrono::milliseconds _earlySendTime;
    // When a batch is being sent (an API request is in flight), this is the
    // count of events from the front of DaemonData::qualityEventsQueued that
    // are being sent.  (More events could be added while the request is in
    // flight.)
    //
    // This is used:
    // - to move the sent events from 'queued' to 'sent' if the request
    //   succeeds
    // - to keep track of whether a request is in flight (avoid creating two
    //   simultaneous requests)
    std::size_t _sendingBatchSize;
    nullable_t<SemVersion> _consentVersion;
};

#endif
