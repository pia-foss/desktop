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
#line HEADER_FILE("vpn.h")

#ifndef CONNECTION_H
#define CONNECTION_H
#pragma once

#include "settings.h"
#include "processrunner.h"
#include "vpnstate.h"

#include <QDateTime>
#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonObject>
#include <QObject>
#include <QFile>
#include <QTimer>

class VPNMethod;

// A descriptor for the desired network adapter (--dev-node) to use.
// Only one subclass of this class (or the class itself) should ever
// be instantiated on any given platform.
class NetworkAdapter
{
public:
    NetworkAdapter(QString devNode) : _devNode(devNode) {}
    virtual ~NetworkAdapter() {}
    virtual QString devNode() const { return _devNode; }
    virtual void setMetricToLowest() { }
    virtual void restoreOriginalMetric() { }
protected:
    QString _devNode;
};

// ResolverRunner runs a local DNS resolver process - either Unbound or
// hnsd (the Handshake resolver).  Both of these are recursive DNS resolvers, so
// they will contact authoritative DNS servers and must be allowed through the
// firewall.
//
// We subclass ProcessRunner to implement the setupProcess() method.
// Doing this allows us to set the process group as "piahnsd", which we need
// to properly manage the resolver firewall rules.  (The group is now also used
// for unbound, but the name has been kept to avoid changing it on user
// systems.)
//
// When running hnsd, ResolverRunner also monitors it to trigger notifications
// if it won't start or doesn't sync any blocks.
class ResolverRunner : public ProcessRunner
{
    Q_OBJECT

public:
    enum class Resolver
    {
        Unbound,
        Handshake,
    };

public:
    ResolverRunner(RestartStrategy::Params restartParams);

signals:
    // Resolver has successfully started.
    void resolverSucceeded(Resolver resolver);
    // Resolver has failed (possibly again), includes the failure time from
    // ProcessRunner (which could be 0 if the failure has just started).
    void resolverFailed(Resolver resolver, std::chrono::milliseconds failureDuration);
    // Indicate whether hnsd is failing to sync (not used for Unbound).
    // - It's failing to sync if it has been running for 5 seconds, but hasn't
    //   synced at least 1 block.
    // - It's not failing to sync once it syncs a block, or if hnsd is disabled
    //   (disconnected / changed DNS, etc.)
    //
    // Other conditions (such as hnsd crashing / failing to start) do not emit
    // this, we keep the current sync failure state.
    void hnsdSyncFailure(bool failing);

public:
    // Change process UID/GID on Linux/MacOS
    void setupProcess(UidGidProcess &process) override;
    bool enable(Resolver resolver, QStringList arguments);
    void disable();

private:
    const Path &getResolverExecutable() const;
    // Only used by Linux - check whether the resolver can bind to low ports
    // (cap_net_bind_service capability)
    bool hasNetBindServiceCapability();

private:
    // Timer used to detect the "hnsd failing to sync" condition - hnsd needs to
    // sync at least 1 block before this timer elapses, or we assume it is not
    // working.
    QTimer _hnsdSyncTimer;
    // The resolver we're currently running when active.  Controls whether we
    // do the hnsd sync timeout, and emitted with success/failure signals.
    Resolver _activeResolver;
};

// ProcessRunner for Shadowsocks - drops UID to 'nobody' on Unix platforms, and
// captures local port number from output.  It does not change the GID, it
// keeps the piavpn GID for firewall exemptions.
class ShadowsocksRunner : public ProcessRunner
{
    Q_OBJECT

public:
    ShadowsocksRunner(RestartStrategy::Params restartParams);

public:
    bool enable(QString program, QStringList arguments);
    virtual void setupProcess(UidGidProcess &process) override;

    // Get the local port - 0 if it hasn't been detected yet since Shadowsocks
    // was started (localPortAssigned() will be emitted in that case).
    quint16 localPort() const {return _localPort;}

signals:
    // The local port has been assigned - happens after the Shadowsocks client
    // is started.
    // Not emitted when localPort() returns to 0 when Shadowsocks is restarted.
    void localPortAssigned();

private:
    // Local port emitted by the Shadowsocks client when starting
    quint16 _localPort;
};

