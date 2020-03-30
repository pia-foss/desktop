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
#line SOURCE_FILE("daemon.cpp")

#include "daemon.h"

#include "vpn.h"
#include "vpnmethod.h"
#include "ipc.h"
#include "jsonrpc.h"
#include "path.h"
#include "version.h"
#include "brand.h"
#include "util.h"
#include "apinetwork.h"

#include <QFile>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QJsonDocument>
#include <chrono>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QRegularExpression>

#if defined(Q_OS_WIN)
#include <Windows.h>
#include <VersionHelpers.h>
#include "win/win_util.h"
#include <AclAPI.h>
#include <AccCtrl.h>
#pragma comment(lib, "advapi32.lib")
#endif

#ifdef Q_OS_WIN
#include "win/win_util.h"
#include <AclAPI.h>
#include <AccCtrl.h>
#pragma comment(lib, "advapi32.lib")
#endif

#ifndef UNIT_TEST
// Hook global error reporting function into daemon instance
void reportError(Error error)
{
    if (auto daemon = g_daemon)
        daemon->reportError(std::move(error));
    else
        qCritical() << error;
}
#endif

namespace
{
    //Initially, we try to load the regions every 15 seconds, until they've been
    //loaded
    const std::chrono::seconds regionsInitialLoadInterval{15};
    //After they're initially loaded, we refresh every 10 minutes
    const std::chrono::minutes regionsRefreshInterval{10};

    //Resource path used to retrieve regions
    const QString regionsResource{QStringLiteral("vpninfo/servers?version=1001&client=x-alpha")};
    const QString shadowsocksRegionsResource{QStringLiteral("vpninfo/shadowsocks_servers")};

    const QByteArray serverListPublicKey = QByteArrayLiteral(
        "-----BEGIN PUBLIC KEY-----\n"
        "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAzLYHwX5Ug/oUObZ5eH5P\n"
        "rEwmfj4E/YEfSKLgFSsyRGGsVmmjiXBmSbX2s3xbj/ofuvYtkMkP/VPFHy9E/8ox\n"
        "Y+cRjPzydxz46LPY7jpEw1NHZjOyTeUero5e1nkLhiQqO/cMVYmUnuVcuFfZyZvc\n"
        "8Apx5fBrIp2oWpF/G9tpUZfUUJaaHiXDtuYP8o8VhYtyjuUu3h7rkQFoMxvuoOFH\n"
        "6nkc0VQmBsHvCfq4T9v8gyiBtQRy543leapTBMT34mxVIQ4ReGLPVit/6sNLoGLb\n"
        "gSnGe9Bk/a5V/5vlqeemWF0hgoRtUxMtU1hFbe7e8tSq1j+mu0SHMyKHiHd+OsmU\n"
        "IQIDAQAB\n"
        "-----END PUBLIC KEY-----"
    );

    // Old default debug logging setting, 1.0 (and earlier) until 1.2-beta.2
    const QStringList debugLogging10{QStringLiteral("*.debug=true"),
                                     QStringLiteral("qt*.debug=false"),
                                     QStringLiteral("latency.*=false")};
    // New value.  Due to an oversight, this is slightly different from the
    // new default value that is now in DaemonSettings::getDefaultDebugLogging().
    // Functionally these will behave the same, but any future migrations will
    // need to look for both old values.
    const QStringList debugLogging12b2{QStringLiteral("*.debug=true"),
                                       QStringLiteral("qt.*.debug=false"),
                                       QStringLiteral("qt.*.info=false"),
                                       QStringLiteral("qt.scenegraph.general*=true")};
}

static DaemonData::CertificateAuthorityMap createCertificateAuthorites()
{
    static const std::initializer_list<std::pair<const char*, const char*>> files = {
        { "ECDSA-256k1", ":/ca/ecdsa_256k1.crt" },
        { "ECDSA-256r1", ":/ca/ecdsa_256r1.crt" },
        { "ECDSA-521", ":/ca/ecdsa_521.crt" },
        { "RSA-2048", ":/ca/rsa_2048.crt" },
        { "RSA-3072", ":/ca/rsa_3072.crt" },
        { "RSA-4096", ":/ca/rsa_4096.crt" },
        { "default", ":/ca/default.crt" },
    };
    DaemonData::CertificateAuthorityMap certificateAuthorities;
    for (const auto& ca : files)
    {
        QFile file(ca.second);
        if (file.open(QFile::ReadOnly | QIODevice::Text))
        {
            QList<QByteArray> lines = file.readAll().split('\n');
            file.close();
            QStringList result;
            result.reserve(lines.size());
            for (auto& line : lines)
                result.append(QString::fromLatin1(line));
            certificateAuthorities.insert(ca.first, result);
        }
        else
            qWarning() << "Unable to load CA" << ca.first;
    }
    return certificateAuthorities;
}

void restrictAccountJson()
{
    Path accountJsonPath = Path::DaemonSettingsDir / "account.json";
#ifdef Q_OS_WIN
    // On Windows, account.json is normally readable by Users due to inheriting
    // ACEs from the Program Files directory.
    //
    // To prevent non-administrator users from reading it, disable inheritance
    // and apply a DACL granting access to SYSTEM and Administrators only.
    // (This is the default DACL of Program Files without the Users entry.)
    //
    // We _don't_ do this by just adding a Deny entry for Users.  Deny entries
    // take precedence over Allow entries, so this would deny Users access even
    // if they have Administrator or SYSTEM rights.
    std::array<EXPLICIT_ACCESS_W, 2> newAces{};
    wchar_t systemName[]{L"SYSTEM"};
    wchar_t administratorsName[]{L"Administrators"};
    // SYSTEM - Full control
    newAces[0].grfAccessPermissions = FILE_ALL_ACCESS;
    newAces[0].grfAccessMode = GRANT_ACCESS;
    newAces[0].grfInheritance = NO_INHERITANCE;
    newAces[0].Trustee.pMultipleTrustee = nullptr;
    newAces[0].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    newAces[0].Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    newAces[0].Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
    newAces[0].Trustee.ptstrName = systemName;
    // Administrators - same as SYSTEM ACE except for name
    newAces[1] = newAces[0];
    newAces[1].Trustee.ptstrName = administratorsName;
    // Create an ACL containing those ACEs
    struct WinCloseAcl
    {
        void operator()(PACL pAcl){if(pAcl) ::LocalFree(static_cast<HLOCAL>(pAcl));}
    };
    WinGenericHandle<PACL, WinCloseAcl> dacl;
    DWORD daclError = ::SetEntriesInAclW(newAces.size(), newAces.data(), nullptr, dacl.receive());
    if(daclError != ERROR_SUCCESS || !dacl)
    {
        qWarning() << "Unable to create DACL for account.json - error"
            << daclError << "-" << SystemError{HERE};
        return;
    }

    // SetNamedSecurityInfoW() takes a mutable pointer to the object name, it's
    // not clear whether it may actually modify it.
    std::wstring secObjName = accountJsonPath.str().toStdWString();
    DWORD setSecInfoError = ::SetNamedSecurityInfoW(
        secObjName.data(), SE_FILE_OBJECT,
        PROTECTED_DACL_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,
        nullptr, nullptr, dacl, nullptr);
    if(setSecInfoError != ERROR_SUCCESS)
    {
        qWarning() << "Unable to apply DACL to account.json - error"
            << setSecInfoError << "-" << SystemError{HERE};
        return;
    }
#else
    if(!QFile::setPermissions(accountJsonPath, QFile::ReadUser|QFile::WriteUser))
    {
        qWarning() << "Unable to update permissions of account.json";
        return;
    }
#endif

    qInfo() << "Successfully applied permissions to account.json";
}

