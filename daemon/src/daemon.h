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
#line HEADER_FILE("daemon.h")

#ifndef DAEMON_H
#define DAEMON_H
#pragma once

#include "settings.h"
#include "async.h"
#include "environment.h"
#include "jsonrpc.h"
#include "latencytracker.h"
#include "networkmonitor.h"
#include "portforwarder.h"
#include "socksserverthread.h"
#include "updatedownloader.h"
#include "vpn.h"
#include "apiclient.h"

#include <QCoreApplication>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QNetworkAccessManager>

class IPCConnection;
class IPCServer;
class LocalMethodRegistry;
class ServerSideInterface;


// Contains all information about a particular connected client.
//
class ClientConnection : public QObject
{
    Q_OBJECT
    friend class Daemon;
public:
    static ClientConnection* getInvokingClient() { return _invokingClient; }
    enum State { Connected, Authenticated, Disconnecting, Disconnected };

    explicit ClientConnection(IPCConnection* connection, LocalMethodRegistry* registry, QObject* parent = nullptr);

    template<typename... Args>
    void post(const QString& name, Args&&... args) { _rpc->post(name, std::forward<Args>(args)...); }

    // Daemon distinguishes between two types of client connections so it knows
    // whether to disconnect the VPN on a client exit, and to handle client
    // crashes.
    //
    // "Active" clients can "own" the VPN connection state.
    // - If the last active client deactivates (indicating a clean exit), the
    //   VPN is disconnected automatically, and the daemon deactivates
    // - If the last active client unexpectedly exits, it's assumed to be a
    //   client crash and DaemonState::invalidClientExit is set
    //
    // "Inactive" clients do not "own" the VPN connection state.  These are CLI
    // clients, new connections that haven't activated yet, or interactive
    // clients that are cleanly exiting.
    // - Inactive clients do not cause the daemon to activate.
    // - Inactive clients can still observe state, issue requests that do not
    //   require the daemon to activate, etc.
    // - Inactive clients can try to connect also, but this will only be
    //   accepted if the daemon is active (due to an Interactive client or the
    //   client-crashed state)
    //
    // Clients are initially inactive.  A client can become active with
    // RPC_notifyClientActivate() and can return to transient using
    // RPC_notifyClientDeactivate().
    bool getActive() const {return _active;}
    void setActive(bool active) {_active = active;}

    bool getKilled() const {return _killed;}

    void kill();

signals:
    void disconnected();

private:
    IPCConnection* _connection;
    static ClientConnection *_invokingClient;
    ServerSideInterface* _rpc;
    bool _active;
    // Whether the client connection is being killed by the server.  If an
    // active client connection unexpectedly exits, this affects the way the
    // daemon remains active (invalidClientExit vs. killedClient)
    bool _killed;
    State _state;
};

// Descriptor for a set of firewall rules to be appled.
//
struct FirewallParams
{
    // These parameters are specified by VPNConnection (some are passed through
    // from the VPNMethod)

    // DNS servers to permit through firewall - set once DNS has been applied
    QStringList effectiveDnsServers;
    // VPN network interface - see VPNMethod::getNetworkAdapter()
    std::shared_ptr<NetworkAdapter> adapter;

    // The following flags indicate which general rulesets are needed. Note that
    // this is after some sanity filtering, i.e. an allow rule may be listed
    // as not needed if there were no block rules preceding it. The rulesets
    // should be thought of as in last-match order.

    bool leakProtectionEnabled; // Apply leak protection (on for KS=always, or for KS=auto when connected)
    bool blockAll;      // Block all traffic by default
    bool allowVPN;      // Exempt traffic through VPN tunnel
    bool allowDHCP;     // Exempt DHCP traffic
    bool blockIPv6;     // Block all IPv6 traffic
    bool allowLAN;      // Exempt LAN traffic, including IPv6 LAN traffic
    bool blockDNS;      // Block all DNS traffic except specified DNS servers
    bool allowPIA;      // Exempt PIA executables
    bool allowLoopback; // Exempt loopback traffic
    bool allowResolver; // Exempt local DNS resolver traffic

