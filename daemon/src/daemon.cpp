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

#include <common/src/common.h>
#include "daemon.h"

#include "vpn.h"
#include "vpnmethod.h"
#include <common/src/ipc.h>
#include <common/src/jsonrpc.h>
#include <common/src/locations.h>
#include <common/src/builtin/path.h>
#include "version.h"
#include "brand.h"
#include <common/src/builtin/util.h>
#include <common/src/apinetwork.h>
#if defined(Q_OS_WIN)
#include "win/wfp_filters.h"
#include "win/win_networks.h"
#include "win/win_interfacemonitor.h"
#elif defined(Q_OS_MAC)
#include "mac/mac_networks.h"
#elif defined(Q_OS_LINUX)
#include "linux/linux_networks.h"
#endif

#include <QFile>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QJsonDocument>
#include <chrono>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QRegExp>
#include <QStringView>

#if defined(Q_OS_WIN)
#include <kapps_core/src/winapi.h>
#include <kapps_core/src/win/win_error.h>
#include <VersionHelpers.h>
#include <common/src/win/win_util.h>
#include <AclAPI.h>
#include <AccCtrl.h>
#include <TlHelp32.h>
#include <Psapi.h>
#pragma comment(lib, "advapi32.lib")
#endif

KAPPS_CORE_LOG_MODULE(daemon, "src/daemon.cpp")

namespace
{
    //Initially, we try to load the regions every 15 seconds, until they've been
    //loaded
    const std::chrono::seconds regionsInitialLoadInterval{15}, publicIpLoadInterval{5};
    //After they're initially loaded, we refresh every 10 minutes
    const std::chrono::minutes regionsRefreshInterval{10}, publicIpRefreshInterval{10};
    const std::chrono::hours modernRegionsMetaRefreshInterval{48};

    // Dedicated IPs are refreshed with the same interval as the regions list
    // (but not necessarily at the same time).  These don't have a "fast"
    // interval because we fetch them once when adding the token, and they don't
    // use a JsonRefresher since DIP info comes from an authenticated API POST.
    const std::chrono::minutes dipRefreshFastInterval{10};
    const std::chrono::hours dipRefreshSlowInterval{24};

    // Resource paths for various regions-related resource (relative to the API
    // base)
    const QString shadowsocksRegionsResource{QStringLiteral("shadow_socks")};
    const QString modernRegionsResource{QStringLiteral("vpninfo/servers/v6")};
    const QString modernRegionMetaResource{QStringLiteral("vpninfo/regions/v2")};

    // Resource paths for IP Address
    const QString ipLookupResource{QStringLiteral("api/client/status")};