Daemon::Daemon(const QStringList& arguments, QObject* parent)
    : QObject(parent)
    , _arguments(arguments)
    , _exitCode(0)
    , _started(false)
    , _stopping(false)
    , _server(nullptr)
    , _methodRegistry(new LocalMethodRegistry(this))
    , _rpc(new RemoteNotificationInterface(this))
    , _connection(new VPNConnection(this))
    , _regionRefresher{QStringLiteral("regions list"), ApiBases::piaApi,
                       regionsResource, regionsInitialLoadInterval,
                       regionsRefreshInterval, serverListPublicKey}
    , _shadowsocksRefresher{QStringLiteral("Shadowsocks regions"),
                            ApiBases::piaApi, shadowsocksRegionsResource,
                            regionsInitialLoadInterval, regionsRefreshInterval,
                            serverListPublicKey}
    , _snoozeTimer(this)
    , _pendingSerializations(0)
{
#ifdef PIA_CRASH_REPORTING
    initCrashReporting();
#endif

    // Load settings if they exist
    readProperties(_data, Path::DaemonSettingsDir, "data.json");
    // Load account.json.  If it doesn't exist, write it out now so we can set
    // its permissions.
    if(!readProperties(_account, Path::DaemonSettingsDir, "account.json"))
    {
        writeProperties(_account.toJsonObject(), Path::DaemonSettingsDir, "account.json");
        // Do this only when writing the file the first time, don't do it on
        // every daemon start in case the user overrides the permissions.
        restrictAccountJson();
    }
    bool settingsFileRead = readProperties(_settings, Path::DaemonSettingsDir, "settings.json");

    // Set up connections to write and notify changes to data objects.  Do this
    // before migrating settings, so we write out those changes immediately if
    // they occur.
    _serializationTimer.setSingleShot(true);
    connect(&_serializationTimer, &QTimer::timeout, this, &Daemon::serialize);

    _accountRefreshTimer.setInterval(86400000);
    connect(&_accountRefreshTimer, &QTimer::timeout, this, &Daemon::refreshAccountInfo);

    auto connectPropertyChanges = [this](NativeJsonObject &object, QSet<QString> Daemon::* pSet)
    {
        connect(&object, &NativeJsonObject::propertyChanged, this,
            [this, pSet](const QString& name)
            {
                auto &set = (*this).*pSet;
                int size = set.size();
                set += name;
                if (set.size() > size)
                    queueNotification(&Daemon::notifyChanges);
            });
    };
    connectPropertyChanges(_data, &Daemon::_dataChanges);
    connectPropertyChanges(_account, &Daemon::_accountChanges);
    connectPropertyChanges(_settings, &Daemon::_settingsChanges);
    connectPropertyChanges(_state, &Daemon::_stateChanges);

    // Set up logging.  Do this before migrating settings so tracing from the
    // migration is written (if debug logging is enabled).
    connect(&_settings, &DaemonSettings::debugLoggingChanged, this, [this]() {
        const auto& value = _settings.debugLogging();
        if (value == nullptr)
            g_logger->configure(false, {});
        else
            g_logger->configure(true, *value);
    });
    connect(g_logger, &Logger::configurationChanged, this, [this](bool logToFile, const QStringList& filters) {
        if (logToFile)
            _settings.debugLogging(filters);
        else {
            _settings.debugLogging(nullptr);
            g_logger->wipeLogFile();
            // Also remove the updown script log file if it exists (failure ignored)
            QFile::remove(Path::UpdownLogFile);
            QFile::remove(Path::UpdownLogFile + oldFileSuffix);
            // Wipe out any diagnostics that had been written.  Failure is
            // logged, but there's no error info from QDir
            QDir diagsDir{Path::DaemonDiagnosticsDir};
            if(!diagsDir.removeRecursively())
                qWarning() << "Couldn't clean diagnostics directory";
        }
    });

    // Set initial value of debug logging
    if(g_logger->logToFile())
        _settings.debugLogging(g_logger->filters());
    else
        _settings.debugLogging(nullptr);

    // Migrate/upgrade any settings to the current daemon version
    upgradeSettings(settingsFileRead);

    // Build the sorted and grouped locations from the cached data
    rebuildLocations();

    // Check whether the host supports split tunnel and record errors
    checkSplitTunnelSupport();

    g_data.certificateAuthorities(createCertificateAuthorites());

    // If the client ID hasn't been set (or is somehow invalid), generate one
    if(!ClientId::isValidId(_account.clientId()))
    {
        ClientId newId;
        _account.clientId(newId.id());
    }

    // We have a client ID, so create the PortForwarder
    _portForwarder = new PortForwarder(this, _account.clientId());

    #define RPC_METHOD(name, ...) LocalMethod(QStringLiteral(#name), this, &THIS_CLASS::RPC_##name)
    _methodRegistry->add(RPC_METHOD(handshake));
    _methodRegistry->add(RPC_METHOD(applySettings).defaultArguments(false));
    _methodRegistry->add(RPC_METHOD(resetSettings));
    _methodRegistry->add(RPC_METHOD(connectVPN));
    _methodRegistry->add(RPC_METHOD(writeDiagnostics));
    _methodRegistry->add(RPC_METHOD(writeDummyLogs));
    _methodRegistry->add(RPC_METHOD(disconnectVPN));
    _methodRegistry->add(RPC_METHOD(login));
    _methodRegistry->add(RPC_METHOD(logout));
    _methodRegistry->add(RPC_METHOD(refreshUpdate));
    _methodRegistry->add(RPC_METHOD(downloadUpdate));
    _methodRegistry->add(RPC_METHOD(cancelDownloadUpdate));
    _methodRegistry->add(RPC_METHOD(crash));
    _methodRegistry->add(RPC_METHOD(notifyClientActivate));
    _methodRegistry->add(RPC_METHOD(notifyClientDeactivate));
    _methodRegistry->add(RPC_METHOD(installKext));
    _methodRegistry->add(RPC_METHOD(startSnooze));
    _methodRegistry->add(RPC_METHOD(stopSnooze));
    _methodRegistry->add(RPC_METHOD(inspectUwpApps));
    _methodRegistry->add(RPC_METHOD(checkDriverState));
    #undef RPC_METHOD

    connect(_connection, &VPNConnection::stateChanged, this, &Daemon::vpnStateChanged);
    connect(_connection, &VPNConnection::firewallParamsChanged, this, &Daemon::queueApplyFirewallRules);
    connect(_connection, &VPNConnection::slowIntervalChanged, this,
        [this](bool usingSlowInterval){_state.usingSlowInterval(usingSlowInterval);});
    connect(_connection, &VPNConnection::error, this, &Daemon::vpnError);
    connect(_connection, &VPNConnection::byteCountsChanged, this, &Daemon::vpnByteCountsChanged);
    connect(_connection, &VPNConnection::scannedOriginalNetwork, this, &Daemon::vpnScannedOriginalNetwork);
    connect(_connection, &VPNConnection::usingTunnelConfiguration, this,
        [this](const QString &deviceName, const QString &deviceLocalAddress,
               const QString &deviceRemoteAddress,
               const QStringList &effectiveDnsServers)
        {
            _state.tunnelDeviceName(deviceName);
            _state.tunnelDeviceLocalAddress(deviceLocalAddress);
            _state.tunnelDeviceRemoteAddress(deviceRemoteAddress);
            _state.effectiveDnsServers(effectiveDnsServers);
            queueApplyFirewallRules();
        });
    connect(_connection, &VPNConnection::hnsdSucceeded, this,
            [this](){_state.hnsdFailing(0);});
    connect(_connection, &VPNConnection::hnsdFailed, this,
        [this](const std::chrono::milliseconds &failureDuration)
        {
            // If we haven't reported this failure yet, and hnsd has been
            // failing for 10 seconds, report it.
            if(_state.hnsdFailing() == 0 && failureDuration >= std::chrono::seconds(10))
                _state.hnsdFailing(QDateTime::currentMSecsSinceEpoch());
        });
    connect(_connection, &VPNConnection::hnsdSyncFailure, this,
        [this](bool failing)
        {
            if(!failing)
                _state.hnsdSyncFailure(0);
            else if(_state.hnsdSyncFailure() == 0)
                _state.hnsdSyncFailure(QDateTime::currentMSecsSinceEpoch());
        });

    connect(&_latencyTracker, &LatencyTracker::newMeasurements, this,
            &Daemon::newLatencyMeasurements);
    // Pass the locations loaded from the cached data to LatencyTracker
    _latencyTracker.updateLocations(_data.locations());
    connect(_portForwarder, &PortForwarder::portForwardUpdated, this,
            &Daemon::portForwardUpdated);

    connect(&_settings, &DaemonSettings::portForwardChanged, this,
            &Daemon::updatePortForwarder);
    updatePortForwarder();

    connect(&_regionRefresher, &JsonRefresher::contentLoaded, this,
            &Daemon::regionsLoaded);
    connect(&_shadowsocksRefresher, &JsonRefresher::contentLoaded, this,
            &Daemon::shadowsocksRegionsLoaded);

    connect(this, &Daemon::firstClientConnected, this, [this]() {
        _latencyTracker.start();
        _regionRefresher.startOrOverride(Path::DaemonSettingsDir / "region_override.json",
                                         Path::ResourceDir / "servers.json",
                                         !_data.locations().empty());
        // Check if any Shadowsocks servers are known
        bool haveSsCache = std::any_of(_data.locations().begin(), _data.locations().end(),
            [](const auto &pServerLoc)
            {
                return pServerLoc && pServerLoc->shadowsocks();
            });
        _shadowsocksRefresher.startOrOverride(Path::DaemonSettingsDir / "shadowsocks_override.json",
                                              Path::ResourceDir / "shadowsocks.json",
                                              haveSsCache);
        _updateDownloader.run(true);
        queueNotification(&Daemon::reapplyFirewallRules);
    });

    connect(this, &Daemon::lastClientDisconnected, this, [this]() {
        _updateDownloader.run(false);
        _regionRefresher.stop();
        _shadowsocksRefresher.stop();
        _latencyTracker.stop();
        queueNotification(&Daemon::RPC_disconnectVPN);
        queueNotification(&Daemon::reapplyFirewallRules);
    });
    connect(&_settings, &DaemonSettings::killswitchChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::allowLANChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::overrideDNSChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::splitTunnelEnabledChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::splitTunnelRulesChanged, this, &Daemon::queueApplyFirewallRules);
    // 'method' causes a firewall rule application because it can toggle split tunnel
    connect(&_settings, &DaemonSettings::methodChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_account, &DaemonAccount::loggedInChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::updateChannelChanged, this,
            [this](){_updateDownloader.setGaUpdateChannel(_settings.updateChannel());});
    connect(&_settings, &DaemonSettings::betaUpdateChannelChanged, this,
            [this](){_updateDownloader.setBetaUpdateChannel(_settings.betaUpdateChannel());});
    connect(&_settings, &DaemonSettings::offerBetaUpdatesChanged, this,
            [this](){_updateDownloader.enableBetaChannel(_settings.offerBetaUpdates());});
    connect(&_updateDownloader, &UpdateDownloader::updateRefreshed, this,
            &Daemon::onUpdateRefreshed);
    connect(&_updateDownloader, &UpdateDownloader::downloadProgress, this,
            &Daemon::onUpdateDownloadProgress);
    connect(&_updateDownloader, &UpdateDownloader::downloadFinished, this,
            &Daemon::onUpdateDownloadFinished);
    connect(&_updateDownloader, &UpdateDownloader::downloadFailed, this,
            &Daemon::onUpdateDownloadFailed);
    _updateDownloader.setGaUpdateChannel(_settings.updateChannel());
    _updateDownloader.setBetaUpdateChannel(_settings.betaUpdateChannel());
    _updateDownloader.enableBetaChannel(_settings.offerBetaUpdates());
    _updateDownloader.reloadAvailableUpdates(Update{_data.gaChannelVersionUri(), _data.gaChannelVersion()},
                                             Update{_data.betaChannelVersionUri(), _data.betaChannelVersion()});

    // Update firewall rules whenever a network scan occurs, this updates the
    // split tunnel configuration.
    connect(_connection, &VPNConnection::scannedOriginalNetwork, this, &Daemon::queueApplyFirewallRules);

    queueApplyFirewallRules();
}

Daemon::Daemon(QObject* parent)
    : Daemon(QCoreApplication::arguments(), parent)
{

}

Daemon::~Daemon()
{
    qInfo() << "Daemon shutdown complete";
}

void Daemon::reportError(Error error)
{
    // TODO: Send error event to all clients
    qCritical() << error;
    _rpc->post(QStringLiteral("error"), error.toJsonObject());
}

bool Daemon::hasActiveClient() const
{
    // This is a linear-time search, but we don't expect to have more than a few
    // clients connected.
    for(const auto &pClient : _clients)
    {
        if(pClient->getActive())
            return true;
    }
    return false;
}

bool Daemon::isActive() const
{
    return _state.invalidClientExit() || hasActiveClient();
}

QString Daemon::RPC_handshake(const QString &version)
{
    return QStringLiteral(PIA_VERSION);
}