    // Have we connected to the VPN since it was enabled?  (Some rules are only
    // activated once we successfully connect, but remain active even if we lose
    // the connection while reconnecting.)
    bool hasConnected;
    // When connected or connecting, whether the VPN is being used as the
    // default route.  ('true' when not connected.)
    bool defaultRoute;

    // Whether to enable split tunnel.  Split tunnel is enabled whenever the
    // setting is enabled, even if the PIA client is not logged in.  This is
    // important to block Only VPN apps - otherwise, they may leak even after
    // PIA is started/connected, because they could have existing connections
    // that were permitted.
    //
    // Bypass VPN apps though are only affected when we connect
    // - persistent connections from those apps would be
    // fine since they bypass the VPN anyway.
    //
    // On Mac, this causes the kext to be loaded, but on Windows the WFP callout
    // is not loaded until we connect (app blocks can be implemented in WFP
    // without loading the callout driver).
    bool enableSplitTunnel;
    // Original network information used (among other things) to manage apps for split
    // tunnel.
    OriginalNetworkScan netScan;
    QVector<QString> excludeApps; // Apps to exclude if VPN exemptions are enabled
    QVector<QString> vpnOnlyApps; // Apps to force on the VPN

    QSet<QString> bypassIpv4Subnets; // IPv4 subnets to bypass VPN
    QSet<QString> bypassIpv6Subnets; // IPv6 subnets to bypass VPN
};
Q_DECLARE_METATYPE(FirewallParams)

class RouteManager
{
    CLASS_LOGGING_CATEGORY("RouteManager")
public:
    virtual void addRoute(const QString &subnet, const QString &gatewayIp, const QString &interfaceName, uint32_t metric=0) const = 0;
    virtual void removeRoute(const QString &subnet, const QString &gatewayIp, const QString &interfaceName) const = 0;
    virtual ~RouteManager() {}
};

class SubnetBypass
{
    CLASS_LOGGING_CATEGORY("SubnetBypass")
public:

    // Inject the RouteManager dependency - also makes
    // this class easily testable
    SubnetBypass(std::unique_ptr<RouteManager> routeManager)
        : _routeManager{std::move(routeManager)}
        , _isEnabled{false}
    {}

    void updateRoutes(const FirewallParams &params);
private:
    bool didNetworkChange(const OriginalNetworkScan &netScan) {return netScan != _lastNetScan;}
    bool didSubnetsChange(const QSet<QString> &subnets) {return subnets != _lastIpv4Subnets;}
    void addAndRemoveSubnets(const FirewallParams &params);
    void clearAllRoutes();
    QString boolToString(bool value) {return value ? "ON" : "OFF";}
    QString stateChangeString(bool oldValue, bool newValue);
private:
    std::unique_ptr<RouteManager> _routeManager;
    OriginalNetworkScan _lastNetScan;
    QSet<QString> _lastIpv4Subnets;
    bool _isEnabled;
};

class DiagnosticsFile
{
public:
    // Function type for processing command output for diagnostics.
    using ProcessOutputFunction = std::function<QByteArray(const QByteArray&)>;

public:
    DiagnosticsFile(const QString &filePath);

private:
    QString diagnosticsCommandHeader(const QString &commandName);

    // Log the time and size of the part that was just written (updates
    // _currentSize).
    void logPart(const QString &title, QElapsedTimer &commandTime);

    // Execute a QProcess and write the output as a file part
    void execAndWriteOutput(QProcess &cmd, const QString &commandName,
                            const ProcessOutputFunction &processOutput);

public:
    // Write the result of a command as a file part
    void writeCommand(const QString &commandName, const QString &command,
                      const QStringList &args,
                      const ProcessOutputFunction &processOutput = {});

