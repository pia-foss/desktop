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
#line HEADER_FILE("vpnmethod.h")

#ifndef VPNMETHOD_H
#define VPNMETHOD_H

#include "vpn.h"
#include <QObject>

// VPNMethod is an interface to a particular method of connecting to the VPN,
// such as OpenVPN, WireGuard, or any future method.
//
// VPNConnection creates a VPNMethod implementation for each connection attempt.
// The VPNMethod implementation should run through one connection attempt and
// emit the appropriate state changes, or any errors, etc.  The implementation
// also provides some context about the connection, such as the tunnel device
// and IP addresses.
//
// VPNMethod implementations may have internal retry strategies for parts of the
// connection, but any failure condition that requires restarting from scratch
// should be handled by VPNConnection - the VPNMethod should just exit in that
// case.
class VPNMethod : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("vpnmethod")

public:
    // The VPN connection proceeds through several states as it is attempted,
    // established, and terminates.
    //
    // The VPN method does not distinguish different types of exits - like
    // connection lost, intentional shutdown, etc.  Instead, VPNConnection
    // determines this based on what it expected VPNMethod to do - any exit that
    // wasn't initiated by VPNConnection is unexpected, etc.
    //
    // (This is more robust than having the individual method indicate different
    // exits.  For example, if VPNConnection intends to shut down the
    // connection, but then an unexpected connection loss happens at the same
    // time, that's fine, just let the connection die.  Or, if VPNMethod
    // errantly thought that a disconnect was intentional, but VPNConnection
    // does not, VPNConnection will correctly re-establish the connection.)
    //
    // VPNMethod only proceeds from earlier states to later states, it can never
    // go back to an earlier state.  However, it can skip states, such as
    // proceeding from Connecting to Exiting (if the connection failed), or
    // from Connecting directly to Exited if a child process crashed, etc.
    enum State
    {
        // The VPNMethod has just been created.  run() will be called to
        // transition to Connecting.
        // (VPNConnection does not generally care about this state vs.
        // Connecting, but the VPNMethod can use the difference for sanity
        // checks.)
        Created,
        // run() has been called and the connection is being attempted.  If the
        // connection succeeds, goes to Connected, otherwise goes to Exiting
        // or Exited.
        Connecting,
        // The connection has been established.
        Connected,
        // The connection (or attempted connection) is being shut down.
        Exiting,
        // The VPNMethod has exited, its work is complete.
        Exited,
    };
    Q_ENUM(State)

public:
    explicit VPNMethod(QObject *pParent, const OriginalNetworkScan &netScan);
    virtual ~VPNMethod() = default;

public:
    State state() const {return _state;}

    // Start the connection attempt.  Can only be called in the Created state.
    //
    // Provides the following information to establish the connection:
    // - connectingConfig - The connection configuration, including the relevant
    //   settings, state, and auth details captured at the time the connection
    //   attempt was started by VPNConnection
    // - vpnServer - A Server selected by VPNConnection for this attempt.
    //   VPNConnection selects a server that has the appropriate service
    //   required by this VPN method (OpenVpnTcp/OpenVpnUdp/WireGuard)
    // - transport - A Transport chosen by TransportSelector
    // - localAddress - A local address chosen by TransportSelector (or a null
    //   QHostAddress if any local address can be used)
    //
    // The following are also provided but should probably be refactored into
    // ConnectionConfig (or somewhere):
    // - Shadowsocks server address (when SS is used, indicates the specific
    //   server chosen from the Shadowsocks location)
    // - Shadowsocks proxy port (when SS is used)
    //
    // run() can also throw errors; these are handled as if raised with
    // raiseError().
    virtual void run(const ConnectionConfig &connectingConfig,
                     const Server &vpnServer,
                     const Transport &transport,
                     const QHostAddress &localAddress,
                     const QHostAddress &shadowsocksServerAddress,
                     quint16 shadowsocksProxyPort) = 0;

    // Shut down the connection.  VPNMethod should (eventually) transition to
    // Exiting/Exited (or directly to Exited).  This may be called more than
    // once.
    virtual void shutdown() = 0;

    // Get the network adapter that's being used for this connection.  Used on
    // Windows and Linux to implement firewall rules.
    //
    // For platforms that use this value, it must be available during the
    // Connected state (it should become available before advancing to that
    // state).  It must only be provided when the device actually exists (do not
    // provide a NetworkAdapter referring to tun0 when tun0 does not exist,
    // etc.)
    //
    // - Windows - Must be a WinNetworkAdapter, the device LUID is used in the
    //   firewall implementation.
    // - Mac - Not used.
    // - Linux - devNode() is used in the firewall implementation.
    virtual std::shared_ptr<NetworkAdapter> getNetworkAdapter() const = 0;

    // Get the current original network scan.  If this changes, networkChanged()
    // is called.  The network scan is not necessarily valid (such as if the
    // network connection was just lost).
    const OriginalNetworkScan &originalNetwork() const {return _netScan;}

    // Update the original network scan - calls networkChanged() if it changes.
    // Used by VpnConnection when the network scan is updated.
    void updateNetwork(const OriginalNetworkScan &newNetwork);

protected:
    // Advance the state.  Updates _state and emits stateChanged().
    // Can be called with the current state (no effect), but VPNMethod cannot
    // return to a prior state.
    void advanceState(State newState);

    // Emit the tunnel configuration, including the tunnel device information
    // and the DNS servers that will be (or have been) applied.
    //
    // This must be done in the Connecting state, and methods must not advance
    // to the Connected state without having emitted this information (in
    // particular, this information is used for the web API proxy, and for
    // Handshake).
    //
    // On Windows, deviceName and deviceRemoteAddress are not required.
    void emitTunnelConfiguration(const QString &deviceName,
                                 const QString &deviceLocalAddress,
                                 const QString &deviceRemoteAddress,
                                 const QStringList &effectiveDnsServers);

    // Emit new byte counts.  This should be provided every 5 seconds once
    // bytecounts become available (usually sometime in the Connecting state).
    void emitBytecounts(quint64 received, quint64 sent);

    // If the firewall parameters specified by VPNMethod change, call this to
    // trigger a firewall update.
    void emitFirewallParamsChanged();

    // Raise an error.  This can be done in any state.
    // This will cause VPNConnection to end the connection attempt.  If the
    // state is not Exited, it will call shutdown.  (If the state is Exited
    // already, it may or may not call shutdown().)
    void raiseError(const Error &err);

private:
    // The network state returned by originalNetwork() has changed.  Override
    // this to update routes, or abort the connection, etc.
    // This can be called in any state, the implementation should check the
    // current state.
    virtual void networkChanged() = 0;

signals:
    void stateChanged();
    void tunnelConfiguration(QString deviceName, QString deviceLocalAddress,
                             QString deviceRemoteAddress,
                             const QStringList &effectiveDnsServers);
    void bytecount(quint64 received, quint64 sent);
    void firewallParamsChanged();
    void error(const Error &err);

private:
    State _state;
    OriginalNetworkScan _netScan;
};

#endif
