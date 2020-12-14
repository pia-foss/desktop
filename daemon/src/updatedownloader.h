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
#line HEADER_FILE("updatedownloader.h")

#ifndef UPDATEDOWNLOADER_H
#define UPDATEDOWNLOADER_H

#include "semversion.h"
#include "async.h"
#include "json.h"
#include "apiclient.h"
#include "jsonrefresher.h"
#include <QObject>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QFile>
#include <QPointer>
#include <QSslKey>
#include <QTimer>
#include <memory>

// Result of a Download RPC
class DownloadResult : public QJsonObject
{
public:
    using QJsonObject::QJsonObject;
#define BuilderField(name, type) \
    DownloadResult& name(const type& value) { insert(QStringLiteral(#name), value); return *this; }

    // If true, the download failed due to an error.
    BuilderField(failed, bool)
    // If true, the download succeeded.  (If failed and succeeded are both
    // false, the download was canceled.)
    BuilderField(succeeded, bool)
    // The version that was being downloaded.  Always set if error or success is
    // set; may be the empty string if there was no update available when the
    // RPC call was made; the result is as if the request was canceled in this
    // case.
    BuilderField(version, QString)
#undef BuilderField
};

// Object representing an update available from an update channel - a version
// string and download URI.
//
// Always has both or neither part set (can never be partially valid), but the
// version string is not necessarily valid at this point.
class Update
{
public:
    // By default, empty URI and version
    Update() = default;
    // Construct Update with the URI and version.  If either is empty, both
    // strings are left empty in the resulting object (there is never a
    // partially-valid Update).
    Update(const QString &uri, const QString &version, const QString &osRequired);

public:
    // A valid Update has a non-empty URI and version.
    bool isValid() const {return !_uri.isEmpty();}
    const QString &uri() const {return _uri;}
    const QString &version() const {return _version;}
    const QString &osRequired() const {return _osRequired;}

    bool operator==(const Update &other) const;
    bool operator!=(const Update &other) const {return !(*this == other);}

private:
    QString _uri, _version, _osRequired;
};

inline QDebug &operator<<(QDebug &dbg, const Update &update)
{
    return dbg << update.uri() << update.version();
}

// UpdateChannel keeps track of the current build available on a given channel
// (GA or beta).
// UpdateDownloader combines two of these to determine the overall update, but
// they can be checked individually to see if a beta is actually available.
class UpdateChannel : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("updatechannel")

public:
    // Platform name for this platform.  (Public so unit tests can use it for
    // mock data.)
    static const QString platformName;

public:
    UpdateChannel();

private:
    // Version metadata has been fetched by the JsonRefresher.  Updates the
    // available URI/version and emits updateChanged() if it changes.
    void onVersionMetadataReady(const QJsonDocument &metadataDoc);

    // Check the version metadata from JsonRefresher and update the available
    // URI/version.  (Doesn't emit updateChanged().)
    void checkVersionMetadata(const QJsonDocument &metadataDoc);

public:
    // Start or stop refreshing the version metadata.
    // Stopping a channel doesn't clear the stored update information; it's kept
    // so the cache is still relatively up-to-date when the channel is started
    // again.  (For example, this happens when a single client disconnects from/
    // reconnects to the daemon; the whole UpdateDownloader is stopped and then
    // started again when the client reconnects; it should stay up-to-date in
    // this case.)
    // If a valid update channel name isn't set (setUpdateChannel("")); calls to
    // run() are ignored.
    void run(bool newRunning, const std::shared_ptr<ApiBase> &pUpdateApi);

    // Discard any cached update.  If the channel is restarted later, or if it
    // is already running now, reverts to the quick timer for an initial reload.
    // Emits a change signal if the cache was not empty already.
    void discardStaleCache();

    // Reload the available update that was persisted in DaemonData.  This must
    // be called before the UpdateDownloader has been started (it doesn't
    // compare the version to any data that might have already been fetched).
    //
    // This does not emit updateChanged(), even if the available update changes.
    // (UpdateDownloader checks the new versions after reloading both channels.)
    void reloadAvailableUpdate(const Update &update, const std::vector<QString> &flags);

    // If this channel is running, refresh update metadata (asynchronously).
    // (Does not discard the current cache before refreshing.)
    void refreshUpdate();

    // Set the current update channel, and indicate whether the new channel
    // should be started (run=true is equivalent to calling start() after
    // setting the channel).
    void setUpdateChannel(const QString &updateChannel, bool newRunning,
                          const std::shared_ptr<ApiBase> &pUpdateApi);

    // Get the current update available from this channel.
    // No update is available if the channel is not running, the channel didn't
    // return any data, or if the channel is running but hasn't successfully
    // received a response yet.
    const Update &update() const {return _update;}

    // Get the current feature flags from this channel.  This is separate from
    // any available update - the OS requirement does not apply to these, and
    // the update channel does not even necessarily have to have a valid update
    // for this platform.
    const std::vector<QString> &flags() const {return _flags;}

signals:
    // Emitted when the available update or feature flags have changed.
    void updateChanged();

private:
    // Loads the latest version metadata periodically.  Valid when an update
    // channel is specified, nullptr if it is set to the empty string.
    std::unique_ptr<JsonRefresher> _pMetadataRefresher;
    // The update available on this channel (even if it's not newer than the
    // current daemon version).
    Update _update;
    std::vector<QString> _flags;
};

// UpdateDownloader tracks what updates are available and manages requests
// to download the newest update.
class UpdateDownloader : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("updatedownloader")

public:
    UpdateDownloader();

private:
    // Check the version available on an update channel - if it's newer than
    // newestVersion (including if newestVersion is null, meaning no versions
    // have been found yet), update newestVersion and availableUpdate.
    // Doesn't emit any changes (used by calculateAvailableUpdate()).
    //
    // If the version in this channel does not support the current OS,
    // osFailedRequirement is set to true, and newestVersion/availableUpdate are
    // unchanged.  (Otherwise, osFailedRequirement is unchanged.)
    void checkUpdateChannel(const UpdateChannel &channel,
                            nullable_t<SemVersion> &newestVersion,
                            Update &availableUpdate,
                            bool &osFailedRequirement) const;

    // Determine the current available update from the update channels, the
    // daemon version, and the _enableBeta flag.
    //
    // The available update isn't stored in UpdateDownloader since it can be
    // computed from the updates available from the channels.
    //
    // If any update is found that does not support this OS, then
    // osFailedRequirement is set to true.  (A valid update may still be
    // returned if one is available on another active update channel.)
    Update calculateAvailableUpdate(bool &osFailedRequirement) const;

    void emitUpdateRefreshed();

public:
    // Start or stop refreshing the version metadata
    void run(bool newRunning, const std::shared_ptr<ApiBase> &pUpdateApi);

    // Reload the available update that was persisted in DaemonData.  This must
    // be called before the UpdateDownloader has been started, but after the
    // channel/beta settings are applied.
    //
    // updateRefreshed() is emitted with the new version/uri state.  This might
    // not be the same as the version/uri that were passed to the function, such
    // as if the version is not newer than the current version, or if the URI is
    // not valid.
    //
    // updateRefreshed() is emitted even if the state does not change as a
    // result of this call (to sync up DaemonData with the current state).
    void reloadAvailableUpdates(const Update &gaUpdate, const Update &betaUpdate,
                                const std::vector<QString> &flags);

    // Refresh available updates (if running)
    void refreshUpdate();

    // Set the current update channels
    void setGaUpdateChannel(const QString &channel, const std::shared_ptr<ApiBase> &pUpdateApi);
    void setBetaUpdateChannel(const QString &channel, const std::shared_ptr<ApiBase> &pUpdateApi);

    // Enable or disable the beta channel.
    // This can be called while running or stopped; the beta channel is started
    // when UpdateDownloader is running and the beta channel is enabled.
    void enableBetaChannel(bool enable, const std::shared_ptr<ApiBase> &pUpdateApi);

    // Start downloading the latest update.  Emits downloadProgress() initially
    // with progress 0, then periodically as the download progresses.
    // When the download completes, downloadFinished() is emitted with the path
    // to the downloaded file.  If the download fails, downloadFailed() is
    // emitted.
    // Only one download can occur at a time.  If a download is already
    // occurring, subsequent calls to downloadUpdate() are ignored (the result
    // is as if the request was canceled).
    // The returned Task is resolved when the request completes (successfully,
    // in error, or canceled - the task is always resolved successfully so the
    // version string can be provided).  The result object indicates the status
    // of the request.
    Async<DownloadResult> downloadUpdate();

    // Cancel a download in progress.
    void cancelDownload();

private:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadReadyRead();
    void onDownloadFinished();
    bool validateOSRequirements(const QString &requirement) const;

signals:
    // Emitted when the available update version has been refreshed.
    // The beta and GA updates are provided just so they can be persisted in
    // DaemonData.
    // This can be emitted with an unchanged availableUpdate (the GA/beta
    // channel data might have changed, or there could be no changes at all).
    void updateRefreshed(const Update &availableUpdate,
                         bool osFailedRequirement, const Update &gaUpdate,
                         const Update &betaUpdate, const std::vector<QString> &flags);
    // Emitted when a download is starting, in progress, or completed.
    // 'progress' ranges from 0-100.
    void downloadProgress(const QString &version, int progress);
    // The download finished.  The absolute path to the downloaded file is
    // provided.
    void downloadFinished(const QString &version, const QString &installerPath);
    // The download attempt failed.  downloadProgress() will not be emitted
    // again until downloadUpdate() is called again.
    // 'version' is the version that was being downloaded.  'error' indicates
    // whether the failure was due to an error or due to the user canceling the
    // download.
    void downloadFailed(const QString &version, bool error);

private:
    // The version of this daemon.
    SemVersion _daemonVersion;
    // Whether we are running based on calls to start()/stop().
    bool _running;
    // The GA and beta update channels
    UpdateChannel _gaChannel, _betaChannel;
    // Whether the beta channel is enabled
    bool _enableBeta;
    // Network reply for the download in progress (prevents us from starting
    // another download)
    QPointer<QNetworkReply> _pDownloadReply;
    // Task to resolve/reject for the download in progress.  Set when
    // _pDownloadReply is set.
    Async<DownloadResult> _pDownloadTask;
    // The version being downloaded.  Normally, this is the same as
    // _availableVersion, but it can be different if a refresh occurs during a
    // download, and the available version changes.  Set when _pDownloadReply is
    // set.
    QString _downloadingVersion;
    // When a download is in progress, the file we are writing to.  This holds
    // an open file when _pDownloadReply is set, it is closed otherwise.
    QFile _installerFile;
};

#endif