void Daemon::RPC_applySettings(const QJsonObject &settings, bool reconnectIfNeeded)
{
    // Filter sensitive settings for logging
    QJsonObject logSettings{settings};
    // Mask proxyCustom.username and proxyCustom.password if they're non-empty.
    // Get the proxyCustom object - if logSettings doesn't have that value or it
    // isn't an object, this returns an empty QJsonObject.
    QJsonObject logSettingsProxyCustom = logSettings.value("proxyCustom").toObject();
    if(!logSettingsProxyCustom.isEmpty())
    {
        // If 'username' and/or 'password' contain non-empty strings, mask them
        auto userRef = logSettingsProxyCustom["username"];
        if(!userRef.toString().isEmpty())
            userRef = QStringLiteral("<masked>");
        auto passRef = logSettingsProxyCustom["password"];
        if(!passRef.toString().isEmpty())
            passRef = QStringLiteral("<masked>");
        // Apply the masked object to logSettings
        logSettings["proxyCustom"] = logSettingsProxyCustom;
    }
    qDebug().noquote() << "Applying settings:" << QJsonDocument(logSettings).toJson(QJsonDocument::Compact);

    // Prevent applying unknown settings.  Although Daemon does attempt to
    // preserve unknown settings for compatibility after a downgrade/upgrade,
    // unknown settings can't be created or changed via an RPC.
    //
    // (This is particularly important to ensure that CLI typos don't create
    // unexpected settings, in particular there is no way to remove unexpected
    // settings.)
    //
    // Check all properties being set and ensure they are known.  (Can't use a
    // range-based for loop here due to Qt's unusual map iterator semantics.)
    for(auto itSetting = settings.begin(); itSetting != settings.end(); ++itSetting)
    {
        if(!_settings.isKnownProperty(itSetting.key()))
        {
            qWarning() << "Reject setting change, unknown property:" << itSetting.key();
            throw Error{HERE, Error::Code::DaemonRPCUnknownSetting};
        }
    }

    bool success = _settings.assign(settings);

    // If the settings affect the location choices, recompute them.
    // Port forwarding and method affect the "best" location selection.
    if(settings.contains(QLatin1String("location")) ||
       settings.contains(QLatin1String("proxyShadowsocksLocation")) ||
       settings.contains(QLatin1String("portForward")) ||
       settings.contains(QLatin1String("method")))
    {
        qInfo() << "Settings affect location choices, rebuild locations";

        rebuildLocations();
    }

    // If applying the settings failed, we won't reconnect (ensures that
    // _state.needsReconnect() is still set before we throw)
    if (!success)
        reconnectIfNeeded = false;

    // If we're going to reconnect immediately, avoid a needsReconnect() blip in
    // state.  VPNConnection still internally detects that a reconnect is
    // needed so we will reconnect later.
    if (!_state.needsReconnect() && !reconnectIfNeeded && _connection->needsReconnect())
        _state.needsReconnect(true);

    if (!success)
    {
        qWarning() << "Not all settings applied:" << *_settings.error();
        throw *_settings.error();
    }

    // If the VPN is enabled, a reconnect is needed, and the caller asked to
    // reconnect, reconnect now.
    if(reconnectIfNeeded)
    {
        if(_state.vpnEnabled() && _connection->needsReconnect())
        {
            qInfo() << "Reconnecting due to setting change with reconnectIfNeeded=true";
            Q_ASSERT(isActive());   // Class invariant, vpnEnabled() is cleared when inactive
            RPC_connectVPN();
        }
        else
        {
            qInfo() << "Setting change had reconnectIfNeeded=true, but no reconnect was needed - enabled:"
                << _state.vpnEnabled() << "- needsReconnect:" << _connection->needsReconnect();
        }
    }
}

void Daemon::RPC_resetSettings()
{
    // Reset the settings by applying all default values.
    // This ensures that any logic that is applied when changing settings takes
    // place (the needs-reconnect and chosen/next location state fields).
    QJsonObject defaultsJson = DaemonSettings{}.toJsonObject();

    // Don't reset these values - remove them before applying the defaults

    // Last daemon version - not a setting
    defaultsJson.remove(QStringLiteral("lastUsedVersion"));

    // Location - not presented as a "setting"
    defaultsJson.remove(QStringLiteral("location"));

    // Help page settings are not reset, as they were most likely changed for
    // troubleshooting.
    defaultsJson.remove(QStringLiteral("debugLogging"));
    defaultsJson.remove(QStringLiteral("offerBetaUpdates"));

    RPC_applySettings(defaultsJson, false);
}

void Daemon::RPC_connectVPN()
{
    // Cannot connect when no active client is connected (there'd be no way for
    // the daemon to know if the user logs out, etc.)
    if(!isActive())
        throw Error{HERE, Error::Code::DaemonRPCDaemonInactive};
    // Cannot connect when not logged in
    if(!_account.loggedIn())
        throw Error{HERE, Error::Code::DaemonRPCNotLoggedIn};

    _snoozeTimer.forceStopSnooze();
    connectVPN();
}

DiagnosticsFile::DiagnosticsFile(const QString &filePath)
    : _diagFile{filePath}, _currentSize{0}
{
    if(!_diagFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qWarning() << "Couldn't write diagnostics to" << filePath
            << "- error:" << _diagFile.error() << _diagFile.errorString();
        throw Error{HERE, Error::Code::DaemonRPCDiagnosticsFailed};
    }

    _fileWriter.setDevice(&_diagFile);
}

QString DiagnosticsFile::diagnosticsCommandHeader(const QString &commandName)
{
    return QStringLiteral("\n/PIA_PART/%1\n").arg(commandName);
}

void DiagnosticsFile::logPart(const QString &title, QElapsedTimer &commandTime)
{
    qint64 newSize = _diagFile.size();
    qint64 partSize = newSize - _currentSize;
    _currentSize = newSize;

    auto cmdTimeNsec = commandTime.nsecsElapsed();
    qInfo() << "Wrote" << title << "in" << (cmdTimeNsec / 1.0e9) << "sec -"
        << partSize << "bytes";
}

void DiagnosticsFile::execAndWriteOutput(QProcess &cmd, const QString &commandName,
                                         const ProcessOutputFunction &processOutput)
{
    QElapsedTimer commandTime;
    commandTime.start();

    _fileWriter << diagnosticsCommandHeader(commandName);

    cmd.start();
    // Only wait 5 seconds for each process.  Occasionally some commands might
    // time out, but it's confusing for users if this takes a long time due to
    // the current lack of feedback that we're preparing the report.
    if(cmd.waitForFinished(5000))
    {
        _fileWriter << "Exit code: " << cmd.exitCode() << endl;
        _fileWriter << "STDOUT: " << endl;
        QByteArray output = cmd.readAllStandardOutput();
        _fileWriter << (processOutput ? processOutput(output) : output) << endl;
        _fileWriter << "STDERR: " << endl;
        _fileWriter << cmd.readAllStandardError() << endl;
    }
    else
    {
        _fileWriter << "Failed to run command: " << cmd.program() << endl;
        _fileWriter << qEnumToString(cmd.error()) << endl;
        _fileWriter << cmd.errorString() << endl;
    }

    logPart(commandName, commandTime);
}

void DiagnosticsFile::writeCommand(const QString &commandName,
                                   const QString &command,
                                   const QStringList &args,
                                   const ProcessOutputFunction &processOutput)
{
    QProcess cmd;
    cmd.setArguments(args);
    cmd.setProgram(command);
    execAndWriteOutput(cmd, commandName, processOutput);
}

#ifdef Q_OS_WIN
// On Windows, we have to be able to set the complete command line string in
// some cases; QProcess::start() explains this in detail.
// QProcess::setNativeArguments() is only available on Windows.
void DiagnosticsFile::writeCommand(const QString &commandName,
                                   const QString &command,
                                   const QString &nativeArgs,
                                   const ProcessOutputFunction &processOutput)
{
    QProcess cmd;
    cmd.setNativeArguments(nativeArgs);
    cmd.setProgram(command);
    execAndWriteOutput(cmd, commandName, processOutput);
}
#endif

void DiagnosticsFile::writeText(const QString &title, const QString &text)
{
    QElapsedTimer commandTime;
    commandTime.start();

    _fileWriter << diagnosticsCommandHeader(title);
    _fileWriter << text << endl;

    // Only the size is really important for logging a text part, but log the
    // time too for consistency (the time to generate the text isn't included,
    // but it can be calculated from the log timestamps)
    logPart(title, commandTime);
}

QJsonValue Daemon::RPC_writeDiagnostics()
{
    // Diagnostics can only be written when debug logging is enabled
    if(!_settings.debugLogging())
    {
        qInfo() << "Not writing diagnostics, logging is not enabled";
        throw Error{HERE, Error::Code::DaemonRPCDiagnosticsNotEnabled};
    }

    qDebug () << "Writing Diagnostics now";

    Path::DaemonDiagnosticsDir.mkpath();

    // Clean old diagnostics files, leave the last 5 (arbitrarily).
    Path::DaemonDiagnosticsDir.cleanDirFiles(5);

    // Generate a file name for the current diagnostics.
    // A unique name is used each time to make sure that any submitted report
    // only includes the correct diagnostics for that report, even if we write
    // diagnostics again before the user finishes submitting the report, etc.
    const auto &nowUtc = QDateTime::currentDateTimeUtc();
    const auto &diagFileName = nowUtc.toString(QStringLiteral("'diag_'yyyyMMdd'_'hhmmsszzz'.txt'"));
    const auto &diagFilePath = Path::DaemonDiagnosticsDir / diagFileName;

    DiagnosticsFile file{diagFilePath};

    writePlatformDiagnostics(file);

    auto writePrettyJson = [&file](const QString &title, QJsonObject object, QStringList keysToRemove={}) {
        for (const auto &key : keysToRemove) object.remove(key);
        file.writeText(title, QJsonDocument(object).toJson(QJsonDocument::Indented));
    };

    writePrettyJson("DaemonState", _state.toJsonObject(), { "groupedLocations", "externalIp", "externalVpnIp", "forwardedPort" });
    // The custom proxy setting is removed because it may contain the proxy
    // credentials.
    writePrettyJson("DaemonSettings", _settings.toJsonObject(), { "proxyCustom" });

    qInfo() << "Finished writing diagnostics file" << diagFilePath;

    return QJsonValue{diagFilePath};
}

void Daemon::RPC_writeDummyLogs()
{
    qDebug () << "Writing dummy logs";
    for(int i = 0; i < 10000; i ++) {
        qDebug () << "Writing dummy logs to fill up space (" << i << "/10000)";
    }
}

void Daemon::RPC_crash()
{
    if (QFile(Path::DaemonDataDir / "crash.txt").exists())
    {
        QMetaObject::invokeMethod(this, [] {
            qInfo() << "Intentionally crashing daemon";
            testCrash();
        }, Qt::QueuedConnection);
    }
    else {
      qWarning () << "Called RPC Crash without crash enabler. Ignored";
    }
}

void Daemon::RPC_disconnectVPN()
{
  _snoozeTimer.forceStopSnooze();
  disconnectVPN();
}

