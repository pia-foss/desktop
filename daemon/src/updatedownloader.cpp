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

#include <common/src/common.h>
#line SOURCE_FILE("updatedownloader.cpp")

#include "updatedownloader.h"
#include "daemon.h"
#include <common/src/exec.h>
#include <common/src/semversion.h>
#include <common/src/testshim.h>
#include "version.h"
#include <common/src/builtin/path.h>
#include "apiclient.h"
#include <common/src/apinetwork.h>
#include <common/src/openssl.h>
#include "brand.h"
#include <common/src/exec.h>
#include <QNetworkReply>
#include <QDir>
#include <QProcess>

#ifdef Q_OS_WIN
#include <kapps_core/src/winapi.h>
#include <VersionHelpers.h>
#endif

#ifdef Q_OS_MAC
#include "mac/mac_version.h"
#endif

// Platform name for the supported platforms
const QString UpdateChannel::platformName =
#if defined(Q_OS_MAC)
        // macOS builds are universal for all supported architectures.
        // TODO - should we really do this?  What happens when we drop Intel
        // support?
        QStringLiteral("mac");
#elif defined(Q_OS_WIN)
    #if defined(Q_PROCESSOR_X86_64)
        QStringLiteral("windows_x64");
    #elif defined(Q_PROCESSOR_X86_32)
        QStringLiteral("windows_x86");
    #else
        #error "Platform name not known for this Windows architecture"
    #endif
#elif defined(Q_OS_LINUX)
    #if defined(Q_PROCESSOR_X86_64)
        QStringLiteral("linux_x64");
    #elif defined(__aarch64__)
        QStringLiteral("linux_arm64");
    #elif defined(__arm__)
        QStringLiteral("linux_armhf");
    #else
        #error "Platform name not known for this Linux architecture"
    #endif
#else
    #error "Platform name not known for this platform"
#endif

namespace
{
    // Initial load interval for version metadata
    const std::chrono::minutes versionInitialInterval{10};
    // Refresh interval after initial load.
    const std::chrono::hours versionRefreshInterval{1};
}

Update::Update(const QString &uri, const QString &version, const QString &osRequired)
{
    if(!uri.isEmpty() && !version.isEmpty())
    {
        _uri = uri;
        _version = version;
        _osRequired = osRequired;
    }
}

bool Update::operator==(const Update &other) const
{
    return uri() == other.uri() && version() == other.version() &&
        osRequired() == other.osRequired();
}

UpdateChannel::UpdateChannel()
{
}

void UpdateChannel::onVersionMetadataReady(const QJsonDocument &metadataDoc)
{
    Q_ASSERT(_pMetadataRefresher);  // Valid when connected
    _pMetadataRefresher->loadSucceeded();

    // Grab the current available version so we can emit the
    // signal only if it changes.
    Update prevUpdate{_update};
    std::vector<QString> prevFlags{_flags};
    checkVersionMetadata(metadataDoc);

    if(prevUpdate != _update || prevFlags != _flags)
        emit updateChanged();
}

void UpdateChannel::checkVersionMetadata(const QJsonDocument &regionsDoc)
{
    // Store feature flags even if the rest of the update is invalid.  This is
    // not related to the actual update being offered.
    const auto &flagList = regionsDoc[QStringLiteral("flags")].toArray();
    _flags.clear();
    _flags.reserve(flagList.size());
    for(const auto &flag : flagList)
    {
        if(flag.isString())
            _flags.push_back(flag.toString());
    }

    // Use a unique branded "latest_version_BRAND", so accidentally fetching a update json
    // for the wrong brand never shows an update
    //
    // For legacy reasons, we use "latest_version_piax" for PIA brand also for compatibility
    // with the servers list, in case we ever moved these data into the servers
    // list.
    const auto &latestObj = regionsDoc[QStringLiteral(BRAND_UPDATE_JSON_KEY_NAME)].toObject();
    const auto &platformVal = latestObj[platformName];
    // If this platform doesn't exist at all, that's fine, there's no update.
    // Don't log any warnings for this.
    if(platformVal.isUndefined())
        return;

    const auto &platformObj = platformVal.toObject();
    const QString &latestVersion = platformObj[QStringLiteral("version")].toString();
    const QString &downloadUrl = platformObj[QStringLiteral("download")].toString();
    const QString &osVersionRequirement = platformObj[QStringLiteral("required")].toString();

    // If something is missing from the server data, log a warning just for
    // diagnostic purposes.
    if(latestVersion.isEmpty() || downloadUrl.isEmpty())
    {
        qWarning() << "Incomplete latest-version info in server info for platform"
            << platformName << "- version:" << latestVersion << "- url:"
            << downloadUrl;
    }

    // Store the update.  (Update ignores partial data if the server returned
    // only a URI or version somehow.)
    _update = Update{downloadUrl, latestVersion, osVersionRequirement};
}

