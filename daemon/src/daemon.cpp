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
#include "locations.h"
#include "path.h"
#include "version.h"
#include "brand.h"
#include "util.h"
#include "apinetwork.h"
#if defined(Q_OS_WIN)
#include "win/wfp_filters.h"
#include "win/win_networks.h"
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

#if defined(Q_OS_WIN)
#include <Windows.h>
#include <VersionHelpers.h>
#include "win/win_util.h"
#include <AclAPI.h>
#include <AccCtrl.h>
#pragma comment(lib, "advapi32.lib")
#endif

#ifdef Q_OS_LINUX
#include "linux/linux_cgroup.h"
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
    const QString regionsResource{QStringLiteral("vpninfo/servers?version=1002&client=x-alpha")};
    const QString shadowsocksRegionsResource{QStringLiteral("vpninfo/shadowsocks_servers")};

    // Resource path for the modern regions list.  This is a placeholder until
    // this actually exists.
    const QString modernRegionsResource{QStringLiteral("vpninfo/servers/v4")};

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

// Populate a ConnectionInfo in DaemonState from VPNConnection's ConnectionConfig
void populateConnection(ConnectionInfo &info, const ConnectionConfig &config)
{
    info.vpnLocation(config.vpnLocation());
    info.vpnLocationAuto(config.vpnLocationAuto());
    switch(config.infrastructure())
    {
        default:
        case ConnectionConfig::Infrastructure::Current:
            info.infrastructure(QStringLiteral("current"));
            break;
        case ConnectionConfig::Infrastructure::Modern:
            info.infrastructure(QStringLiteral("modern"));
            break;
    }
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
        case ConnectionConfig::DnsType::Local:
            info.dnsType(QStringLiteral("local"));
            break;
        case ConnectionConfig::DnsType::Existing:
            info.dnsType(QStringLiteral("existing"));
            break;
        case ConnectionConfig::DnsType::Custom:
            info.dnsType(QStringLiteral("custom"));
            break;
    }
    info.openvpnCipher(config.openvpnCipher());
    info.openvpnAuth(config.openvpnAuth());
    info.openvpnServerCertificate(config.openvpnServerCertificate());
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

void SubnetBypass::clearAllRoutes()
{
    for(const auto &subnet : _lastIpv4Subnets)
        _routeManager->removeRoute(subnet, _lastNetScan.gatewayIp(), _lastNetScan.interfaceName());

    _lastIpv4Subnets.clear();
}

void SubnetBypass::addAndRemoveSubnets(const FirewallParams &params)
{
    auto subnetsToRemove{_lastIpv4Subnets - params.bypassIpv4Subnets};
    auto subnetsToAdd{params.bypassIpv4Subnets - _lastIpv4Subnets};

    // Remove routes for old subnets
    for(const auto &subnet : subnetsToRemove)
        _routeManager->removeRoute(subnet, params.netScan.gatewayIp(), params.netScan.interfaceName());

    // Add routes for new subnets
    for(auto subnet : subnetsToAdd)
        _routeManager->addRoute(subnet, params.netScan.gatewayIp(), params.netScan.interfaceName());
}

QString SubnetBypass::stateChangeString(bool oldValue, bool newValue)
{
    if(oldValue != newValue)
        return QStringLiteral("%1 -> %2").arg(boolToString(oldValue), boolToString(newValue));
    else
        return boolToString(oldValue);
}