void Daemon::RPC_notifyClientActivate()
{
    ClientConnection *pClient = ClientConnection::getInvokingClient();

    if(!pClient)
    {
        qWarning() << "Invalid invoking client in client RPC";
        return;
    }

    if(pClient->getActive())
    {
        // Ignore this, no effect, client is already active
        qInfo() << "Client" << pClient << "activated again";
        return;
    }

    bool wasActive = isActive();

    qInfo() << "Client" << pClient << "has activated";
    pClient->setActive(true);

    if(!wasActive && isActive())
        emit firstClientConnected();
}

void Daemon::RPC_notifyClientDeactivate()
{
    ClientConnection *pClient = ClientConnection::getInvokingClient();

    if(!pClient)
    {
        qWarning() << "Invalid invoking client in client RPC";
        return;
    }

    // Duplicate deactivations or deactivates from inactive clients are OK;
    // trace them specifically and don't check hasInteractiveClient().
    if(!pClient->getActive())
    {
        qInfo() << "Client" << pClient << "is already inactive";
        return;
    }

    // We know isActive() == true because there is an interactive client present
    Q_ASSERT(isActive());

    qDebug () << "Client" << pClient << "has deactivated";
    pClient->setActive(false);

    // Since a client is exiting cleanly, clear the invalid client exit flag
    _state.invalidClientExit(false);

    // If it was the last interactive client, shut down
    if(!isActive())
        emit lastClientDisconnected();
}

void Daemon::RPC_startSnooze(qint64 seconds)
{
  _snoozeTimer.startSnooze(seconds);
}

void Daemon::RPC_stopSnooze()
{
  _snoozeTimer.stopSnooze();
}

QJsonValue Daemon::RPC_installKext()
{
    // Not implemented; overridden on Mac OS with implementation
    throw Error{HERE, Error::Code::Unknown};
}

QJsonValue Daemon::RPC_inspectUwpApps(const QJsonArray &)
{
    // Not implemented; overridden on Windows with implementation
    throw Error{HERE, Error::Code::Unknown};
}

void Daemon::RPC_checkDriverState()
{
    // Not implemented; overridden on Windows with implementation
    throw Error{HERE, Error::Code::Unknown};
}

Async<void> Daemon::RPC_login(const QString& username, const QString& password)
{
    return ApiClient::instance()
            ->postRetry(
                QStringLiteral("api/client/v2/token"),
                QJsonDocument({
                    { QStringLiteral("username"), username },
                    { QStringLiteral("password"), password },
                }))
            ->then(this, [this, username, password](const QJsonDocument& json) {
                QString token = json[QLatin1String("token")].toString();
                if (token.isEmpty())
                    throw Error(HERE, Error::ApiBadResponseError);
                return loadAccountInfo({}, {}, token)
                        ->then(this, [=](const QJsonObject& account) {
                            _account.username(username);
                            _account.password({});
                            _account.token(token);
                            _account.openvpnUsername(token.left(token.length() / 2));
                            _account.openvpnPassword(token.right(token.length() - (token.length() / 2)));
                            _account.assign(account);
                            _account.loggedIn(true);
                        });
            })
            ->except(this, [this, username, password](const Error& error) {
                if (error.code() == Error::ApiUnauthorizedError)
                    throw error;
                // Proceed with empty account info; the client can still connect
                // if the naked username/password is valid for OpenVPN auth.
                resetAccountInfo();
                _account.username(username);
                _account.password(password);
                _account.openvpnUsername(username);
                _account.openvpnPassword(password);
                _account.loggedIn(true);
            });
}

void Daemon::RPC_logout()
{
    // If the VPN is connected, disconnect before logging out.  We don't wait
    // for this to complete though.
    RPC_disconnectVPN();

    // Reset account data along with relevant settings
    QString tokenToExpire = _account.token();

    _account.reset();
    _settings.recentLocations({});
    _state.openVpnAuthFailed(0);

    if(!tokenToExpire.isEmpty()) {
        ApiClient::instance()
                        ->postRetry(QStringLiteral("api/client/v2/expire_token"), QJsonDocument(), ApiClient::tokenAuth(tokenToExpire))
                        ->notify(this, [this](const Error &error, const QJsonDocument &json) {
            if(error) {
                qWarning () << "Token expire failed with error code: " << error.code();
            } else {
                QString status = json[QLatin1String("status")].toString();
                if (status != QStringLiteral("success"))
                    qWarning () << "API Token expire: Bad result: " << json.toJson();
                else
                    qDebug () << "API Token expire: success";
            }
        });
    }

}

void Daemon::RPC_refreshUpdate()
{
    _updateDownloader.refreshUpdate();
}

Async<QJsonValue> Daemon::RPC_downloadUpdate()
{
    return _updateDownloader.downloadUpdate();
}

void Daemon::RPC_cancelDownloadUpdate()
{
    _updateDownloader.cancelDownload();
}

void Daemon::start()
{
    // Perform any startup actions such as listening on sockets and setting
    // up timers here, then emit started().

    _server = new LocalSocketIPCServer(this);
    connect(_server, &IPCServer::newConnection, this, &Daemon::clientConnected);
    connect(_rpc, &RemoteNotificationInterface::messageReady, _server, &IPCServer::sendMessageToAllClients);
    _server->listen();

    connect(&_account, &DaemonAccount::loggedInChanged, this, [this]() {
        if (_account.loggedIn())
            _accountRefreshTimer.start();
        else
            _accountRefreshTimer.stop();
    });
    if (_account.loggedIn())
    {
        _accountRefreshTimer.start();
        refreshAccountInfo();
    }

    qInfo() << "Daemon started and waiting for connections...";

    _started = true;
    emit started();
}

void Daemon::stop()
{
    if (!_started)
        return;
    if (!_stopping)
        qInfo() << "Daemon stopping...";
    _stopping = true;

    // Perform any necessary actions before the message loop can be stopped
    // here, then emit stopped().

    if (_connection->state() != VPNConnection::State::Disconnected)
    {
        _connection->disconnectVPN();
        return;
    }

    qInfo() << "Daemon cleanly stopped";

    _started = false;
    emit stopped();
}

void Daemon::clientConnected(IPCConnection* connection)
{
    auto client = new ClientConnection(connection, _methodRegistry, this);
    _clients.insert(connection, client);
    qInfo() << "New client" << client << "connected, total client count now"
        << _clients.size() << "- have active client:" << hasActiveClient();
    // No need to check if this is the first client, new clients are initially
    // Transient clients so this does not affect isActive().
    connect(client, &ClientConnection::disconnected, this, [this, client, connection]() {
        _clients.remove(connection);
        qInfo() << "Client" << client << "disconnected, total client count now"
            << _clients.size() << "- have active client:" << hasActiveClient();
        // If the client was active, this exit is unexpected (assume the client
        // crashed).  If this would have caused the daemon to deactivate, set
        // invalidClientExit() to remain active.
        if(client->getActive() && !isActive())
        {
            qWarning() << "Client" << client << "disconnected but did not deactivate";
            _state.invalidClientExit(true);
            // This causes the daemon to remain active (we don't emit
            // lastClientDisconnected())
            Q_ASSERT(isActive());
        }
    });

    QJsonObject all;
    all.insert(QStringLiteral("data"), g_data.toJsonObject());
    all.insert(QStringLiteral("account"), g_account.toJsonObject());
    all.insert(QStringLiteral("settings"), g_settings.toJsonObject());
    all.insert(QStringLiteral("state"), g_state.toJsonObject());
    client->post(QStringLiteral("data"), all);
}

QJsonObject getProperties(const NativeJsonObject& object, const QSet<QString>& properties)
{
    QJsonObject result;
    for (const QString& property : properties)
    {
        const QJsonValue& value = object.get(property);
        result.insert(property, value.isUndefined() ? QJsonValue::Null : value);
    }
    return result;
}

void Daemon::notifyChanges()
{
    QJsonObject all;
    if (!_dataChanges.empty())
    {
        all.insert(QStringLiteral("data"), getProperties(_data, std::exchange(_dataChanges, {})));
        _pendingSerializations |= 1;
    }
    if (!_accountChanges.empty())
    {
        all.insert(QStringLiteral("account"), getProperties(_account, std::exchange(_accountChanges, {})));
        _pendingSerializations |= 2;
    }
    if (!_settingsChanges.empty())
    {
        all.insert(QStringLiteral("settings"), getProperties(_settings, std::exchange(_settingsChanges, {})));
        _pendingSerializations |= 4;
    }
    if (!_stateChanges.empty())
    {
        all.insert(QStringLiteral("state"), getProperties(_state, std::exchange(_stateChanges, {})));
    }
    serialize();
    _rpc->post(QStringLiteral("data"), all);
}

void Daemon::serialize()
{
    if (_pendingSerializations)
    {
        if (!_serializationTimer.isActive())
        {
            if (_pendingSerializations & 1)
                writeProperties(_data.toJsonObject(), Path::DaemonSettingsDir, "data.json");
            if (_pendingSerializations & 2)
                writeProperties(_account.toJsonObject(), Path::DaemonSettingsDir, "account.json");
            if (_pendingSerializations & 4)
            {
                QJsonObject settings = _settings.toJsonObject();
                settings.remove(QStringLiteral("debugLogging"));
                writeProperties(settings, Path::DaemonSettingsDir, "settings.json");
            }
            _pendingSerializations = 0;
            _serializationTimer.start(5000);
        }
    }
}