void UpdateChannel::run(bool newRunning, const std::shared_ptr<ApiBase> &pUpdateApi)
{
    if(_pMetadataRefresher)
    {
        if(newRunning)
            _pMetadataRefresher->start(pUpdateApi);
        else
            _pMetadataRefresher->stop();
    }
}

void UpdateChannel::discardStaleCache()
{
    // Revert to the initial load interval, and if we're currently running
    // issue a request now.
    refreshUpdate();
    // If there is a cached update, clear it.
    if(_update.isValid())
    {
        _update = {};
        emit updateChanged();
    }
}

void UpdateChannel::reloadAvailableUpdate(const Update &update, const std::vector<QString> &flags)
{
    // If we're on a valid channel, restore the available update.
    // If we're not on a valid channel, this channel doesn't provide any
    // updates.
    if(_pMetadataRefresher)
    {
        // This must be called before the channel is started; we can't have
        // fetched an available update yet.
        Q_ASSERT(!_pMetadataRefresher->isRunning());
        Q_ASSERT(!_update.isValid());

        _update = update;
        _flags = flags;

        // Does not emit updateChanged(); UpdateDownloader rechecks the updates
        // after reloading both channels.
    }
}

void UpdateChannel::refreshUpdate()
{
    if(_pMetadataRefresher)
        _pMetadataRefresher->refresh();
    // Otherwise, there is no effect, no update channel is set.
}

void UpdateChannel::setUpdateChannel(const QString &updateChannel, bool newRunning,
                                     const std::shared_ptr<ApiBase> &pUpdateApi)
{
    _pMetadataRefresher.reset();
    qInfo() << "Switching to update channel" << updateChannel;

    // Remove any update that's being advertised until we fetch the new channel.
    // (We might never fetch any new data if the update channel was cleared, or
    // if the update channel doesn't actually exist, etc.)
    if(_update.isValid())
    {
        _update = {};
        emit updateChanged();
    }

    if(!updateChannel.isEmpty())
    {
        // Make a new JsonRefresher with the new URI.  This has several
        // desirable effects:
        // - it changes the URI that we're loading
        // - it ensures that any in-flight loads of the old URI are discarded
        // - it triggers a refresh right away (if running)
        // - it switches to the short polling interval until a load for the new
        //   URI succeeds
        _pMetadataRefresher.reset(new JsonRefresher{QStringLiteral("version data"),
                                                    updateChannel,
                                                    versionInitialInterval,
                                                    versionRefreshInterval});
        connect(_pMetadataRefresher.get(), &JsonRefresher::contentLoaded, this,
                &UpdateChannel::onVersionMetadataReady);
    }

    // If the caller wants the object in the running state, start it now.
    // 'run' is explicitly passed rather than having UpdateDownloader call
    // start() because setUpdateChannel() would otherwise implicitly stop the
    // channel, which is pretty subtle.
    // We don't store the running state explicitly in UpdateChannel because
    // UpdateDownloader already knows it; we'd just be duplicating state.
    run(newRunning, pUpdateApi);
}