    // Write the result of a command as a file part if predicate is true
    void writeCommandIf(bool predicate, const QString &commandName,
                        const QString &command,
                        const QStringList &args,
                        const ProcessOutputFunction &processOutput = {});

#ifdef Q_OS_WIN
    // On Windows, we have to be able to set the complete command line string in
    // some cases; QProcess::start() explains this in detail.
    // QProcess::setNativeArguments() is only available on Windows.
    void writeCommand(const QString &commandName, const QString &command,
                      const QString &nativeArgs,
                      const ProcessOutputFunction &processOutput = {});

    void writeCommandIf(bool predicate,
                        const QString &commandName,
                        const QString &command,
                        const QString &nativeArgs,
                        const ProcessOutputFunction &processOutput = {});
#endif

    // Write a text blob as a file part
    void writeText(const QString &title, const QString &text);

private:
    QFile _diagFile;
    QTextStream _fileWriter;
    qint64 _currentSize;
};

class Daemon;

class SnoozeTimer : public QObject {
  Q_OBJECT

  QTimer _snoozeTimer;
  Daemon *_daemon;

  // The snooze length in seconds
  qint64 _snoozeLength;

public:
  SnoozeTimer(Daemon *d);

  void startSnooze(qint64 seconds);
  void stopSnooze();
  void forceStopSnooze();

protected slots:
  void handleTimeout();
  void handleNewConnectionState(VPNConnection::State state);
};

// The main application class for the daemon, housing the main thread
// message loop and associated functionality.
//
class Daemon : public QObject, public Singleton<Daemon>
{
    Q_OBJECT
    friend class SnoozeTimer;
    CLASS_LOGGING_CATEGORY("daemon")

public:
    explicit Daemon(QObject* parent = nullptr);
    ~Daemon();

    Environment &environment() {return _environment;}
    ApiClient &apiClient() {return _apiClient;}

    DaemonData& data() { return _data; }
    DaemonAccount& account() { return _account; }
    DaemonSettings& settings() { return _settings; }
    DaemonState& state() { return _state; }

    void reportError(Error error);

    virtual std::shared_ptr<NetworkAdapter> getNetworkAdapter() = 0;

    // Get the _state.original* fields as an OriginalNetworkScan
    OriginalNetworkScan originalNetwork() const;

    bool vpnHasDefaultRoute() { return _settings.splitTunnelEnabled() ?  _settings.defaultRoute() : true; }

protected:
    virtual void applyFirewallRules(const FirewallParams& params) {}

protected:
    Async<QJsonObject> loadAccountInfo(const QString& username, const QString& password, const QString& token);
    void resetAccountInfo();
    void upgradeSettings(bool existingSettingsFile);
    void queueApplyFirewallRules() { queueNotification(&Daemon::reapplyFirewallRules); }

    // Check whether any active clients are connected
    bool hasActiveClient() const;
    // Check whether the daemon is currently active.  When this changes, we emit
    // firstClientConnected() or lastClientDisconnected().
    // Normally the daemon is active when any active client is connected, but it
    // can also remain active if an active client exits unexpectedly
    // (DaemonState::invalidClientExit / killedClient).
    bool isActive() const;

    IMPLEMENT_NOTIFICATIONS(Daemon)

protected:
    // RPC functions

    QString RPC_handshake(const QString& version);
    void RPC_applySettings(const QJsonObject& settings, bool reconnectIfNeeded = false);
    void RPC_resetSettings();
    void RPC_connectVPN();
    QJsonValue RPC_writeDiagnostics();
    void RPC_writeDummyLogs();
    void RPC_crash();
    void RPC_disconnectVPN();
    void RPC_notifyClientActivate();
    void RPC_notifyClientDeactivate();
    void RPC_startSnooze(qint64 seconds);
    void RPC_stopSnooze();
    Async<void> RPC_emailLogin(const QString &email);
    Async<void> RPC_setToken(const QString& token);
    Async<void> RPC_login(const QString& username, const QString& password);
    void RPC_logout();
    // Refresh update metadata (asynchronously)
    void RPC_refreshUpdate();
    // Download an update advertised in DaemonData::availableVersion.
    Async<QJsonValue> RPC_downloadUpdate();
    // Cancel an update download that is ongoing
    void RPC_cancelDownloadUpdate();

