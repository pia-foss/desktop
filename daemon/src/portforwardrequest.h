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
#line HEADER_FILE("portforwardrequest.h")

#ifndef PORTFORWARDREQUEST_H
#define PORTFORWARDREQUEST_H

#include "settings/daemonaccount.h"
#include "settings/daemonstate.h"
#include "apiclient.h"

// PortForwardRequest is the interface to the port forwarding implementation.
// Historically there was an alternate implementation for the legacy
// infrastructure - now this is just implemented by PortForwardRequestModern.
//
// PortForwardRequest should start a port forwarding attempt as soon as it is
// created.  PortForwarder will set the current state to 'Attempting' after
// creating a PortForwardRequest.
//
// PortForwardRequest can then update with one of the following states:
// - State::Success (provide the port number)
// - State::Failed (request failed - do not retry)
// - State::Retry (abandon this request and try again)
//
// After reporting State::Success, the PortForwardRequest can later update to
// State::Failed or State::Retry if the forwarded port is lost.  No updates
// should be sent after reaching State::Failed or State::Retry.
class PortForwardRequest : public QObject
{
    Q_OBJECT

public:
    // State values for stateUpdated()
    enum class State
    {
        Success,
        Retry,
        Failed,
    };

public:
    virtual ~PortForwardRequest() = default;

signals:
    void stateUpdated(State state, int port);
};


// This is the modern infrastructure implementation of PortForwardRequest.
// - This uses the portForwardPayload and portForwardSignature from
//   DaemonAccount (collectively, the "PF token").
// - If a PF token already exists, and it won't expire soon, this attempts to
//   reuse that token.  If this fails, it will clear the token and go to the
//   Retry state to trigger a new request to attempt to obtain a new token.
// - Otherwise, it will attempt to obtain a new token, and then bind that port
//   on the current server.  If this fails, it will go to the Failed state - no
//   retry is possible.
// - bindPort will be called periodically to keep the port bound.  If this
//   fails, it will wipe the token and go to the Retry state to attempt to
//   allocate and bind a new port.
class PortForwardRequestModern : public PortForwardRequest
{
    Q_OBJECT

private:
    struct PfToken
    {
        QString _payload, _signature;
    };

public:
    PortForwardRequestModern(ApiClient &apiClient, DaemonAccount &account,
                             DaemonState &state, const Environment &environment);

private:
    // Parse the PF payload from DaemonAccount into a JSON document.
    QJsonDocument parsePfPayload();

    // Check whether DaemonAccount holds a valid PF token, and if so, if it will
    // expire soon.
    //
    // Returns true if there is a valid PF token in DaemonAccount.  Returns
    // false otherwise (the token is not valid or will expire soon, and should
    // be discarded).
    bool checkAccountPfToken();

    PfToken handleGetSigResult(const QJsonDocument &result);

    // Retry with a new token if allowed by _canRetry (emit the retry state, and
    // let PortForwarder retry).  Otherwise, emit the failed state, since no
    // retry is possible.
    void retryNewToken();

    // Bind the port to the current server.  If this was the initial bind, it
    // also starts the recurring bind timer and emits the Success state with the
    // bound port.
    void bindWithToken(const PfToken &token, bool initial);

private:
    ApiClient &_apiClient;
    DaemonAccount &_account;
    FixedApiBase _pfApiBase;
    QString _gateway;
    // Whether we are able to retry with a new token if the request fails.
    // If we just obtained a new token and it fails immediately, we won't retry;
    // an old token or a token that fails after being bound for a while can be
    // retried.
    bool _canRetry;
    // Timer for recurring bind requests
    QTimer _bindTimer;
};

#endif
