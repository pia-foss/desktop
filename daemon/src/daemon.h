// Copyright (c) 2023 Private Internet Access, Inc.
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
#line HEADER_FILE("daemon.h")

#ifndef DAEMON_H
#define DAEMON_H
#pragma once

#include <common/src/settings/daemonaccount.h>
#include <common/src/settings/daemondata.h>
#include "model/state.h"
#include <common/src/settings/daemonsettings.h>
#include <common/src/async.h>
#include "environment.h"
#include <common/src/jsonrpc.h>
#include "latencytracker.h"
#include "networkmonitor.h"
#include "portforwarder.h"
#include "socksserverthread.h"
#include "updatedownloader.h"
#include "servicequality.h"
#include "vpn.h"
#include "apiclient.h"
#include "automation.h"
#include <kapps_net/src/firewallparams.h>

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
    //   client crash and StateModel::invalidClientExit is set
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

// From kapps-net
Q_DECLARE_METATYPE(kapps::net::FirewallParams)

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
    void writeText(const QString &title, QString text);

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
    StateModel& state() { return _state; }

    virtual std::shared_ptr<NetworkAdapter> getNetworkAdapter() = 0;

    // Get the _state.original* fields as an OriginalNetworkScan
    OriginalNetworkScan originalNetwork() const;

    void forcePublicIpRefresh();

protected:
    virtual void applyFirewallRules(kapps::net::FirewallParams params) {}

protected:
    Async<QJsonObject> loadAccountInfo(const QString& username, const QString& password, const QString& token);
    void resetAccountInfo();
    void upgradeSettings(bool existingSettingsFile);
    void queueApplyFirewallRules() { queueNotification(&Daemon::reapplyFirewallRules); }

    // Check whether any active clients are connected
    bool hasActiveClient() const;
    // Check whether the daemon is currently active.  When this changes, we emit
    // daemonActivated() or daemonDeactivated().
    // Normally the daemon is active when any active client is connected, but it
    // can also remain active if an active client exits unexpectedly
    // (StateModel::invalidClientExit / killedClient), or if background mode is
    // enabled in Settings.
    bool isActive() const;

    IMPLEMENT_NOTIFICATIONS(Daemon)