class OriginalNetworkScan
{
public:
    OriginalNetworkScan() = default;    // Required by Q_DECLARE_METATYPE
    OriginalNetworkScan(const QString &gatewayIp, const QString &interfaceName,
                        const QString &ipAddress, const QString &ipAddress6)
        : _gatewayIp{gatewayIp}, _interfaceName{interfaceName}, _ipAddress{ipAddress},
          _ipAddress6{ipAddress6}
    {
    }

    bool operator==(const OriginalNetworkScan& rhs) const
    {
        return gatewayIp() == rhs.gatewayIp() &&
            interfaceName() == rhs.interfaceName() &&
            ipAddress() == rhs.ipAddress();
    }

    bool operator!=(const OriginalNetworkScan& rhs) const
    {
        return !(*this == rhs);
    }

    // Check whether the OriginalNetworkScan has valid (non-empty) values for
    // all fields.
    bool ipv4Valid() const {return !gatewayIp().isEmpty() && !interfaceName().isEmpty() && !ipAddress().isEmpty();}

    // Whether the host has IPv6 available (as a global IP)
    bool hasIpv6() const {return !ipAddress6().isEmpty();}

public:
    void gatewayIp(const QString &value) {_gatewayIp = value;}
    void interfaceName(const QString &value) {_interfaceName = value;}
    void ipAddress(const QString &value) {_ipAddress = value;}
    void ipAddress6(const QString &value) {_ipAddress6 = value;}

    const QString &gatewayIp() const {return _gatewayIp;}
    const QString &interfaceName() const {return _interfaceName;}
    const QString &ipAddress() const {return _ipAddress;}

    // An IP address from the default IPv6 interface.  This may not be the same
    // interface as the default IPv4 interface reported above.
    const QString &ipAddress6() const {return _ipAddress6;}

private:
    QString _gatewayIp, _interfaceName, _ipAddress, _ipAddress6;
};

// Custom logging for OriginalNetworkScan instances
QDebug operator<<(QDebug debug, const OriginalNetworkScan& netScan);

Q_DECLARE_METATYPE(OriginalNetworkScan)

// TransportSelector - used by VPNConnection to select transport settings for
// each attempt.  This implements automatic fallback to other transport settings
// if the selected settings do not work.
//
// Some of this is currently specific to OpenVPN, because OpenVPN has both UDP
// and TCP support - beginAttempt() uses Service::OpenVpnUdp and
// Service::OpenVpnTcp.  Eventually if we have multiple ports and/or multiple
// protocols for WireGuard, this can be generalized.
class TransportSelector
{
    Q_GADGET

public:
    // Initially TransportSelector just has a default preferred setting, it's
    // intended to reset() it before the first connection attempt.
    TransportSelector();

    // Explicitly provide the timeout for preferred transport before starting to try alternate transports.
    TransportSelector(const std::chrono::seconds &transportTimeout);
private:
    // Add alternates to the alternates list.  Skips the preferred transport.
    void addAlternates(const QString &protocol, const DescendingPortSet &ports);

public:
    // Reset TransportSelector for a new connection sequence.
    void reset(const QString &protocol, uint port, bool useAlternates,
               const DescendingPortSet &udpPorts,
               const DescendingPortSet &tcpPorts);

    // Get the preferred transport for the most recently used server (based on
    // the user's selected transport and the server's default port)
    const Transport &lastPreferred() const {return _lastPreferred;}

    // Get the transport that was most recently used
    const Transport &lastUsed() const {return _lastUsed;}

    // Get the local address that was found for the last transport setting (the
    // local address we should use when connecting using that transport).
    // Returns a null QHostAddress if any local address can be used.
    QHostAddress lastLocalAddress() const;