/*
=== Beta channel caching ===

The beta channel is cached when it is enabled (even when clients disconnect or
the daemon shuts down.  However, it is not cached if the channel is disabled.

Caching while the channel is enabled is the same as for the GA channel.  This
ensures that restarting the client/daemon/computer restores the state that it
showed previously - any available update comes back and isn't treated as 'new'
(it doesn't pop a notification again).

Disabling the channel wipes the cache, because that stale cache could otherwise
sit around for a long time, and then re-enabling the cache would immediately
show an old beta before updating to the newest one.  The downside is that we
might have a delay between enabling the channel and showing an available update,
but that's acceptable.
*/

UpdateDownloader::UpdateDownloader()
    : _daemonVersion{99999, 99999, 99999}, _running{false}, _enableBeta{false}
{
    // If the daemon's version can't be parsed, we log an error and proceed with
    // the default version above that will never offer an upgrade.  This might
    // happen for developer builds if they're doing really crazy stuff; it
    // doesn't prevent the app from running.
    try
    {
        _daemonVersion = SemVersion{QString::fromStdString(Version::semanticVersion())};
    }
    catch(const Error &ex)
    {
        qCritical() << "Daemon version" << Version::semanticVersion()
            << "is invalid, can't check for updates -" << ex;
    }

    // Whenever the channels refresh, emit updateRefreshed().  The available
    // update might not have changed, but the individual channel data definitely
    // has, and it's persistently cached in DaemonData.
    QObject::connect(&_gaChannel, &UpdateChannel::updateChanged, this,
                     &UpdateDownloader::emitUpdateRefreshed);
    QObject::connect(&_betaChannel, &UpdateChannel::updateChanged, this,
                     &UpdateDownloader::emitUpdateRefreshed);
}

void UpdateDownloader::checkUpdateChannel(const UpdateChannel &channel,
                                          nullable_t<SemVersion> &newestVersion,
                                          Update &availableUpdate,
                                          bool &osFailedRequirement) const
{
    if(!channel.update().isValid())
        return; // Nothing available from this channel

    auto channelVersion = SemVersion::tryParse(channel.update().version());
    if(!channelVersion)
    {
        qWarning() << "Version" << channel.update().version()
            << "is not valid, ignoring this channel";
        return;
    }

    // If this version is the same or older than the currently known newest
    // version, ignore it
    if(newestVersion && *channelVersion <= *newestVersion)
        return;

    // This version is newer, or there is no other version known yet.  Check the
    // OS requirement
    if(!channel.update().osRequired().isEmpty() &&
        !validateOSRequirements(channel.update().osRequired()))
    {
        qWarning() << "This OS version does not meet the requirement"
            << channel.update().osRequired() << "for version"
            << channel.update().version();
        osFailedRequirement = true;
        return;
    }

    // This version is newer, and the current OS is supported.  Use this version
    newestVersion = channelVersion;
    availableUpdate = channel.update();
}

Update UpdateDownloader::calculateAvailableUpdate(bool &osFailedRequirement) const
{
    // The semantic version of the newst version available (so far, as we check
    // the channels.  Null if no versions are available.
    nullable_t<SemVersion> newestVersion;
    Update availableUpdate;

    osFailedRequirement = false;

    // Check whether there is a build available from the GA channel.
    checkUpdateChannel(_gaChannel, newestVersion, availableUpdate, osFailedRequirement);
    qInfo() << "checked GA channel:" << !!newestVersion << availableUpdate;

    // Check whether there is a build available from the beta channel.
    // If beta is disabled, this has no effect, because we don't keep a cache of
    // the beta channel when it's disabled.
    checkUpdateChannel(_betaChannel, newestVersion, availableUpdate, osFailedRequirement);
    qInfo() << "checked beta channel:" << !!newestVersion << availableUpdate;

    // If no advertised version was found, there's nothing to offer
    if(!newestVersion)
        return {};

    // If the newest version is newer than the installed version, offer it.
    if(*newestVersion > _daemonVersion)
        return availableUpdate;

    // If a beta is installed, and the beta channel is no longer enabled, offer
    // the advertised version as long as it's different, even if it's older.
    // This ensures that the current GA release is offered again after disabling
    // beta, even if it's older.
    if(_daemonVersion.isPrereleaseType(u"beta") && !_enableBeta
        && *newestVersion != _daemonVersion)
    {
        qInfo() << "Offering GA version" << availableUpdate << "as downgrade due to disabling beta channel";
        return availableUpdate;
    }

    // Otherwise, there's no update.
    return {};
}