protected:
    // RPC functions

    // Settings
    void RPC_applySettings(const QJsonObject& settings, bool reconnectIfNeeded = false);
    void RPC_resetSettings();
    // Dedicated IPs aren't actually stored in settings (due to the token being
    // treated as a password), but they're used by the client in mostly the same
    // way.
    // Add a dedicated IP by specifying a dedicated IP token.  The request
    // succeeds if the token is valid and not expired (and the API can be
    // reached to confirm this).
    Async<void> RPC_addDedicatedIp(const QString &token);
    // Remove a dedicated IP by specifying the generated region ID for the DIP
    // region (since the client can't see dedicated IP tokens).
    void RPC_removeDedicatedIp(const QString &dipRegionId);
    // If one or more dedicated IPs have changed IP address (triggering a client
    // notification), clear that notification by dismissing the change.  This
    // resets AccountDedicatedIp::lastIpChange() for all dedicated IPs.
    void RPC_dismissDedicatedIpChange();

    // Connection

    // Connect the VPN.  This indicates a manual connection request and will
    // count a connection attempt if applicable.  Connections for other reasons
    // (rules, snooze-end, etc.), should use connectVPN() directly.
    void RPC_connectVPN();
    void RPC_disconnectVPN();
    void RPC_startSnooze(qint64 seconds);
    void RPC_stopSnooze();
    // Get the current "best" region in a country.  While countries themselves
    // can't be properly selected, the client does provide "country" favorites
    // that automatically select the best region in the country at the time of
    // connection.
    //
    // This is done via RPC so the client doesn't have to know the details of
    // the daemon's selection algorithm, which prefers auto-safe and non-geo
    // regions in general, etc.  This can't be used for UI as it does not
    // indicate when the preferences change; it can only really be used to
    // connect to the resulting region.  The intent is that this will be
    // replaced with proper country selections.
    QString RPC_getCountryBestRegion(const QString &country);

    // Diagnostics
    QJsonValue RPC_writeDiagnostics();
    void RPC_writeDummyLogs();
    void RPC_crash();

    // Refresh server metadata (asynchronously)
    // Refreshes most information that's normally loaded periodically, such as
    // dedicated IPs, update information, client notifications, etc.  Used for
    // testing to avoid waiting on long timers.
    void RPC_refreshMetadata();
    // Send service quality events early (if enabled, used by a dev tool)
    void RPC_sendServiceQualityEvents();

    // Client activation
    void RPC_notifyClientActivate();
    void RPC_notifyClientDeactivate();

    // Login
    // Request an email login link
    Async<void> RPC_emailLogin(const QString &email);
    // Apply a token recieved client-side via a piavpn:login URI
    Async<void> RPC_setToken(const QString& token);
    Async<void> RPC_login(const QString& username, const QString& password);
    // Retry a login after we were unable to reach the API (in the "API
    // unreachable" state).  The client can't access the stored account password
    // in this state, so it must explicitly ask the daemon to retry with the
    // stored credentials.
    Async<void> RPC_retryLogin();
    void RPC_logout();

    // Updates
    // Download an update advertised in DaemonData::availableVersion.
    Async<QJsonValue> RPC_downloadUpdate();
    // Cancel an update download that is ongoing
    void RPC_cancelDownloadUpdate();

    // Misc.
    Async<void> RPC_submitRating(int rating);

    // These RPCs are platform-specific; platform daemons override them with
    // implementation.

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

    // The number of connected clients (GUIs) has increased to 1, or the daemon
    // has activated due to background mode
    void daemonActivated();
    // The number of connected clients (GUIs) has decreased to 0, or the daemon
    // has otherwise deactivated
    void daemonDeactivated();

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
                         VPNConnection::State oldState,
                         const ConnectionConfig &connectingConfig,
                         const ConnectionConfig &connectedConfig,
                         const nullable_t<Server> &connectedServer,
                         const nullable_t<Transport> &chosenTransport,
                         const nullable_t<Transport> &actualTransport);
    void vpnError(const Error& error);
    void vpnByteCountsChanged();
    void newLatencyMeasurements(const LatencyTracker::Latencies &measurements);
    void portForwardUpdated(int port);

    // Store new locations built from one of the regions lists and update
    // dependent properties - used by rebuildModernLocations().
    void applyBuiltLocations(LocationsById newLocations,
                             kapps::regions::Metadata metadata);

    // Build the locations list from the modern regions list.  Returns true if
    // the new locations list is not empty, meaning the new data can be cached.
    // The new locations are also applied.
    //
    // regionsObj can be the cached object from DaemonData or new data retrieved
    // (which should then be cached if successful).  Latencies from DaemonData
    // are used.
    bool rebuildModernLocations(const QJsonObject &regionsObj,
                                const QJsonArray &shadowsocksObj,
                                const QJsonObject &metadataObj);

    // Rebuild either the legacy or modern locations from the cached data,
    // depending on the infrastructure setting.  Used when latencies are updated
    // or when initially building the regions list.
    void rebuildActiveLocations();

    // Handle region list results from JsonRefresher
    void shadowsocksRegionsLoaded(const QJsonDocument &shadowsocksRegionsJsonDoc);
    void modernRegionsLoaded(const QJsonDocument &modernRegionsJsonDoc);
    void modernRegionsMetaLoaded(const QJsonDocument &modernRegionsJsonDoc);
    void publicIpLoaded(const QJsonDocument &publicIpDoc);
    void updatePublicIpRefresher (VPNConnection::State state);
    void onNetworksChanged(const std::vector<NetworkConnection> &networks);

    void refreshAccountInfo();
    void applyDedicatedIpJson(const QJsonObject &tokenData, AccountDedicatedIp &dipInfo);
    void applyRefreshedDedicatedIp(const QJsonObject &tokenData, int traceIdx,
                                   std::vector<AccountDedicatedIp> &dedicatedIps);
    void refreshDedicatedIps();
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
                           bool osFailedRequirement,
                           const Update &gaUpdate, const Update &betaUpdate,
                           const std::vector<QString> &flags);
    void onUpdateDownloadProgress(const QString &version, int progress);
    void onUpdateDownloadFinished(const QString &version,
                                  const QString &installerPath);
    void onUpdateDownloadFailed(const QString &version, bool error);

    // Update the port forwarder's enabled state, based on the current setting
    // or the setting value that we connected with
    void updatePortForwarder();

    void traceMemory();

    // Set _state.overridesActive() or _state.overridesFailed() and log
    // appropriately
    void setOverrideActive(const QString &resourceName);
    void setOverrideFailed(const QString &resourceName);

    // Update StateModel::nextConfig following a property change from
    // StateModel, DaemonSettings, or DaemonAccount
    void updateNextConfig();

    void logCommand(const QString &cmd, const QStringList &args);
    // Log the current routing table; used after connecting
    void logRoutingTable();

    // Apply the current matching automation rule's action - used when it has
    // just triggered or when the daemon activates.  If the action has an
    // effect, performs the connect/disconnect and updates
    // automationLastTrigger.  (The action has no effect if we're already in the
    // desired state.)
    void applyCurrentAutomationRule();

    void onAutomationRuleTriggered(const nullable_t<AutomationRule> &currentRule,
                                   Automation::Trigger trigger);

    Error connectVPN(ServiceQuality::ConnectionSource source);
    void disconnectVPN(ServiceQuality::ConnectionSource source);