    // Begin a new connection attempt.  Updates lastPreferred(), lastUsed() and
    // lastLocalAddress().  Returns the OpenVPN server that will be used to
    // connect for the current transport.
    //
    // The OriginalNetworkScan is populated with the scan results.  The IP
    // address is always populated; the gateway IP and interface are platform-
    // dependent.
    //
    // delayNext is set to indicate whether to delay before the next attempt if
    // this attempt fails.
    const Server *beginAttempt(const Location &location, const QHostAddress &localAddress,
                               bool &delayNext);

private:
    // The selected transport, based on the user's current settings.  This port
    // may be 0 if "default" is selected.
    Transport _selected;
    // The preferred transport for the server being attempted, and the transport
    // actually being attempted.  These ports are never 0.  _preferredForServer
    // differs from _preferred if the default port was selected, and the
    // resolved port is needed to determine if the actual transport is really
    // different from the user's selection.
    Transport _lastPreferred, _lastUsed;
    std::vector<Transport> _alternates;
    QHostAddress _lastLocalAddress;
    std::size_t _nextAlternate;
    QDeadlineTimer _startAlternates;
    bool _useAlternateNext;
    // Timeout for preferred transport before starting to try alternate transports
    std::chrono::seconds  _transportTimeout;
};

// Holds the configuration details that we provide via DaemonState for the
// last/current connection (connectedLocation, etc.) and the current attempting
// connection (connectingLocation, etc.).
//
// In general, this is _not_ specific to the VPN method being used - the intent
// is that the majority of settings should work with any VPN method.
//
// This is not possible in all cases, so there is some method-specific logic in
// this class, such as forcing some settings off for some methods, knowledge of
// the credentials required for each method, etc.  The intent is that over time,
// these differences should be reduced; we should support most settings with all
// methods.
class ConnectionConfig
{
    Q_GADGET

public:
    enum class Infrastructure
    {
        Current,
        Modern,
    };
    Q_ENUM(Infrastructure);
    enum class Method
    {
        OpenVPN,
        Wireguard,
    };
    Q_ENUM(Method);
    enum class DnsType
    {
        Pia,
        Handshake,
        Local,
        Existing,
        Custom
    };
    Q_ENUM(DnsType);
    enum class ProxyType
    {
        None,
        Custom,
        Shadowsocks
    };
    Q_ENUM(ProxyType);

    static QHostAddress parseIpv4Host(const QString &host);

public:
    ConnectionConfig();
    // Capture the current location and proxy settings from DaemonSettings and
    // DaemonState
    ConnectionConfig(DaemonSettings &settings, DaemonState &state, DaemonAccount &account);

public:
    // Check if the ConnectionConfig was able to load everything required for a
    // connection attempt.
    // If this returns true:
    // - vpnLocation() is valid
    // - If a proxy is being used, socksHost() is valid
    // - If a Shadowsocks proxy is being used, shadowsocksLocation() is valid
    //   and has Shadowsocks service configuration
    bool canConnect() const;

    // Check if the configuration has changed between two ConnectionConfigs.
    // This is different from operator==(), because it compares locations using
    // ID only - other location metadata changes are not considered a change.
    bool hasChanged(const ConnectionConfig &other) const;

    // The effective infrastructure.  Derived from the infrastructure() setting;
    // "default" is replaced with an actual value.
    Infrastructure infrastructure() const {return _infrastructure;}

    // The effective VPN method.  Normally the method() setting; may be forced
    // to OpenVPN if an auth token is not available for WireGuard.
    Method method() const {return _method;}

    // Enabled if the method was forced to OpenVPN due to lack of an auth token
    // (causes an in-client notification).
    bool methodForcedByAuth() const {return _methodForcedByAuth;}

    // The effective VPN location, and whether that location was an 'auto'
    // selection or an explicit selection from the user.
    const QSharedPointer<Location> &vpnLocation() const {return _pVpnLocation;}
    bool vpnLocationAuto() const {return _vpnLocationAuto;}