    // These RPCs are platform-specific; platform daemons override them with
    // implementation.
    // Attempt to load the network kext on Mac
    virtual QJsonValue RPC_installKext();
    // Inspect UWP apps on Windows to determine which have executables and which
    // are WWA apps.  (Must be done by the daemon because we read the package
    // manifests to check this.)
    //
    // familyIds is an array of UWP package family IDs.
    //
    // The result is an object with:
    // - exe: Array of UWP package family IDs as above representing apps with an
    //   executable.
    // - wwa: Array of UWP package family IDs as above representing WWA apps.
    //
    // Apps that can't be identified are omitted from the results.
    virtual QJsonValue RPC_inspectUwpApps(const QJsonArray &familyIds);
    // Do a manual check of the driver states on Windows.
    // (Used when pia-service.exe is invoked to install a relevant driver, it
    // signals the running service to re-check the state.)
    virtual void RPC_checkDriverState();

    // Write platform-specific diagnostics to implement RPC_writeDiagnostics()
    virtual void writePlatformDiagnostics(DiagnosticsFile &file) = 0;

    // Overview of common diagnostic information
    QString diagnosticsOverview() const;
signals:
    // The daemon has started and will need the message loop to be pumped.
    void started();
    // The daemon has stopped and no longer needs the message loop.
    void stopped();

    // The number of connected clients (GUIs) has increased to 1
    void firstClientConnected();
    // The number of connected clients (GUIs) has decreased to 0
    void lastClientDisconnected();

    // The daemon is about to connect to the VPN.  Some platform daemons do last
    // minute initialization the first time this happens.
    void aboutToConnect();

    // Networks have changed
    void networksChanged();

public slots:
    // Run the core daemon in the current message loop.
    virtual void start();
    // Request that the daemon stop and exit.
    virtual void stop();

private:
    void clientConnected(IPCConnection* connection);
    void notifyChanges();
    void serialize();
    Async<void> loadVpnIp();
    void vpnStateChanged(VPNConnection::State state,
                         const ConnectionConfig &connectingConfig,
                         const ConnectionConfig &connectedConfig,
                         const nullable_t<Server> &connectedServer,
                         const nullable_t<Transport> &chosenTransport,
                         const nullable_t<Transport> &actualTransport);
    void vpnError(const Error& error);
    void vpnByteCountsChanged();
    void newLatencyMeasurements(ConnectionConfig::Infrastructure infrastructure,
                                const LatencyTracker::Latencies &measurements);
    void portForwardUpdated(int port);

    // Store new locations built from one of the regions lists and update
    // dependent properties - used by rebuild*Location().
    void applyBuiltLocations(const LocationsById &newLocations);

    // Build the locations list from the legacy regions and Shadowsocks lists.
    // Returns true if the resulting location list is not empty (indicating that
    // the data is valid and should be cached).
    //
    // If the legacy infrastructure is selected, the new locations are applied,
    // and all dependent properties are rebuilt.  (If it is not active, the new
    // locations list is just discarded, it's just used to make sure the new
    // data are valid before we cache it.)
    //
    // serversObj and shadowsocksObj can be the cached objects from DaemonData,
    // or new data retrieved (which should then be cached if successfully
    // loaded).  Latencies from DaemonData are used.
    bool rebuildLegacyLocations(const QJsonObject &serversObj,
                                const QJsonObject &shadowsocksObj);
    // Build the locations list from the modern regions list.  Like
    // rebuildLegacyLocations(), returns true if the new locations list is not
    // empty, meaning the new data can be cached.
    //
    // If the modern infrastructure is selected, the new locations are applied,
    // like rebuildLegacyLocations().
    //
    // regionsObj can be the cached object from DaemonData or new data retrieved
    // (which should then be cached if successful).  Latencies from DaemonData
    // are used.
    bool rebuildModernLocations(const QJsonObject &regionsObj,
                                const QJsonObject &legacyShadowsocksObj);

    // Rebuild either the legacy or modern locations from the cached data,
    // depending on the infrastructure setting.  Used when latencies are updated
    // or when initially building the regions list.
    void rebuildActiveLocations();

