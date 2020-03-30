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
#include "apiclient.h"
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <chrono>

PortForwarder::PortForwarder(QObject *pParent, const QString &clientId)
    : QObject{pParent},
      _clientId{clientId},
      _connectionState{State::Disconnected},
      _forwardingEnabled{false},
      _pPortForwardRequest{}
{
}

void PortForwarder::updateConnectionState(State connectionState)
{
    if(connectionState == _connectionState)
        return; // No change, nothing to do

    if(connectionState == State::ConnectedSupported)
    {
        // Invariant - cleared in any state other than ConnectedSupported
        Q_ASSERT(!_pPortForwardRequest);
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
        _pPortForwardRequest.abandon();

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
        emit portForwardUpdated(_forwardingEnabled ? PortForwardState::Unavailable : PortForwardState::Inactive);
        break;
    }
}

void PortForwarder::requestPort()
{
   _pPortForwardRequest = ApiClient::instance()
                ->getForwardedPort(QStringLiteral("?client_id=%1").arg(_clientId))
                ->then(this, [this](const QJsonDocument& json) {
                    const auto &port = json[QStringLiteral("port")].toInt();
                    emit portForwardUpdated(port ? port : PortForwardState::Failed);
                })
                ->except(this, [this](const Error& err) {
                    qWarning() << "Couldn't request port forward due to error:" << err;
                    emit portForwardUpdated(PortForwardState::Failed);
                });

   emit portForwardUpdated(PortForwardState::Attempting);
}

// The existing client uses lowercase characters for this encoding
const QChar ClientId::encodeChars[encodeBase]
{
    '0','1','2','3','4','5','6','7','8','9','a','b',
    'c','d','e','f','g','h','i','j','k','l','m','n',
    'o','p','q','r','s','t','u','v','w','x','y','z'
};

bool ClientId::isValidId(const QString &id)
{
    if(id.size() != 50)
        return false;

    auto isIdChar = [](QChar c)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z');
    };
    return std::all_of(id.begin(), id.end(), isIdChar);
}

ClientId::ClientId()
{
    IdBits idNum;
    auto &generator{*QRandomGenerator::system()};
    generator.generate(idNum.begin(), idNum.end());

    initialize(idNum);
}

ClientId::ClientId(IdBits idNum)
{
    initialize(idNum);
}

void ClientId::initialize(IdBits &idNum)
{
    // A 256-bit number requires 50 base36 digits to represent, but we
    // encode in 6-digit groups, so use a buffer of 54 characters.  The
    // first 4 characters will always end up being 0.
    QChar encodeBuf[54]{};
    // Start from the last position
    QChar *pOutPos = &encodeBuf[53];
    // Encode the number to 54 base36 digits
    encodeNextGroup(idNum, pOutPos);
    encodeNextGroup(idNum, pOutPos);
    encodeNextGroup(idNum, pOutPos);
    encodeNextGroup(idNum, pOutPos);
    encodeNextGroup(idNum, pOutPos);
    encodeNextGroup(idNum, pOutPos);
    encodeNextGroup(idNum, pOutPos);
    encodeNextGroup(idNum, pOutPos);
    encodeNextGroup(idNum, pOutPos);
    // At this point, idNum should be all-0
    Q_ASSERT(std::all_of(idNum.begin(), idNum.end(),
                         [](quint32 word){return word == 0;}));
    // The first 4 characters should be '0'
    Q_ASSERT(encodeBuf[0] == '0' && encodeBuf[1] == '0' &&
             encodeBuf[2] == '0' && encodeBuf[3] == '0');
    // pOutPos should point to the position right before the beginning
    // of encodeBuf
    Q_ASSERT(pOutPos == std::prev(&encodeBuf[0]));

    //Use the 50 meaningful digits to set id
    _id = QString{&encodeBuf[4], 50};
}

// Divide one of the words of IdBits by groupLimit and carry in the
// remainder from the next word's division
quint32 ClientId::dividePart(quint32 &divideWord, quint32 carryIn)
{
    // carryIn is necessarily less than groupLimit since it came from a prior
    // division
    Q_ASSERT(carryIn < groupLimit);

    // Combine the carry with this word.  Use a 64-bit intermediate.
    quint64 dividend = carryIn;
    dividend <<= 32;
    dividend += divideWord;

    // Find the carry out
    quint64 carryOut = dividend % groupLimit;
    // This is necessarily a valid 32-bit integer since groupLimit is less than
    // 2^32.
    Q_ASSERT(carryOut <= std::numeric_limits<quint32>::max());

    // Find the quotient
    quint64 quotient = dividend / groupLimit;
    // Since dividend is at most (2^32 * groupLimit - 1), the quotient is less
    // than 2^32.
    Q_ASSERT(quotient <= std::numeric_limits<quint32>::max());

    divideWord = static_cast<quint32>(quotient);
    return static_cast<quint32>(carryOut);
}

// Extract the next 6 base36 digits from idNum
quint32 ClientId::extractNextGroup(IdBits &idNum)
{
    quint32 carry;
    carry = dividePart(idNum[0], 0); // No carry into the most significant word
    carry = dividePart(idNum[1], carry);
    carry = dividePart(idNum[2], carry);
    carry = dividePart(idNum[3], carry);
    carry = dividePart(idNum[4], carry);
    carry = dividePart(idNum[5], carry);
    carry = dividePart(idNum[6], carry);
    carry = dividePart(idNum[7], carry);
    return carry;
}

// Extract and encode one base36 digit
// value is divided by 36, and pOutPos is backed up by 1 character
void ClientId::encodeBase36(quint32 &value, QChar *&pOutPos)
{
   *pOutPos = encodeChars[value % 36];
   --pOutPos;
   value /= 36;
}

// Encode a group of 6 base36 digits
// pOutPos is backed up by 6 places
void ClientId::encodeBase36Group(quint32 value, QChar *&pOutPos)
{
    Q_ASSERT(value < groupLimit);
    encodeBase36(value, pOutPos);
    encodeBase36(value, pOutPos);
    encodeBase36(value, pOutPos);
    encodeBase36(value, pOutPos);
    encodeBase36(value, pOutPos);
    encodeBase36(value, pOutPos);
    Q_ASSERT(value == 0);
}

// Extract and encode the next group of 6 base36 digits from IdBits.
// pOutPos is backed up by 6 places.
void ClientId::encodeNextGroup(IdBits &idNum, QChar *&pOutPos)
{
    const quint32 group = extractNextGroup(idNum);
    encodeBase36Group(group, pOutPos);
}
