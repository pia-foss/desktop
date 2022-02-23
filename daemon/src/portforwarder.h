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

#include "common.h"
#line HEADER_FILE("portforwarder.h")

#ifndef PORTFORWARDER_H
#define PORTFORWARDER_H

#include <QTimer>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QScopedPointer>
#include <array>
#include "apiclient.h"
#include "async.h"
#include "settings/daemonstate.h"
#include "portforwardrequest.h"

// PortForwarder forwards a port through the VPN connection when the VPN is
// connected and the option is enabled.
//
// Port forwarding is controlled by the DaemonSettings::portForward setting.
// This setting can be changed while the VPN is connected, but disabling it will
// not remove the forwarded port, because there doesn't appear to be any API to
// remove it.
class PortForwarder : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("portforwarder");

public:
    enum class State
    {
        Disconnected,
        // Connected to a server that supports PF.
        ConnectedSupported,
        // Connected to a server that does not support PF (any infra).
        ConnectedUnsupported
    };

    using PortForwardState = DaemonState::PortForwardState;

public:
    // Create PortForwarder with the DaemonAccount, which is used to load/store
    // the PF token.
    //
    // The ApiClient will be used to make API requests, it must outlive
    // PortForwarder.
    //
    // Port forwarding is initially disabled when PortForwarder is created.
    PortForwarder(ApiClient &apiClient, DaemonAccount &account,
                  DaemonState &state, Environment &environment);

public:
    // Update the current VPN connection state.
    // If the VPN becomes connected (to a region that supports PF), and port
    // forwarding is enabled, a new port forward request will be started.
    //
    // (VPNConnection has a number of states, but PortForwarder only cares
    // whether we are connected or not and whether the connected region supports
    // PF.)
    void updateConnectionState(State connectionState);

    // Specify whether port forwarding is currently enabled.
    // If port forwarding becomes enabled, the VPN is already connected, and a
    // port hasn't been forwarded on this connection, a new port forward
    // request will be started.
    void enablePortForwarding(bool enabled);

signals:
    // This signal is emitted whenever the forwarded port changes - it's updated
    // with a positive value when we have forwarded a port, or updated to a
    // special value if no port has been forwarded, the request is in progress,
    // etc.
    void portForwardUpdated(int port);

private:
    // Request a port forward (create a new PortRequester)
    void requestPort();

private:
    ApiClient &_apiClient;
    // Components of Daemon needed by the PF request implementations
    DaemonAccount &_account;
    DaemonState &_state;
    Environment &_environment;
    // Whether the VPN is connected
    State _connectionState;
    // Whether port forwarding is currently enabled
    bool _forwardingEnabled;
    // If we've started a port forwarding request, this is the asynchronous
    // request.  After this request completes, this remains set until the
    // connection goes down (to indicate that we don't need to request a port
    // again).
    //
    // This is never set when _connectionState is not State::ConnectedSupported.
    // (It can be set with _forwardingEnabled is false if the setting was
    // toggled while connected.)
    std::unique_ptr<PortForwardRequest> _pPortForwardRequest;
    // If the port forward request enters the Retry state, we start this timer
    // to retry after a delay.
    QTimer _retryTimer;
};

#endif // PORTFORWARDER_H