void UpdateDownloader::emitUpdateRefreshed()
{
    bool osFailedRequirement = false;
    Update availableUpdate = calculateAvailableUpdate(osFailedRequirement);
    emit updateRefreshed(availableUpdate, osFailedRequirement, _gaChannel.update(),
                         _betaChannel.update(), _gaChannel.flags());
}

void UpdateDownloader::run(bool newRunning, const std::shared_ptr<ApiBase> &pUpdateApi)
{
    _running = newRunning;
    _gaChannel.run(_running, pUpdateApi);
    _betaChannel.run(_running && _enableBeta, pUpdateApi);
}

void UpdateDownloader::reloadAvailableUpdates(const Update &gaUpdate,
                                              const Update &betaUpdate,
                                              const std::vector<QString> &flags)
{
    // Daemon calls this before UpdateDownloader has ever been started.
    Q_ASSERT(!_running);

    // Reload each channel
    _gaChannel.reloadAvailableUpdate(gaUpdate, flags);

    // The cached beta update only makes sense if the beta channel is enabled.
    // Normally this cache should be empty anyway, but validate it since we
    // can't be sure.
    if(_enableBeta)
        _betaChannel.reloadAvailableUpdate(betaUpdate, {}); // Beta "flags" aren't used
    else if(betaUpdate.isValid())
        qWarning() << "Ignoring cached beta update" << betaUpdate << "- beta channel is not enabled";

    // Emit an update change to update Daemon with the current available update.
    emitUpdateRefreshed();
}

void UpdateDownloader::refreshUpdate()
{
    if(_running)
    {
        _gaChannel.refreshUpdate();
        _betaChannel.refreshUpdate();
    }
}

void UpdateDownloader::setGaUpdateChannel(const QString &channel,
                                          const std::shared_ptr<ApiBase> &pUpdateApi)
{
    _gaChannel.setUpdateChannel(channel, _running, pUpdateApi);
}

void UpdateDownloader::setBetaUpdateChannel(const QString &channel,
                                            const std::shared_ptr<ApiBase> &pUpdateApi)
{
    _betaChannel.setUpdateChannel(channel, _running && _enableBeta, pUpdateApi);
}

void UpdateDownloader::enableBetaChannel(bool enable, const std::shared_ptr<ApiBase> &pUpdateApi)
{
    if(_enableBeta == enable)
        return;

    _enableBeta = enable;
    // Call run() again to change the beta channel's state if necessary
    run(_running, pUpdateApi);

    // If the beta is being turned off, wipe the cache after stopping it.  We
    // don't keep the old cache because we don't want to offer a stale beta if
    // the channel is re-enabled.
    if(!enable)
    {
        // If there was a beta available (even if we weren't currently offering
        // it), discarding the cache emits a change from the update channel,
        // which causes us to emit updateRefreshed().
        _betaChannel.discardStaleCache();
    }
    // Otherwise, we don't need to emit anything.  We don't keep a beta update
    // cached when the channel is off, so nothing changes until we fetch the
    // beta channel.
    else
    {
        // Can't have a beta update yet
        Q_ASSERT(!_betaChannel.update().isValid());
    }
}