    // Old default debug logging setting, 1.0 (and earlier) until 1.2-beta.2
    const QStringList debugLogging10{QStringLiteral("*.debug=true"),
                                     QStringLiteral("qt*.debug=false"),
                                     QStringLiteral("latency.*=false")};
    // Default from 1.2-beta.2 to 2.4.  Due to an oversight, there are two
    // slightly different versions of this (one was applied on upgrade, the
    // other was applied when enabling debug logging).
    const QStringList debugLogging12b2a{QStringLiteral("*.debug=true"),
                                        QStringLiteral("qt.*.debug=false"),
                                        QStringLiteral("qt.*.info=false"),
                                        QStringLiteral("qt.scenegraph.general*=true")};
    const QStringList debugLogging12b2b{QStringLiteral("*.debug=true"),
                                        QStringLiteral("qt*.debug=false"),
                                        QStringLiteral("latency.*=false"),
                                        QStringLiteral("qt.scenegraph.general*=true")};
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

// Populate a ConnectionInfo in StateModel from VPNConnection's ConnectionConfig
ConnectionInfo populateConnection(const ConnectionConfig &config)
{
    QString method;
    switch(config.method())
    {
        default:
        case ConnectionConfig::Method::OpenVPN:
            method = QStringLiteral("openvpn");
            break;
        case ConnectionConfig::Method::Wireguard:
            method = QStringLiteral("wireguard");
            break;
    }

    QString dnsType;
    switch(config.dnsType())
    {
        default:
        case ConnectionConfig::DnsType::Pia:
            dnsType = QStringLiteral("pia");
            break;
        case ConnectionConfig::DnsType::Local:
            dnsType = QStringLiteral("local");
            break;
        case ConnectionConfig::DnsType::Existing:
            dnsType = QStringLiteral("existing");
            break;
        case ConnectionConfig::DnsType::HDns:
            dnsType = QStringLiteral("hdns");
            break;
        case ConnectionConfig::DnsType::Custom:
            dnsType = QStringLiteral("custom");
            break;
    }

    QString proxy, proxyCustom;
    QSharedPointer<const Location> pProxyShadowsocks;
    bool proxyShadowsocksLocationAuto{false};
    switch(config.proxyType())
    {
        default:
        case ConnectionConfig::ProxyType::None:
            proxy = QStringLiteral("none");
            break;
        case ConnectionConfig::ProxyType::Custom:
        {
            proxy = QStringLiteral("custom");
            proxyCustom = config.customProxy().host();
            if(config.customProxy().port())
            {
                proxyCustom += ':';
                proxyCustom += QString::number(config.customProxy().port());
            }
            break;
        }
        case ConnectionConfig::ProxyType::Shadowsocks:
            proxy = QStringLiteral("shadowsocks");
            pProxyShadowsocks = config.shadowsocksLocation();
            proxyShadowsocksLocationAuto = config.shadowsocksLocationAuto();
            break;
    }

    return {config.vpnLocation(), config.vpnLocationAuto(), std::move(method),
        config.methodForcedByAuth(), std::move(dnsType), config.openvpnCipher(),
        config.otherAppsUseVpn(), std::move(proxy), std::move(proxyCustom),
        std::move(pProxyShadowsocks), std::move(proxyShadowsocksLocationAuto),
        config.requestPortForward()};
};

Daemon::Daemon(QObject* parent)
    : QObject(parent)
    , _started(false)
    , _stopping(false)
    , _server(nullptr)
    , _methodRegistry(new LocalMethodRegistry(this))
    , _rpc(new RemoteNotificationInterface(this))
    , _connection(new VPNConnection(this))
    , _environment{_state}
    , _apiClient{}
    , _modernLatencyTracker{}
    , _portForwarder{_apiClient, _account, _state, _environment}
    , _modernRegionRefresher{QStringLiteral("modern regions"),
                             modernRegionsResource, regionsInitialLoadInterval,
                             regionsRefreshInterval}
    , _modernRegionMetaRefresher{QStringLiteral("modern regions meta"),
                             modernRegionMetaResource, regionsInitialLoadInterval,
                             modernRegionsMetaRefreshInterval}
    , _shadowsocksRefresher{QStringLiteral("Shadowsocks regions"),
                            shadowsocksRegionsResource,
                            regionsInitialLoadInterval, regionsRefreshInterval}
    , _publicIpRefresher{QStringLiteral("Public IP Address"),
                            ipLookupResource,
                            publicIpLoadInterval, publicIpRefreshInterval}
    , _snoozeTimer(this)
    , _pendingSerializations(0)
{
#ifdef PIA_CRASH_REPORTING
    initCrashReporting(false);
#endif

    // Redact dedicated IP addresses and tokens from logs.  We can't just avoid
    // tracing these, because OpenVPN and WireGuard may trace them, etc.  Set
    // this up before reading the account information so the redactions will
    // apply if saved dedicated IPs are loaded.
    connect(&_account, &DaemonAccount::dedicatedIpsChanged, this, [this]()
    {
        for(const auto &dedicatedIp : _account.dedicatedIps())
        {
            // We can use the dedicated IP region ID in the redaction.
            // This is a random value, so:
            // - the IP/token can't be derived from the region ID
            // - a suspected IP/token can't even be matched up to the region ID
            //   (this is why we use a random value and not a hash of some kind)
            Logger::addRedaction(dedicatedIp.ip(), QStringLiteral("DIP IP %1").arg(dedicatedIp.id()));
            Logger::addRedaction(dedicatedIp.dipToken(), QStringLiteral("DIP token %1").arg(dedicatedIp.id()));
            // The CN is currently not unique per DIP, but redact it too in
            // case that changes.
            Logger::addRedaction(dedicatedIp.cn(), QStringLiteral("DIP CN %1").arg(dedicatedIp.id()));
        }
    });

    // Let the client know whether we have an account token (but not the actual
    // token value)
    connect(&_account, &DaemonAccount::tokenChanged, this, [this]()
    {
        _state.hasAccountToken(!_account.token().isEmpty());
    });

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

    _dedicatedIpRefreshTimer.setInterval(msec(dipRefreshFastInterval));
    connect(&_dedicatedIpRefreshTimer, &QTimer::timeout, this, &Daemon::refreshDedicatedIps);

    _memTraceTimer.setInterval(msec(std::chrono::minutes(5)));
    connect(&_memTraceTimer, &QTimer::timeout, this, &Daemon::traceMemory);

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
    _state.propertyChanged = [this](kapps::core::StringSlice name)
    {
        if(_stateChanges.insert(name.to_string()).second)
            queueNotification(&Daemon::notifyChanges);
        // Update nextConfig unless it was nextConfig itself that changed.
        // (Even if it was nextConfig, updating would be a no-op since
        // nextConfig does not depend on itself, but ignore it for robustness)
        if(name != "nextConfig")
            updateNextConfig();
    };

    // StateModel::nextConfig depends on StateModel (above), DaemonSettings, and
    // DaemonAccount.  Updating it for any possible property change is a bit
    // aggressive, but ConnectionConfig does inspect a lot of properties, and a
    // no-op change is ignored by StateModel.
    connect(&_settings, &NativeJsonObject::propertyChanged, this, &Daemon::updateNextConfig);
    connect(&_account, &NativeJsonObject::propertyChanged, this, &Daemon::updateNextConfig);

    auto updateLogger =  [this]() {
        const auto& value = _settings.debugLogging();
        if (value == nullptr)
            g_logger->configure(false, _settings.largeLogFiles(), {});
        else
            g_logger->configure(true, _settings.largeLogFiles(), *value);
    };

    // Set up logging.  Do this before migrating settings so tracing from the
    // migration is written (if debug logging is enabled).
    connect(&_settings, &DaemonSettings::debugLoggingChanged, this, updateLogger);
    connect(&_settings, &DaemonSettings::largeLogFilesChanged, this, updateLogger);

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

    // If .pia-early-debug exists in the daemon data directory, enable debug
    // logging now.  This provides a way to capture early debug tracing during
    // the first daemon startup, and for QA to always keep debug mode enabled
    // through uninstall/reinstall.  The installers create the daemon's
    // .pia-early-debug if the installing user has a ~/.pia-early-debug.
    const Path earlyDebugFile = Path::DaemonDataDir / "." BRAND_CODE "-early-debug";
    if(QFile::exists(earlyDebugFile))
    {
        if(!g_logger->logToFile())
        {
            g_logger->configure(true, _settings.largeLogFiles(), DaemonSettings::defaultDebugLogging);
            qInfo() << "Enabled debug logging due to" << earlyDebugFile;
        }
        else
        {
            qInfo() << "Debug logging already enabled, ignore" << earlyDebugFile;
        }
        QFile::remove(earlyDebugFile);
    }

    // Set initial value of debug logging
    if(g_logger->logToFile())
        _settings.debugLogging(g_logger->filters());
    else
        _settings.debugLogging(nullptr);

    // Migrate/upgrade any settings to the current daemon version
    upgradeSettings(settingsFileRead);

    // Load locations from the cached data, if there is any.  Don't start
    // fetching yet or check for region overrides / bundled region lists; that
    // is done when the daemon activates.
    //
    // The daemon doesn't really need the built locations until it activates,
    // but piactl exposes them and user scripts might be using this.
    rebuildActiveLocations();

    #define RPC_METHOD(name, ...) LocalMethod(QStringLiteral(#name), this, &Daemon::RPC_##name)
    _methodRegistry->add(RPC_METHOD(applySettings).defaultArguments(false));
    _methodRegistry->add(RPC_METHOD(resetSettings));
    _methodRegistry->add(RPC_METHOD(getCountryBestRegion));
    _methodRegistry->add(RPC_METHOD(addDedicatedIp));
    _methodRegistry->add(RPC_METHOD(removeDedicatedIp));
    _methodRegistry->add(RPC_METHOD(dismissDedicatedIpChange));
    _methodRegistry->add(RPC_METHOD(connectVPN));
    _methodRegistry->add(RPC_METHOD(disconnectVPN));
    _methodRegistry->add(RPC_METHOD(startSnooze));
    _methodRegistry->add(RPC_METHOD(stopSnooze));
    _methodRegistry->add(RPC_METHOD(writeDiagnostics));
    _methodRegistry->add(RPC_METHOD(writeDummyLogs));
    _methodRegistry->add(RPC_METHOD(crash));
    _methodRegistry->add(RPC_METHOD(refreshMetadata));
    _methodRegistry->add(RPC_METHOD(sendServiceQualityEvents));
    _methodRegistry->add(RPC_METHOD(notifyClientActivate));
    _methodRegistry->add(RPC_METHOD(notifyClientDeactivate));
    _methodRegistry->add(RPC_METHOD(emailLogin));
    _methodRegistry->add(RPC_METHOD(setToken));
    _methodRegistry->add(RPC_METHOD(login));
    _methodRegistry->add(RPC_METHOD(retryLogin));
    _methodRegistry->add(RPC_METHOD(logout));
    _methodRegistry->add(RPC_METHOD(downloadUpdate));
    _methodRegistry->add(RPC_METHOD(cancelDownloadUpdate));
    _methodRegistry->add(RPC_METHOD(submitRating));
    _methodRegistry->add(RPC_METHOD(inspectUwpApps));
    _methodRegistry->add(RPC_METHOD(checkDriverState));
    _methodRegistry->add(RPC_METHOD(systemSleep));
    _methodRegistry->add(RPC_METHOD(systemWake));
    #undef RPC_METHOD

    connect(_connection, &VPNConnection::stateChanged, this, &Daemon::vpnStateChanged);
    connect(_connection, &VPNConnection::firewallParamsChanged, this, &Daemon::queueApplyFirewallRules);
    connect(_connection, &VPNConnection::slowIntervalChanged, this,
        [this](bool usingSlowInterval){_state.usingSlowInterval(usingSlowInterval);});
    connect(_connection, &VPNConnection::error, this, &Daemon::vpnError);
    connect(_connection, &VPNConnection::byteCountsChanged, this, &Daemon::vpnByteCountsChanged);
    connect(_connection, &VPNConnection::usingTunnelConfiguration, this,
        [this](const QString &deviceName, const QString &deviceLocalAddress,
               const QString &deviceRemoteAddress)
        {
            _state.tunnelDeviceName(deviceName);
            _state.tunnelDeviceLocalAddress(deviceLocalAddress);
            _state.tunnelDeviceRemoteAddress(deviceRemoteAddress);
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

    connect(&_modernLatencyTracker, &LatencyTracker::newMeasurements, this,
            &Daemon::newLatencyMeasurements);
    // No locations are loaded yet - they're loaded when the daemon activates

    connect(&_portForwarder, &PortForwarder::portForwardUpdated, this,
            &Daemon::portForwardUpdated);

    connect(&_settings, &DaemonSettings::portForwardChanged, this,
            &Daemon::updatePortForwarder);
    updatePortForwarder();

    connect(&_environment, &Environment::overrideActive, this,
            &Daemon::setOverrideActive);
    connect(&_environment, &Environment::overrideFailed, this,
            &Daemon::setOverrideFailed);

    connect(&_modernRegionRefresher, &JsonRefresher::contentLoaded, this,
            &Daemon::modernRegionsLoaded);
    connect(&_modernRegionRefresher, &JsonRefresher::overrideActive, this,
            [this](){Daemon::setOverrideActive(QStringLiteral("modern regions list"));});
    connect(&_modernRegionRefresher, &JsonRefresher::overrideFailed, this,
            [this](){Daemon::setOverrideFailed(QStringLiteral("modern regions list"));});

    connect(&_modernRegionMetaRefresher, &JsonRefresher::contentLoaded, this,
            &Daemon::modernRegionsMetaLoaded);
    connect(&_modernRegionMetaRefresher, &JsonRefresher::overrideActive, this,
            [this](){Daemon::setOverrideActive(QStringLiteral("modern regions meta"));});
    connect(&_modernRegionMetaRefresher, &JsonRefresher::overrideFailed, this,
            [this](){Daemon::setOverrideFailed(QStringLiteral("modern regions meta"));});
    connect(&_shadowsocksRefresher, &JsonRefresher::contentLoaded, this,
            &Daemon::shadowsocksRegionsLoaded);
    connect(&_shadowsocksRefresher, &JsonRefresher::overrideActive, this,
            [this](){Daemon::setOverrideActive(QStringLiteral("shadowsocks list"));});
    connect(&_shadowsocksRefresher, &JsonRefresher::overrideFailed, this,
            [this](){Daemon::setOverrideFailed(QStringLiteral("shadowsocks list"));});
    connect(&_publicIpRefresher, &JsonRefresher::contentLoaded, this,
            &Daemon::publicIpLoaded);

    connect(this, &Daemon::daemonActivated, this, [this]() {
        // Reset override states since we are (re)activating
        _state.overridesFailed({});
        _state.overridesActive({});

        _environment.reload();

        _modernLatencyTracker.start();
        _dedicatedIpRefreshTimer.start();

        _modernRegionRefresher.startOrOverride(environment().getModernRegionsListApi(),
                                               Path::ModernRegionOverride,
                                               Path::ModernRegionBundle,
                                               _environment.getRegionsListPublicKey(),
                                               QJsonDocument{_data.cachedModernRegionsList()});
        _modernRegionMetaRefresher.startOrOverride(environment().getModernRegionsListApi(),
                                               Path::ModernRegionMetaOverride,
                                               Path::ModernRegionMetaBundle,
                                               _environment.getRegionsListPublicKey(),
                                               QJsonDocument{_data.modernRegionMeta()});
        _shadowsocksRefresher.startOrOverride(environment().getModernRegionsListApi(),
                                              Path::ModernShadowsocksOverride,
                                              Path::ModernShadowsocksBundle,
                                              _environment.getRegionsListPublicKey(),
                                              QJsonDocument{_data.cachedModernShadowsocksList()});
        updatePublicIpRefresher(_connection->state());
        _updateDownloader.run(true, _environment.getUpdateApi());

        _memTraceTimer.start();
        traceMemory();

        // Refresh metadata right away too
        RPC_refreshMetadata();

        // Check the active automation rule and apply it if needed - the only
        // action that can be relevant here is "connect", since we can't be
        // connected when inactive
        applyCurrentAutomationRule();

        queueNotification(&Daemon::reapplyFirewallRules);
    });

    connect(this, &Daemon::daemonDeactivated, this, [this]() {
        _updateDownloader.run(false, _environment.getUpdateApi());
        _modernRegionRefresher.stop();
        _modernRegionMetaRefresher.stop();
        _shadowsocksRefresher.stop();
        _dedicatedIpRefreshTimer.stop();
        _modernLatencyTracker.stop();
        queueNotification(&Daemon::RPC_disconnectVPN);
        queueNotification(&Daemon::reapplyFirewallRules);
        updatePublicIpRefresher(_connection->state());
        _state.externalIp({});
    });
    connect(&_settings, &DaemonSettings::killswitchChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::allowLANChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::overrideDNSChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::bypassSubnetsChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::splitTunnelEnabledChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::splitTunnelRulesChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::routedPacketsOnVPNChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::connectOnWakeChanged, this, &Daemon::queueApplyFirewallRules);
    _state.existingDNSServers.changed = [this]{queueApplyFirewallRules();};
    _state.systemSleeping.changed = [this]{handleSleepWakeTransitions();};
    // 'method' causes a firewall rule application because it can toggle split tunnel
    connect(&_settings, &DaemonSettings::methodChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_account, &DaemonAccount::loggedInChanged, this, &Daemon::queueApplyFirewallRules);

#ifdef Q_OS_MAC
    // Our macOS Split tunnel extension does logging itself, so we need to inform it when logging state changes
    // so it can adjust to reflect our new settings
    connect(&_settings, &DaemonSettings::debugLoggingChanged, this, &Daemon::queueApplyFirewallRules);
#endif

    connect(_connection, &VPNConnection::stateChanged, this, &Daemon::updatePublicIpRefresher);
    connect(_connection, &VPNConnection::stateChanged, this, &Daemon::handleSleepWakeTransitions);
    connect(&_settings, &DaemonSettings::updateChannelChanged, this,
            [this]()
            {
                _updateDownloader.setGaUpdateChannel(_settings.updateChannel(), _environment.getUpdateApi());
            });
    connect(&_settings, &DaemonSettings::betaUpdateChannelChanged, this,
            [this]()
            {
                _updateDownloader.setBetaUpdateChannel(_settings.betaUpdateChannel(), _environment.getUpdateApi());
            });
    connect(&_settings, &DaemonSettings::offerBetaUpdatesChanged, this,
            [this]()
            {
                _updateDownloader.enableBetaChannel(_settings.offerBetaUpdates(), _environment.getUpdateApi());
            });
    connect(&_updateDownloader, &UpdateDownloader::updateRefreshed, this,
            &Daemon::onUpdateRefreshed);
    connect(&_updateDownloader, &UpdateDownloader::downloadProgress, this,
            &Daemon::onUpdateDownloadProgress);
    connect(&_updateDownloader, &UpdateDownloader::downloadFinished, this,
            &Daemon::onUpdateDownloadFinished);
    connect(&_updateDownloader, &UpdateDownloader::downloadFailed, this,
            &Daemon::onUpdateDownloadFailed);
    _updateDownloader.setGaUpdateChannel(_settings.updateChannel(), _environment.getUpdateApi());
    _updateDownloader.setBetaUpdateChannel(_settings.betaUpdateChannel(), _environment.getUpdateApi());
    _updateDownloader.enableBetaChannel(_settings.offerBetaUpdates(), _environment.getUpdateApi());
    _updateDownloader.reloadAvailableUpdates(Update{_data.gaChannelVersionUri(), _data.gaChannelVersion(),
                                                    _data.gaChannelOsRequired()},
                                             Update{_data.betaChannelVersionUri(), _data.betaChannelVersion(),
                                                    _data.betaChannelOsRequired()},
                                             _data.flags());

    connect(&_settings, &DaemonSettings::automationRulesChanged, this,
            [this]()
            {
                _automation.setRules(_settings.automationRules());
            });
    _automation.setRules(_settings.automationRules());

    connect(&_automation, &Automation::ruleTriggered, this,
            &Daemon::onAutomationRuleTriggered);

    queueApplyFirewallRules();

    if(isActive()) {
        emit daemonActivated();
    }
#if defined(Q_OS_MAC)
    _pNetworkMonitor = createMacNetworks();
#elif defined(Q_OS_LINUX)
    _pNetworkMonitor = createLinuxNetworks();
#elif defined(Q_OS_WIN)
    _pNetworkMonitor = createWinNetworks();
#endif

    if(_pNetworkMonitor)
    {
        connect(_pNetworkMonitor.get(), &NetworkMonitor::networksChanged, this,
                &Daemon::onNetworksChanged);
        onNetworksChanged(_pNetworkMonitor->getNetworks());

        connect(_pNetworkMonitor.get(), &NetworkMonitor::networksChanged,
                &_automation, &Automation::setNetworks);
    }

    // Only keep the last 5 daemon crash dumps.
    // This is important as in rare circumstances the daemon could go into a
    // cycling crash loop and the dumps generated could fill up the user's available HD.
    auto daemonCrashDir = Path::DaemonDataDir / QStringLiteral("crashes");
    daemonCrashDir.cleanDirFiles(5);

    auto ver = SemVersion::tryParse(_settings.serviceQualityAcceptanceVersion());
    bool enableQualityEventsFlag = _data.hasFlag(QStringLiteral("service_quality_events"))
            && !_settings.serviceQualityAcceptanceVersion().isEmpty();

    _pServiceQuality.emplace(_apiClient, _environment, _account, _data, enableQualityEventsFlag, ver);
    auto updateEventsEnabled = [this]()
    {
        auto ver = SemVersion::tryParse(_settings.serviceQualityAcceptanceVersion());
        bool enableQualityEvents = _data.hasFlag(QStringLiteral("service_quality_events"))
                && !_settings.serviceQualityAcceptanceVersion().isEmpty();
        _pServiceQuality->enable(enableQualityEvents, ver);
    };
    connect(&_settings, &DaemonSettings::serviceQualityAcceptanceVersionChanged, this,
            updateEventsEnabled);
    connect(&_data, &DaemonData::flagsChanged, this, updateEventsEnabled);
}

Daemon::~Daemon()
{
    qInfo() << "Daemon shutdown complete";
}

OriginalNetworkScan Daemon::originalNetwork() const
{
    return {_state.originalGatewayIp().toStdString(), _state.originalInterface().toStdString(),
            _state.originalInterfaceIp().toStdString(), _state.originalInterfaceNetPrefix(),
             _state.originalMtu(),
            _state.originalInterfaceIp6().toStdString(), _state.originalGatewayIp6().toStdString(),
            _state.originalMtu6()};
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
    return _state.invalidClientExit() || _state.killedClient() ||
        hasActiveClient() || _settings.persistDaemon();
}

void Daemon::RPC_applySettings(const QJsonObject &settings, bool reconnectIfNeeded)
{
    mustBeAwake(); // If this runs, the system must be awake
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
    qDebug() << "Applying settings:" << logSettings;

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

    bool wasActive = isActive();
    bool wasAutomationEnabled = _settings.automationEnabled();

    bool success = _settings.assign(settings);

    if(isActive() && !wasActive) {
        qInfo () << "Going active after settings changed";
        emit daemonActivated();
    } else if (wasActive && !isActive()) {
        qInfo () << "Going inactive after settings changed";
        emit daemonDeactivated();
    }

    // If the settings affect location choices, rebuild locations from servers
    // lists
    if(settings.contains(QLatin1String("includeGeoOnly")) ||
       settings.contains(QLatin1String("manualServer")))
    {
        qInfo() << "Settings affect possible locations, rebuild locations";
        rebuildActiveLocations();
    }
    // Otherwise, If the settings affect the location choices, recompute them.
    // Port forwarding and method affect the "best" location selection.
    else if(settings.contains(QLatin1String("location")) ||
       settings.contains(QLatin1String("proxyShadowsocksLocation")) ||
       settings.contains(QLatin1String("portForward")) ||
       settings.contains(QLatin1String("method")))
    {
        qInfo() << "Settings affect location choices, recalculate location preferences";
        calculateLocationPreferences();
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

            // Keep the last automation trigger if there is one when
            // reconnecting this way.  Currently, this only happens when
            // changing locations from the GUI while already connected.  This
            // isn't presented as a "connect" action, so the connection state
            // should still reflect the automation trigger.  All other
            // connection sources clear or update the trigger.
            auto automationLastTrigger = _state.automationLastTrigger();
            Error err = connectVPN(ServiceQuality::ConnectionSource::Manual);
            _state.automationLastTrigger(automationLastTrigger);
            if(err)
            {
                // This is unlikely since the VPN was already enabled, if it
                // does happen somehow just trace it.
                qWarning() << "Unable to reconnect to apply settings:" << err;
            }
        }
        else
        {
            qInfo() << "Setting change had reconnectIfNeeded=true, but no reconnect was needed - enabled:"
                << _state.vpnEnabled() << "- needsReconnect:" << _connection->needsReconnect();
        }
    }

    // If automation was just disabled, clear the current trigger rule (but
    // don't change state).  Although the current connection/disconnection was
    // caused by a trigger, it's odd if this stays around, and there's no way to
    // clear it other than manually disconnecting or connecting.
    if(wasAutomationEnabled && !_settings.automationEnabled())
        _state.automationLastTrigger({});
    // If automation was just enabled, apply the current rule.
    if(!wasAutomationEnabled && _settings.automationEnabled())
    {
        qInfo() << "Apply current automation rule due to enabling automation";
        applyCurrentAutomationRule();
    }
    // If automation rule was removed from list, the entry on the dashboard is also removed.
    bool lastTriggerStillExists{false};
    for(const auto &rule : _settings.automationRules())
    {
        if (_state.automationLastTrigger() && *_state.automationLastTrigger() == rule)
        {
            lastTriggerStillExists = true; // Found the last trigger, it's still here
            break; // Found it, no need to keep looking
        }
    }
    if (!lastTriggerStillExists)
        _state.automationLastTrigger({}); // The triggered rule was removed, clear the trigger
}

void Daemon::RPC_resetSettings()
{
    mustBeAwake(); // If this runs, the system must be awake
    // Reset the settings by applying all default values.
    // This ensures that any logic that is applied when changing settings takes
    // place (the needs-reconnect and chosen/next location state fields).
    QJsonObject defaultsJson = DaemonSettings{}.toJsonObject();

    // Some settings are excluded - remove them before applying the defaults
    for(const auto &excludedSetting : DaemonSettings::settingsExcludedFromReset())
        defaultsJson.remove(excludedSetting);

    RPC_applySettings(defaultsJson, false);
}

QString Daemon::RPC_getCountryBestRegion(const QString &country)
{
    NearestLocations nearest{_state.availableLocations()};
    const auto &countryLower = country.toLower();
    const auto &pBestInCountry = nearest.getBestMatchingLocation(
        [&](const Location &loc)
        {
            auto pRegionDisplay = _state.regionsMetadata().getRegionDisplay(loc.id().toStdString());
            return pRegionDisplay && qs::toQString(pRegionDisplay->country()).toLower() == countryLower;
        });

    if(pBestInCountry)
        return pBestInCountry->id();

    qWarning() << "Could not select region for country" << country;
    throw Error{HERE, Error::Code::JsonRPCInvalidRequest};
}

Async<void> Daemon::RPC_addDedicatedIp(const QString &token)
{
    return _apiClient.postRetry(*_environment.getApiv2(), QStringLiteral("dedicated_ip"),
        QJsonDocument{QJsonObject{
            {
                QStringLiteral("tokens"),
                QJsonArray{token}
            }
        }}, ApiClient::autoAuth(_account.username(), _account.password(), _account.token()))
        ->next(this, [this](const Error &err, const QJsonDocument &json)
            {
                if(err)
                    throw err;

                // We passed one token, so the result should be a one-element
                // array.  If it's not for any reason (not an array, does not
                // contain 1 element, etc.), we get an empty QJsonObject here.
                QJsonObject tokenData = json.array().at(0).toObject();

                // If any of the JSON casts below fail, an exception is thrown,
                // which means we reject the RPC and do not add the token (the
                // response was malformed).
                const QString &token = json_cast<QString>(tokenData["dip_token"], HERE);
                const QString &status = json_cast<QString>(tokenData["status"], HERE);

                // There's a specific response for "expired"
                if(status == QStringLiteral("expired"))
                {
                    qWarning() << "Token is already expired, can't add it";
                    throw Error{HERE, Error::Code::DaemonRPCDedicatedIpTokenExpired};
                }

                // There's a specific response for an invalid token too
                if(status == QStringLiteral("invalid"))
                {
                    qWarning() << "Token is not valid, got status" << status;
                    throw Error{HERE, Error::Code::DaemonRPCDedicatedIpTokenInvalid};
                }

                // If the status is anything other than "active" at this point,
                // treat it as an error ("error" can be sent if the server
                // encounters an internal error)
                if(status != QStringLiteral("active"))
                {
                    qWarning() << "Can't check token, got unexpected status" << status;
                    throw Error{HERE, Error::Code::ApiBadResponseError};
                }

                // Build the AccountDedicatedIp from the information returned
                AccountDedicatedIp dipInfo;
                dipInfo.dipToken(token);
                applyDedicatedIpJson(tokenData, dipInfo);

                auto dedicatedIps = _account.dedicatedIps();

                // Check if we already have this token (in which case "adding"
                // essentially just caused us to refresh it, reuse the region
                // ID), or if we don't (generate a new region ID)
                auto itExisting = std::find_if(dedicatedIps.begin(), dedicatedIps.end(),
                    [&token](const AccountDedicatedIp &dip){return dip.dipToken() == token;});
                if(itExisting != dedicatedIps.end())
                {
                    // Already have it; use the same region ID and update
                    // in-place
                    dipInfo.id(itExisting->id());
                    *itExisting = std::move(dipInfo);
                }
                else
                {
                    // Don't have it, generate a new region ID.
                    do
                    {
                        dipInfo.id(QStringLiteral("dip-%1")
                            .arg(QRandomGenerator::global()->generate(), 8, 16, QChar{'0'}));
                    }
                    while(std::find_if(dedicatedIps.begin(), dedicatedIps.end(),
                            [&dipInfo](const AccountDedicatedIp &dip){return dip.id() == dipInfo.id();})
                                != dedicatedIps.end());
                    dedicatedIps.push_back(std::move(dipInfo));
                }

                _account.dedicatedIps(std::move(dedicatedIps));

                rebuildActiveLocations();
            });
}

void Daemon::RPC_removeDedicatedIp(const QString &dipRegionId)
{
    auto dedicatedIps = _account.dedicatedIps();

    auto newEnd = std::remove_if(dedicatedIps.begin(), dedicatedIps.end(),
        [&dipRegionId](const AccountDedicatedIp &dip){return dip.id() == dipRegionId;});
    dedicatedIps.erase(newEnd, dedicatedIps.end());

    _account.dedicatedIps(std::move(dedicatedIps));

    rebuildActiveLocations();
}

void Daemon::RPC_dismissDedicatedIpChange()
{
    auto dedicatedIps = _account.dedicatedIps();

    for(auto &dip : dedicatedIps)
        dip.lastIpChange(0);
    _account.dedicatedIps(std::move(dedicatedIps));
    rebuildActiveLocations();
}

void Daemon::RPC_connectVPN()
{
    mustBeAwake(); // If this runs, the system must be awake
    Error err = connectVPN(ServiceQuality::ConnectionSource::Manual);
    // If we can't connect now (not logged in or daemon is inactive), forward
    // the error to the client.  The CLI has specific support for these errors.
    if(err)
        throw err;

    // Increment the session counter, if ratings_1 flag is set
    if(_data.hasFlag(QStringLiteral("ratings_1")) &&
        _settings.ratingEnabled() &&
        QStringLiteral(BRAND_CODE) == QStringLiteral("pia"))
    {
        _settings.sessionCount(_settings.sessionCount() + 1);
    }
}

void Daemon::RPC_disconnectVPN()
{
    mustBeAwake(); // If this runs, the system must be awake
    // There's no additional logic needed for a manual disconnect.  This can't
    // fail, we don't count these, etc.
    disconnectVPN(ServiceQuality::ConnectionSource::Manual);
}

void Daemon::RPC_startSnooze(qint64 seconds)
{
    mustBeAwake(); // If this runs, the system must be awake
    _snoozeTimer.startSnooze(seconds);
}

void Daemon::RPC_stopSnooze()
{
    mustBeAwake(); // If this runs, the system must be awake
    _snoozeTimer.stopSnooze();
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
        QByteArray output = Logger::redactText(cmd.readAllStandardOutput());
        _fileWriter << (processOutput ? processOutput(output) : output) << endl;
        _fileWriter << "STDERR: " << endl;
        _fileWriter << Logger::redactText(cmd.readAllStandardError()) << endl;
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
    UidGidProcess cmd;
    cmd.setArguments(args);
    cmd.setProgram(command);
    // Drop the "piavpn" group for diagnostic processes on Linux/Mac - there are
    // firewall rules to specifically permit piavpn, and the ping/dig tests are
    // intended to indicate the behavior of non-PIA processes.  This is ignored
    // on Windows.
#if defined(Q_OS_MAC)
    cmd.setGroup(QStringLiteral("staff"));
#elif defined(Q_OS_LINUX)
    cmd.setGroup(QStringLiteral("root"));
#endif
    execAndWriteOutput(cmd, commandName, processOutput);
}

void DiagnosticsFile::writeCommandIf(bool predicate,
                                     const QString &commandName,
                                     const QString &command,
                                     const QStringList &args,
                                     const ProcessOutputFunction &processOutput)
{
    if(predicate)
        writeCommand(commandName, command, args, processOutput);
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

void DiagnosticsFile::writeCommandIf(bool predicate,
                                     const QString &commandName,
                                     const QString &command,
                                     const QString &nativeArgs,
                                     const ProcessOutputFunction &processOutput)
{
    if(predicate)
        writeCommand(commandName, command, nativeArgs, processOutput);
}
#endif

void DiagnosticsFile::writeText(const QString &title, QString text)
{
    QElapsedTimer commandTime;
    commandTime.start();

    _fileWriter << diagnosticsCommandHeader(title);
    _fileWriter << Logger::redactText(std::move(text)) << endl;

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

    try
    {
        writePrettyJson("DaemonState",
            adaptNljToQt(_state.getJsonObject()),
            { "availableLocations", "regionsMetadata", "groupedLocations",
              "externalIp", "externalVpnIp", "forwardedPort" });
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Unable to write DaemonState:"
            << ex.what();
    }
    // The custom proxy setting is removed because it may contain the proxy
    // credentials.
    writePrettyJson("DaemonSettings", _settings.toJsonObject(), { "proxyCustom" });

    qInfo() << "Finished writing diagnostics file" << diagFilePath;

    return QJsonValue{diagFilePath};
}

QString Daemon::diagnosticsOverview() const
{
    auto boolToString = [](bool b) { return b == true ? QStringLiteral("true") : QStringLiteral("false"); };

    const auto &connectedConfig = _state.connectedConfig();
    const auto &actualTransport = _state.actualTransport();

    const auto &connectionMethod = connectedConfig.method();
    const bool isConnected = _state.connectionState() == QStringLiteral("Connected");

    auto ifOpenVPN = [&connectionMethod](const QString &displayString) {
        return connectionMethod == QStringLiteral("openvpn") ? displayString : QStringLiteral("");
    };

    // We need a more explicit description of the killswitch state.
    // "Advanced" killswitch can cause a lot of problems so we need to emphasize it.
    auto killswitchText = [](const QString &killswitchState) {
        if(killswitchState == QStringLiteral("on"))
            return QStringLiteral("%1 - Advanced killswitch is active!").arg(killswitchState);
        else
            return killswitchState;
    };

    // Generate split tunnel app diagnostic string (show the apps in each ST mode)
    QStringList bypassRules;
    QStringList vpnOnlyRules;
    for(const auto &rule : _settings.splitTunnelRules())
    {
        if(rule.mode() == "exclude")
            bypassRules << rule.path();
        else
            vpnOnlyRules << rule.path();
    }

    auto commonDiagnostics = [&] {
        auto strings = QStringList {
            QStringLiteral("Connected: %1").arg(boolToString(isConnected)),
            QStringLiteral("Split Tunnel enabled: %1").arg(boolToString(_settings.splitTunnelEnabled())),
            QStringLiteral("Split Tunnel DNS enabled: %1").arg(_settings.splitTunnelEnabled() ? boolToString(_settings.splitTunnelDNS()) : "N/A"),
            QStringLiteral("Split Tunnel Bypass Apps: %1").arg(!_settings.splitTunnelEnabled() ? "N/A" : bypassRules.isEmpty() ?  "None" : bypassRules.join(", ")),
            QStringLiteral("Split Tunnel VpnOnly Apps: %1").arg(!_settings.splitTunnelEnabled() ? "N/A" : vpnOnlyRules.isEmpty() ?  "None" : vpnOnlyRules.join(", ")),
            QStringLiteral("VPN has default route: %1").arg(boolToString(_settings.splitTunnelEnabled() ?  _settings.defaultRoute() : true)),
            QStringLiteral("Killswitch: %1").arg(killswitchText(_settings.killswitch())),
            QStringLiteral("Allow LAN: %1").arg(boolToString(_settings.allowLAN())),
#ifdef Q_OS_WIN
            ifOpenVPN(QStringLiteral("Windows IP config: %1").arg(_settings.windowsIpMethod())),
#endif
            QStringLiteral("Use Small Packets: %1").arg(boolToString(_settings.mtu() != 0))
        };
        strings.removeAll(QStringLiteral(""));

        return strings.join('\n');
    };

    auto connectedDiagnostics = [&] {
        auto strings = QStringList {
            QStringLiteral("Connection method: %1").arg(connectionMethod),
            ifOpenVPN(QStringLiteral("Protocol: %1").arg(actualTransport->protocol())),
            ifOpenVPN(QStringLiteral("Port: %1").arg(actualTransport->port())),
            ifOpenVPN(QStringLiteral("Cipher: %1").arg(_settings.cipher())),
            QStringLiteral("DNS selection: %1").arg(connectedConfig.dnsType())
        };
        strings.removeAll(QStringLiteral(""));

        return strings.join('\n');
    };

    if(isConnected)
        return commonDiagnostics() + "\n" + connectedDiagnostics();
    else
        return commonDiagnostics();
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

void Daemon::RPC_refreshMetadata()
{
    refreshDedicatedIps();
    refreshAccountInfo();
    _updateDownloader.refreshUpdate();
}

void Daemon::RPC_sendServiceQualityEvents()
{
    _pServiceQuality->sendEventsNow();
}

void Daemon::RPC_notifyClientActivate()
{
    mustBeAwake(); // If this runs, the system must be awake
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
        emit daemonActivated();
}

void Daemon::RPC_notifyClientDeactivate()
{
    mustBeAwake(); // If this runs, the system must be awake
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

    // Since a client is exiting cleanly, clear the invalid client exit and
    // killed client flags
    _state.invalidClientExit(false);
    _state.killedClient(false);

    // If it was the last interactive client, shut down
    if(!isActive())
        emit daemonDeactivated();
}

Async<void> Daemon::RPC_emailLogin(const QString &email)
{
    mustBeAwake(); // If this runs, the system must be awake
    qDebug () << "Requesting email login";
    return _apiClient.postRetry(*_environment.getApiv2(),
                QStringLiteral("login_link"),
                QJsonDocument(
                    QJsonObject({
                          { QStringLiteral("email"), email},
                  })))
            ->then(this, [this](const QJsonDocument& json) {
        Q_UNUSED(json);
        qDebug () << "Email login request success";
    })->except(this, [](const Error& error) {
        qWarning () << "Email login request failed " << error.errorString();
        throw error;
    });
}

Async<void> Daemon::RPC_setToken(const QString &token)
{
    return loadAccountInfo({}, {}, token)
            ->then(this, [=](const QJsonObject& account) {
                _account.password({});
                _account.token(token);
                _account.openvpnUsername(token.left(token.length() / 2));
                _account.openvpnPassword(token.right(token.length() - (token.length() / 2)));
                _account.assign(account);
                _account.loggedIn(true);
                // We're now logged in, apply automation rules if they already
                // exist.
                applyCurrentAutomationRule();
            })
            ->except(this, [](const Error& error) {
                throw error;
            });
}

Async<void> Daemon::RPC_login(const QString& username, const QString& password)
{
    mustBeAwake(); // If this runs, the system must be awake
    // If there's already an attempt, abort it before starting another,
    // otherwise they could overwrite each other's results.
    if(_pLoginRequest)
    {
        _pLoginRequest->abort({HERE, Error::Code::TaskRejected});
        // Don't need abandon() here since abort() completes the outer task
        _pLoginRequest.reset();
    }

    // Hang on to the login API result in _pLoginRequest so we can abort it if
    // we log out or log in.  We have to use an AbortableTask,
    _pLoginRequest = _apiClient.postRetry(
                *_environment.getApiv2(), QStringLiteral("token"),
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
                            // We're now logged in, apply automation rules if
                            // they already exist.
                            applyCurrentAutomationRule();
                        });
            })
            ->except(this, [this, username, password](const Error& error) {
                resetAccountInfo();
                if (error.code() == Error::ApiUnauthorizedError ||
                    error.code() == Error::ApiPaymentRequiredError ||
                    error.code() == Error::ApiRateLimitedError)
                    throw error;

                // Proceed with empty account info; the client can still connect
                // if the naked username/password is valid for OpenVPN auth.
                _account.username(username);
                _account.password(password);
                _account.openvpnUsername(username);
                _account.openvpnPassword(password);
                _account.loggedIn(true);
                // We're now logged in, apply automation rules if they already
                // exist.
                applyCurrentAutomationRule();
            })
            .abortable();
    return _pLoginRequest;
}

Async<void> Daemon::RPC_retryLogin()
{
    if(_account.username().isEmpty() || _account.password().isEmpty())
    {
        // We can't retry the login if we don't have stored credentials.  This
        // normally is prevented by the client UI, it only shows this action in
        // the appropriate state.  It could happen if the client sends a
        // retryLogin RPC when:
        // - the daemon is actually logged in with a token (we no longer have
        //   the password)
        // - the daemon is logged out (we don't have any credentials)
        qWarning() << "Can't retry login, do not have stored credentials";
        return Async<void>::reject({HERE, Error::Code::DaemonRPCNotLoggedIn});
    }

    return RPC_login(_account.username(), _account.password());
}

void Daemon::RPC_logout()
{
    mustBeAwake(); // If this runs, the system must be awake
    // If we were still trying to log in, abort that attempt, otherwise we might
    // log back in after logging out.  In particular, this could happen if we
    // weren't able to get a token initially, used credential auth to connect,
    // then the user logs out while we're obtaining a token through the tunnel.
    if(_pLoginRequest)
    {
        _pLoginRequest->abort({HERE, Error::Code::TaskRejected});
        // Don't need abandon() here since abort() completes the outer task
        _pLoginRequest.reset();
    }

    // If the VPN is connected, disconnect before logging out.  We don't wait
    // for this to complete though.
    RPC_disconnectVPN();

    // Reset account data along with relevant settings
    QString tokenToExpire = _account.token();

    // Wipe out account, except for dedicated IPs
    auto dedicatedIps = _account.dedicatedIps();
    _account.reset();
    _account.dedicatedIps(std::move(dedicatedIps));
    _settings.recentLocations({});
    _state.openVpnAuthFailed(0);

    if(!tokenToExpire.isEmpty())
    {
        _apiClient.postRetry(*_environment.getApiv2(), QStringLiteral("expire_token"),
                             QJsonDocument(), ApiClient::tokenAuth(tokenToExpire))
            ->notify(this, [this](const Error &error, const QJsonDocument &json)
        {
            if(error) {
                qWarning () << "Token expire failed with error code: " << error.code();
            } else {
                QString status = json[QLatin1String("status")].toString();
                if (status != QStringLiteral("success"))
                    qWarning () << "API Token expire: Bad result: " << json;
                else
                    qDebug () << "API Token expire: success";
            }
        });
    }

}

Async<QJsonValue> Daemon::RPC_downloadUpdate()
{
    return _updateDownloader.downloadUpdate();
}

void Daemon::RPC_cancelDownloadUpdate()
{
    _updateDownloader.cancelDownload();
}

Async<void> Daemon::RPC_submitRating(int rating)
{
    qDebug () << "Submitting Rating";
    return _apiClient.postRetry(*_environment.getApiv2(),
                QStringLiteral("rating"),
                QJsonDocument(
                    QJsonObject({
                          { QStringLiteral("rating"), rating},
                          { QStringLiteral("application"), QStringLiteral("desktop")},
                          { QStringLiteral("platform"), UpdateChannel::platformName},
                          { QStringLiteral("version"), QString::fromStdString(Version::semanticVersion())}
                  })), ApiClient::tokenAuth(_account.token()))
            ->then(this, [this](const QJsonDocument& json) {
        Q_UNUSED(json);
        qDebug () << "Submit rating request success";
    })->except(this, [](const Error& error) {
        qWarning () << "Submit rating request failed " << error.errorString();
        throw error;
    });
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

void Daemon::RPC_systemSleep()
{
    qInfo() << "Received system sleep notification";
    systemSleep();
}

void Daemon::systemSleep()
{
    // Whenever the system goes to sleep while connected we will enable killswitch
    // to protect from any potential leaks during sleep, disconnect if VPN is connected,
    // and reconnect it when it wakes.
    //
    // We do this mainly as a workaround for a macOS issue where a long sleep would
    // result in crashes after running out of file descriptors. MacOS wants to wake up
    // every hour or so during sleep, does a few network checks and goes back to sleep.
    // During those checks, we detect the VPN connection is dead (makes sense as we've been asleep),
    // which triggers a reconnect.
    // It is during these reconnects that we seem to be leaking some resources.
    // Given it's a very hard bug to track, we aim to fix it by just disconnecting the VPN
    // before sleep.
    // It is anyway a good thing to do in general, as it doesn't make sense to be connected
    // while the system sleeps.

    _state.systemSleeping(true);
}

void Daemon::RPC_systemWake()
{
    qInfo() << "Received system wake notification";
    systemWake();
}

void Daemon::systemWake()
{
    _state.systemSleeping(false);
}

void Daemon::mustBeAwake()
{
    if(_state.systemSleeping())
    {
        qWarning() << "System was thought to be sleeping, but must be awake";
        systemWake();
    }
}

// Connected to systemSleeping and connection state changes.
void Daemon::handleSleepWakeTransitions()
{
    if(_state.systemSleeping())
    {
        if(_connection->state() == VPNConnection::State::Connected &&
           _settings.killswitch() != QLatin1String("off"))
        {
            qInfo() << "VPN will disconnect for sleep";
            _settings.connectOnWake(true);
            _connection->disconnectVPN();
        }
    }
    else
    {
        if(_settings.connectOnWake())
        {
            if(_connection->state() == VPNConnection::State::Disconnected)
            {
                qInfo() << "Woke from sleep, will reconnect VPN";
                _settings.connectOnWake(false);
                _connection->connectVPN(_state.needsReconnect());
            }
            else if(_connection->state() == VPNConnection::State::Disconnecting)
            {
                qInfo() << "Woke from sleep too quick, will reconnect once disconnected";
            }
            else if(_connection->state() == VPNConnection::State::Connected)
            {
                // Safeguard. If this ever happens there's an issue with this function's logic or the connections.
                _settings.connectOnWake(false);
                qWarning() << "Already connected when waking, will remove connect on wake flag. This should not happen";
            }
        }
        // Else: We are not sleeping and we did not just wake up, do nothing.
        // This should be the most common case from connection state changes
    }
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
    traceMemory();

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

        // If the client was active, this exit is unexpected.  Either the daemon
        // was killing the connection due to lack of response, or we assume the
        // client had crashed.  The daemon was active and should remain active.
        if(client->getActive())
        {
            // Reset the current invalidClientExit() / killedClient() flags, so
            // we can switch between those if needed (if a killed connection is
            // followed by a client crash, etc.)
            _state.killedClient(false);
            _state.invalidClientExit(false);
            // If this would have caused the daemon to deactivate, set either
            // invalidClientExit() or killedClient() to remain active.
            if(!isActive())
            {
                if(client->getKilled())
                {
                    qWarning() << "Client" << client << "connection was killed by daemon, will remain active";
                    _state.killedClient(true);
                }
                else
                {
                    qWarning() << "Client" << client << "disconnected but did not deactivate, will remain active";
                    _state.invalidClientExit(true);
                }
                // This causes the daemon to remain active (we don't emit
                // daemonDeactivated())
                Q_ASSERT(isActive());
            }
        }
    });

