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

#ifndef SETTINGS_DAEMONDATA_H
#define SETTINGS_DAEMONDATA_H

#include "common.h"
#include "json.h"
#include "locations.h"
#include "events.h"
#include <deque>
#include <unordered_map>

class COMMON_EXPORT AppMessage : public NativeJsonObject
{
    Q_OBJECT
public:
    using TextTranslations = std::unordered_map<QString, QString>;
public:
    AppMessage() {}

    AppMessage(const AppMessage &other) {*this = other;}
    AppMessage &operator=(const AppMessage &other)
    {
        id(other.id());
        hasLink(other.hasLink());
        messageTranslations(other.messageTranslations());
        linkTranslations(other.linkTranslations());
        settingsAction(other.settingsAction());
        viewAction(other.viewAction());
        uriAction(other.uriAction());
        return *this;
    }

    bool operator==(const AppMessage &other) const
    {
        return id() == other.id() && hasLink() == other.hasLink() &&
        messageTranslations() == other.messageTranslations() &&
        linkTranslations() == other.linkTranslations() &&
        settingsAction() == other.settingsAction() &&
        viewAction() == other.viewAction() &&
        uriAction() == other.uriAction();
    }

    bool operator!=(const AppMessage &other) const
    {
        return !(*this == other);
    }

public:
    JsonField(quint64, id, 0)
    JsonField(bool, hasLink, false)
    JsonField(TextTranslations, messageTranslations, {})
    JsonField(TextTranslations, linkTranslations, {})
    JsonField(QJsonObject, settingsAction, {})
    JsonField(QString, viewAction, {})
    JsonField(QString, uriAction, {})
};


// Class encapsulating 'data' properties of the daemon; these are cached
// and persist between daemon instances.
//
// Most of these are fetched from servers at runtime in some way (regions lists,
// update metadata, latency measurements); they're cached for use in case we
// aren't able to fetch the data at some point in the future, and so we (likely)
// have some data to work with at startup.
//
// Cached data are stored in the original format received from the server, not
// an internalized format.  The caches may be reused even after a daemon update
// - internalized formats are likely to change in an update, but the format from
// the server is unlikely to change.  Internalized versions of the same data are
// provided in DaemonState.
//
// Some data here is not fetched but instead calculated by the daemon
// (latencies, winDnscacheOriginalPath, service quality data).  We usually
// preserve these on updates, but they can be discarded with little impact if
// the format changes.
class COMMON_EXPORT DaemonData : public NativeJsonObject
{
    Q_OBJECT
public:
    DaemonData();

    // Latency measurements (by location ID).  These are restored when the
    // daemon is started, but any new measurements will replace the cached
    // values.
    JsonField(LatencyMap, modernLatencies, {})

    // Cached regions lists.  This is the exact JSON content from the actual
    // regions list; it hasn't been digested or interpreted by the daemon.  This
    // works well when the daemon is upgraded; the API format doesn't change so
    // we can re-interpret the existing cache even if the internal model has
    // changed.
    JsonField(QJsonArray, cachedModernShadowsocksList, {})
    JsonField(QJsonObject, cachedModernRegionsList, {})

    JsonField(QJsonObject, modernRegionMeta, {})

    // Persistent caches of the version advertised by update channel(s).  This
    // is mainly provided to provide consistent UX if the client/daemon are
    // restarted while an update is available (they restore the same "update
    // available" state they had when shut down; they don't come up with no
    // update available and then detect it as a new update again).
    //
    // These are only used to restore the UpdateDownloader state; they're not
    // used by the client.
    //
    // Note that 1.0.1 and older stored availableVersion/availableVersionUri
    // that were used as both the persistent cache and the available version
    // notification to the client UI.  These are intentionally discarded (by
    // the DiscardUnknownProperties behavior) when updating to the new style;
    // the data do not need to be migrated because they would have advertised
    // the version that was just installed.
    JsonField(QString, gaChannelVersion, {})
    JsonField(QString, gaChannelVersionUri, {})
    JsonField(QString, gaChannelOsRequired, {})
    JsonField(QString, betaChannelVersion, {})
    JsonField(QString, betaChannelVersionUri, {})
    JsonField(QString, betaChannelOsRequired, {})

#if defined(Q_OS_WINDOWS)
    // Original service command for the Dnscache service on Windows.  PIA must
    // temporarily override this in order to suppress the service for split
    // tunnel DNS.
    //
    // Although PIA attempts to restore the original value as soon as possible,
    // it's persisted in case the daemon would crash or for whatever reason be
    // unable to restore the service.  The daemon then recovers if it starts up
    // again.
    //
    // This should only change with an OS update, so we keep the last detected
    // value once it is found for resiliency (we never clear this).
    JsonField(QString, winDnscacheOriginalPath, {})
#endif

    // Flags published in the release channel that we are currently loading
    JsonField(std::vector<QString>, flags, {})

    // In-app communication message
    JsonField(AppMessage, appMessage, {})

    // Service quality events - only used if the user has opted in to providing
    // service quality information back to us (see
    // DaemonSettings::sendServiceQualityEvents).  If that setting is not
    // enabled, these are all empty.

    // Current aggregation ID - needed so we can differentiate a large number of
    // errors from a few users vs. a few errors from a lot of users.  Rotated
    // every 24 hours for privacy (see qualityAggIdRotateTime).
    JsonField(QString, qualityAggId, {})
    // The next time we should rotate serviceAggId (UTC Unix time, ms)
    JsonField(qint64, qualityAggIdRotateTime, 0)
    // Events that have been generated but not sent yet.  We try to send the
    // events when 20 have been batched, but this could have more than 20 events
    // if we weren't able to send them at that time (we'll try again later).
    //
    // This is a deque because we add to the back and remove from the front;
    // events are removed once they're sent, but more events might already have
    // been generated.
    JsonField(std::deque<ServiceQualityEvent>, qualityEventsQueued, {})
    // Events that have been generated and sent in the last 24 hours.  These are
    // retained just so the UI can display them.
    //
    // Deque for the same reason - we add sent events to the back and remove
    // from the front as they roll off.
    JsonField(std::deque<ServiceQualityEvent>, qualityEventsSent, {})

    // Check if a single flag exists on the list of flags
    bool hasFlag (const QString &flag) const;
};


#endif