    // Credentials for authenticating to the VPN.
    //
    // The VPN username and password are always available.  (These are the
    // split token returned by the web API if a token exchange was possible, or
    // the actual account credentials otherwise.)
    //
    // A token is only available if a token exchange had occurred.  (If this is
    // not available, WireGuard will not be allowed as a method, OpenVPN will be
    // used instead.)
    const QString &vpnUsername() const {return _vpnUsername;}
    const QString &vpnPassword() const {return _vpnPassword;}
    const QString &vpnToken() const {return _vpnToken;}

    // The type of DNS selected by the user.
    DnsType dnsType() const {return _dnsType;}
    // For DnsType::Custom, the DNS server addresses - guaranteed to have at
    // least one entry for Custom.  Not provided for any other type.
    const QStringList &customDns() const {return _customDns;}

    // Get the effective DNS server addresses to use.
    //
    // For the modern infrastructure, this applies MACE (by selecting a
    // different PIA DNS address).  (piaLegacyDnsOverride is not used.)
    //
    // For the legacy infrastructure, MACE is not applied, and the PIA DNS
    // server list may optionally be overridden with piaLegacyDnsOverride (if
    // the method provides DNS server addresses from the server - WireGuard).
    QStringList getDnsServers(const QStringList &piaLegacyDnsOverride) const;

    // Get the effective DNS server addresses to use for the modern
    // infrastructure.  This does depend on MACE, which simply selects a
    // different DNS address.
    QStringList getModernDnsServers() const;

    // Cryptographic settings for OpenVPN; only captured when method is OpenVPN
    QString openvpnCipher() const {return _openvpnCipher;}
    QString openvpnAuth() const {return _openvpnAuth;}
    QString openvpnServerCertificate() const {return _openvpnServerCertificate;}

    quint16 localPort() const {return _localPort;}
    uint mtu() const {return _mtu;}

    // Whether to use the VPN as the default route.  Turning off the default
    // route requires split tunnel - if split tunnel was not enabled when
    // connecting, this will be true.  Split tunnel is only permitted with
    // OpenVPN.
    bool defaultRoute() const {return _defaultRoute;}

    // Proxy information.  canConnect() checks requirements of these fields
    // based on the selected proxy type.  Proxies are only permitted with
    // OpenVPN.
    ProxyType proxyType() const {return _proxyType;}
    const QHostAddress &socksHost() const {return _socksHostAddress;}
    const CustomProxy &customProxy() const {return _customProxy;}
    const QSharedPointer<Location> &shadowsocksLocation() const {return _pShadowsocksLocation;}
    bool shadowsocksLocationAuto() const {return _shadowsocksLocationAuto;}

    // Whether to try alternate transports.  Requires OpenVPN and no proxy.
    bool automaticTransport() const {return _automaticTransport;}

    // Whether port forwarding is enabled (even if the selected region does not
    // support it).  Only permitted with OpenVPN.
    bool requestPortForward() const {return _requestPortForward;}

    // Whether MACE is enabled.  Only permitted with PIA DNS and OpenVPN.
    bool requestMace() const {return _requestMace;}

    // For the WireGuard method only, whether to use kernel support if available
    bool wireguardUseKernel() const {return _wireguardUseKernel;}

private:
    // The infrastructure in use.  This affects the way MACE, port forwarding,
    // etc. are applied.
    Infrastructure _infrastructure;
    // The method used to connect to the VPN
    Method _method;
    bool _methodForcedByAuth;

    // The VPN location used for this connection
    QSharedPointer<Location> _pVpnLocation;
    // Whether the VPN location was an automatic selection
    bool _vpnLocationAuto;

    // The credentials to use for this connection.  Some methods use the split
    // "OpenVPN username/password", others use the complete token.
    QString _vpnUsername, _vpnPassword, _vpnToken;

    // Selected DNS type and custom DNS servers
    DnsType _dnsType;
    QStringList _customDns;

    QString _openvpnCipher, _openvpnAuth, _openvpnServerCertificate;