void SubnetBypass::updateRoutes(const FirewallParams &params)
{
    // We only need to create routes if:
    // - split tunnel is enabled
    // - we're connected
    // - the VPN has the default route
    // - the netScan is valid
    bool shouldBeEnabled = params.enableSplitTunnel &&
                           params.hasConnected &&
                           params.defaultRoute &&
                           params.netScan.ipv4Valid();

    qInfo() << "SubnetBypass:" << stateChangeString(_isEnabled, shouldBeEnabled);

    // If subnet bypass is not enabled but was enabled previously, disable the subnet bypass
    if(!shouldBeEnabled && _isEnabled)
    {
        qInfo() << "Clearing all subnet bypass routes";
        clearAllRoutes();
        _isEnabled = false;
        _lastIpv4Subnets.clear();
        _lastNetScan = {};
    }
    // Otherwise, enable it
    else if(shouldBeEnabled)
    {
        // We're only interested in IPv4 subnets (if they change) - we don't need to add
        // routes on IPv6 as we don't modify the IPv6 routing table.
        // We also need to update routes if the network changes - as that network info is used to
        // create the routes (gatewayIp, interfaceName, etc)
        if(didSubnetsChange(params.bypassIpv4Subnets) || didNetworkChange(params.netScan))
        {
            if(didNetworkChange(params.netScan))
            {
                qInfo() << "Network info changed from"
                    << _lastNetScan << "to" << params.netScan << "Clearing routes";
                clearAllRoutes();
            }

            addAndRemoveSubnets(params);

            _isEnabled = true;
            _lastIpv4Subnets = params.bypassIpv4Subnets;
            _lastNetScan = params.netScan;
        }
    }
}

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
    , _legacyLatencyTracker{ConnectionConfig::Infrastructure::Current}
    , _modernLatencyTracker{ConnectionConfig::Infrastructure::Modern}
    , _portForwarder{_apiClient, _account, _state, _environment}
    , _regionRefresher{QStringLiteral("regions list"),
                       regionsResource, regionsInitialLoadInterval,
                       regionsRefreshInterval}
    , _shadowsocksRefresher{QStringLiteral("Shadowsocks regions"),
                            shadowsocksRegionsResource,
                            regionsInitialLoadInterval, regionsRefreshInterval}
    , _modernRegionRefresher{QStringLiteral("modern regions"),
                             modernRegionsResource, regionsInitialLoadInterval,
                             regionsRefreshInterval}
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

    // DaemonState::nextConfig depends on DaemonSettings, DaemonState, and DaemonAccount
    connect(&_state, &NativeJsonObject::propertyChanged, this, &Daemon::updateNextConfig);
    connect(&_settings, &NativeJsonObject::propertyChanged, this, &Daemon::updateNextConfig);
    connect(&_account, &NativeJsonObject::propertyChanged, this, &Daemon::updateNextConfig);

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
            g_logger->configure(true, DaemonSettings::defaultDebugLogging);
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

    // If the client ID hasn't been set (or is somehow invalid), generate one
    if(!ClientId::isValidId(_account.clientId()))
    {
        ClientId newId;
        _account.clientId(newId.id());
    }

    #define RPC_METHOD(name, ...) LocalMethod(QStringLiteral(#name), this, &THIS_CLASS::RPC_##name)
    _methodRegistry->add(RPC_METHOD(handshake));
    _methodRegistry->add(RPC_METHOD(applySettings).defaultArguments(false));
    _methodRegistry->add(RPC_METHOD(resetSettings));
    _methodRegistry->add(RPC_METHOD(connectVPN));
    _methodRegistry->add(RPC_METHOD(writeDiagnostics));
    _methodRegistry->add(RPC_METHOD(writeDummyLogs));
    _methodRegistry->add(RPC_METHOD(disconnectVPN));
    _methodRegistry->add(RPC_METHOD(login));
    _methodRegistry->add(RPC_METHOD(emailLogin));
    _methodRegistry->add(RPC_METHOD(setToken));
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

    connect(&_legacyLatencyTracker, &LatencyTracker::newMeasurements, this,
            &Daemon::newLatencyMeasurements);
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

    connect(&_regionRefresher, &JsonRefresher::contentLoaded, this,
            &Daemon::regionsLoaded);
    connect(&_regionRefresher, &JsonRefresher::overrideActive, this,
            [this](){Daemon::setOverrideActive(QStringLiteral("regions list"));});
    connect(&_regionRefresher, &JsonRefresher::overrideFailed, this,
            [this](){Daemon::setOverrideFailed(QStringLiteral("regions list"));});
    connect(&_shadowsocksRefresher, &JsonRefresher::contentLoaded, this,
            &Daemon::shadowsocksRegionsLoaded);
    connect(&_shadowsocksRefresher, &JsonRefresher::overrideActive, this,
            [this](){Daemon::setOverrideActive(QStringLiteral("shadowsocks list"));});
    connect(&_shadowsocksRefresher, &JsonRefresher::overrideFailed, this,
            [this](){Daemon::setOverrideFailed(QStringLiteral("shadowsocks list"));});
    connect(&_modernRegionRefresher, &JsonRefresher::contentLoaded, this,
            &Daemon::modernRegionsLoaded);
    connect(&_modernRegionRefresher, &JsonRefresher::overrideActive, this,
            [this](){Daemon::setOverrideActive(QStringLiteral("modern regions list"));});
    connect(&_modernRegionRefresher, &JsonRefresher::overrideFailed, this,
            [this](){Daemon::setOverrideFailed(QStringLiteral("modern regions list"));});

    connect(this, &Daemon::firstClientConnected, this, [this]() {
        // Reset override states since we are (re)activating
        _state.overridesFailed({});
        _state.overridesActive({});

        _environment.reload();

        _legacyLatencyTracker.start();
        _modernLatencyTracker.start();
        _regionRefresher.startOrOverride(environment().getRegionsListApi(),
                                         Path::LegacyRegionOverride,
                                         Path::LegacyRegionBundle,
                                         _environment.getRegionsListPublicKey(),
                                         _data.cachedLegacyRegionsList());
        _shadowsocksRefresher.startOrOverride(environment().getRegionsListApi(),
                                              Path::LegacyShadowsocksOverride,
                                              Path::LegacyShadowsocksBundle,
                                              _environment.getRegionsListPublicKey(),
                                              _data.cachedLegacyShadowsocksList());
        _modernRegionRefresher.startOrOverride(environment().getModernRegionsListApi(),
                                               Path::ModernRegionOverride,
                                               Path::ModernRegionBundle,
                                               _environment.getRegionsListPublicKey(),
                                               _data.cachedModernRegionsList());
        _updateDownloader.run(true, _environment.getUpdateApi());

        queueNotification(&Daemon::reapplyFirewallRules);
    });

    connect(this, &Daemon::lastClientDisconnected, this, [this]() {
        _updateDownloader.run(false, _environment.getUpdateApi());
        _regionRefresher.stop();
        _shadowsocksRefresher.stop();
        _modernRegionRefresher.stop();
        _legacyLatencyTracker.stop();
        _modernLatencyTracker.stop();
        queueNotification(&Daemon::RPC_disconnectVPN);
        queueNotification(&Daemon::reapplyFirewallRules);
    });
    connect(&_settings, &DaemonSettings::killswitchChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::allowLANChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::overrideDNSChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::bypassSubnetsChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::splitTunnelEnabledChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_settings, &DaemonSettings::splitTunnelRulesChanged, this, &Daemon::queueApplyFirewallRules);
    // 'method' causes a firewall rule application because it can toggle split tunnel
    connect(&_settings, &DaemonSettings::methodChanged, this, &Daemon::queueApplyFirewallRules);
    connect(&_account, &DaemonAccount::loggedInChanged, this, &Daemon::queueApplyFirewallRules);
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
    _updateDownloader.reloadAvailableUpdates(Update{_data.gaChannelVersionUri(), _data.gaChannelVersion(), {}},
                                             Update{_data.betaChannelVersionUri(), _data.betaChannelVersion(), {}});

    queueApplyFirewallRules();

    if(isActive()) {
        emit firstClientConnected();
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
                &Daemon::networksChanged);
        networksChanged(_pNetworkMonitor->getNetworks());
    }

    // Check whether the host supports split tunnel and record errors.
    // This function will also attempt to create the net_cls VFS on Linux if it doesn't exist.
    checkSplitTunnelSupport();
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