Async<DownloadResult> UpdateDownloader::downloadUpdate()
{
    bool ignored;
    Update availableUpdate = calculateAvailableUpdate(ignored);

    // The client doesn't provide this UI when no update is available or an
    // update is already being downloaded, don't need to provide feedback to the
    // client for these cases.
    if(!availableUpdate.isValid())
    {
        qWarning() << "Can't download update, no update is available";
        return Async<DownloadResult>::resolve();
    }
    if(_pDownloadReply)
    {
        qWarning() << "Already downloading an update, can't start again";
        return Async<DownloadResult>::resolve(DownloadResult().version(availableUpdate.version()));
    }

    // Class invariant - open when _pDownloadReply is set
    Q_ASSERT(!_installerFile.isOpen());

    QUrl reqUrl{availableUpdate.uri()};

    // Open the installer file in the download location.
    Path downloadPath{Path::DaemonUpdateDir / reqUrl.fileName()};
    _installerFile.unsetError();
    _installerFile.setFileName(downloadPath);

    // Attempt to clean any old downloads that exist to limit accumulation of
    // installers.  Failure does not prevent us from downloading the new file
    // though.
    // Note that QDir::removeRecursively() returns true (success) if the
    // directory doesn't exist ("expected result already reached" per doc).
    if(!QDir{Path::DaemonUpdateDir}.removeRecursively())
    {
        qWarning() << "Unable to clean update directory:"
            << Path::DaemonUpdateDir;
    }

    Path::DaemonUpdateDir.mkpath();
    if(!_installerFile.open(QFile::OpenModeFlag::WriteOnly | QFile::OpenModeFlag::Truncate))
    {
        // Can't open the file for some reason.  This could legitimately happen,
        // ensure that it's visible if it does.
        qError() << "Can't download installer - can't open file"
            << downloadPath << "due to error" << _installerFile.error();
        emit downloadFailed(availableUpdate.version(), true);
        // This call did initiate a download, but it failed.
        return Async<DownloadResult>::resolve(DownloadResult().version(availableUpdate.version()).failed(true));
    }

    QNetworkRequest downloadReq{availableUpdate.uri()};
    _pDownloadReply = ApiNetwork::instance()->getAccessManager().get(downloadReq);
    _pDownloadReply->setParent(this);
    _pDownloadTask = Async<DownloadResult>::create();
    _downloadingVersion = availableUpdate.version();
    // There is no timeout on this download, but the user can cancel it
    // manually if it appears to be stuck but does not fail.
    connect(_pDownloadReply, &QNetworkReply::downloadProgress, this,
            &UpdateDownloader::onDownloadProgress);
    connect(_pDownloadReply, &QIODevice::readyRead, this,
            &UpdateDownloader::onDownloadReadyRead);
    connect(_pDownloadReply, &QNetworkReply::finished, this,
            &UpdateDownloader::onDownloadFinished);
    emit downloadProgress(_downloadingVersion, 0);

    return _pDownloadTask;
}

void UpdateDownloader::cancelDownload()
{
    // Client only shows this UI when a download is in progress, don't need to
    // provide feedback for this case.
    if(!_pDownloadReply)
    {
        qWarning() << "Can't cancel download, no download is taking place";
        return;
    }

    _pDownloadReply->abort();
}

void UpdateDownloader::onDownloadProgress(qint64 bytesReceived,
                                          qint64 bytesTotal)
{
    // Class invariant - valid when this signal is connected
    Q_ASSERT(_pDownloadReply);
    // Class invariant - set when _pDownloadReply is set
    Q_ASSERT(!_downloadingVersion.isEmpty());

    // This signal can be emitted with bytesTotal == 0 if the download fails
    // before getting the content length from the server (or presumably if the
    // file isn't found, etc.).
    int progressPct = 0;
    if(bytesTotal > 0 && bytesReceived >= 0)
        progressPct = static_cast<int>(bytesReceived * 100 / bytesTotal);
    emit downloadProgress(_downloadingVersion, progressPct);
}

void UpdateDownloader::onDownloadReadyRead()
{
    // Class invariant - valid when this signal is connected
    Q_ASSERT(_pDownloadReply);
    // Class invariant - file open when _pDownloadReply is set
    Q_ASSERT(_installerFile.isOpen());

    // Write the new data to the file.
    // QIODevice doesn't provide any way to observe the new data without copying
    // it, so we make a copy here just to write it and throw the copy away.
    if(_installerFile.write(_pDownloadReply->readAll()) < 0)
    {
        // The write failed, cancel the download by aborting the network
        // request.  This will cause onDownloadFinished() to be called with an
        // error, which will close the file and emit downloadFailed().
        qError() << "Failed to write to installer file"
            << _installerFile.fileName() << "-" << _installerFile.error();
        _pDownloadReply->abort();
    }
}

