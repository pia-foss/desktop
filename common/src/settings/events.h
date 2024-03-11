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

#ifndef SETTINGS_EVENTS_H
#define SETTINGS_EVENTS_H

#include "../common.h"
#include "../json.h"
#include "version.h"
#include "product.h"

// Event properties for ServiceQualityEvents - in an object property as expected
// by the API.  This would make more sense as a nested class, but moc doesn't
// support nested classes.
class COMMON_EXPORT EventProperties : public NativeJsonObject
{
    Q_OBJECT
private:
    static const QString &semanticVersionQstr();
    static const QString &userAgentQstr();

public:
    EventProperties() = default;
    EventProperties(const EventProperties &other) { *this = other; }
    EventProperties &operator=(const EventProperties &other)
    {
        user_agent(other.user_agent());
        platform(other.platform());
        version(other.version());
        prerelease(other.prerelease());
        vpn_protocol(other.vpn_protocol());
        connection_source(other.connection_source());
        time_to_connect(other.time_to_connect());
        return *this;
    }
    bool operator==(const EventProperties &other) const
    {
        return user_agent() == other.user_agent() &&
            platform() == other.platform() &&
            version() == other.version() &&
            prerelease() == other.prerelease() &&
            vpn_protocol() == other.vpn_protocol() &&
            time_to_connect() == other.time_to_connect() &&
            connection_source() == other.connection_source();
    }
    bool operator!=(const EventProperties &other) const {return !(*this == other);}

public:
    // User agent; client information
    JsonField(QString, user_agent, userAgentQstr())
    JsonField(QString, platform, PIA_PLATFORM_NAME)
    JsonField(QString, version, semanticVersionQstr())
    JsonField(bool, prerelease, PIA_VERSION_IS_PRERELEASE)
    // The VPN protocol being used
    JsonField(QString, vpn_protocol, {}, {"OpenVPN", "WireGuard"})
    // The source of the connection:
    // - "Manual" - The user directly initiated this connection (e.g. by
    //   clicking the Connect button).  CLI "connect" is also interpreted
    //   as manual.
    // - "Automatic" - The connection was initiated automatically (e.g. by
    //   ending Snooze or an automation rule.)
    JsonField(QString, connection_source, {}, {"Manual", "Automatic"})
    JsonField(Optional<float>, time_to_connect, nullptr)
};

// If the user has opted in to providing service quality information back to us
// (see DaemonSettings::sendServiceQualityEvents), service quality events are
// stored using this format.  We usually batch up to 20 events before sending
// them, and then hold onto events that were sent for up to 24 hours so they
// can be shown in the UI.
//
// Everything sent with an event is stored here, even though the
// version/platform/user agent/etc. rarely change, to ensure that we don't
// inadvertently send old events with a newer version, etc., after upgrading.
// This also conveniently means that we can make this match the format expected
// by the API.  (That's why the properties here use snake_case instead of our
// usual camelCase.)
class COMMON_EXPORT ServiceQualityEvent : public NativeJsonObject
{
    Q_OBJECT

public:
    ServiceQualityEvent() = default;
    ServiceQualityEvent(const ServiceQualityEvent &other) { *this = other; }
    ServiceQualityEvent &operator=(const ServiceQualityEvent &other)
    {
        aggregated_id(other.aggregated_id());
        event_unique_id(other.event_unique_id());
        event_name(other.event_name());
        event_time(other.event_time());
        event_token(other.event_token());
        event_properties(other.event_properties());
        return *this;
    }
    bool operator==(const ServiceQualityEvent &other) const
    {
        return aggregated_id() == other.aggregated_id() &&
            event_unique_id() == other.event_unique_id() &&
            event_name() == other.event_name() &&
            event_time() == other.event_time() &&
            event_token() == other.event_token() &&
            event_properties() == other.event_properties();
    }
    bool operator!=(const ServiceQualityEvent &other) const {return !(*this == other);}

public:
    // Aggregation ID at the time this event was generated - needed so we can
    // differentiate a large number of errors from a few users vs. a few errors
    // from a lot of users.  See DaemonState::qualityAggId, which is rotated
    // every 24 hours for privacy.
    JsonField(QString, aggregated_id, {})
    // Unique ID for this event.  This ensures a given event is only stored once
    // even if an API error causes us to retry a request after the API actually
    // received the payload.
    JsonField(QString, event_unique_id, {})
    // The event that occurred:
    // - Attempt - the VPN connection has been enabled.  Note that this is
    //   different from an "attempt" used elsewhere in PIA Desktop; this does
    //   _not_ count each connection attempt before a connection is established,
    //   and it does not count reconnects.
    // - Established - We successfully established the VPN connection after
    //   previously generating an "attempt" (i.e. after the connection was
    //   enabled, does not count reconnects)
    // - Canceled - The VPN connection was disabled before having established
    //   a connection (i.e. the user canceled).
    //
    // Note that connections in PIA Desktop rarely ever fail completely, in
    // most circumstances we just keep trying forever.  Mostly this occurs for
    // an authentication failure as there is no point retrying that error.  If
    // these occur in any significant number, they're apparent as the residual
    // (attempt - (established + canceled)).
    JsonField(QString, event_name, {}, {"VPN_CONNECTION_ATTEMPT",
        "VPN_CONNECTION_ESTABLISHED", "VPN_CONNECTION_CANCELED"})
    // Event timestamp - UTC Unix time in _seconds_ (not milliseconds like we
    // use elsewhere).
    // Note that PIA only stores timestamps with hourly granularity for privacy.
    JsonField(qint64, event_time, 0)
    // "Event token" - this identifies the product being used (here, PIA Desktop)
    // and the environment (staging or production, which for us corresponds to
    // prerelease builds vs. general releases).  This is shown as "Product ID"
    // in the UI.
#if PIA_VERSION_IS_PRERELEASE
    #if defined(Q_OS_WIN)
        #define PIA_PRODUCT_EVENT_TOKEN "b3fc3640ac22aedba0983c2cb38d60a3"  // Windows staging
    #elif defined(Q_OS_MACOS)
        #define PIA_PRODUCT_EVENT_TOKEN "3f670cc9ed90701ae79097eff72bc103"  // macOS staging
    #elif defined(Q_OS_LINUX)
        #define PIA_PRODUCT_EVENT_TOKEN "2b6487b0b55f39fd72214f47a8075179"  // Linux staging
    #endif
#else
    #if defined(Q_OS_WIN)
        #define PIA_PRODUCT_EVENT_TOKEN "c4f380b3d4b3a8c25402324f960911e2"  // Windows production
    #elif defined(Q_OS_MACOS)
        #define PIA_PRODUCT_EVENT_TOKEN "f3ccd0c159f102d2c5fd3383cb3b354d"  // macOS production
    #elif defined(Q_OS_LINUX)
        #define PIA_PRODUCT_EVENT_TOKEN "fd40e5dc1611d76649607f86334f27ea"  // Linux production
    #endif
#endif
    JsonField(QString, event_token, QStringLiteral(PIA_PRODUCT_EVENT_TOKEN))

    JsonObjectField(EventProperties, event_properties, {})
};

#endif