OriginalNetworkScan Daemon::originalNetwork() const
{
    return {_state.originalGatewayIp(), _state.originalInterface(),
            _state.originalInterfaceIp(), _state.originalInterfaceIp6()};
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

    bool wasActive = isActive();

    bool success = _settings.assign(settings);

    if(isActive() && !wasActive) {
        qInfo () << "Going active after settings changed";
        emit firstClientConnected();
    } else if (wasActive && !isActive()) {
        qInfo () << "Going inactive after settings changed";
        emit lastClientDisconnected();
    }

    // If the settings affect location choices, rebuild locations from servers
    // lists
    if(settings.contains(QLatin1String("infrastructure")) ||
       settings.contains(QLatin1String("includeGeoOnly")))
    {
        qInfo() << "Infrastructure changed, rebuild locations";
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

    // Persist Daemon - not presented as a "setting"
    defaultsJson.remove(QStringLiteral("persistDaemon"));

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

    writePrettyJson("DaemonState", _state.toJsonObject(), { "availableLocations", "groupedLocations", "externalIp", "externalVpnIp", "forwardedPort" });
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

    auto commonDiagnostics = [&] {
        auto strings = QStringList {
            QStringLiteral("Connected: %1").arg(boolToString(isConnected)),
            QStringLiteral("Split Tunnel enabled: %1").arg(boolToString(_settings.splitTunnelEnabled())),
            QStringLiteral("VPN has default route: %1").arg(boolToString(_settings.splitTunnelEnabled() ?  _settings.defaultRoute() : true)),
            QStringLiteral("Killswitch: %1").arg(_settings.killswitch()),
            QStringLiteral("Allow LAN: %1").arg(boolToString(_settings.allowLAN())),
            QStringLiteral("Network: %1").arg(_settings.infrastructure()),
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

    // Since a client is exiting cleanly, clear the invalid client exit and
    // killed client flags
    _state.invalidClientExit(false);
    _state.killedClient(false);

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

Async<void> Daemon::RPC_emailLogin(const QString &email)
{
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
            })
            ->except(this, [](const Error& error) {
                throw error;
            });
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
            })
            .abortable();
    return _pLoginRequest;
}