void Daemon::vpnStateChanged(VPNConnection::State state,
                             const ConnectionConfig &connectingConfig,
                             const ConnectionConfig &connectedConfig,
                             const nullable_t<Transport> &chosenTransport,
                             const nullable_t<Transport> &actualTransport)
{
    if (_stopping && state == VPNConnection::State::Disconnected)
    {
        stop();
        return;
    }

    if (!_connection->needsReconnect())
        _state.needsReconnect(false);
    _state.connectionState(qEnumToString(state));
    _state.chosenTransport(chosenTransport);
    _state.actualTransport(actualTransport);

    // Populate a ConnectionInfo in DaemonState from VPNConnection's ConnectionConfig
    auto populateConnection = [](ConnectionInfo &info, const ConnectionConfig &config)
    {
        info.vpnLocation(config.vpnLocation());
        info.vpnLocationAuto(config.vpnLocationAuto());
        switch(config.method())
        {
            default:
            case ConnectionConfig::Method::OpenVPN:
                info.method(QStringLiteral("openvpn"));
                break;
            case ConnectionConfig::Method::Wireguard:
                info.method(QStringLiteral("wireguard"));
                break;
        }
        info.methodForcedByAuth(config.methodForcedByAuth());
        switch(config.dnsType())
        {
            default:
            case ConnectionConfig::DnsType::Pia:
                info.dnsType(QStringLiteral("pia"));
                break;
            case ConnectionConfig::DnsType::Handshake:
                info.dnsType(QStringLiteral("handshake"));
                break;
            case ConnectionConfig::DnsType::Existing:
                info.dnsType(QStringLiteral("existing"));
                break;
            case ConnectionConfig::DnsType::Custom:
                info.dnsType(QStringLiteral("custom"));
                break;
        }
        info.defaultRoute(config.defaultRoute());
        switch(config.proxyType())
        {
            default:
            case ConnectionConfig::ProxyType::None:
                info.proxy(QStringLiteral("none"));
                info.proxyCustom({});
                info.proxyShadowsocks({});
                info.proxyShadowsocksLocationAuto(false);
                break;
            case ConnectionConfig::ProxyType::Custom:
            {
                info.proxy(QStringLiteral("custom"));
                QString host = config.customProxy().host();
                if(config.customProxy().port())
                {
                    host += ':';
                    host += QString::number(config.customProxy().port());
                }
                info.proxyCustom(host);
                info.proxyShadowsocks({});
                info.proxyShadowsocksLocationAuto(false);
                break;
            }
            case ConnectionConfig::ProxyType::Shadowsocks:
                info.proxy(QStringLiteral("shadowsocks"));
                info.proxyCustom({});
                info.proxyShadowsocks(config.shadowsocksLocation());
                info.proxyShadowsocksLocationAuto(config.shadowsocksLocationAuto());
                break;
        }
        info.portForward(config.requestPortForward());
    };
    populateConnection(_state.connectingConfig(), connectingConfig);
    populateConnection(_state.connectedConfig(), connectedConfig);

    queueNotification(&Daemon::reapplyFirewallRules);

    // Latency measurements only make sense when we're not connected to the VPN
    if(state == VPNConnection::State::Disconnected && isActive())
    {
        _latencyTracker.start();
        // Kick off a region refresh so we typically rotate servers on a
        // reconnect.  Usually the request right after connecting covers this,
        // but this is still helpful in case we were not able to load the
        // resource then.
        _regionRefresher.refresh();
        _shadowsocksRefresher.refresh();
    }
    else
        _latencyTracker.stop();

    // Update ApiNetwork and PortForwarder.
    updatePortForwarder();
    if(state == VPNConnection::State::Connected)
    {
        // Use the VPN interface for all API requests
        QHostAddress tunnelLocalAddr{_state.tunnelDeviceLocalAddress()};
        if(tunnelLocalAddr.protocol() != QAbstractSocket::NetworkLayerProtocol::IPv4Protocol)
        {
            qWarning() << "Could not start SOCKS server for invalid local address"
                << _state.tunnelDeviceLocalAddress();
            ApiNetwork::instance()->setProxy({});
        }
        else
        {
            _socksServer.start(tunnelLocalAddr, _state.tunnelDeviceName());
            if(!_socksServer.port())
            {
                qWarning() << "SOCKS proxy failed to start";
                ApiNetwork::instance()->setProxy({});
            }
            else
            {
                QNetworkProxy localProxy{QNetworkProxy::ProxyType::Socks5Proxy,
                                         QStringLiteral("127.0.0.1"),
                                         _socksServer.port()};
                // This proxy does not support hostname lookup, UDP, or
                // listening
                localProxy.setCapabilities(QNetworkProxy::TunnelingCapability);
                localProxy.setUser(QString::fromLatin1(SocksConnection::username));
                localProxy.setPassword(QString::fromLatin1(_socksServer.password()));
                ApiNetwork::instance()->setProxy(localProxy);
            }
        }

        // Figure out if the connected location supports PF
        Q_ASSERT(connectedConfig.vpnLocation());    // Guarantee by VPNConnection, valid in this state
        if(connectedConfig.vpnLocation()->portForward())
            _portForwarder->updateConnectionState(PortForwarder::State::ConnectedSupported);
        else
            _portForwarder->updateConnectionState(PortForwarder::State::ConnectedUnsupported);

        // Perform a refresh immediately after connect so we get a new IP on reconnect.
        _regionRefresher.refresh();
        _shadowsocksRefresher.refresh();

        // If we haven't obtained a token yet, try to do that now that we're
        // connected (the API is likely reachable through the tunnel).
        if(_account.token().isEmpty())
            refreshAccountInfo();
    }
    else
    {
        ApiNetwork::instance()->setProxy({});
        _socksServer.stop();
        _portForwarder->updateConnectionState(PortForwarder::State::Disconnected);
    }

    if(state == VPNConnection::State::Connected && connectedConfig.requestMace())
    {
        _connection->activateMACE();
    }

    // If the connection is in any state other than Connected:
    // - clear the VPN IP address - it's no longer valid
    // - reset the connection timestamp
    // If the VPN is about to reconnect, we'll update both of these if we reach
    // the Connected state again.
    if (state != VPNConnection::State::Connected)
    {
        _state.externalVpnIp({});
        _state.connectionTimestamp(0);
        // Discard any ongoing request to get the VPN IP.  (If it hasn't
        // completed yet, we're abandoning it, but it might also have
        // completed.)
        _pVpnIpRequest.abandon();

        // Clear warnings that are only valid in the Connected state
        _state.hnsdFailing(0);
        _state.hnsdSyncFailure(0);
    }
    // The VPN is connected - if we haven't found the VPN IP yet, find it
    else if (_state.externalVpnIp().isEmpty())
    {
        QElapsedTimer monotonicTimer;
        monotonicTimer.start();
        _state.connectionTimestamp(monotonicTimer.msecsSinceReference());

        // Get the user's VPN IP address now that we're connected
        _pVpnIpRequest = ApiClient::instance()
                ->getVpnIpRetry(QStringLiteral("api/client/status"))
                ->then(this, [this](const QJsonDocument& json) {
                    _state.externalVpnIp(json[QStringLiteral("ip")].toString());
                })
                ->except(this, [this](const Error& err) {
                    qWarning() << "Couldn't get VPN IP address due to error:" << err;
                });
    }

    // Clear the external IP address if we're completely disconnected.  Leave
    // this set if we go to any state like Connecting, Reconnecting,
    // Interrupted, etc., because we only find this address again when a
    // connection is initiated by an RPC.
    if (state == VPNConnection::State::Disconnected)
    {
        _state.externalIp({});
        _state.tunnelDeviceName({});
        _state.tunnelDeviceLocalAddress({});
        _state.tunnelDeviceRemoteAddress({});
        _state.effectiveDnsServers({});
        // The original network configuration is not meaningful when not
        // connected; we don't have any way to know if it changes.  (When
        // connected, we rely on the VPN connection breaking to know when it
        // changes.)
        _state.originalGatewayIp({});
        _state.originalInterface({});
        _state.originalInterfaceIp({});
    }

    // Clear fatal errors when we successfully establish a connection.  (Don't
    // clear them when disconnecting, because fatal errors cause a disconnect.)
    if(state == VPNConnection::State::Connected)
    {
        // Connected, so we were able to authenticate.
        _state.openVpnAuthFailed(0);
    }

    // Indicate unexpected loss of connection
    if(state == VPNConnection::State::Interrupted)
        _state.connectionLost(QDateTime::currentMSecsSinceEpoch());

    // Clear nonfatal errors when we're no longer attempting to connect - either
    // connected or disconnected.
    if(state == VPNConnection::State::Connected ||
       state == VPNConnection::State::Disconnected ||
       state == VPNConnection::State::Disconnecting)
    {
        _state.connectionLost(0);
        _state.proxyUnreachable(0);
        _state.dnsConfigFailed(0);
    }

    // If we've connected, queue a notification to dump the routing table.  This
    // is strictly for debug logging, so it's OK if this ends up happening after
    // we disconnect occasionally.
    if(state == VPNConnection::State::Connected)
        queueNotification(&Daemon::logRoutingTable);

    queueApplyFirewallRules();
}

void Daemon::vpnError(const Error& error)
{
    switch(error.code())
    {
        // Report auth errors - VpnConnection has stopped trying to connect
        case Error::Code::OpenVPNAuthenticationError:
            _state.openVpnAuthFailed(QDateTime::currentMSecsSinceEpoch());
            break;
        // Report nonfatal errors too
        case Error::Code::OpenVPNDNSConfigError:
            _state.dnsConfigFailed(QDateTime::currentMSecsSinceEpoch());
            break;
        case Error::Code::OpenVPNProxyResolveError:
        case Error::Code::OpenVPNProxyAuthenticationError:
        case Error::Code::OpenVPNProxyError:
            _state.proxyUnreachable(QDateTime::currentMSecsSinceEpoch());
            break;
        default:
            break;
    }

    reportError(error);
}

void Daemon::vpnByteCountsChanged()
{
    _state.bytesReceived(_connection->bytesReceived());
    _state.bytesSent(_connection->bytesSent());
    _state.intervalMeasurements(_connection->intervalMeasurements());
}

// Find original gateway IP and interface
void Daemon::vpnScannedOriginalNetwork(const OriginalNetworkScan &netScan)
{
    _state.originalGatewayIp(netScan.gatewayIp());
    _state.originalInterface(netScan.interfaceName());
    _state.originalInterfaceIp(netScan.ipAddress());
    qInfo() << QStringLiteral("originalGatewayIp = %1").arg(_state.originalGatewayIp());
    qInfo() << QStringLiteral("originalInterface = %1").arg(_state.originalInterface());
    qInfo() << QStringLiteral("originalInterfaceIp = %1").arg(_state.originalInterfaceIp());
}

void Daemon::newLatencyMeasurements(const LatencyTracker::Latencies &measurements)
{
    SCOPE_LOGGING_CATEGORY("daemon.latency");

    bool locationsChanged = false;

    for(const auto &measurement : measurements)
    {
        // Look for a ServerLocation with this ID, and set its latency.
        // There can only be one location with this ID - the locations are sent
        // as attributes of a JSON object, and the JSON parser won't load more
        // than one value for the same attribute name.
        auto itLocation = std::find_if(_data.locations().begin(),
                                       _data.locations().end(),
                                       [&](const auto &pLocation)
                                       {
                                           return pLocation->id() == measurement.first;
                                       });
        // If the location still exists, store the new latency.
        if(itLocation != _data.locations().end())
        {
            // Get pointer from iterator-to-pointer
            const auto &pLocation = *itLocation;
            pLocation->latency(static_cast<double>(measurement.second.count()));

            // We applied at least one measurement, rebuild the grouped
            // locations and trigger updates
            locationsChanged = true;
        }
    }

    if(locationsChanged)
    {
        // Rebuild the grouped locations, since the locations changed
        rebuildLocations();

        // At the moment, Daemon only detects changes in properties of
        // DaemonData itself, not properties of nested objects.  As a
        // workaround, emit the change notifications here so the latency change
        // is detected and propagated to clients.
        //
        // Daemon doesn't actually listen to locationsChanged, but emit it too
        // in case anything else does
        emit _data.locationsChanged();
        emit _data.propertyChanged(QStringLiteral("locations"));
    }
}

