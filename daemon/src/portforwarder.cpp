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
#line SOURCE_FILE("portforwarder.cpp")

#include "portforwarder.h"
#include "testshim.h"
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <chrono>

namespace
{
    std::chrono::seconds pfRetryDelay{5};
}

PortForwarder::PortForwarder(ApiClient &apiClient, DaemonAccount &account,
                             DaemonState &state, Environment &environment)
    : _apiClient{apiClient},
      _account{account},
      _state{state},
      _environment{environment},
      _connectionState{State::Disconnected},
      _forwardingEnabled{false},
      _pPortForwardRequest{}
{
    _retryTimer.setInterval(msec(pfRetryDelay));
    _retryTimer.setSingleShot(true);
    connect(&_retryTimer, &QTimer::timeout, this, &PortForwarder::requestPort);
}

void PortForwarder::updateConnectionState(State connectionState)
{
    if(connectionState == _connectionState)
        return; // No change, nothing to do

    if(connectionState == State::ConnectedSupported)
    {
        // Invariant - cleared in any state other than ConnectedSupported
        Q_ASSERT(!_pPortForwardRequest);
        Q_ASSERT(!_retryTimer.isActive());
        _connectionState = State::ConnectedSupported;
        // The connection is established, so request a port if forwarding is enabled
        if(_forwardingEnabled)
            requestPort();
    }
    else
    {
        // The connection was lost, or we've connected to a region that doesn't
        // support PF
        _connectionState = connectionState;
        // If a request is in progress, abort it.  If the request had completed,
        // the forwarded port has been lost.
        _pPortForwardRequest.reset();
        _retryTimer.stop();

        // If a port had been forwarded, it's gone now
        if(!_forwardingEnabled || connectionState == State::Disconnected)
            emit portForwardUpdated(PortForwardState::Inactive);
        else
            emit portForwardUpdated(PortForwardState::Unavailable);
    }
}

void PortForwarder::enablePortForwarding(bool enabled)
{
    if(_forwardingEnabled == enabled)
        return; // Nothing to do

    _forwardingEnabled = enabled;

    switch(_connectionState)
    {
    default:
    case State::Disconnected:
        break;
    case State::ConnectedSupported:
        // Nothing happens, stay in the Inactive state.  We have to reconnect to
        // request a port now that the setting is enabled.  (It's also possible
        // that we have already requested a port if the setting was toggled off
        // and back on while connected, keep the port in that case.)
        break;
    case State::ConnectedUnsupported:
        // If we're connected to a region that doesn't support port forwarding,
        // update the state to either Inactive or Unavailable.  (This shows or
        // hides the notice in the client.)  Do this dynamically (unlike in the
        // ConnectedSupported state where we continue to show the port once it's
        // forwarded even if the setting is turned off).

        // Class invariant - can only be set in ConnectedSupported state.
        // (Guarantees that we're not about to overwrite a valid port forward.)
        Q_ASSERT(!_pPortForwardRequest);
        Q_ASSERT(!_retryTimer.isActive());
        emit portForwardUpdated(_forwardingEnabled ? PortForwardState::Unavailable : PortForwardState::Inactive);
        break;
    }
}

void PortForwarder::requestPort()
{
    // Use the appropriate implementation for the current infrastructure.
    // DaemonState::connectedConfig will be set at this point since we are
    // currently connected - Daemon updates the state if we lose the connection.
    // If, somehow, we were to try to do a PF request while not connected, this
    // would just default to the modern implementation.
    _pPortForwardRequest.reset(
        new PortForwardRequestModern{_apiClient, _account, _state, _environment});

    emit portForwardUpdated(PortForwardState::Attempting);
    connect(_pPortForwardRequest.get(), &PortForwardRequest::stateUpdated,
            this, [this](PortForwardRequest::State state, int port)
            {
                switch(state)
                {
                    case PortForwardRequest::State::Success:
                        emit portForwardUpdated(port);
                        break;
                    case PortForwardRequest::State::Retry:
                        qInfo() << "PF request signaled Retry state, will retry after delay";
                        // Go back to the Attempting state
                        emit portForwardUpdated(PortForwardState::Attempting);
                        // Retry after a delay
                        _retryTimer.start();
                        break;
                    default:
                    case PortForwardRequest::State::Failed:
                        qInfo() << "PF request signaled failure";
                        emit portForwardUpdated(PortForwardState::Failed);
                        break;
                }
            });
}