void UpdateDownloader::onDownloadFinished()
{
    // Class invariant - valid when this signal is connected
    Q_ASSERT(_pDownloadReply);
    // Class invariant - valid when _pDownloadReply is set
    Q_ASSERT(_pDownloadTask);
    // Class invariant - file open when _pDownloadReply is set
    Q_ASSERT(_installerFile.isOpen());
    // Class invariant - set when _pDownloadReply is set
    Q_ASSERT(!_downloadingVersion.isEmpty());

    // Delete the reply when we're done here
    _pDownloadReply->deleteLater();
    _installerFile.close();

    // Reset _pDownloadReply, _pDownloadTask, and _downloadingVersion since the
    // download is finished and we've closed the file.
    QPointer<QNetworkReply> pFinishedReply;
    _pDownloadReply.swap(pFinishedReply);
    Async<DownloadResult> pFinishedTask;
    _pDownloadTask.swap(pFinishedTask);
    QString finishedVersion;
    _downloadingVersion.swap(finishedVersion);

    // Check if the download failed
    DownloadResult taskResult;
    taskResult.version(finishedVersion);
    auto error = pFinishedReply->error();
    if(error != QNetworkReply::NetworkError::NoError)
    {
        qInfo() << "Installer download of" << finishedVersion << "from"
            << pFinishedReply->url() << "failed with error:"
            << qEnumToString(error);
        // Delete the partial file - failure is ignored
        _installerFile.remove();
        // 'OperationCanceledError' indicates that the user canceled the
        // download (we only call abort() due to a user cancellation).
        bool dueToError = error != QNetworkReply::NetworkError::OperationCanceledError;
        emit downloadFailed(finishedVersion, dueToError);
        // The result is an error if we detected an error above, canceled
        // otherwise.
        taskResult.failed(dueToError);
    }
    else
    {
        // Otherwise, we're done, the download succeeded
#ifdef Q_OS_LINUX
        // Add the executable bit on Linux so the client can execute the
        // downloaded installer.
        Exec::cmd(QStringLiteral("chmod"), {"a+x", _installerFile.fileName()});
#endif
        emit downloadFinished(finishedVersion, _installerFile.fileName());
        taskResult.succeeded(true);
    }
    // Resolve the existing task
    pFinishedTask->resolve(std::move(taskResult));
}

bool UpdateDownloader::validateOSRequirements(const QString &requirement) const
{
#ifdef Q_OS_MAC
  nullable_t<SemVersion> reqVer = SemVersion::tryParse(requirement);
  SemVersion curVer = getMacOSVersion();
  qDebug () << "Validate OS version against " << requirement;

  if(reqVer.isNull()) {
    qWarning() << "Failed to parse reqVer ";
    return true;
  }
  if(curVer.compare(reqVer.get()) >= 0) {
    qDebug () << "Requirement satisfied";
    return true;
  } else {
    qDebug () << "Requirement was not satisfied";
    return false;
  }
#endif

#ifdef Q_OS_WIN
  bool intParseSuccess = false;
  int reqVerNum = requirement.toInt(&intParseSuccess);
  if(intParseSuccess) {
    qDebug () << "Found required version num: " << reqVerNum;
    OSVERSIONINFOEXW versEx{};
        versEx.dwOSVersionInfoSize = sizeof(versEx);
        ::GetVersionExW(reinterpret_cast<OSVERSIONINFO*>(&versEx));
    qDebug () << "OS Build version num: " << versEx.dwBuildNumber;
    if(versEx.dwBuildNumber >= reqVerNum) {
      qDebug () << "Requirement satisfied";
      return true;
    } else {
      qDebug () << "Requirement was not satisfied";
      return false;
    }
  }
#endif

  // Always return true for linux
  return true;
}
