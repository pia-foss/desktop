// Copyright (c) 2019 London Trust Media Incorporated
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
#include "settings.h"

// PortRequester manages one attempt to request a port forward through the VPN.
// PortRequester is created by PortForwarder when the VPN is connected and port
// forwarding is enabled.
//
// PortRequester implements the API request to forward a port, and it retries
// the request a few times if it fails.
class PortRequester : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("portforwarder");

public:
    // PortRequester initiates the request to forward a port when it's created.
    // The portForwardComplete signal will be emitted when the request completes
    // (either successfully or unsuccessfully).
    PortRequester(const QUrl &requestUrl);

signals:
    // When the request completes, this event is emitted.  If the request was
    // successful, the forwarded port is provided.  If all retries failed, the
    // signal is emitted with port == 0.
    void portForwardComplete(int port);

private:
    // Begin another request if another attempt can be made, or signal failure
    // if all attempts have failed.
    void beginNextAttempt();

    // Read the forwarded port from the reply body.  If the request failed, this
    // returns 0, otherwise it returns the port number.
    int readForwardReply(QNetworkReply &reply);

    // Handle a reply to the forward request.
    void forwardReplyFinished(QNetworkReply &reply);

private:
    // Number of attempts that have occurred
    int _attemptCount;
    // Definition of the request for a port forward
    QNetworkRequest _forwardRequest;
    // Timeout for each attempt
    QTimer _requestTimeout;
    // Manager for network requests.  Dynamically allocated so it can be mocked
    // in unit tests.
    QScopedPointer<QNetworkAccessManager> _pNetworkManager;
};

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
        ConnectedSupported,
        ConnectedUnsupported
    };

    using PortForwardState = DaemonState::PortForwardState;

public:
    // Create PortForwarder with the client ID string (see
    // DaemonAccount::clientId)
    // Port forwarding is initially disabled when PortForwarder is created.
    PortForwarder(QObject *pParent, const QString &clientId);

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
    void portForwardUpdated(int port, bool needsReconnect=false);

private:
    // Request a port forward (create a new PortRequester)
    void requestPort();

private:
    // URL we request to create a port forward
    QUrl _requestUrl;
    // Whether the VPN is connected
    State _connectionState;
    // Whether port forwarding is currently enabled
    bool _forwardingEnabled;
    // If we've started a port forwarding request, this is the PortRequester
    // managing that request.  After this request completes, this remains set
    // until the connection goes down (to indicate that we don't need to request
    // a port again).
    //
    // This is never set when _connectionState is not State::ConnectedSupported.
    // (It can be set with _forwardingEnabled is false if the setting was
    // toggled while connected.)
    QScopedPointer<PortRequester> _pCurrentRequest;
};

// The client ID we use for requests - a 256-bit number encoded in Base-36
// TODO - This needs to be persisted
class ClientId
{
private:
    static const quint32 encodeBase = 36;
    // 36^6 - the largest number of base36 digits that fits into a 32-bit
    // word, ~31 bits
    static constexpr quint32 groupLimit = encodeBase*encodeBase*encodeBase *
                                          encodeBase*encodeBase*encodeBase;
    // Characters used to encode the digits
    static const QChar encodeChars[encodeBase];

public:
    using IdBits = std::array<quint32, 8>;

    // Test if a QString is a valid client ID.
    static bool isValidId(const QString &id);

public:
    // Initialize a ClientId with a random value
    ClientId();

    // Initialize a ClientId with a specified value
    ClientId(IdBits idNum);

private:
    // Initialize ClientId from a numeric value
    // idNum is zeroed by this method
    void initialize(IdBits &idNum);

    // Divide one of the words of IdBits by groupLimit, carry in the remainder
    // from the prior word's division, and return the carry out to the next
    // word
    quint32 dividePart(quint32 &divideWord, quint32 carryIn);

    // Extract the next 6 base36 digits from idNum
    quint32 extractNextGroup(IdBits &idNum);

    // Extract and encode one base36 digit
    // value is divided by 36, and pOutPos is backed up by 1 character
    void encodeBase36(quint32 &value, QChar *&pOutPos);

    // Encode a group of 6 base36 digits
    // pOutPos is backed up by 6 places
    void encodeBase36Group(quint32 value, QChar *&pOutPos);

    // Extract and encode the next group of 6 base36 digits from IdBits.
    // pOutPos is backed up by 6 places.
    void encodeNextGroup(IdBits &idNum, QChar *&pOutPos);

public:
    const QString &id() const {return _id;}

private:
    QString _id;
};

#endif // PORTFORWARDER_H