protected:
    bool _started, _stopping;

    IPCServer* _server;
    QHash<IPCConnection*, ClientConnection*> _clients;
    LocalMethodRegistry* _methodRegistry;
    RemoteNotificationInterface* _rpc;

    VPNConnection* _connection;

    // TODO - We have two representations of basically the same information
    // (the connection configuration derived from the current settings):
    // - ConnectionConfig (vpn.h) - this is a proper model of the connection
    //   configuration with strong invariants
    // - ConnectionInfo (settings.h) - this is a NativeJsonObject model of the
    //   same information with much weaker invariants (most values converted to
    //   strings, etc.)
    //
    // We should get rid of the ConnectionInfo and instead implement specific
    // JSON conversions for ConnectionConfig so we don't need an explicit
    // NativeJsonObject representation, and we can put ConnectionConfig values
    // in StateModel.  In the meantime, the ConnectionConfigs are held here for
    // internal use, and StateModel holds the derived ConnectionInfos.
    ConnectionConfig _connectingConfig;
    ConnectionConfig _connectedConfig;

    DaemonData _data;
    DaemonAccount _account;
    DaemonSettings _settings;
    StateModel _state;

    Environment _environment;
    ApiClient _apiClient;

    LatencyTracker _modernLatencyTracker;
    PortForwarder _portForwarder;
    JsonRefresher _modernRegionRefresher, _modernRegionMetaRefresher,
                  _shadowsocksRefresher, _publicIpRefresher;
    SocksServerThread _socksServer;
    UpdateDownloader _updateDownloader;
    SnoozeTimer _snoozeTimer;
    std::unique_ptr<NetworkMonitor> _pNetworkMonitor;
    Automation _automation;
    // ServiceQuality is created after the data/settings are loaded
    nullable_t<ServiceQuality> _pServiceQuality;

    QSet<QString> _dataChanges;
    QSet<QString> _accountChanges;
    QSet<QString> _settingsChanges;
    std::unordered_set<std::string> _stateChanges;

    unsigned int _pendingSerializations;
    QTimer _serializationTimer;

    QTimer _accountRefreshTimer;
    QTimer _dedicatedIpRefreshTimer;

    QTimer _checkForAppMessagesTimer;
    QTimer _memTraceTimer;

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