void Daemon::portForwardUpdated(int port)
{
    qInfo() << "Forwarded port updated to" << port;
    _state.forwardedPort(port);
}

void Daemon::updateSupportedVpnPorts(const QJsonObject &serversObj)
{
    const auto &portInfo = serversObj["info"].toObject()["vpn_ports"].toObject();
    const QString warningMsg{"Could not find supported vpn_ports from region data. Error: %1"};

    try
    {
        _data.udpPorts(JsonCaster{portInfo["udp"]});
        _data.tcpPorts(JsonCaster{portInfo["tcp"]});
    }
    catch (const std::exception &ex)
    {
        qWarning() << warningMsg.arg(ex.what());
    }

    // If our currently selected ports are not present in the supported ports, then reset to 0 (auto)
    if (!_data.udpPorts().contains(_settings.remotePortUDP())) _settings.remotePortUDP(0);
    if (!_data.tcpPorts().contains(_settings.remotePortTCP())) _settings.remotePortTCP(0);
}

void Daemon::regionsLoaded(const QJsonDocument &regionsJsonDoc)
{
    const auto &serversObj = regionsJsonDoc.object();

    // update the available port numbers for udp/tcp
    updateSupportedVpnPorts(serversObj);

    //Build ServerLocations from the JSON document
    ServerLocations newLocations = updateServerLocations(_data.locations(),
                                                         serversObj);

    //If no locations were found, treat this as an error, since it would
    //prevent any connections from being made
    if(newLocations.empty())
    {
        qWarning() << "Server location data could not be loaded.  Received"
            << regionsJsonDoc.toJson();
        return;
    }

    // The data were loaded successfully, store it in DaemonData
    _data.locations(newLocations);

    // Update the grouped locations too
    rebuildLocations();

    //Update the locations in LatencyTracker
    _latencyTracker.updateLocations(newLocations);

    // A load succeeded, tell JsonRefresher to switch to the long interval
    _regionRefresher.loadSucceeded();
}

void Daemon::shadowsocksRegionsLoaded(const QJsonDocument &shadowsocksRegionsJsonDoc)
{
    const auto &shadowsocksRegionsObj = shadowsocksRegionsJsonDoc.object();

    // Build new ServerLocations
    auto newLocations = updateShadowsocksLocations(_data.locations(),
                                                   shadowsocksRegionsObj);
    _data.locations(newLocations);

    // Rebuild grouped locations and consequent location state
    rebuildLocations();

    _shadowsocksRefresher.loadSucceeded();
}

void Daemon::refreshAccountInfo()
{
    if (!_account.loggedIn())
        return;
    if (_account.username().isEmpty() || (_account.token().isEmpty() && _account.password().isEmpty()))
        RPC_logout();
    else if (_account.token().isEmpty())
    {
        // Try to exchange username+password for a token.
        RPC_login(_account.username(), _account.password())
                ->notify(this, [this](const Error& error) {
                    if (error.code() == Error::ApiUnauthorizedError)
                        RPC_logout();
                });
    }
    else
    {
        loadAccountInfo(_account.username(), _account.password(), _account.token())
                ->notify(this, [this](const Error& error, const QJsonObject& account) {
                    if (!error)
                        _account.assign(account);
                    else if (error.code() == Error::ApiUnauthorizedError)
                        RPC_logout();
                });
    }
}

// Load account info from the web API and return the result asynchronously
// as a QJsonObject appropriate for assigning to DaemonAccount.
//
// If the result fails with Error::ApiUnauthorizedError, the supplied login
// credentials should be considered invalid and the users should be logged
// out. For other errors, the API should merely be considered unreachable
// and the app should try to proceed anyway.
//
Async<QJsonObject> Daemon::loadAccountInfo(const QString& username, const QString& password, const QString& token)
{
    const QString resource = token.isEmpty() ? QStringLiteral("api/client/account") : QStringLiteral("api/client/v2/account");

    return ApiClient::instance()
            ->getRetry(resource, ApiClient::autoAuth(username, password, token))
            ->then(this, [=](const QJsonDocument& json) {
                if (!json.isObject())
                    throw Error(HERE, Error::ApiBadResponseError);

                QJsonObject result;
                QJsonValue value;
                // Define a helper to assign (and optionally transform) a value from
                // the source data if present there, otherwise a default value.
                #define assignOrDefault(field, dataName, ...) do { if (!(value = json[QLatin1String(dataName)]).isUndefined()) { __VA_ARGS__ } else { value = json_cast<QJsonValue>(DaemonAccount::default_##field()); } result.insert(QStringLiteral(#field), value); } while(false)
                assignOrDefault(plan, "plan");
                assignOrDefault(active, "active");
                assignOrDefault(canceled, "canceled");
                assignOrDefault(recurring, "recurring");
                assignOrDefault(needsPayment, "needs_payment");
                assignOrDefault(daysRemaining, "days_remaining");
                assignOrDefault(renewable, "renewable");
                assignOrDefault(renewURL, "renew_url", {
                    if (value == QStringLiteral("https://www.privateinternetaccess.com/pages/client-support/"))
                        value = QStringLiteral("https://www.privateinternetaccess.com/pages/client-sign-in");
                });
                assignOrDefault(expirationTime, "expiration_time", {
                    value = value.toDouble() * 1000.0;
                });
                assignOrDefault(expireAlert, "expire_alert");
                assignOrDefault(expired, "expired");
                assignOrDefault(email, "email");
                #undef assignOrDefault

                return result;
            });
}

void Daemon::resetAccountInfo()
{
    // Reset all fields except loggedIn and clientId
    QJsonObject blank = DaemonAccount().toJsonObject();
    blank.remove(QStringLiteral("loggedIn"));
    blank.remove(QStringLiteral("clientId"));
    _account.assign(blank);
}

void Daemon::reapplyFirewallRules()
{
    FirewallParams params {};

    const ConnectionInfo *pConnSettings = nullptr;
    // If we are currently attempting to connect, use those settings to update
    // the firewall.
    if(_state.connectingConfig().vpnLocation())
        pConnSettings = &_state.connectingConfig();
    // If the VPN is enabled, have we connected since it was enabled?
    // Note that it is possible for vpnEnabled() to be true while in the
    // "Disconnected" state - this happens when a fatal error causes the app to
    // stop reconnecting after connection loss.  We _do_ want the "auto"
    // killswitch to be engaged in this case.
    if(_state.vpnEnabled() && _state.connectedConfig().vpnLocation())
    {
        // Yes, we have connected
        params.hasConnected = true;
        // If we aren't currently attempting another connection, use the current
        // connection to update the firewall
        if(!pConnSettings && _state.connectionState() == QStringLiteral("Connected"))
            pConnSettings = &_state.connectedConfig();
    }

    bool killswitchEnabled = false;
    // If the daemon is not active (no client is connected) or the user is not
    // logged in to an account, we do not apply the KS.
    if (!isActive() || !_account.loggedIn())
        killswitchEnabled = false;
    else if (_settings.killswitch() == QLatin1String("on"))
        killswitchEnabled = true;
    else if (_settings.killswitch() == QLatin1String("auto") && _state.vpnEnabled())
        killswitchEnabled = params.hasConnected;

    const bool vpnActive = _state.vpnEnabled();

    params.effectiveDnsServers = _state.effectiveDnsServers();
    if(VPNMethod *pMethod = _connection->vpnMethod())
    {
        params.adapter = pMethod->getNetworkAdapter();
    }

    // For OpenVPN, split tunnel is available, and does not require a reconnect
    // to toggle.  However, for WireGuard, split tunnel is not currently
    // available at all.
    //
    // Check the connected/connecting method if we're currently connected or
    // connecting; otherwise use the chosen method.
    bool allowSplitTunnel = true;
    if(pConnSettings)
        allowSplitTunnel = pConnSettings->method() == QStringLiteral("openvpn");
    else
        allowSplitTunnel = _settings.method() == QStringLiteral("openvpn");

    if(allowSplitTunnel && _settings.splitTunnelEnabled())
    {
        params.enableSplitTunnel = true;
        params.splitTunnelNetScan = { _state.originalGatewayIp(), _state.originalInterface(),  _state.originalInterfaceIp() };
    }

    params.excludeApps.reserve(_settings.splitTunnelRules().size());
    params.vpnOnlyApps.reserve(_settings.splitTunnelRules().size());

    for(const auto &rule : _settings.splitTunnelRules())
    {
        qInfo() << "split tunnel rule:" << rule.path() << rule.mode();
        // Ignore anything with a rule type we don't recognize
        if(rule.mode() == QStringLiteral("exclude"))
            params.excludeApps.push_back(rule.path());
        else if(rule.mode() == QStringLiteral("include"))
            params.vpnOnlyApps.push_back(rule.path());
    }

    // Though split tunnel in general can be toggled while connected,
    // defaultRoute can't.  The user can toggle split tunnel as long as the
    // effective value for defaultRoute doesn't change.  If it does, we'll still
    // update split tunnel, but the default route change will require a
    // reconnect.
    params.defaultRoute = pConnSettings ? pConnSettings->defaultRoute() : true;

    // When not using the VPN as the default route, force Handshake into the
    // VPN with an "include" rule.  (Just routing the Handshake seeds into the
    // VPN is not sufficient; hnsd uses a local recursive DNS resolver that will
    // query authoritative DNS servers, and we want that to go through the VPN.)
    if(!params.defaultRoute)
        params.vpnOnlyApps.push_back(Path::HnsdExecutable);

    params.blockAll = killswitchEnabled && params.defaultRoute;
    params.allowVPN = params.allowDHCP = params.blockAll;
    params.blockIPv6 = (vpnActive || killswitchEnabled) && _settings.blockIPv6();
    params.allowLAN = _settings.allowLAN() && (params.blockAll || params.blockIPv6);
    // Block DNS when:
    // - not using Existing DNS
    // - the VPN connection is enabled, and
    // - we've connected at least once since the VPN was enabled
    params.blockDNS = pConnSettings && pConnSettings->dnsType() != QStringLiteral("existing") && vpnActive && params.hasConnected;
    params.allowPIA = params.allowLoopback = (params.blockAll || params.blockIPv6 || params.blockDNS);
    params.allowHnsd = params.blockDNS && pConnSettings && pConnSettings->dnsType() == QStringLiteral("handshake");

    qInfo() << "Reapplying firewall rules;"
            << "state:" << qEnumToString(_connection->state())
            << "clients:" << _clients.size()
            << "loggedIn:" << _account.loggedIn()
            << "killswitch:" << _settings.killswitch()
            << "vpnEnabled:" << _state.vpnEnabled()
            << "blockIPv6:" << _settings.blockIPv6()
            << "allowLAN:" << _settings.allowLAN()
            << "dnsType:" << (pConnSettings ? pConnSettings->dnsType() : QStringLiteral("N/A"))
            << "dnsServers:" << params.effectiveDnsServers;

    applyFirewallRules(params);

    _state.killswitchEnabled(killswitchEnabled);
}