    quint16 _localPort;
    uint _mtu;

    // Whether to use the VPN as the default route for this connection.  This
    // isn't exactly the same as the defaultRoute value at the time of
    // connection - it also depends on whether split tunnel is enabled.
    // We don't store the split tunnel setting though - the user can still
    // toggle that while connected, but if it changes the defaultRoute value,
    // we will require a reconnect.
    bool _defaultRoute;

    // The proxy type used for this connection
    ProxyType _proxyType;
    // The parsed address for the SOCKS proxy host - used to connect and to
    // interpret some errors from OpenVPN.
    QHostAddress _socksHostAddress;
    // Custom proxy configuration; meaningful if a custom proxy was selected.
    CustomProxy _customProxy;
    // Shadowsocks server location; meaningful if a Shadowsocks proxy was
    // selected.
    QSharedPointer<Location> _pShadowsocksLocation;
    // Whether the Shadowsocks location was an automatic selection
    bool _shadowsocksLocationAuto;

    // Flags for additional features for this connection
    bool _automaticTransport, _requestPortForward, _requestMace;

    bool _wireguardUseKernel;
};

class VPNConnection : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("vpn")

public:
    using State = VpnState::State;

    // The steps required to establish a connection.  "ConnectingOpenVPN" is
    // where most of the work happens, but in some cases there are steps
    // beforehand.
    // This is meaningful only in a Connecting or Reconnecting state.  It's
    // reset to Initializing whenever we schedule a call to doConnect().
    enum ConnectionStep
    {
        // Haven't done anything yet, starting a new attempt
        Initializing,
        // Fetching non-VPN IP, only done for first connection attempt with the
        // current settings
        FetchingIP,
        // Starting proxy, only done when starting up Shadowsocks client with
        // ephemeral port
        StartingProxy,
        // OpenVPN has been started and is connecting
        ConnectingOpenVPN,
    };
    Q_ENUM(ConnectionStep)

    using Limits = VpnState::Limits;

public:
    explicit VPNConnection(QObject* parent = nullptr);

    State state() const { return _state; }
    quint64 bytesReceived() const { return _receivedByteCount; }
    quint64 bytesSent() const { return _sentByteCount; }
    const QList<IntervalBandwidth> &intervalMeasurements() const {return _intervalMeasurements;}
    void activateMACE ();

    bool needsReconnect();
    // Get the current VPN method, if one exists - for updating firewall params
    // specified by the VPN method.  If a valid method is returned, it remains
    // valid at least until any mutating member of VPNConnection is called.
    VPNMethod *vpnMethod() const {return _method;}
    const QStringList &effectiveDnsServers() const {return _effectiveDnsServers;}

    // Update the current network in the VPNMethod when it has changed.
    void updateNetwork(const OriginalNetworkScan &newNetwork);
    void scheduleMaceDnsCacheFlush();

public slots:
    void connectVPN(bool force);
    void disconnectVPN();

private:
    void beginConnection();
    // Whether to use slow reconnect intervals based on the current attempt
    // count
    bool useSlowInterval() const;
    void doConnect();
    void vpnMethodStateChanged();
    void raiseError(const Error& error);

signals:
    void error(const Error& error);
    // Indicates whether VPNConnection is using the "slow" attempt interval
    // while attempting a connection.  Enabled when enough attempts have been
    // made to trigger the slow intervals.  Resets to false when going to a
    // non-connecting state, or when the settings change during a series of
    // connection attempts.
    void slowIntervalChanged(bool usingSlowInterval);
    // When VPNConnection's state changes, the new connecting/connected
    // configurations are provided.
    // Note that the Locations in these configurations are mutable, but
    // slots receiving this signal *must not* modify them...they're mutable to
    // work with JsonField-implemented fields in DaemonState.
    // In the Connected state only, chosenTransport and actualTransport are
    // provided.
    void stateChanged(State state,
                      const ConnectionConfig &connectingConfig,
                      const ConnectionConfig &connectedConfig,
                      const nullable_t<Server> &connectedServer,
                      const nullable_t<Transport> &chosenTransport,
                      const nullable_t<Transport> &actualTransport);
    void firewallParamsChanged();
    // The total sent/received bytecounts and the interval measurements have
    // changed.
    void byteCountsChanged();
    // Signals forwarded from ResolverRunner for each resolver
    void unboundSucceeded();
    void unboundFailed(std::chrono::milliseconds failureDuration);
    void hnsdSucceeded();
    void hnsdFailed(std::chrono::milliseconds failureDuration);
    void hnsdSyncFailure(bool failing);
    void usingTunnelConfiguration(const QString &deviceName,
                                  const QString &deviceLocalAddress,
                                  const QString &deviceRemoteAddress,
                                  const QStringList &effectiveDnsServers);