void Daemon::RPC_logout()
{
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

    _account.reset();
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
                // lastClientDisconnected())
                Q_ASSERT(isActive());
            }
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
                    qWarning() << "API was reachable but indicates were are not connected, may indicate routing problem";
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
    // - the VPN IP (DaemonState::externalVpnIp)
    // - whether there might be a connection problem (DaemonState::connectionProblem)
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

    populateConnection(_state.connectingConfig(), connectingConfig);
    populateConnection(_state.connectedConfig(), connectedConfig);
    _state.connectedServer(connectedServer);

    queueNotification(&Daemon::reapplyFirewallRules);

    // Latency measurements only make sense when we're not connected to the VPN
    if(state == VPNConnection::State::Disconnected && isActive())
    {
        _legacyLatencyTracker.start();
        _modernLatencyTracker.start();
        // Kick off a region refresh so we typically rotate servers on a
        // reconnect.  Usually the request right after connecting covers this,
        // but this is still helpful in case we were not able to load the
        // resource then.
        _regionRefresher.refresh();
        _shadowsocksRefresher.refresh();
        _modernRegionRefresher.refresh();
    }
    else
    {
        _legacyLatencyTracker.stop();
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
        _regionRefresher.refresh();
        _shadowsocksRefresher.refresh();
        _modernRegionRefresher.refresh();

        // If we haven't obtained a token yet, try to do that now that we're
        // connected (the API is likely reachable through the tunnel).
        if(_account.token().isEmpty())
            refreshAccountInfo();
    }
    else
    {
        ApiNetwork::instance()->setProxy({});
        _socksServer.stop();
        _portForwarder.updateConnectionState(PortForwarder::State::Disconnected);
    }

    if(state == VPNConnection::State::Connected && connectedConfig.requestMace() &&
       connectedConfig.infrastructure() == ConnectionConfig::Infrastructure::Current)
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
    if (state == VPNConnection::State::Disconnected)
    {
        _state.externalIp({});
        _state.tunnelDeviceName({});
        _state.tunnelDeviceLocalAddress({});
        _state.tunnelDeviceRemoteAddress({});
        _state.effectiveDnsServers({});
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

void Daemon::newLatencyMeasurements(ConnectionConfig::Infrastructure infrastructure,
                                    const LatencyTracker::Latencies &measurements)
{
    SCOPE_LOGGING_CATEGORY("daemon.latency");

    bool locationsAffected = false;
    LatencyMap newLatencies;
    if(infrastructure == ConnectionConfig::Infrastructure::Modern)
        newLatencies = _data.modernLatencies();
    else
        newLatencies = _data.latencies();

    // Determine the active infrastructure from the setting
    ConnectionConfig::Infrastructure activeInfra{ConnectionConfig::Infrastructure::Current};
    if(_settings.infrastructure() != QStringLiteral("current"))
        activeInfra = ConnectionConfig::Infrastructure::Modern;

    for(const auto &measurement : measurements)
    {
        newLatencies[measurement.first] = static_cast<double>(msec(measurement.second));
        // If this infrastructure is active, and the location is still present,
        // rebuild the locations later with the new latencies
        if(infrastructure == activeInfra &&
           _state.availableLocations().find(measurement.first) != _state.availableLocations().end())
        {
            locationsAffected = true;
        }
    }

    if(infrastructure == ConnectionConfig::Infrastructure::Modern)
        _data.modernLatencies(newLatencies);
    else
        _data.latencies(newLatencies);

    if(locationsAffected)
    {
        // Rebuild the locations, including the grouped locations and location
        // choices, since the latencies changed
        rebuildActiveLocations();
    }
}

void Daemon::portForwardUpdated(int port)
{
    qInfo() << "Forwarded port updated to" << port;
    _state.forwardedPort(port);
}

void Daemon::applyBuiltLocations(const LocationsById &newLocations)
{
    // If geo-only locations are disabled, remove them.
    // The LatencyTrackers still ping all locations, so we have latency
    // measurements if the locations are re-enabled, but remove them from
    // availableLocations and groupedLocations so all parts of the program will
    // ignore them:
    // - if a geo location is selected, the selection will be treated as 'auto' instead
    // - favorites/recents for geo locations are ignored
    // - piactl does not display or accept them
    // - the regions lists (both VPN and Shadowsocks) do not display them
    if(!_settings.includeGeoOnly())
    {
        LocationsById nonGeoLocations;
        nonGeoLocations.reserve(newLocations.size());
        for(const auto &locEntry : newLocations)
        {
            if(locEntry.second && !locEntry.second->geoOnly())
                nonGeoLocations[locEntry.first] = locEntry.second;
        }
        _state.availableLocations(nonGeoLocations);
    }
    else
    {
        // The data were loaded successfully, store it in DaemonData
        _state.availableLocations(newLocations);
    }

    // Update the grouped locations from the new stored locations
    _state.groupedLocations(buildGroupedLocations(_state.availableLocations()));

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

bool Daemon::rebuildLegacyLocations(const QJsonObject &serversObj,
                                    const QJsonObject &shadowsocksObj)
{
    // Build legacy Locations from the JSON
    LocationsById newLocations = buildLegacyLocations(_data.latencies(),
                                                      serversObj,
                                                      shadowsocksObj);

    // If no locations were found, treat this as an error, since it would
    // prevent any connections from being made
    if(newLocations.empty())
        return false;

    // Apply the legacy locations to the legacy latency tracker (regardless of
    // whether legacy is actually selected right now)
    _legacyLatencyTracker.updateLocations(newLocations);

    // If the legacy infrastructure is active, apply the new locations -
    // otherwise we're just checking the data to make sure we can cache it.
    if(_settings.infrastructure() == QStringLiteral("current"))
        applyBuiltLocations(newLocations);
    return true;
}

bool Daemon::rebuildModernLocations(const QJsonObject &regionsObj,
                                    const QJsonObject &legacyShadowsocksObj)
{
    LocationsById newLocations = buildModernLocations(_data.modernLatencies(),
                                                      regionsObj,
                                                      legacyShadowsocksObj);

    // Like the legacy list, if no regions are found, treat this as an error
    // and keep the data we have (which might still be usable).
    if(newLocations.empty())
        return false;

    // Apply the modern locations to the modern latency tracker
    _modernLatencyTracker.updateLocations(newLocations);

    // If the modern infrastructure is active, apply the new locations -
    // otherwise we're just checking the data to make sure we can cache it.
    if(_settings.infrastructure() != QStringLiteral("current"))
        applyBuiltLocations(newLocations);
    return true;
}

void Daemon::rebuildActiveLocations()
{
    if(_settings.infrastructure() == QStringLiteral("current"))
    {
        rebuildLegacyLocations(_data.cachedLegacyRegionsList(),
                               _data.cachedLegacyShadowsocksList());
    }
    else    // "modern" or "default"
    {
        rebuildModernLocations(_data.cachedModernRegionsList(),
                               _data.cachedLegacyShadowsocksList());
    }
}

void Daemon::regionsLoaded(const QJsonDocument &regionsJsonDoc)
{
    const auto &serversObj = regionsJsonDoc.object();

    // Don't allow a regions list update to leave us with no regions.  If a
    // problem causes this, we're better off keeping the stale regions around.
    if(!rebuildLegacyLocations(serversObj, _data.cachedLegacyShadowsocksList()))
    {
        qWarning() << "Server location data could not be loaded.  Received"
            << regionsJsonDoc.toJson();
        // Don't update cachedLegacyRegionsList, keep the last content (which
        // might still be usable, the new content is no good).
        // Don't treat this as a successful load (don't notify JsonRefresher)
        return;
    }

    _data.cachedLegacyRegionsList(serversObj);
    // A load succeeded, tell JsonRefresher to switch to the long interval
    _regionRefresher.loadSucceeded();
}

void Daemon::shadowsocksRegionsLoaded(const QJsonDocument &shadowsocksRegionsJsonDoc)
{
    const auto &shadowsocksRegionsObj = shadowsocksRegionsJsonDoc.object();

    // It's unlikely that the Shadowsocks regions list could totally hose us,
    // but the same resiliency is here for robustness.
    if(!rebuildLegacyLocations(_data.cachedLegacyRegionsList(), shadowsocksRegionsObj) &&
       !rebuildModernLocations(_data.cachedModernRegionsList(), shadowsocksRegionsObj))
    {
        qWarning() << "Shadowsocks location data could not be loaded.  Received"
            << shadowsocksRegionsJsonDoc.toJson();
        // Don't update cachedLegacyShadowsocksList, keep the last content
        // (which might still be usable, the new content is no good).
        // Don't treat this as a successful load (don't notify JsonRefresher)
        return;
    }

    _data.cachedLegacyShadowsocksList(shadowsocksRegionsObj);
    _shadowsocksRefresher.loadSucceeded();
}

void Daemon::modernRegionsLoaded(const QJsonDocument &modernRegionsJsonDoc)
{
    const auto &modernRegionsObj = modernRegionsJsonDoc.object();

    // As above, if this results in an empty list, don't cache the unusable data
    if(!rebuildModernLocations(modernRegionsObj, _data.cachedLegacyShadowsocksList()))
    {
        qWarning() << "Modern location data could not be loaded.  Received"
            << modernRegionsJsonDoc.toJson();
        // Don't update the cache - keep the existing data, which might still be
        // usable.  Don't treat this as a successful load.
        return;
    }

    _data.cachedModernRegionsList(modernRegionsObj);
    _modernRegionRefresher.loadSucceeded();
}

void Daemon::networksChanged(const std::vector<NetworkConnection> &networks)
{
    OriginalNetworkScan defaultConnection;
    qInfo() << "Networks changed: currently" << networks.size() << "networks";

    // Relevant only to macOS
    QString macosPrimaryServiceKey;

    int netIdx=0;
    for(const auto &network : networks)
    {
        qInfo() << "Network" << netIdx;
        qInfo() << " - itf:" << network.networkInterface();
        qInfo() << " - def4:" << network.defaultIpv4();
        qInfo() << " - def6:" << network.defaultIpv6();
        qInfo() << " - gw4:" << network.gatewayIpv4();
        qInfo() << " - gw6:" << network.gatewayIpv6();
        qInfo() << " - ip4:" << network.addressesIpv4().size();
        int i=0;
        for(const auto &addr : network.addressesIpv4())
        {
            qInfo() << "   " << i << "-" << addr;
            ++i;
        }
        qInfo() << " - ip6:" << network.addressesIpv6().size();
        i=0;
        for(const auto &addr : network.addressesIpv6())
        {
            qInfo() << "   " << i << "-" << addr;
            ++i;
        }

        if(network.defaultIpv4())
        {
            if(network.gatewayIpv4() != Ipv4Address{})
                defaultConnection.gatewayIp(network.gatewayIpv4().toString());
            else
                defaultConnection.gatewayIp({});

            defaultConnection.interfaceName(network.networkInterface());

            if(!network.addressesIpv4().empty())
                defaultConnection.ipAddress(network.addressesIpv4().front().toString());
            else
                defaultConnection.ipAddress({});

#ifdef Q_OS_MACOS
            macosPrimaryServiceKey = network.macosPrimaryServiceKey();
#endif
        }
        if(network.defaultIpv6())
        {
            if(!network.addressesIpv6().empty())
                defaultConnection.ipAddress6(network.addressesIpv6().front().toString());
            else
                defaultConnection.ipAddress6({});
        }
        ++netIdx;
    }

    _state.originalGatewayIp(defaultConnection.gatewayIp());
    _state.originalInterface(defaultConnection.interfaceName());
    _state.originalInterfaceIp(defaultConnection.ipAddress());
    _state.originalInterfaceIp6(defaultConnection.ipAddress6());

    // Relevant only to macOS
    _state.macosPrimaryServiceKey(macosPrimaryServiceKey);

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
    ApiBase &base = token.isEmpty() ? *_environment.getApiv1() : *_environment.getApiv2();

    return _apiClient.getRetry(base, QStringLiteral("account"), ApiClient::autoAuth(username, password, token))
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
                assignOrDefault(username, "username");
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

    params.enableSplitTunnel = _settings.splitTunnelEnabled();

    // For convenience we expose the netScan in params.
    // This way we can use it in code that takes a FirewallParams argument
    // - such as the split tunnel code
    params.netScan = originalNetwork();

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

    for(const auto &subnetRule : _settings.bypassSubnets())
    {
        // We only support bypass rule types for subnets
        if(subnetRule.mode() != QStringLiteral("exclude"))
            continue;

        QString normalizedSubnet = subnetRule.normalizedSubnet();
        auto protocol = subnetRule.protocol();

        if(protocol == QAbstractSocket::IPv4Protocol)
            params.bypassIpv4Subnets << normalizedSubnet;
        else if(protocol == QAbstractSocket::IPv6Protocol)
            params.bypassIpv6Subnets << normalizedSubnet;
        else
            // Invalid subnet results in QAbsractSocket::UnknownNetworkLayerProtocol
            qWarning() << "Invalid bypass subnet:" << subnetRule.subnet() << "Skipping";
    }

    // Though split tunnel in general can be toggled while connected,
    // defaultRoute can't.  The user can toggle split tunnel as long as the
    // effective value for defaultRoute doesn't change.  If it does, we'll still
    // update split tunnel, but the default route change will require a
    // reconnect.
    params.defaultRoute = pConnSettings ? pConnSettings->defaultRoute() : true;

    // When not using the VPN as the default route, force Handshake and Unbound
    // into the VPN with an "include" rule.  (Just routing the Handshake seeds
    // into the VPN is not sufficient; hnsd uses a local recursive DNS resolver
    // that will query authoritative DNS servers, and we want that to go through
    // the VPN.)
    if(!params.defaultRoute)
    {
        params.vpnOnlyApps.push_back(Path::HnsdExecutable);
        params.vpnOnlyApps.push_back(Path::UnboundExecutable);
    }

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
    params.allowResolver = params.blockDNS && pConnSettings && (pConnSettings->dnsType() == QStringLiteral("handshake") || pConnSettings->dnsType() == QStringLiteral("local"));

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

// Check whether the host supports split tunnel and record errors
// This function will also attempt to create the net_cls VFS on Linux if it doesn't exist
void Daemon::checkSplitTunnelSupport()
{
    // Early exit if we already have split tunnel errors
    if(!_state.splitTunnelSupportErrors().isEmpty())
        return;

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

    // If the network monitor couldn't be created, libnl is missing.  (This was
    // not required in some releases, but it is now used to monitor the default
    // route, mainly because it can change while connected with WireGuard.)
    if(!_pNetworkMonitor)
        errors.push_back(QStringLiteral("libnl_invalid"));

    // This cgroup must be mounted in this location for this feature.
    QFileInfo cgroupFile(Path::ParentVpnExclusionsFile);
    if(!cgroupFile.exists())
    {
        // Try to create the net_cls VFS (if we have no other errors)
        if(errors.isEmpty())
        {
            if(!CGroup::createNetCls())
                errors.push_back(QStringLiteral("cgroups_invalid"));
        }
        else
        {
            errors.push_back(QStringLiteral("cgroups_invalid"));
        }
    }
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

    _portForwarder.enablePortForwarding(pfEnabled);
}

void Daemon::setOverrideActive(const QString &resourceName)
{
    FUNCTION_LOGGING_CATEGORY("daemon.override");
    qInfo() << "Override is active:" << resourceName;
    QStringList overrides = _state.overridesActive();
    overrides.push_back(resourceName);
    _state.overridesActive(overrides);
}

void Daemon::setOverrideFailed(const QString &resourceName)
{
    FUNCTION_LOGGING_CATEGORY("daemon.override");
    qWarning() << "Override could not be loaded:" << resourceName;
    QStringList overrides = _state.overridesFailed();
    overrides.push_back(resourceName);
    _state.overridesFailed(overrides);
}

void Daemon::updateNextConfig(const QString &changedPropertyName)
{
    // Ignore changes in nextConfig itself.  Should theoretically have no effect
    // anyway since _state.nextConfig() below won't actually cause a change
    // (nextConfig doesn't depend on itself), but explicitly ignore for
    // robustness.
    if(changedPropertyName == QStringLiteral("nextConfig"))
        return;

    ConnectionConfig connection{_settings, _state, _account};
    ConnectionInfo info{};
    populateConnection(info, connection);
    _state.nextConfig(info);
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

    // Handshake was removed in 2.2.0.
#if !INCLUDE_FEATURE_HANDSHAKE
    DaemonSettings::DNSSetting handshakeValue;
    handshakeValue = QStringLiteral("handshake");
    if(previous < SemVersion{2, 2, 0} && _settings.overrideDNS() == handshakeValue)
    {
        // Migrate to the "Local Resolver" setting.  Since Handshake's testnet
        // never actually had any useful domains in it, this is essentially what
        // the Handshake setting used to do - it would virtually always fall
        // back to the ICANN DNS root.
        qInfo() << "Migrating DNS setting to local resolver from Handshake";
        _settings.overrideDNS(QStringLiteral("local"));
    }
#endif

    // Some alpha builds prior to 2.2.0 were released with other values for
    // this setting.
    if(previous < SemVersion{2, 2, 0})
        _settings.macStubDnsMethod(QStringLiteral("NX"));
}

void Daemon::calculateLocationPreferences()
{
    // Pick the best location
    NearestLocations nearest{_state.availableLocations()};
    // PF currently requires OpenVPN.  This duplicates some logic from
    // ConnectionConfig, but the hope is that over time we'll support all/most
    // settings with WireGuard too, so these checks will just go away.
    bool portForwardEnabled = false;
    if(_settings.method() == QStringLiteral("openvpn") ||
        _settings.infrastructure() != QStringLiteral("current"))
    {
        portForwardEnabled = _settings.portForward();
    }
    _state.vpnLocations().bestLocation(nearest.getNearestSafeVpnLocation(portForwardEnabled));

    // Find the user's chosen location (nullptr if it's 'auto' or doesn't exist)
    const auto &locationId = _settings.location();
    _state.vpnLocations().chosenLocation({});
    if(locationId != QLatin1String("auto"))
    {
        auto itChosenLocation = _state.availableLocations().find(locationId);
        if(itChosenLocation != _state.availableLocations().end())
            _state.vpnLocations().chosenLocation(itChosenLocation->second);
    }

    // Find the user's chosen SS location similarly, also ensure that it has
    // Shadowsocks
    const auto &ssLocId = _settings.proxyShadowsocksLocation();
    QSharedPointer<Location> pSsLoc;
    if(ssLocId != QLatin1String("auto"))
    {
        auto itSsLocation = _state.availableLocations().find(ssLocId);
        if(itSsLocation != _state.availableLocations().end())
            pSsLoc = itSsLocation->second;
    }
    if(pSsLoc && !pSsLoc->hasService(Service::Shadowsocks))
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
    if(pNextLocation->hasService(Service::Shadowsocks))
        _state.shadowsocksLocations().bestLocation(pNextLocation);
    else
    {
        // If no SS locations are known, this is set to nullptr
        _state.shadowsocksLocations().bestLocation(nearest.getBestMatchingLocation(
            [](auto loc){ return loc.hasService(Service::Shadowsocks); }));
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
    _data.flags(gaUpdate.flags());
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