    QJsonObject all;
    all.insert(QStringLiteral("data"), _data.toJsonObject());
    QJsonObject accountJsonObj = _account.toJsonObject();
    for(const auto &sensitiveProp : DaemonAccount::sensitiveProperties())
        accountJsonObj.remove(sensitiveProp);
    all.insert(QStringLiteral("account"), std::move(accountJsonObj));
    all.insert(QStringLiteral("settings"), _settings.toJsonObject());
    QJsonObject stateJson;
    try
    {
        stateJson = adaptNljToQt(_state.getJsonObject());
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Unable to serialize state:" << ex.what();
    }
    all.insert(QStringLiteral("state"), stateJson);
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

QJsonObject getProperties(const JsonState<clientjson::json> &object,
    const std::unordered_set<std::string> &properties)
{
    try
    {
        clientjson::json result{};
        for(const auto &name : properties)
        {
            // Individual properties can fail without failing everything
            try
            {
                result.emplace(name, object.getProperty(name));
            }
            catch(const std::exception &ex)
            {
                KAPPS_CORE_WARNING() << "Unable to serialize property" << name
                    << "-" << ex.what();
            }
        }
        return adaptNljToQt(result);
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Unable to serialize properties" << properties
            << "-" << ex.what();
    }
    return {};
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
        QSet<QString> newAccountChanges;
        _accountChanges.swap(newAccountChanges);
        // Don't send sensitive properties to clients, but we still need to
        // write them to disk
        for(const auto &sensitiveProp : DaemonAccount::sensitiveProperties())
            newAccountChanges.remove(sensitiveProp);
        all.insert(QStringLiteral("account"), getProperties(_account, newAccountChanges));
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

struct IpResult
{
    QString address;    // VPN IP address
    bool problem;   // True if we think there is a connection problem
};

IpResult parseVpnIpResponse(const QJsonDocument &json)
{
    IpResult result{};
    result.address = json[QStringLiteral("ip")].toString();

    // Check if the API says we are connected (using a PIA IP).
    // If not, it indicates a routing problem.  If the value is
    // missing for any reason, assume it's fine by default (do not
    // show a warning).
    result.problem = !json[QStringLiteral("connected")].toBool(true);
    if(result.problem)
    {
        qWarning() << "API indicates we are not connected, may indicate a routing problem";
    }
    else
    {
        qWarning() << "VPN IP routing confirmed";
    }
    return result;
}

class VpnIpProbeTask : public Task<IpResult>
{
public:
    VpnIpProbeTask(ApiClient &apiClient, Environment &environment)
    {
        _pPiaApiTask = apiClient.getVpnIpRetry(*environment.getIpAddrApi(),
                                               QStringLiteral("api/client/status"),
                                               std::chrono::seconds{10})
            ->then(this, [this](const QJsonDocument &json)
            {
                auto keepAlive = sharedFromThis();
                const auto &result = parseVpnIpResponse(json);

                if(result.problem)
                {
                    qWarning() << "API was reachable but indicates we are not connected, may indicate routing problem";
                }
                else
                {
                    qInfo() << "API was reachable and confirmed connection";
                }
                // If the proxy API task was still ongoing, abandon it
                if(_pProxyApiTask && !_pProxyApiTask->isFinished())
                    _pProxyApiTask.abandon();
                resolve(result);
            })
            ->except(this, [this](const Error &err)
            {
                auto keepAlive = sharedFromThis();
                // If the proxy task hasn't finished yet, wait for it to finish.
                if(_pProxyApiTask && !_pProxyApiTask->isFinished())
                {
                    qInfo() << "Still waiting for API proxy to respond";
                }
                else
                {
                    bool proxyReachable = false;
                    if(_pProxyApiTask && _pProxyApiTask->isResolved())
                        proxyReachable = _pProxyApiTask->result();
                    resolveApiUnreachable(proxyReachable);
                }
            });

        // This task results in a boolean value; indicates whether the API proxy
        // is reachable.
        _pProxyApiTask = Async<DelayTask>::create(std::chrono::seconds{4})
            ->then(this, [this, &apiClient, &environment]()
            {
                return apiClient.getVpnIpRetry(*environment.getIpProxyApi(),
                                               QStringLiteral("api/client/status"),
                                               std::chrono::seconds{6});
            })
            ->next(this, [this](const Error &err, const QJsonDocument &)
            {
                auto keepAlive = sharedFromThis();
                if(err)
                {
                    qWarning() << "API is not reachable through proxy:" << err;
                }

                bool proxyReachable = !err;
                if(_pPiaApiTask->isFinished())
                {
                    // If the normal API request has completed, but we haven't
                    // resolved this task, the API was not reachable.
                    resolveApiUnreachable(proxyReachable);
                }
                else
                {
                    qInfo() << "Still waiting for PIA API to respond to status request";
                }
                return proxyReachable;
            });
    }

private:
    // Resolve the task when the API isn't directly reachable.  proxyReachable
    // indicates whether the API proxy was reachable.
    void resolveApiUnreachable(bool proxyReachable)
    {
        if(proxyReachable)
        {
            // Proxy was reachable.  Can't confirm routing, but Internet is
            // reachable.
            qWarning() << "API is reachable through proxy only, can't confirm routing";
            resolve({{}, false});
        }
        else
        {
            // Proxy wasn't reachable either, likely indicates connection
            // problem.
            qWarning() << "API is not reachable through any source, may indicate a connection problem";
            resolve({{}, true});
        }
    }

private:
    Async<void> _pPiaApiTask;
    Async<bool> _pProxyApiTask;
};

Async<void> Daemon::loadVpnIp()
{
    // Test the connection to determine:
    // - the VPN IP (StateModel::externalVpnIp)
    // - whether there might be a connection problem (StateModel::connectionProblem)
    //
    // Connection problems include incorrect routing (routing via physical
    // interface instead of VPN interface), or lack of Internet connectivity
    // (broken DNS, missing routes, etc.).
    //
    // This algorithm is somewhat complex due to several requirements:
    // 1. The IP address can only be determined using the proper PIA API, not a
    //    proxy (would give the IP of the proxy)
    // 2. We can't wait too long to report a connection problem (if we wait more
    //    than 10 seconds or so, users would probably have already noticed the
    //    problem).
    // 3. The PIA API may in rare cases only be reachable through the proxy
    // 4. The API proxy is pretty slow, commonly takes 3+ seconds to respond
    // 5. The IP address API is known to take a while to become reachable in some
    //    cases
    //
    // To meet all these requirements, we use the following strategy
    // 1. Try to get status from both the PIA API and the proxy in parallel.  We
    //    give the PIA API a short head start, but both are timed to finish
    //    around the 10-second mark.
    // 2. If the PIA API request completes, abandon the proxy request.  (If the
    //    proxy request completes, continue the PIA API request until it times
    //    out.)
    // 3. Depending on the results from above, trigger the connection problem
    //    warning:
    //    - if the PIA API was reachable, use that result (check connected flag)
    //    - if only the proxy was reachable, asssume it's an API reachability
    //      problem and do not show a warning
    //    - if neither was reachable, assume there is a connection problem
    // 4. If the PIA API wasn't reachable (regardless of proxy reachability),
    //    continue trying to fetch the VPN IP for up to 2 minutes.
    //
    // So problems will typically be reported within ~10 seconds.  If a
    // connection is just very poor and the IP eventually is retrieved, the
    // connection problem warning may appear and then disappear later, which is
    // reasonable.
    return Async<VpnIpProbeTask>::create(_apiClient, _environment)
        ->then(this, [this](const IpResult &result)
        {
            _state.externalVpnIp(result.address);
            _state.connectionProblem(result.problem);
            // If we didn't get an IP address, continue trying to reach the API
            if(result.address.isEmpty())
            {
                return _apiClient.getVpnIpRetry(*_environment.getIpAddrApi(),
                                                QStringLiteral("api/client/status"),
                                                std::chrono::minutes{2})
                    ->then(this, [this](const QJsonDocument &json)
                    {
                        const auto &result = parseVpnIpResponse(json);
                        _state.externalVpnIp(result.address);
                        _state.connectionProblem(result.problem);
                    })
                    ->except(this, [this](const Error &err)
                    {
                        qWarning() << "API is unreachable, can't determine VPN IP -"
                            << err;
                        // Does not necessarily indicate a connection problem, the
                        // API isn't reachable from all regions.
                    });
            }
            else
                return Async<void>::resolve();
        });
}

void Daemon::vpnStateChanged(VPNConnection::State state,
                             VPNConnection::State oldState,
                             const ConnectionConfig &connectingConfig,
                             const ConnectionConfig &connectedConfig,
                             const nullable_t<Server> &connectedServer,
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

    _connectingConfig = connectingConfig;
    _connectedConfig = connectedConfig;
    _state.connectingConfig(populateConnection(connectingConfig));
    _state.connectedConfig(populateConnection(connectedConfig));
    _state.connectedServer(connectedServer);

    queueNotification(&Daemon::reapplyFirewallRules);

    // Latency measurements only make sense when we're not connected to the VPN
    if(state == VPNConnection::State::Disconnected && isActive())
    {
        _modernLatencyTracker.start();
        // Kick off a region refresh so we typically rotate servers on a
        // reconnect.  Usually the request right after connecting covers this,
        // but this is still helpful in case we were not able to load the
        // resource then.
        _modernRegionRefresher.refresh();
        _shadowsocksRefresher.refresh();
    }
    else
    {
        _modernLatencyTracker.stop();
    }

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
            _portForwarder.updateConnectionState(PortForwarder::State::ConnectedSupported);
        else
            _portForwarder.updateConnectionState(PortForwarder::State::ConnectedUnsupported);

        // Perform a refresh immediately after connect so we get a new IP on reconnect.
        _modernRegionRefresher.refresh();
        _shadowsocksRefresher.refresh();

        // If we haven't obtained a token yet, try to do that now that we're
        // connected (the API is likely reachable through the tunnel).
        if(_account.token().isEmpty())
            refreshAccountInfo();

        refreshDedicatedIps();

        // When a new connection is established (not a reconnection), count a
        // successful session.  New connections are indicated by prior state
        // Connecting, reconnections would have prior state Reconnecting.
        if(_settings.surveyRequestEnabled() && oldState == VpnState::Connecting)
        {
            _settings.successfulSessionCount(_settings.successfulSessionCount() + 1);
        }
    }
    else
    {
        ApiNetwork::instance()->setProxy({});
        _socksServer.stop();
        _portForwarder.updateConnectionState(PortForwarder::State::Disconnected);
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
        _state.connectionProblem(false);
    }
    // The VPN is connected - if we haven't found the VPN IP yet, find it
    else if (_state.externalVpnIp().isEmpty())
    {
        QElapsedTimer monotonicTimer;
        monotonicTimer.start();
        _state.connectionTimestamp(monotonicTimer.msecsSinceReference());

        // Get the user's VPN IP address now that we're connected
        _pVpnIpRequest = loadVpnIp();
    }

    // Clear the external IP address if we're completely disconnected.  Leave
    // this set if we go to any state like Connecting, Reconnecting,
    // Interrupted, etc., because we only find this address again when a
    // connection is initiated by an RPC.
    if(state == VPNConnection::State::Disconnected)
    {
        _state.tunnelDeviceName({});
        _state.tunnelDeviceLocalAddress({});
        _state.tunnelDeviceRemoteAddress({});
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

    // Inform ServiceQuality of the Connected and Disconnected states
    if(state == VPNConnection::State::Connected)
        _pServiceQuality->vpnConnected();
    else if(state == VPNConnection::State::Disconnected)
        _pServiceQuality->vpnDisconnected();

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

void Daemon::newLatencyMeasurements(const LatencyTracker::Latencies &measurements)
{
    SCOPE_LOGGING_CATEGORY("daemon.latency");

    LatencyMap newLatencies;
    newLatencies = _data.modernLatencies();

    for(const auto &measurement : measurements)
    {
        newLatencies[measurement.first] = static_cast<double>(msec(measurement.second));
    }

    _data.modernLatencies(newLatencies);

    // Rebuild the locations, including the grouped locations and location
    // choices, since the latencies changed
    rebuildActiveLocations();
}

void Daemon::portForwardUpdated(int port)
{
    qInfo() << "Forwarded port updated to" << port;
    _state.forwardedPort(port);
}

void Daemon::applyBuiltLocations(LocationsById newLocations,
                                 kapps::regions::Metadata metadata)
{
    // The LatencyTrackers still ping all locations, so we have latency
    // measurements if the locations are re-enabled, but remove them from
    // availableLocations and groupedLocations so all parts of the program will
    // ignore them:
    // - if a geo location is selected, the selection will be treated as 'auto' instead
    // - favorites/recents for geo locations are ignored
    // - piactl does not display or accept them
    // - the regions lists (both VPN and Shadowsocks) do not display them
    LocationsById nonGeoLocations;
    if(!_settings.includeGeoOnly())
    {
        nonGeoLocations.reserve(newLocations.size());
        for(const auto &locEntry : newLocations)
        {
            if(locEntry.second && !locEntry.second->geoLocated())
                nonGeoLocations[locEntry.first] = locEntry.second;
        }
        _state.availableLocations(std::move(nonGeoLocations));
    }
    else
        _state.availableLocations(std::move(newLocations));
    _state.regionsMetadata(std::move(metadata));

    // Update the grouped locations from the new stored locations
    std::vector<CountryLocations> groupedLocations;
    std::vector<QSharedPointer<const Location>> dedicatedIpLocations;
    buildGroupedLocations(_state.availableLocations(),
                          _state.regionsMetadata(),
                          groupedLocations,
                          dedicatedIpLocations);
    _state.groupedLocations(std::move(groupedLocations));
    _state.dedicatedIpLocations(std::move(dedicatedIpLocations));

    // Find the closest expiration time for any dedicated IP, and find the most
    // recent dedicated IP change
    const auto &accountDips = _account.dedicatedIps();
    auto itDip = accountDips.begin();
    if(itDip == accountDips.end())
    {
        _state.dedicatedIpExpiring(0);
        _state.dedicatedIpDaysRemaining(0);
        _state.dedicatedIpChanged(0);
    }
    else
    {
        quint64 nextExpiration = itDip->expire();
        quint64 lastDipChange = itDip->lastIpChange();
        for(++itDip; itDip != accountDips.end(); ++itDip)
        {
            // All DIPs have an expiration time, we always find a value for
            // nextExpiration
            if(itDip->expire() < nextExpiration)
                nextExpiration = itDip->expire();
            // lastDipChange may be 0 if we haven't seen any DIPs that have
            // actually changed yet.  Any actual change is >0
            if(itDip->lastIpChange() > lastDipChange)
                lastDipChange = itDip->lastIpChange();
        }

        // std::chrono::days requires C++20
        using days = std::chrono::duration<int, std::ratio<86400>>;

        // Display an upcoming expiration if it's within 5 days.  The "display
        // threshold" for this expiration is the timestamp when we would start
        // displaying it.
        quint64 expirationDisplayThreshold = nextExpiration - msec(days{5});
        std::chrono::milliseconds nowMs{QDateTime::currentMSecsSinceEpoch()};
        if(expirationDisplayThreshold <= static_cast<quint64>(msec(nowMs)))
        {
            _state.dedicatedIpExpiring(expirationDisplayThreshold);
            std::chrono::milliseconds timeRemaining{nextExpiration};
            timeRemaining -= nowMs;
            // Add 12 hours (=0.5 days) and then truncate to effectively round
            // to the nearest day.  (std::chrono::round requires C++17)
            auto daysRemaining = std::chrono::duration_cast<days>(timeRemaining + std::chrono::hours{12});
            // The remaining time might be negative due to clock skew or if we
            // just haven't polled the DIP yet to remove it.
            _state.dedicatedIpDaysRemaining(std::max(0, daysRemaining.count()));
        }
        else
        {
            // No expirations in the next 7 days.
            _state.dedicatedIpExpiring(0);
            _state.dedicatedIpDaysRemaining(0);
        }

        _state.dedicatedIpChanged(lastDipChange);
    }

    // Calculate new location preferences
    calculateLocationPreferences();

    // Update the available ports
    DescendingPortSet udpPorts, tcpPorts;
    for(const auto &locationEntry : _state.availableLocations())
    {
        locationEntry.second->allPortsForService(Service::OpenVpnUdp, udpPorts);
        locationEntry.second->allPortsForService(Service::OpenVpnTcp, tcpPorts);
    }
    _state.openvpnUdpPortChoices(udpPorts);
    _state.openvpnTcpPortChoices(tcpPorts);
}

bool Daemon::rebuildModernLocations(const QJsonObject &regionsObj,
                                    const QJsonArray &shadowsocksObj,
                                    const QJsonObject &metadataObj)
{
    try
    {
        auto newLocations = buildModernLocations(_data.modernLatencies(),
                                                 regionsObj,
                                                 shadowsocksObj,
                                                 metadataObj,
                                                 _account.dedicatedIps(),
                                                 _settings.manualServer());

        // Like the legacy list, if no regions are found, treat this as an error
        // and keep the data we have (which might still be usable).
        if(newLocations.first.empty() ||
            newLocations.second.countryDisplays().empty() ||
            newLocations.second.regionDisplays().empty())
        {
            return false;
        }

        // Apply the modern locations to the modern latency tracker
        _modernLatencyTracker.updateLocations(newLocations.first);

        applyBuiltLocations(std::move(newLocations.first),
                            std::move(newLocations.second));

        return true;
    }
    catch(const std::exception &ex)
    {
        qWarning() << "Unable to build locations:" << ex.what();
    }
    return false;
}

void Daemon::rebuildActiveLocations()
{
    rebuildModernLocations(_data.cachedModernRegionsList(),
                           _data.cachedModernShadowsocksList(),
                           _data.modernRegionMeta());
}

void Daemon::shadowsocksRegionsLoaded(const QJsonDocument &shadowsocksRegionsJsonDoc)
{
    const auto &shadowsocksRegionsObj = shadowsocksRegionsJsonDoc.array();

    // It's unlikely that the Shadowsocks regions list could totally hose us,
    // but the same resiliency is here for robustness.
    if(!rebuildModernLocations(_data.cachedModernRegionsList(), shadowsocksRegionsObj, _data.modernRegionMeta()))
    {
        qWarning() << "Shadowsocks location data could not be loaded.  Received"
            << shadowsocksRegionsJsonDoc;
        // Don't update cachedModernShadowsocksList, keep the last content
        // (which might still be usable, the new content is no good).
        // Don't treat this as a successful load (don't notify JsonRefresher)
        return;
    }

    _data.cachedModernShadowsocksList(shadowsocksRegionsObj);
    _shadowsocksRefresher.loadSucceeded();
}

void Daemon::modernRegionsLoaded(const QJsonDocument &modernRegionsJsonDoc)
{
    const auto &modernRegionsObj = modernRegionsJsonDoc.object();

    // If this results in an empty list, don't cache the unusable data.  This
    // would totally hose the client and more likely indicates a problem in the
    // servers list - keep whatever content we had before even though it's
    // older.
    if(!rebuildModernLocations(modernRegionsObj, _data.cachedModernShadowsocksList(), _data.modernRegionMeta()))
    {
        qWarning() << "Modern location data could not be loaded.  Received"
            << modernRegionsJsonDoc;
        // Don't update the cache - keep the existing data, which might still be
        // usable.  Don't treat this as a successful load.
        return;
    }

    _data.cachedModernRegionsList(modernRegionsObj);
    _modernRegionRefresher.loadSucceeded();
}

void Daemon::modernRegionsMetaLoaded(const QJsonDocument &modernRegionsMetaJsonDoc)
{
    const auto &modernRegionsMetaObj = modernRegionsMetaJsonDoc.object();

    // Currently, since some metadata are still incorporated into the Locations
    // models, it's possible (but unlikely) that the metadata could hose the
    // regions list.  Once the client has fully moved metadata references to the
    // new metadata objects, we should be able to stop putting metadata into the
    // Location objects and remove this.
    if(!rebuildModernLocations(_data.cachedModernRegionsList(), _data.cachedModernShadowsocksList(), modernRegionsMetaObj))
    {
        qWarning() << "Modern region metadata could not be loaded.  Received"
            << modernRegionsMetaJsonDoc;
        // Don't update the cache - keep the existing data, which might still be
        // usable.  Don't treat this as a successful load.
        return;
    }

    _data.modernRegionMeta(modernRegionsMetaObj);
    _modernRegionMetaRefresher.loadSucceeded();
}

void Daemon::publicIpLoaded(const QJsonDocument &publicIpDoc)
{
    qDebug () << "Loaded public IP";
    if(publicIpDoc[QStringLiteral("connected")].isBool()  // Check to see if there is a "connected" key
      && !publicIpDoc[QStringLiteral("connected")].toBool(true)) { // and that it is indeed "false"
        _state.externalIp(publicIpDoc["ip"].toString());
        _publicIpRefresher.loadSucceeded();
    }
}

void Daemon::updatePublicIpRefresher (VPNConnection::State state)
{
    if((state == VPNConnection::State::Disconnected || state == VPNConnection::State::Connecting) && isActive())
    {
         _publicIpRefresher.start(environment().getIpAddrApi());
    }
    else
    {
         _publicIpRefresher.stop();
    }
}

void Daemon::forcePublicIpRefresh ()
{
     _publicIpRefresher.refresh();
}

void Daemon::onNetworksChanged(const std::vector<NetworkConnection> &networks)
{
    OriginalNetworkScan defaultConnection;
    qInfo() << "Networks changed: currently" << networks.size() << "networks";

    // Relevant only to macOS
    QString macosPrimaryServiceKey;

    // Automation rule conditions representing any currently connected wireless
    // networks
    std::vector<AutomationRuleCondition> wifiNetworkConditions;

    int netIdx=0;
    for(const auto &network : networks)
    {
        qInfo() << "Network" << netIdx;
        qInfo() << " - itf:" << network.networkInterface();
        qInfo() << " - med:" << traceEnum(network.medium());
        qInfo() << " - enc:" << network.wifiEncrypted();
        qInfo() << " - ssid:" << network.wifiSsid();
        qInfo() << " - def4:" << network.defaultIpv4();
        qInfo() << " - def6:" << network.defaultIpv6();
        qInfo() << " - gw4:" << network.gatewayIpv4();
        qInfo() << " - gw6:" << network.gatewayIpv6();
        qInfo() << " - mtu4:" << network.mtu4();
        qInfo() << " - mtu6:" << network.mtu6();
        qInfo() << " - ip4:" << network.addressesIpv4().size();
        int i=0;
        for(const auto &addr : network.addressesIpv4())
        {
            qInfo() << "   " << i << "-" << addr.first << "/" << addr.second;
            ++i;
        }
        qInfo() << " - ip6:" << network.addressesIpv6().size();
        i=0;
        for(const auto &addr : network.addressesIpv6())
        {
            qInfo() << "   " << i << "-" << addr.first << "/" << addr.second;
            ++i;
        }

        if(network.defaultIpv4())
        {
            if(network.gatewayIpv4() != kapps::core::Ipv4Address{})
                defaultConnection.gatewayIp(network.gatewayIpv4().toString());
            else
                defaultConnection.gatewayIp({});

            defaultConnection.interfaceName(network.networkInterface().toStdString());
            defaultConnection.mtu(network.mtu4());

            if(!network.addressesIpv4().empty())
            {
                defaultConnection.ipAddress(network.addressesIpv4().front().first.toString());
                defaultConnection.prefixLength(network.addressesIpv4().front().second);
            }
            else
            {
                defaultConnection.ipAddress({});
                defaultConnection.prefixLength(0);
            }

#ifdef Q_OS_MACOS
            macosPrimaryServiceKey = network.macosPrimaryServiceKey();
#endif
        }
        if(network.defaultIpv6())
        {
            defaultConnection.mtu6(network.mtu6());
            if(!network.addressesIpv6().empty())
            {
                defaultConnection.ipAddress6(network.addressesIpv6().front().first.toString());
                defaultConnection.gatewayIp6(network.gatewayIpv6().toString());
            }
            else
                defaultConnection.ipAddress6({});
        }

        if(!network.wifiSsid().isEmpty())
        {
            wifiNetworkConditions.push_back({});
            wifiNetworkConditions.back().ruleType(QStringLiteral("ssid"));
            wifiNetworkConditions.back().ssid(network.wifiSsid());
        }

        ++netIdx;
    }

    _state.originalGatewayIp(QString::fromStdString(defaultConnection.gatewayIp()));
    _state.originalInterface(QString::fromStdString(defaultConnection.interfaceName()));
    _state.originalInterfaceNetPrefix(defaultConnection.prefixLength());
    _state.originalMtu(defaultConnection.mtu());
    _state.originalInterfaceIp(QString::fromStdString(defaultConnection.ipAddress()));
    _state.originalInterfaceIp6(QString::fromStdString(defaultConnection.ipAddress6()));
    _state.originalGatewayIp6(QString::fromStdString(defaultConnection.gatewayIp6()));
    _state.originalMtu6(defaultConnection.mtu6());

    // Relevant only to macOS
    _state.macosPrimaryServiceKey(macosPrimaryServiceKey);

    _state.automationCurrentNetworks(std::move(wifiNetworkConditions));

    // Emit this here so it'll run callbacks before firewall rules update
    emit networksChanged();

    queueApplyFirewallRules();
    _connection->updateNetwork(originalNetwork());
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

void Daemon::applyDedicatedIpJson(const QJsonObject &tokenData,
                                  AccountDedicatedIp &dipInfo)
{
    QString oldIp = dipInfo.ip();
    // API returns the expire time in seconds; we store timestamps in
    // milliseconds
    quint64 newExpire = json_cast<quint64>(tokenData["dip_expire"], HERE) * 1000;
    if(newExpire == 0)
    {
        qWarning() << "Dedicated IP" << dipInfo.id()
            << "returned invalid expiration 0";
        // This shouldn't happen, but just in case, do not ever store an
        // expiration timestamp of 0 - this would make the region look like a
        // non-DIP region.
        //
        // We should store _something_ though since this might be a new DIP,
        // just use 1000 instead (1 second after midnight, Jan 1, 1970 -
        // functionally equivalent assuming no time travel)
        newExpire = 1000;
    }
    if(newExpire != dipInfo.expire())
    {
        qInfo() << "Dedicated IP" << dipInfo.id() << "updated expiration from"
            << dipInfo.expire() << "to" << newExpire;
        dipInfo.expire(newExpire);
    }
    dipInfo.regionId(json_cast<QString>(tokenData["id"], HERE));
    dipInfo.serviceGroups(json_cast<std::vector<QString>>(tokenData["groups"], HERE));
    dipInfo.ip(json_cast<QString>(tokenData["ip"], HERE));
    dipInfo.cn(json_cast<QString>(tokenData["cn"], HERE));
    // If the dedicated IP was known and has changed, set the last change
    // timestamp.  Don't clear this if it didn't change, the timestamps are only
    // cleared when the user dismisses the notification.
    if(!oldIp.isEmpty() && oldIp != dipInfo.ip())
    {
        qInfo() << "Dedicated IP" << dipInfo.id() << "has changed IP address";
        dipInfo.lastIpChange(QDateTime::currentMSecsSinceEpoch());
    }
}

void Daemon::applyRefreshedDedicatedIp(const QJsonObject &tokenData, int traceIdx,
                                       std::vector<AccountDedicatedIp> &dedicatedIps)
{
    const QString &token = json_cast<QString>(tokenData["dip_token"], HERE);

    auto itExistingDip = std::find_if(dedicatedIps.begin(), dedicatedIps.end(),
        [&token](const AccountDedicatedIp &dip){return dip.dipToken() == token;});
    // If the token is no longer known, don't re-add it, a refresh may have
    // raced with a remove request.
    if(itExistingDip == dedicatedIps.end())
    {
        qInfo() << "Ignoring DIP token" << traceIdx << "- it has already been removed";
        return;
    }

    // If the token has expired, remove it - we don't show any specific message
    // for this, since we displayed the "about to expire" message for some time
    // prior to this.
    //
    // "invalid" is not common here, but if it occurs, remove the token (this
    // might happen if the token is so old that it has been completely purged).
    // If any any unexpected value (including empty) occurs, leave the token
    // alone.
    const QString &status = json_cast<QString>(tokenData["status"], HERE);
    if(status == QStringLiteral("expired") || status == QStringLiteral("invalid"))
    {
        qInfo() << "Removing token" << traceIdx << "/" << itExistingDip->id()
            << "due to updated status" << status;
        dedicatedIps.erase(itExistingDip);
        return;
    }

    if(status != QStringLiteral("active"))
    {
        qWarning() << "Not updating token" << traceIdx << "/"
            << itExistingDip->id() << "due to unexpected status" << status;
        return;
    }

    // It's present and still active, update it.
    applyDedicatedIpJson(tokenData, *itExistingDip);
}

void Daemon::refreshDedicatedIps()
{
    if(_account.dedicatedIps().empty())
        return; // Nothing to do

    // Get the current dedicated IP tokens
    QJsonArray dipTokens;
    for(const auto &dip : _account.dedicatedIps())
    {
        dipTokens.push_back(dip.dipToken());
    }

    // Hang on to the number of tokens we requested just for diagnostics.  It's
    // possible that _account.dedicatedIps() could change while we're waiting
    // for the response if the user adds/removes tokens.
    auto expectedSize = dipTokens.size();
    qInfo() << "Refresh info for" << dipTokens.size() << "dedicated IPs";
    _apiClient.postRetry(*_environment.getApiv2(), QStringLiteral("dedicated_ip"),
        QJsonDocument{QJsonObject{{QStringLiteral("tokens"), dipTokens}}},
        ApiClient::autoAuth(_account.username(), _account.password(), _account.token()))
        ->notify(this, [this, expectedSize](const Error &err,
                                            const QJsonDocument &json)
        {
            if(err)
            {
                qWarning() << "Unable to refresh dedicated IP info:" << err;
                return;
            }

            _dedicatedIpRefreshTimer.setInterval(msec(dipRefreshSlowInterval));
            auto dedicatedIps = _account.dedicatedIps();
            int priorSize = dedicatedIps.size();

            // If _account.dedicatedIps() has already changed, log this.  Note
            // that this won't necessarily log all changes since it just checks
            // the length, but any changes are handled correctly by
            // applyRefreshedDedicatedIp() (tokens that no longer exist are
            // ignored, any tokens that were added after the request are not
            // updated).
            if(expectedSize != priorSize)
            {
                qInfo() << "Stored dedicated IP count changed from"
                    << expectedSize << "to" << priorSize
                    << "while waiting for this response";
            }

            // If the response JSON is not an array, we get an empty array here
            // and nothing happens.
            const auto &dipJsonArray = json.array();

            // If we didn't get the expected number of tokens back, trace it.
            // This is handled correctly though, it's the same as if tokens were
            // added while waiting for the response.
            if(dipJsonArray.size() != expectedSize)
            {
                qWarning() << "Received" << dipJsonArray.size()
                    << "responses after requesting" << expectedSize << "tokens";
            }

            int traceIdx = 0;
            for(const auto &dipJson : dipJsonArray)
            {
                try
                {
                    applyRefreshedDedicatedIp(dipJson.toObject(), traceIdx, dedicatedIps);
                }
                catch(const Error &ex)
                {
                    qWarning() << "Not updating DIP token"
                        << traceIdx << "- invalid response:" << ex;
                }
                ++traceIdx;
            }

            int newSize = dedicatedIps.size();
            qInfo() << "Dedicated IP count changed from" << priorSize << "to"
                << newSize << "after refresh, changed by" << (newSize-priorSize);
            _account.dedicatedIps(std::move(dedicatedIps));
            rebuildActiveLocations();
        });

    if(dipTokens.count() > 0 && _data.hasFlag(QStringLiteral("check_renew_dip"))) {
        auto renewToken = dipTokens.first().toString();
        _apiClient.postRetry(*_environment.getApiv2(), QStringLiteral("check_renew_dip"),
                             QJsonDocument{QJsonObject{{QStringLiteral("token"), renewToken}}},
                             ApiClient::autoAuth(_account.username(), _account.password(), _account.token()))
                ->notify(this, [this, expectedSize](const Error &err,
                                                    const QJsonDocument &json)
                {
                    Q_UNUSED(json);
                    if(err)
                    {
                        qWarning() << "Unable to send renewal notification:" << err;
                        return;
                    }

                    qDebug() << "Renewal notification sent successfully.";
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
Async<QJsonObject> Daemon::loadAccountInfo(const QString& username, const QString& password, const QString& token)
{
    ApiBase &base = token.isEmpty() ? *_environment.getApiv1() : *_environment.getApiv2();

    return _apiClient.getRetry(base, QStringLiteral("account"), ApiClient::autoAuth(username, password, token))
            ->then(this, [=](const QJsonDocument& json) {
                if (!json.isObject())
                    throw Error(HERE, Error::ApiBadResponseError);

                QJsonObject result;
                QJsonValue value;
                // Define a helper to assign (and optionally transform) a value from
                // the source data if present there, otherwise a default value.
                #define assignOrDefault(field, dataName, ...) do { if (!(value = json[QLatin1String(dataName)]).isUndefined()) { __VA_ARGS__ } else { value = json_cast<QJsonValue>(DaemonAccount::default_##field(), HERE); } result.insert(QStringLiteral(#field), value); } while(false)
                assignOrDefault(plan, "plan");
                assignOrDefault(active, "active");
                assignOrDefault(canceled, "canceled");
                assignOrDefault(recurring, "recurring");
                assignOrDefault(needsPayment, "needs_payment");
                assignOrDefault(daysRemaining, "days_remaining");
                assignOrDefault(renewable, "renewable");
                assignOrDefault(renewURL, "renew_url", {
                    // If the web response directs us to the client-support/helpdesk, we set it to the subscription panel
                    if (value == QStringLiteral("https://www.privateinternetaccess.com/pages/client-support/") ||
                        value == QStringLiteral("https://helpdesk.privateinternetaccess.com/"))
                    {
                        value = QStringLiteral("https://www.privateinternetaccess.com/pages/client-control-panel#subscription-overview");
                    }
                });
                assignOrDefault(expirationTime, "expiration_time", {
                    value = value.toDouble() * 1000.0;
                });
                assignOrDefault(expireAlert, "expire_alert");
                assignOrDefault(expired, "expired");
                assignOrDefault(username, "username");
                #undef assignOrDefault

                return result;
            });
}

void Daemon::resetAccountInfo()
{
    // Reset all fields except loggedIn
    QJsonObject blank = DaemonAccount().toJsonObject();
    blank.remove(QStringLiteral("dedicatedIps"));
    _account.assign(blank);
}

void Daemon::reapplyFirewallRules()
{
    kapps::net::FirewallParams params {};

    // Are we connected right now?
    params.isConnected = _state.connectionState() == QStringLiteral("Connected");

    // If we are currently attempting to connect, use those settings to update
    // the firewall.

    nullable_t<ConnectionConfig> connectionSettings;
    if(_connectingConfig.vpnLocation())
        connectionSettings = _connectingConfig;

    // Used by macOS to access network information for the default interface
    params.macosPrimaryServiceKey = _state.macosPrimaryServiceKey().toStdString();

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
        if(!connectionSettings && params.isConnected)
            connectionSettings = _connectedConfig;
    }

    // If the daemon is not active (no client is connected) or the user is not
    // logged in to an account, we do not apply the KS.
    if (!isActive() || !_account.loggedIn())
        params.leakProtectionEnabled = false;
    else if (_settings.killswitch() == QLatin1String("on"))
        params.leakProtectionEnabled = true;
    else if (_state.systemSleeping() && _settings.connectOnWake())
    {
        params.leakProtectionEnabled = true;
    }
    else if (_settings.killswitch() == QLatin1String("auto") && _state.vpnEnabled())
        params.leakProtectionEnabled = params.hasConnected;

    const bool vpnActive = _state.vpnEnabled();

    if(VPNMethod *pMethod = _connection->vpnMethod())
    {
#if defined(Q_OS_UNIX)
        // Use tunnel interface name on Linux/MacOS, i.e "tun2"
        params.tunnelDeviceName = _state.tunnelDeviceName().toStdString();
#elif defined(Q_OS_WIN)
        // Use luid (stored as a string) on Windows
        const auto &pAdapter = pMethod->getNetworkAdapter();
        if(pAdapter)
        {
            auto &winAdapter = std::static_pointer_cast<WinNetworkAdapter>(pAdapter);
            params.tunnelDeviceName = QString::number(winAdapter->luid()).toStdString();
        }
#endif
    }

    params.tunnelDeviceLocalAddress = _state.tunnelDeviceLocalAddress().toStdString();
    params.tunnelDeviceRemoteAddress = _state.tunnelDeviceRemoteAddress().toStdString();

    params.enableSplitTunnel = _settings.splitTunnelEnabled();

    // For convenience we expose the netScan in params.
    // This way we can use it in code that takes a FirewallParams argument
    // - such as the split tunnel code
    params.netScan = originalNetwork();

    // vpnOnlyApps and excludedApps are set up by applyFirewallRules(), this
    // differs between Windows and POSIX

    for(const auto &subnetRule : _settings.bypassSubnets())
    {
        // We only support bypass rule types for subnets
        if(subnetRule.mode() != QStringLiteral("exclude"))
            continue;

        auto normalizedSubnet = subnetRule.normalizedSubnet();
        auto protocol = subnetRule.protocol();

        if(protocol == QAbstractSocket::IPv4Protocol)
            params.bypassIpv4Subnets.insert(normalizedSubnet.toStdString());
        else if(protocol == QAbstractSocket::IPv6Protocol)
            params.bypassIpv6Subnets.insert(normalizedSubnet.toStdString());
        else
            // Invalid subnet results in QAbsractSocket::UnknownNetworkLayerProtocol
            qWarning() << "Invalid bypass subnet:" << subnetRule.subnet() << "Skipping";
    }

    // Though split tunnel in general can be toggled while connected,
    // defaultRoute can't.  The user can toggle split tunnel as long as the
    // effective value for params.bypassDefaultApps doesn't change.  If it does,
    // we'll still update split tunnel, but this change will require a
    // reconnect.
    if(connectionSettings)
    {
        params.bypassDefaultApps = !connectionSettings->otherAppsUseVpn();
        params.setDefaultRoute = connectionSettings->setDefaultRoute();
        params.splitTunnelDnsEnabled = _settings.splitTunnelDNS();
        params.mtu = connectionSettings->mtu();

        // Convert dns from QStringList to std::vector<std::string>
        params.effectiveDnsServers = qs::stdVecFromStringList(connectionSettings->getDnsServers());
    }

    // Set existing DNS servers
    params.existingDNSServers = _state.existingDNSServers();

    // We can't block everything when the default behavior is to bypass.
    params.blockAll = params.leakProtectionEnabled && !params.bypassDefaultApps;
    params.allowVPN = params.allowDHCP = params.blockAll;
    params.blockIPv6 = (vpnActive || params.leakProtectionEnabled) && _settings.blockIPv6();
    params.allowLAN = _settings.allowLAN() && (params.blockAll || params.blockIPv6);
    // Block DNS when:
    // - the current connection configuration overrides the default DNS
    // - the VPN connection is enabled, and
    // - we've connected at least once since the VPN was enabled
    params.blockDNS = connectionSettings && connectionSettings->setDefaultDns() && vpnActive && params.hasConnected;

    params.allowPIA = (params.blockAll || params.blockIPv6 || params.blockDNS);
    params.allowLoopback = params.allowPIA || params.enableSplitTunnel;
    params.allowResolver = params.blockDNS && connectionSettings &&
        connectionSettings->dnsType() == ConnectionConfig::DnsType::Local;

    // Linux only
    // Should routed packets go over VPN or bypass ?
    params.routedPacketsOnVPN = _settings.routedPacketsOnVPN();

    qInfo() << "Reapplying firewall rules;"
            << "state:" << qEnumToString(_connection->state())
            << "clients:" << _clients.size()
            << "loggedIn:" << _account.loggedIn()
            << "killswitch:" << _settings.killswitch()
            << "vpnEnabled:" << _state.vpnEnabled()
            << "blockIPv6:" << _settings.blockIPv6()
            << "allowLAN:" << _settings.allowLAN()
            << "dnsType:" << (connectionSettings ? qEnumToString(connectionSettings->dnsType()) : QLatin1String("N/A"))
            << "dnsServers:" << (connectionSettings ? connectionSettings->getDnsServers() : QStringList{});

    bool killswitchEnabled = params.leakProtectionEnabled;
    applyFirewallRules(std::move(params));
    _state.killswitchEnabled(killswitchEnabled);
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

    _portForwarder.enablePortForwarding(pfEnabled);
}

void Daemon::setOverrideActive(const QString &resourceName)
{
    FUNCTION_LOGGING_CATEGORY("daemon.override");
    qInfo() << "Override is active:" << resourceName;
    std::deque<QString> overrides = _state.overridesActive();
    overrides.push_back(resourceName);
    _state.overridesActive(overrides);
}

void Daemon::setOverrideFailed(const QString &resourceName)
{
    FUNCTION_LOGGING_CATEGORY("daemon.override");
    qWarning() << "Override could not be loaded:" << resourceName;
    std::deque<QString> overrides = _state.overridesFailed();
    overrides.push_back(resourceName);
    _state.overridesFailed(overrides);
}

void Daemon::updateNextConfig()
{
    ConnectionConfig connection{_settings, _state, _account};
    _state.nextConfig(populateConnection(connection));
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
    }(existingSettingsFile ? _settings.lastUsedVersion() : QString::fromStdString(Version::semanticVersion()));
    // Always write the current version for any future settings upgrades.
    _settings.lastUsedVersion(QString::fromStdString(Version::semanticVersion()));

    // If the user has manually installed a beta release, typically by opting
    // into the beta via the web site, enable beta updates.  This occurs when
    // installing over any non-beta release (including alphas, etc.) or if there
    // was no prior installation.
    //
    // We don't do any other change though (such as switching back) - users that
    // have opted into beta may receive GA releases when there isn't an active
    // beta, and they should continue to receive betas.
    auto daemonVersion = SemVersion::tryParse(QString::fromStdString(Version::semanticVersion()));
    if (daemonVersion && daemonVersion->isPrereleaseType(u"beta") &&
        !_settings.offerBetaUpdates() &&
        (!previous.isPrereleaseType(u"beta") || !existingSettingsFile))
    {
        qInfo() << "Enabling beta updates due to installing" << Version::semanticVersion();
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
            _settings.debugLogging(debugLogging12b2a);   // This updates Logger too
    }
    else
        qInfo() << "not checking debug upgrade";

    // If the prior installed version was less than 1.6.0, adjust the
    // permissions of account.json.  Do this only on upgrade (or new installs,
    // handled by Daemon constructor) in case a user decides to grant access to
    // this file.
    if(previous < SemVersion{1, 6, 0})
        restrictAccountJson();

    // Default debug logging was changed in a prerelease of 2.3.1 released
    // after 2.3.0 (now includes RHI tracing for Direct3D on Windows).
    if(previous <= SemVersion{2, 3, 0})
    {
        const auto &oldFilters = _settings.debugLogging();
        if(oldFilters &&
           (oldFilters.get() == debugLogging12b2a ||
            oldFilters.get() == debugLogging12b2b))
        {
            _settings.debugLogging(DaemonSettings::defaultDebugLogging);
        }
    }

    // CBC cipher mode were removed in 2.9, upgrade these to the corresponding
    // GCM-mode cipher.
    if(_settings.cipher() == QStringLiteral("AES-128-CBC"))
        _settings.cipher(QStringLiteral("AES-128-GCM"));
    else if(_settings.cipher() == QStringLiteral("AES-256-CBC"))
        _settings.cipher(QStringLiteral("AES-256-GCM"));

    // The "proxy" setting was split up into "proxyEnabled" and "proxyType" in
    // 3.0 for consistency with split tunnel, automation, etc.
    if(previous < SemVersion{3, 0, 0})
    {
        // If there was no "proxy" value or it wasn't a string, we'll get an
        // empty value here, which is ignored.
        const auto &legacyProxyValue = _settings.get("proxy").toString();
        if(legacyProxyValue == QStringLiteral("none"))
        {
            _settings.proxyType(QStringLiteral("shadowsocks"));
            _settings.proxyEnabled(false);
        }
        else if(legacyProxyValue == QStringLiteral("custom") ||
            legacyProxyValue == QStringLiteral("shadowsocks"))
        {
            _settings.proxyType(legacyProxyValue);
            _settings.proxyEnabled(true);
        }
    }
    if (_settings.proxyType() == QStringLiteral("custom") &&
        _settings.proxyCustom().host().isEmpty()) {
        //  if proxy type "custom" is selected but the SOCKS5 host is
        //  empty, revert proxy type to "shadowsocks" just to avoid
        //  the situation where the connection fails silently. PP-1062
        _settings.proxyType(QStringLiteral("shadowsocks"));
    }
    if(previous < SemVersion{3, 3, 0}) {
        const auto &oldServiceQualityFlag = _settings.get("sendServiceQualityEvents").toBool();

        if(oldServiceQualityFlag) {
            _settings.serviceQualityAcceptanceVersion ("3.2.0+06857");
        } else {
            _settings.serviceQualityAcceptanceVersion ("");
        }
    }
}

void Daemon::calculateLocationPreferences()
{
    // Pick the best location
    NearestLocations nearest{_state.availableLocations()};

    QSharedPointer<const Location> pVpnBest{nearest.getNearestSafeVpnLocation(_settings.portForward())};

    // Find the user's chosen location (nullptr if it's 'auto' or doesn't exist)
    const auto &locationId = _settings.location();
    QSharedPointer<const Location> pVpnChosen;
    if(locationId != QLatin1String("auto"))
    {
        auto itChosenLocation = _state.availableLocations().find(locationId.toStdString());
        if(itChosenLocation != _state.availableLocations().end()
           && !itChosenLocation->second->offline())
            pVpnChosen = itChosenLocation->second;
    }

    // Find the user's chosen SS location similarly, also ensure that it has
    // Shadowsocks
    const auto &ssLocId = _settings.proxyShadowsocksLocation();
    QSharedPointer<const Location> pSsChosen;
    if(ssLocId != QLatin1String("auto"))
    {
        auto itSsLocation = _state.availableLocations().find(ssLocId.toStdString());
        if(itSsLocation != _state.availableLocations().end()
            && !itSsLocation->second->offline())
            pSsChosen = itSsLocation->second;
    }
    if(pSsChosen && !pSsChosen->hasService(Service::Shadowsocks))
        pSsChosen = {};    // Selected location does not have Shadowsocks

    // Determine the next location we would use
    QSharedPointer<const Location> pVpnNext{pVpnChosen ? pVpnChosen : pVpnBest};

    // The best Shadowsocks location depends on the next VPN location
    QSharedPointer<const Location> pSsBest, pSsNext;
    // pVpnNext can be nullptr if no locations are known, we can't pick any
    // Shadowsocks locations in that case.
    if(pVpnNext)
    {
        // If the next location has SS, use that, that will add the least latency
        if(pVpnNext->hasService(Service::Shadowsocks))
            pSsBest = pVpnNext;
        else
        {
            // If no SS locations are known, this is set to nullptr
            pSsBest = nearest.getBestMatchingLocation(
                [](auto loc){ return loc.hasService(Service::Shadowsocks); });
        }

        // Determine the next SS location
        pSsNext = pSsChosen ? pSsChosen : pSsBest;
    }

    _state.vpnLocations({std::move(pVpnChosen), std::move(pVpnBest),
                         std::move(pVpnNext)});
    _state.shadowsocksLocations({std::move(pSsChosen), std::move(pSsBest),
                                 std::move(pSsNext)});
}

void Daemon::onUpdateRefreshed(const Update &availableUpdate,
                               bool osFailedRequirement,
                               const Update &gaUpdate, const Update &betaUpdate,
                               const std::vector<QString> &flags)
{
    _state.availableVersion(availableUpdate.version());
    _data.gaChannelVersion(gaUpdate.version());
    _data.gaChannelVersionUri(gaUpdate.uri());
    _data.gaChannelOsRequired(gaUpdate.osRequired());
    _data.flags(flags);
    _data.betaChannelVersion(betaUpdate.version());
    _data.betaChannelVersionUri(betaUpdate.uri());
    _data.betaChannelOsRequired(betaUpdate.osRequired());

    // Only set osUnsupported if no updates are available.  It's possible, for
    // example, that there might be a supported GA update available, and also a
    // beta update that has dropped support for this OS version.
    _state.osUnsupported(!availableUpdate.isValid() && osFailedRequirement);
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
        qInfo() << proc.readAllStandardOutput().data();
        qInfo() << "stderr:";
        qInfo() << proc.readAllStandardError().data();
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

void Daemon::applyCurrentAutomationRule()
{
    if(!_state.automationCurrentMatch())
    {
        qInfo() << "No automation rule is active, nothing to do";
        return;
    }

    if(!_settings.automationEnabled())
    {
        qInfo() << "Automation rules are not enabled, not applying action";
        return;
    }

    // Don't apply rules when not logged in.  They wouldn't really have any
    // effect (connect would fail, disconnect would be a no-op), but this is
    // more robust.
    if(!_account.loggedIn())
    {
        qInfo() << "Not logged in, not applying action";
        return;
    }

    const AutomationRule &currentRule = _state.automationCurrentMatch().get();

    Error err{};
    if(currentRule.action().connection() == QStringLiteral("enable"))
    {
        if(!_state.vpnEnabled())
        {
            qInfo() << "Connect now due to automation rule:" << currentRule;
            // connectVPN() can fail if no account is logged in, etc. - in that
            // case, trace that the trigger failed.
            err = connectVPN(ServiceQuality::ConnectionSource::Automatic);
        }
        else
        {
            qInfo() << "Automation rule triggered while already connected:"
                << currentRule;
            // Don't call connectVPN() when the VPN is already enabled, the only
            // thing it could do is reconnect to apply settings, which we don't
            // really want here.  Connect rules can end a snooze, but in that
            // state the VPN isn't enabled.
        }
    }
    else if(currentRule.action().connection() == QStringLiteral("disable"))
    {
        if(_state.vpnEnabled())
        {
            qInfo() << "Disconnect now due to automation rule:" << currentRule;
        }
        else
        {
            qInfo() << "Automation rule triggered while already disconnected:"
                << currentRule;
        }
        // Call disconnectVPN() even if the VPN is already disabled, because it
        // can also end a snooze.
        disconnectVPN(ServiceQuality::ConnectionSource::Automatic);
    }
    else
    {
        qWarning() << "Unknown automation connect action:"
            << currentRule.action().connection();
    }

    // If the trigger failed (only possible with connect), don't set the last
    // trigger, and trace that the trigger failed.
    if(err)
    {
        qWarning() << "Automation rule trigger failed:" << err;
    }
    else
    {
        // Connect/disconnect request succeeded, or we were already in the
        // desired state, indicate the most recent trigger
        _state.automationLastTrigger(currentRule);
    }
}

void Daemon::onAutomationRuleTriggered(const nullable_t<AutomationRule> &currentRule,
                                       Automation::Trigger trigger)
{
    qInfo() << "Rule triggered (" << traceEnum(trigger) << "):"
        << currentRule;

    // Always update automationCurrentMatch.  This shows the
    // "connected" indicator in Settings.
    _state.automationCurrentMatch(currentRule);

    // Check if we need to apply the current rule's action - the
    // daemon must be active, automation rules must be enabled, and there must
    // be a matching rule
    if(!isActive())
    {
        qInfo() << "Daemon is not active, not applying action";
    }
    else
    {
        applyCurrentAutomationRule();
    }
}

Error Daemon::connectVPN(ServiceQuality::ConnectionSource source)
{
    // Cannot connect when no active client is connected (there'd be no way for
    // the daemon to know if the user logs out, etc.)
    if(!isActive())
        return {HERE, Error::Code::DaemonRPCDaemonInactive};
    // Cannot connect when not logged in
    if(!_account.loggedIn())
        return {HERE, Error::Code::DaemonRPCNotLoggedIn};

    // Check if any VPN errors are present, if so prevent the connection
    // and return an error.
    const auto &vpnErrors = _state.vpnSupportErrors();
    qInfo() << "vpnSupportErrors: " << vpnErrors;
    if(!vpnErrors.empty())
        return {HERE, Error::Code::VPNSupportError};

    // Note that for the rest of this logic, it is possible that the VPN is
    // already enabled (_state.vpnEnabled() is already true).  We still want to
    // apply most of the remaining logic, and in particular
    // VpnConnection::connectVPN() will start a reconnect if one is needed to
    // apply settings.

    // Stop any snooze that's active, since something wants to connect the VPN.
    // If this _is_ due to snooze, it updates the snooze state again after we
    // start reconnecting.
    _snoozeTimer.forceStopSnooze();

    emit aboutToConnect();

    _state.vpnEnabled(true);

    // VpnConnection::connectVPN() returns false if the VPN was already
    // enabled and no reconnect was needed.  In that case, do not log an
    // attempt event, since this request had no effect.
    if(_connection->connectVPN(_state.needsReconnect()))
    {
        // A new connection attempt started - either the VPN wasn't enabled, or
        // it was and a reconnect was needed.
        //
        // If this is a reconnect while still connecting, the previous "attempt"
        // event is left unresolved, since it was neither canceled nor
        // established.
        ServiceQuality::VpnProtocol protocol{ServiceQuality::VpnProtocol::OpenVPN};
        if(_settings.method() == QStringLiteral("wireguard"))
            protocol = ServiceQuality::VpnProtocol::WireGuard;
        _pServiceQuality->vpnEnabled(protocol, source);
    }
    _state.needsReconnect(false);

    // Clear any previous automation trigger since we've connected for some
    // other reason.  If this _was_ due to an automation trigger, then
    // applyCurrentAutomationRule() overwrites this with the triggering rule.
    _state.automationLastTrigger({});

    return {};
}

void Daemon::disconnectVPN(ServiceQuality::ConnectionSource source)
{
    _snoozeTimer.forceStopSnooze();
    _state.vpnEnabled(false);

    _pServiceQuality->vpnDisabled(source);

    _connection->disconnectVPN();

    // As in connectVPN(), clear any previous automation trigger.  If this
    // _was_ due to an automation trigger, then applyCurrentAutomationRule()
    // overwrites this with the triggering rule.
    _state.automationLastTrigger({});
}

#ifdef Q_OS_UNIX
void logMetricsForProcessUnix (const QString &identifier, uint pid) {
        QProcess psProcess;
        psProcess.start(QStringLiteral("/bin/ps"), QStringList ()
                        << QStringLiteral("-p") << QString::number(pid)
                        << QStringLiteral("-o") << QStringLiteral("rss=,pcpu="));
        psProcess.waitForFinished();

        if(psProcess.exitCode() != 0)
            return;

        auto psParts = QString::fromUtf8(psProcess.readAllStandardOutput()).trimmed().split(QRegularExpression("\\s+"));
        if(psParts.count() == 2) {
            // Write metrics to the log with the format:
            // "Metrics: client_mem=13251,client_cpu=0.2"

            qInfo() << QStringLiteral("Metrics: %1_mem=%2,%1_cpu=%3").arg(identifier).arg(psParts[0]).arg(psParts[1]);
        } else {
            qWarning () << "Unexpected output from ps - " << psParts;
            return;
        }

}
void logProcessMemoryUnix (const QString &identifier, const QString &name)
{
    // Determine the PID of the process using pgrep
    QProcess pgrepProcess;
    QStringList pgrepArgs
    {
#ifdef Q_OS_MAC
        // BSD pgrep by default excludes its own ancestors.  Since we use this
        // to trace pia-daemon itself, include ancestors with -a
        QStringLiteral("-a"),
#endif
        name
    };
    pgrepProcess.start(QStringLiteral("pgrep"), std::move(pgrepArgs));
    pgrepProcess.waitForFinished();
    // Found at-least one process
    if(pgrepProcess.exitCode() == 0) {
        auto lines = pgrepProcess.readAllStandardOutput().split('\n');

        for(int i = 0; i < lines.size(); ++i) {
            bool ok = true;
            auto pid = lines.at(i).trimmed().toUInt(&ok);
            // We are logging metrics for all processes which match this name.
            // ideally, there should be just one process. However, if there are more,
            // a suffix is added to the identifier
            if(ok) {
                logMetricsForProcessUnix(
                            i == 0 ? identifier : QStringLiteral("%1_%2").arg(identifier).arg(i),
                            pid);
            }
        }
    }
}
#endif


#ifdef Q_OS_WIN
void logProcessMemoryWindows(const QString &id, DWORD pid) {
    WinHandle clientProcess{::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                                          false, pid)};
    DWORD dwError{ERROR_SUCCESS};
    if(!clientProcess)
        dwError = ::GetLastError();
    PROCESS_MEMORY_COUNTERS_EX procMem{};
    procMem.cb = sizeof(procMem);
    BOOL gotMemory = FALSE;
    if(clientProcess)
    {
        gotMemory = ::GetProcessMemoryInfo(clientProcess.get(),
                                           reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&procMem),
                                           procMem.cb);
        if(!gotMemory)
            dwError = ::GetLastError();
    }

    if(gotMemory)
    {
        qInfo () << QStringLiteral("Metrics: %1_privatemem=%2,%1_nonpaged_pool=%3,%1_paged_pool=%4,%1_workingset=%5")
                    .arg(id)
                    .arg(procMem.PrivateUsage)
                    .arg(procMem.QuotaNonPagedPoolUsage)
                    .arg(procMem.QuotaPagedPoolUsage)
                    .arg(procMem.WorkingSetSize);
    }
    else
    {
        qWarning() << "Could not access memory for " << id;
    }
}
#endif

void Daemon::traceMemory()
{
    qDebug () << "Tracing memory";
#ifdef Q_OS_MACOS
    logProcessMemoryUnix(QStringLiteral("client"), QStringLiteral(BRAND_NAME));
    logProcessMemoryUnix(QStringLiteral("daemon"), QStringLiteral(BRAND_CODE "-daemon"));
    logProcessMemoryUnix(QStringLiteral("openvpn"), QStringLiteral(BRAND_CODE "-openvpn"));
    logProcessMemoryUnix(QStringLiteral("wireguard"), QStringLiteral(BRAND_CODE "-wireguard-go"));
#endif

#ifdef Q_OS_LINUX
    logProcessMemoryUnix(QStringLiteral("client"), QStringLiteral(BRAND_CODE "-client"));
    logProcessMemoryUnix(QStringLiteral("daemon"), QStringLiteral(BRAND_CODE "-daemon"));
    logProcessMemoryUnix(QStringLiteral("openvpn"), QStringLiteral(BRAND_CODE "-openvpn"));
    logProcessMemoryUnix(QStringLiteral("wireguard"), QStringLiteral(BRAND_CODE "-wireguard-go"));
#endif

#ifdef Q_OS_WIN
    std::unordered_map<QStringView, QString> targets;

    targets[QStringView{BRAND_CODE L"-client.exe"}] = QStringLiteral("client");
    targets[QStringView{BRAND_CODE L"-service.exe"}] = QStringLiteral("daemon");
    targets[QStringView{BRAND_CODE L"-openvpn.exe"}] = QStringLiteral("openvpn");
    targets[QStringView{BRAND_CODE L"-wgservice.exe"}] = QStringLiteral("wireguard");

    WinHandle procSnapshot{::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};

    PROCESSENTRY32W proc{};
    proc.dwSize = sizeof(proc);

    BOOL nextProc = ::Process32FirstW(procSnapshot.get(), &proc);
    while(nextProc)
    {
        // Avoid wchar_t[] constructor for QStringView which assumes the string
        // fills the entire array
        QStringView processName{&proc.szExeFile[0]};
        auto itTarget = targets.find(processName);
        if(itTarget != targets.end()) {
           logProcessMemoryWindows(itTarget->second, proc.th32ProcessID);
        }
        nextProc = ::Process32NextW(procSnapshot.get(), &proc);
    }
    DWORD dwError = ::GetLastError();
    if(dwError != ERROR_SUCCESS && dwError != ERROR_NO_MORE_FILES)
    {
        qWarning() << "Unable to enumerate processes:" << kapps::core::WinErrTracer{dwError};
    }
#endif
}

ClientConnection::ClientConnection(IPCConnection *connection, LocalMethodRegistry* registry, QObject *parent)
    : QObject(parent)
    , _connection(connection)
    , _rpc(new ServerSideInterface(registry, this))
    , _active(false)
    , _killed(false)
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
    connect(_connection, &IPCConnection::remoteLagging, this, [this]
    {
        qWarning() << "Killing connection" << this << "due to unacknowledged messages";
        kill();
    });

    connect(_connection, &IPCConnection::messageReceived, [this](const QByteArray & msg) {
      ClientConnection::_invokingClient = this;
      qInfo() << "Received message from client" << this;
      auto cleanup = raii_sentinel([]{_invokingClient = nullptr;});
      _rpc->processMessage(msg);
    });
    connect(_rpc, &ServerSideInterface::messageReady, _connection, &IPCConnection::sendMessage);
}
ClientConnection* ClientConnection::_invokingClient = nullptr;

void ClientConnection::kill()
{
    if (_state < Disconnecting)
    {
        _killed = true;
        _connection->close();
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
    // Disconnect the VPN.  Note that this calls forceStopSnooze().
    // This is considered an "automatic" disconnect even though "snooze" is a
    // user interaction, because it doesn't likely indicate a problem if the
    // connection was still in progress (which is probably unlikely to occur
    // anyway for snooze).  The user didn't really ask to "disconnect", they
    // asked to "snooze" and "disconnect" is just how we implement the first
    // part of the snooze.
    _daemon->disconnectVPN(ServiceQuality::ConnectionSource::Automatic);
    // Indicate that this disconnection is for snooze.
    g_state.snoozeEndTime(0);
    // Store the snooze length so we can start the timer after disconnect completes
    _snoozeLength = seconds;
}

void SnoozeTimer::stopSnooze()
{
    qInfo() << "Ending snooze";
    // Connect now, but keep the snooze time since we're resuming from snooze
    // (all other connections reset snooze - note that this calls
    // forceStopSnooze(), which also stops the snooze timer)
    auto snoozeEndTime = g_state.snoozeEndTime();
    Error err = _daemon->connectVPN(ServiceQuality::ConnectionSource::Automatic);
    if(err)
    {
        // Can't resume from snooze, connect was not possible.  (Should rarely
        // happen since we clear snooze when deactivating or logging out.)  If
        // it does happen somehow, leave the snoozeEndTime at -1 and trace the
        // error.
        qWarning() << "Unable to reconnect after snooze:" << err;
    }
    else
        g_state.snoozeEndTime(snoozeEndTime);
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

void SnoozeTimer::handleNewConnectionState(VPNConnection::State state)
{
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