private:
    void updateAttemptCount(int newCount);
    void setState(State state);
    void updateByteCounts(quint64 received, quint64 sent);
    void scheduleNextConnectionAttempt();
    void queueConnectionAttempt();
    // Copy settings to begin a connection attempt.  If the settings are loaded
    // successfully, the state is updated to successState and this returns true.
    //
    // If the settings cannot be loaded (one or more required locations could
    // not be found), it instead transitions to failureState and returns false.
    // _connectingConfig is cleared in this case.
    bool copySettings(State successState, State failureState);

private:
    State _state;
    ConnectionStep _connectionStep;
    // The most recent VPNMethod.  This can be set in any state, including
    // Disconnected, where it may still refer to the process used for the last
    // connection.
    VPNMethod* _method;
    // Runner for hnsd process, enabled when we connect with Handshake DNS.
    ResolverRunner _resolverRunner;
    // Runner for ss-local process, enabled when we connect with a Shadowsocks
    // proxy.
    ShadowsocksRunner _shadowsocksRunner;
    // Stored settings as of last/current connection.  These are valid in any
    // state, they are the settings that will be used for the next connection
    // (even in the Connected state; they'll be applied when a reconnect occurs)
    QJsonObject _connectionSettings;
    // The effective DNS servers once they've been applied by the VPN method
    QStringList _effectiveDnsServers;
    // The configuration we are currently attempting to connect with.
    ConnectionConfig _connectingConfig;
    // The configuration that we last connected with.
    ConnectionConfig _connectedConfig;

    // The connecting / connected servers.  These are stored like
    // _connectingConfig/_connectedConfig, but only the connected server is
    // reported by state changes, and only while connected.  _connectingServer
    // is updated for each connection attempt (unlike _connectingConfig)
    Server _connectingServer, _connectedServer;

    // Timer for ensuring a minimum interval between connection attempts
    QDeadlineTimer _timeUntilNextConnectionAttempt;
    // Timer to throttle connection attempts.  Used when a connection attempt is
    // being delayed until _timeUntilNextConnectionAttempt elapses.  This is
    // used in any of the Connecting/Reconnecting states, it does not run in any
    // other state.
    QTimer _connectTimer;
    // Number of connection attempts performed for this connection.  This can be
    // nonzero in any Connecting/Reconnecting state; in any other state it is
    // zero.
    int _connectionAttemptCount;
    TransportSelector _transportSelector;
    // When connecting with Shadowsocks, this is the server IP selected.
    // VPNConnection selects a server when starting the proxy, but it passes the
    // IP to the VPNMethod to set up routes.
    QHostAddress _shadowsocksServerIp;
    // Accumulated received/sent traffic over this connection. This includes
    // all traffic, even across multiple OpenVPN processes.
    quint64 _receivedByteCount, _sentByteCount;
    // Last traffic counts received from the current OpenVPN process
    quint64 _lastReceivedByteCount, _lastSentByteCount;
    // Interval measurements for the current OpenVPN process
    QList<IntervalBandwidth> _intervalMeasurements;
    // Cached value if we already determined we need a reconnect to apply settings
    bool _needsReconnect;
};

#endif // CONNECTION_H