void Daemon::checkSplitTunnelSupport()
{
    QJsonArray errors;

#if defined(Q_OS_WIN)
    // WFP has serious issues in Windows 7 RTM.  Though we still support the
    // client on Win 7 RTM, the split tunnel feature requires SP1 or newer.
    //
    // Some of the issues:
    // https://support.microsoft.com/en-us/help/981889/a-windows-filtering-platform-wfp-driver-hotfix-rollup-package-is-avail
    if(!::IsWindows7SP1OrGreater()) {
        errors.push_back(QStringLiteral("win_version_invalid"));
    }
#endif

#ifdef Q_OS_LINUX
    // This cgroup must be mounted in this location for this feature.
    QFileInfo cgroupFile(Path::ParentVpnExclusionsFile);
    if(!cgroupFile.exists())
        errors.push_back(QStringLiteral("cgroups_invalid"));

    // iptables 1.6.1 is required.
    QProcess iptablesVersion;
    iptablesVersion.start(QStringLiteral("iptables"), QStringList{QStringLiteral("--version")});
    iptablesVersion.waitForFinished();
    auto output = iptablesVersion.readAllStandardOutput();
    auto outputNewline = output.indexOf('\n');
    // First line only
    if(outputNewline >= 0)
        output = output.left(outputNewline);
    auto match = QRegularExpression{R"(([0-9]+)(\.|)([0-9]+|)(\.|)([0-9]+|))"}.match(output);
    // Note that captured() returns QString{} by default if the pattern didn't
    // match, so these will be 0 by default.
    auto major = match.captured(1).toInt();
    auto minor = match.captured(3).toInt();
    auto patch = match.captured(5).toInt();
    qInfo().nospace() << "iptables version " << output << " -> " << major << "."
        << minor << "." << patch;
    // SemVersion implements a suitable operator<(), we don't use it to parse
    // the version because we're not sure that iptables will always return three
    // parts in its version number though.
    if(SemVersion{major, minor, patch} < SemVersion{1, 6, 1})
        errors.push_back(QStringLiteral("iptables_invalid"));
#endif

    if(!errors.isEmpty())
        _state.splitTunnelSupportErrors(errors);
}

void Daemon::updatePortForwarder()
{
    bool pfEnabled = false;
    // If we're currently connected, and the connected region supports PF, keep
    // the setting that we connected with.
    //
    // Enabling PF would require a reconnect to request a port.  Disabling PF
    // has no effect until reconnecting either, we keep the forwarded port.
    if(_connection->state() == VPNConnection::State::Connected &&
       _state.connectedConfig().vpnLocation() &&
       _state.connectedConfig().vpnLocation()->portForward())
    {
        pfEnabled = _state.connectedConfig().portForward();
    }
    // Otherwise, use the current setting.  If we're connected to a region that
    // lacks PF, the setting _can_ be updated on the fly, because that just
    // toggles between the Inactive/Unavailable states.
    else
        pfEnabled = _settings.portForward();

    _portForwarder->enablePortForwarding(pfEnabled);
}

static QString decryptOldPassword(const QString& bytes)
{
    static constexpr char key[] = RUBY_MIGRATION;
    static constexpr int keyLength = sizeof(key) - 1;

    if (bytes.startsWith(QLatin1String("\0\0\0\0", 4)))
    {
        if (!keyLength)
            return {};

        QString s;
        s.reserve(bytes.length() - 4);
        const char* k = key;
        for (auto it = bytes.begin() + 4; it != bytes.end(); ++it, ++k)
        {
            if (!*k)
            {
                s.append(bytes.midRef(4 + keyLength));
                break;
            }
            s.push_back(it->unicode() ^ *k);
        }
        return s;
    }
    return bytes;
}

static QString translateOldHandshake(const QString& value)
{
    if (value == QStringLiteral("rsa2048")) return QStringLiteral("RSA-2048");
    if (value == QStringLiteral("rsa3072")) return QStringLiteral("RSA-3072");
    if (value == QStringLiteral("rsa4096")) return QStringLiteral("RSA-4096");
    if (value == QStringLiteral("ecdsa256k1")) return QStringLiteral("ECDSA-256k1");
    if (value == QStringLiteral("ecdsa256")) return QStringLiteral("ECDSA-256r1");
    if (value == QStringLiteral("ecdsa521")) return QStringLiteral("ECDSA-521");
    return QStringLiteral("default");
}

// Migrate settings from prior daemon versions or from an upgraded legacy version,
// or if the argument is false, just perform basic settings initialization such
// as automatically opting into betas if running a fresh beta install etc.
void Daemon::upgradeSettings(bool existingSettingsFile)
{
    // Determine the last used version for these settings. If this is a fresh
    // settings file, this is just the current version. If the lastUsedVersion
    // field is missing, version 1.0.0 is assumed instead.
    SemVersion previous = [](const QString& p) -> SemVersion {
        auto result = SemVersion::tryParse(p);
        return result != nullptr ? std::move(*result) : SemVersion(1, 0, 0);
    }(existingSettingsFile ? _settings.lastUsedVersion() : QStringLiteral(PIA_VERSION));
    // Always write the current version for any future settings upgrades.
    _settings.lastUsedVersion(QStringLiteral(PIA_VERSION));

    // If the user has manually installed a beta release, typically by opting
    // into the beta via the web site, enable beta updates.  This occurs when
    // installing over any non-beta release (including alphas, etc.) or if there
    // was no prior installation.
    //
    // We don't do any other change though (such as switching back) - users that
    // have opted into beta may receive GA releases when there isn't an active
    // beta, and they should continue to receive betas.
    auto daemonVersion = SemVersion::tryParse(u"" PIA_VERSION);
    if (daemonVersion && daemonVersion->isPrereleaseType(u"beta") &&
        !_settings.offerBetaUpdates() &&
        (!previous.isPrereleaseType(u"beta") || !existingSettingsFile))
    {
        qInfo() << "Enabling beta updates due to installing" << PIA_VERSION;
        _settings.offerBetaUpdates(true);
    }

    // Migrate any settings from any previous legacy version, which the
    // installer leaves for us as a 'legacy' settings value.
    QJsonValue legacyValue = _settings.get("legacy");
    if (legacyValue.isObject())
    {
        QJsonObject legacy = legacyValue.toObject();

        QJsonValue value;
        QString username, password;

        if ((value = legacy.take(QStringLiteral("user"))).isString())
            username = value.toString();
        if ((value = legacy.take(QStringLiteral("pass"))).isString())
            password = decryptOldPassword(value.toString());
        // Never migrate an 'x' username.  The legacy client worked with them as
        // a fluke, but they do not work with the new client.
        if (!username.startsWith('x') && !_account.loggedIn() && _account.username().isEmpty() && _account.password().isEmpty() && !username.isEmpty())
        {
            _account.username(username);
            _account.password(password);
        }
        if ((value = legacy.take(QStringLiteral("killswitch"))).isBool())
            _settings.killswitch(value.toBool() ? QStringLiteral("on") : QStringLiteral("auto"));
        if ((value = legacy.take(QStringLiteral("lport"))).isString())
            _settings.localPort(value.toString().toUShort());
        if ((value = legacy.take(QStringLiteral("mssfix"))).isBool())
            _settings.mtu(value.toBool() ? 1250 : 0);
        if ((value = legacy.take(QStringLiteral("portforward"))).isBool())
            _settings.portForward(value.toBool());
        if ((value = legacy.take(QStringLiteral("rport"))).isString()) // note: before "proto"
            legacy.value(QStringLiteral("proto")) == QStringLiteral("tcp") ? _settings.remotePortTCP(value.toString().toUShort()) : _settings.remotePortUDP(value.toString().toUShort());
        if ((value = legacy.take(QStringLiteral("proto"))).isString())
            _settings.protocol(value.toString());
        if ((value = legacy.take(QStringLiteral("default_region"))).isString())
            _settings.location(value.toString());
        if ((value = legacy.take(QStringLiteral("show_popup_notifications"))).isBool())
            _settings.desktopNotifications(value.toBool());
        if ((value = legacy.take(QStringLiteral("symmetric_cipher"))).isString())
            _settings.cipher(value.toString().toUpper().replace(QStringLiteral("NONE"), QStringLiteral("none")));
        if ((value = legacy.take(QStringLiteral("symmetric_auth"))).isString())
            _settings.auth(value.toString().toUpper().replace(QStringLiteral("NONE"), QStringLiteral("none")));
        if ((value = legacy.take(QStringLiteral("handshake_enc"))).isString())
            _settings.serverCertificate(translateOldHandshake(value.toString()));
        if ((value = legacy.take(QStringLiteral("connect_on_startup"))).isBool())
            _settings.connectOnLaunch(value.toBool());
        if ((value = legacy.take(QStringLiteral("mace"))).isBool())
            _settings.enableMACE(value.toBool());
        if ((value = legacy.take(QStringLiteral("favorite_regions"))).isArray())
            _settings.set("favoriteLocations", value);

        // Don't migrate the Allow LAN setting, since it had a default value of "off" which
        // causes users to experience unpexpected issues with the new auto killswitch.
        legacy.remove(QStringLiteral("allow_lan"));

        // Keep the 'legacy' settings object around even if it is now empty, as a reminder
        // that we upgraded from a legacy client (this might affect how we update settings
        // in future releases.
        _settings.set("legacy", legacy);
    }

    bool upgradedFromLegacy = legacyValue.isObject();

    // Migrate settings from versions prior to 1.1
    if (previous < SemVersion(1, 1))
    {
        // 'blockIPv6' was mistakenly released to private beta with the default set
        // to 'false'. This has never been exposed in UI, so reset it to 'true'.
        _settings.reset_blockIPv6();

        // 'allowLAN' was default false in the legacy client, but the LAN usually
        // remained accessible since the killswitch worked differently there. Since
        // upgraded users therefore often find the LAN inexplicably unusable, we've
        // disabled migrating that setting in 1.1, and will reset that setting back
        // to true for any users that previously upgraded.
        if (upgradedFromLegacy)
            _settings.reset_allowLAN();
    }

    if(previous < SemVersion{1, 2, 0, {PrereleaseTag{u"beta"}, PrereleaseTag{u"2"}}})
    {
        // If the user had the default debug filters in the old release, apply
        // the new default debug filters.
        const auto &oldFilters = _settings.debugLogging();
        qInfo() << "checking debug upgrade:" << oldFilters;
        if(oldFilters && oldFilters.get() == debugLogging10)
            _settings.debugLogging(debugLogging12b2);   // This updates Logger too
    }
    else
        qInfo() << "not checking debug upgrade";

    // If the prior installed version was less than 1.6.0, adjust the
    // permissions of account.json.  Do this only on upgrade (or new installs,
    // handled by Daemon constructor) in case a user decides to grant access to
    // this file.
    if(previous < SemVersion{1, 6, 0})
        restrictAccountJson();
}