    // Handle region list results from JsonRefresher
    void regionsLoaded(const QJsonDocument &regionsJsonDoc);
    void shadowsocksRegionsLoaded(const QJsonDocument &shadowsocksRegionsJsonDoc);
    void modernRegionsLoaded(const QJsonDocument &modernRegionsJsonDoc);
    void onNetworksChanged(const std::vector<NetworkConnection> &networks);

    void refreshAccountInfo();
    void reapplyFirewallRules();


private:
    // Rebuild location preferences from the grouped locations.  Used when
    // settings are changed that affect the location preferences.  (Latency and
    // region list changes result in rebuilding the entire region list.)
    void calculateLocationPreferences();
    // Rebuild the chosen/best/next location selections (without rebuilding the
    // entire list).  Used when data changes that affect the location
    // selections.
    void updateChosenLocations();
    void onUpdateRefreshed(const Update &availableUpdate,
                           const Update &gaUpdate, const Update &betaUpdate);
    void onUpdateDownloadProgress(const QString &version, int progress);
    void onUpdateDownloadFinished(const QString &version,
                                  const QString &installerPath);
    void onUpdateDownloadFailed(const QString &version, bool error);

    // Check whether the host supports split tunnel and record errors
    // This function will also attempt to create the net_cls VFS on Linux if it doesn't exist
    void checkSplitTunnelSupport();

    // Update the port forwarder's enabled state, based on the current setting
    // or the setting value that we connected with
    void updatePortForwarder();

    // Set _state.overridesActive() or _state.overridesFailed() and log
    // appropriately
    void setOverrideActive(const QString &resourceName);
    void setOverrideFailed(const QString &resourceName);

    // Update DaemonState::nextConfig following a property change from
    // DaemonState, DaemonSettings, or DaemonAccount
    void updateNextConfig(const QString &changedPropertyName);

    void logCommand(const QString &cmd, const QStringList &args);
    // Log the current routing table; used after connecting
    void logRoutingTable();

    void connectVPN();
    void disconnectVPN();
protected:
    bool _started, _stopping;

    IPCServer* _server;
    QHash<IPCConnection*, ClientConnection*> _clients;
    LocalMethodRegistry* _methodRegistry;
    RemoteNotificationInterface* _rpc;

    VPNConnection* _connection;

    DaemonData _data;
    DaemonAccount _account;
    DaemonSettings _settings;
    DaemonState _state;

    Environment _environment;
    ApiClient _apiClient;

    // Latencies are measured for both infrastructures regardless of which one
    // is active, so we are able to switch infrastructures at any time.
    LatencyTracker _legacyLatencyTracker;
    LatencyTracker _modernLatencyTracker;
    PortForwarder _portForwarder;
    JsonRefresher _regionRefresher, _shadowsocksRefresher, _modernRegionRefresher;
    SocksServerThread _socksServer;
    UpdateDownloader _updateDownloader;
    SnoozeTimer _snoozeTimer;
    std::unique_ptr<NetworkMonitor> _pNetworkMonitor;

    QSet<QString> _dataChanges;
    QSet<QString> _accountChanges;
    QSet<QString> _settingsChanges;
    QSet<QString> _stateChanges;

    unsigned int _pendingSerializations;
    QTimer _serializationTimer;

    QTimer _accountRefreshTimer;

    // Ongoing login attempt.  If we try to log in again or log out, we need to
    // abort the prior attempt.  This is an AbortableTask so it'll still
    // complete with an error when it's aborted - this is an RPC result so it
    // still needs to return an error.
    Async<AbortableTask<void>> _pLoginRequest;

    // Ongoing attempt to get the VPN IP address.  This can retry for a long
    // time, so we discard it if we leave the Connected state.
    Async<void> _pVpnIpRequest;

};

#define g_daemon (Daemon::instance())

#define g_account (g_daemon->account())
#define g_data (g_daemon->data())
#define g_settings (g_daemon->settings())
#define g_state (g_daemon->state())

#endif // DAEMON_H