void Daemon::rebuildLocations()
{
    // Update the grouped locations from the new stored locations
    _state.groupedLocations(buildGroupedLocations(_data.locations()));

    // Pick the best location
    NearestLocations nearest{_data.locations()};
    // PF currently requires OpenVPN.  This duplicates some logic from
    // ConnectionConfig, but the hope is that over time we'll support all/most
    // settings with WireGuard too, so these checks will just go away.
    bool portForwardEnabled = _settings.method() == QStringLiteral("openvpn") ? _settings.portForward() : false;
    _state.vpnLocations().bestLocation(nearest.getNearestSafeVpnLocation(portForwardEnabled));

    // Find the user's chosen location (nullptr if it's 'auto' or doesn't exist)
    const auto &locationId = _settings.location();
    if(locationId == QLatin1String("auto"))
        _state.vpnLocations().chosenLocation({});
    else
        _state.vpnLocations().chosenLocation(_data.locations().value(locationId));

    // Find the user's chosen SS location similarly, also ensure that it has
    // Shadowsocks
    const auto &ssLocId = _settings.proxyShadowsocksLocation();
    QSharedPointer<ServerLocation> pSsLoc;
    if(ssLocId != QLatin1String("auto"))
        pSsLoc = _data.locations().value(ssLocId);
    if(pSsLoc && !pSsLoc->shadowsocks())
        pSsLoc = {};    // Selected location does not have Shadowsocks
    _state.shadowsocksLocations().chosenLocation(pSsLoc);

    // Determine the next location we would use
    if(_state.vpnLocations().chosenLocation())
        _state.vpnLocations().nextLocation(_state.vpnLocations().chosenLocation());
    else
        _state.vpnLocations().nextLocation(_state.vpnLocations().bestLocation());

    // The best Shadowsocks location depends on the next VPN location
    auto pNextLocation = _state.vpnLocations().nextLocation();
    if(!pNextLocation)
    {
        // No locations are known, can't do anything else.
        _state.shadowsocksLocations().bestLocation({});
        _state.shadowsocksLocations().chosenLocation({});
        return;
    }

    // If the next location has SS, use that, that will add the least latency
    if(pNextLocation->shadowsocks())
        _state.shadowsocksLocations().bestLocation(pNextLocation);
    else
    {
        NearestLocations nearest{_data.locations()};
        // If no SS locations are known, this is set to nullptr
        _state.shadowsocksLocations().bestLocation(nearest.getNearestSafeServiceLocation(
            [](auto loc){ return loc.shadowsocks(); }));
    }

    // Determine the next SS location
    if(_state.shadowsocksLocations().chosenLocation())
        _state.shadowsocksLocations().nextLocation(_state.shadowsocksLocations().chosenLocation());
    else
        _state.shadowsocksLocations().nextLocation(_state.shadowsocksLocations().bestLocation());
}

void Daemon::onUpdateRefreshed(const Update &availableUpdate,
                               const Update &gaUpdate, const Update &betaUpdate)
{
    _state.availableVersion(availableUpdate.version());
    _data.gaChannelVersion(gaUpdate.version());
    _data.gaChannelVersionUri(gaUpdate.uri());
    _data.betaChannelVersion(betaUpdate.version());
    _data.betaChannelVersionUri(betaUpdate.uri());
}

void Daemon::onUpdateDownloadProgress(const QString &version, int progress)
{
    _state.updateDownloadProgress(progress);
    // If an installer had been downloaded before, the new download might be
    // overwriting it.
    _state.updateInstallerPath({});
    _state.updateVersion(version);
    _state.updateDownloadFailure(0);
}

void Daemon::onUpdateDownloadFinished(const QString &version, const QString &installerPath)
{
    // There's no ongoing download now
    _state.updateDownloadProgress(-1);
    _state.updateInstallerPath(installerPath);
    _state.updateVersion(version);
    _state.updateDownloadFailure(0);
}

void Daemon::onUpdateDownloadFailed(const QString &version, bool error)
{
    _state.updateDownloadProgress(-1);
    _state.updateInstallerPath({});
    // If the failure was due to an error, report it; if it was due to user
    // cancellation, just clean up.
    if(error)
    {
        _state.updateVersion(version);
        _state.updateDownloadFailure(QDateTime::currentMSecsSinceEpoch());
    }
    else
    {
        _state.updateVersion({});
        _state.updateDownloadFailure(0);
    }
}

void Daemon::logCommand(const QString &cmd, const QStringList &args)
{
    qInfo() << cmd << args;

    QProcess proc;
    proc.setProgram(cmd);
    proc.setArguments(args);

    proc.start();
    // This is strictly for diagnostics, only wait ~1 second to avoid blocking
    // the daemon for very long
    if(proc.waitForFinished(1000))
    {
        qInfo() << "status:" << proc.exitStatus() << "- code:" << proc.exitCode();
        qInfo() << "stdout:";
        qInfo().noquote() << proc.readAllStandardOutput();
        qInfo() << "stderr:";
        qInfo().noquote() << proc.readAllStandardError();
    }
    else
    {
        qInfo() << "Failed to execute - status:" << proc.exitStatus() << "- code:" << proc.exitCode();
    }
}

void Daemon::logRoutingTable()
{
    logCommand(QStringLiteral("netstat"), QStringList{QStringLiteral("-nr")});
    // On Linux, also log ip route show, eventually we want to move over to
    // iproute2 if it's always available
#if defined(Q_OS_LINUX)
    logCommand(QStringLiteral("ip"), QStringList{QStringLiteral("route"), QStringLiteral("show")});
#endif
    // On Windows, also trace DNS configuration, this has had problems in the
    // past
#if defined(Q_OS_WIN)
    logCommand(QStringLiteral("netsh"), {"interface", "ipv4", "show", "dnsservers"});
#endif
}

void Daemon::connectVPN()
{
    emit aboutToConnect();

    _state.vpnEnabled(true);

    _connection->connectVPN(_state.needsReconnect());
    _state.needsReconnect(false);
}

void Daemon::disconnectVPN()
{
    _state.vpnEnabled(false);
    _connection->disconnectVPN();
}

ClientConnection::ClientConnection(IPCConnection *connection, LocalMethodRegistry* registry, QObject *parent)
    : QObject(parent)
    , _connection(connection)
    , _rpc(new ServerSideInterface(registry, this))
    , _active(false)
    , _state(Connected)
{
    auto setDisconnected = [this]() {
        if (_state < Disconnected)
        {
            _state = Disconnected;
            emit disconnected();
        }
        _connection = nullptr;
    };
    connect(_connection, &IPCConnection::disconnected, this, setDisconnected);
    connect(_connection, &IPCConnection::destroyed, this, setDisconnected);

    connect(_connection, &IPCConnection::messageReceived, [this](const QByteArray & msg) {
      ClientConnection::_invokingClient = this;
      qInfo() << "Received message from client" << this;
      auto cleanup = raii_sentinel([]{_invokingClient = nullptr;});
      _rpc->processMessage(msg);
    });
    connect(_rpc, &ServerSideInterface::messageReady, _connection, &IPCConnection::sendMessage);
}
ClientConnection* ClientConnection::_invokingClient = nullptr;

void ClientConnection::disconnect()
{
    if (_state < Disconnecting)
    {
        _connection->disconnect();
    }
}

SnoozeTimer::SnoozeTimer(Daemon *daemon) : QObject(nullptr)
{
    _daemon = daemon;
    connect(&_snoozeTimer, &QTimer::timeout, this, &SnoozeTimer::handleTimeout);
    connect(_daemon->_connection, &VPNConnection::stateChanged, this, &SnoozeTimer::handleNewConnectionState);
    _snoozeTimer.setSingleShot(true);
}

void SnoozeTimer::startSnooze(qint64 seconds)
{
    qInfo() << "Starting snooze for" << seconds << "seconds";
    g_state.snoozeEndTime(0);
    _snoozeLength = seconds;
    _daemon->disconnectVPN();
}

void SnoozeTimer::stopSnooze()
{
    qInfo() << "Ending snooze";
    _snoozeTimer.stop();
    _daemon->connectVPN();
}

// Discard any active snooze timer and reset snooze end time
void SnoozeTimer::forceStopSnooze()
{
    _snoozeTimer.stop();
    g_state.snoozeEndTime(-1);
}

void SnoozeTimer::handleTimeout()
{
    if(g_state.snoozeEndTime() - getMonotonicTime() > 1000) {
        qWarning () << "Snooze timeout appears to have triggered prematurely";
    }
    qInfo() << "Snooze time has elapsed, end snooze";
    stopSnooze();
}

void SnoozeTimer::handleNewConnectionState(VPNConnection::State state,
                                           const ConnectionConfig &connectingConfig,
                                           const ConnectionConfig &connectedConfig,
                                           const nullable_t<Transport> &chosenTransport,
                                           const nullable_t<Transport> &actualTransport)
{
    Q_UNUSED(connectingConfig);
    Q_UNUSED(connectedConfig);
    Q_UNUSED(chosenTransport);
    Q_UNUSED(actualTransport);

    if(state == VPNConnection::State::Connected && g_state.snoozeEndTime() > 0) {
        qInfo() << "Reconnect from end of snooze has completed";
        // Set snooze end time to -1 to signify that the snooze has completed and connection is back up
        g_state.snoozeEndTime(-1);
    }

    if(state == VPNConnection::State::Disconnected && g_state.snoozeEndTime() == 0) {
        qInfo() << "Snooze disconnect has completed, start snooze timer for" << _snoozeLength << "seconds";
        // Set the snooze end time to the timestamp when it will actually end
        g_state.snoozeEndTime(getMonotonicTime() + _snoozeLength * 1000);
        _snoozeTimer.setInterval(static_cast<int>(_snoozeLength) * 1000);
        _snoozeTimer.start();
    }
}
