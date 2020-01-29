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
    // Number of times we will attempt the port forward request.
    // This is the same as the existing client (which retries 3 times; 4 total
    // attempts)
    const int forwardAttempts{4};
    // Timeout for the first attempt.  The existing client uses a 5-second
    // initial timeout, and increases it by 5 seconds for each retry.
    const std::chrono::seconds initialTimeout{5};
    // Amount to increase the timeout for each retry
    const std::chrono::seconds timeoutBackoff{5};

    // Partial URL used to request a port forward.  (PortForwarder adds on the
    // client ID.)
    // Note that this is not secure, we rely on it being sent through the VPN
    // interface
    const QString requestUrlPrefix{QStringLiteral("http://209.222.18.222:2000/?client_id=")};
}

PortRequester::PortRequester(const QUrl &requestUrl)
    : _attemptCount{0},
      _forwardRequest{requestUrl},
      _pNetworkManager{TestShim::create<QNetworkAccessManager>()}
{
    _requestTimeout.setSingleShot(true);
    beginNextAttempt();
}

void PortRequester::beginNextAttempt()
{
    // If we've already reached the maximum attempts, emit failure and be done.
    if(_attemptCount >= forwardAttempts)
    {
        qWarning() << "Port forward request failed" << _attemptCount << "attempts, giving up";
        emit portForwardComplete(0);
        return;
    }

    auto attemptTimeout = initialTimeout + timeoutBackoff * _attemptCount;
    ++_attemptCount;

    QPointer<QNetworkReply> pForwardReply{_pNetworkManager->get(_forwardRequest)};
    pForwardReply->setParent(this);
    // Capture pForwardReply by value, so this lambda will always use the reply
    // associated with the request that was started here, even if another
    // request is started before the signal is handled
    //
    // It's not possible for the QNetworkReply to be destroyed before this
    // signal is handled, since it's deleted by this signal handler.
    connect(pForwardReply, &QNetworkReply::finished, this,
            [=](){
                Q_ASSERT(pForwardReply);
                pForwardReply->deleteLater();
                forwardReplyFinished(*pForwardReply);
            });

    // Set a timeout for this attempt
    Q_ASSERT(!_requestTimeout.isActive());
    _requestTimeout.setInterval(std::chrono::milliseconds(attemptTimeout).count());
    // It's possible for the timeout signal to be handled after the reply was
    // already deleted, if the reply completes just as the timeout is elapsing.
    // The captured pForwardReply will be nullptr if this happens.
    connect(&_requestTimeout, &QTimer::timeout, this,
            [=](){
                if(!pForwardReply)
                    return; // Already destroyed, nothing to do
                qWarning() << "Request" << _attemptCount
                    << "for port forward timed out after"
                    << attemptTimeout.count() << "seconds";
                // Request timed out - abort it.  This emits the finished()
                // signal, which will take care of cleaning up the reply and
                // trying again
                pForwardReply->abort();
            });
    _requestTimeout.start();
}

int PortRequester::readForwardReply(QNetworkReply &reply)
{
    auto requestError = reply.error();
    if(requestError != QNetworkReply::NetworkError::NoError)
    {
        qWarning() << "Couldn't request port forward due to error:" << requestError;
        return 0;
    }

    auto replyPayload = reply.readAll();
    // Read the JSON response
    QJsonParseError replyParseError;
    const auto &replyJson = QJsonDocument::fromJson(replyPayload, &replyParseError);

    if(replyJson.isNull())
    {
        qWarning() << "Couldn't read port forward reply due to error:"
            << replyParseError.error << "at position" << replyParseError.offset;
        qWarning() << "Retrieved JSON:" << replyPayload;
        return 0;
    }

    // If the server returned an error, log it and fail
    const auto &replyErr = replyJson[QStringLiteral("error")];
    // If the document had a value for 'error', that's a failure
    if(!replyErr.isUndefined() && !replyErr.isNull())
    {
        qWarning() << "Port could not be forwarded due to error:"
            << replyErr;
        return 0;
    }

    // Get the actual port value.  (toInt() returns 0 if the value isn't
    // actually an integer - 0 isn't a valid port number.)
    const auto &replyPort = replyJson[QStringLiteral("port")].toInt();
    if(replyPort <= 0 || replyPort > std::numeric_limits<quint16>::max())
    {
        qError() << "Invalid port value from port forward request:" << replyPort;
        qError() << "Received JSON:" << replyPayload;
        return 0;
    }

    return replyPort;
}

void PortRequester::forwardReplyFinished(QNetworkReply &reply)
{
    // Stop the timer if it's running since the reply finished.
    _requestTimeout.stop();

    int forwardedPort = readForwardReply(reply);
    if(forwardedPort)
    {
        // The request succeeded; emit the port number.  We're done.
        emit portForwardComplete(forwardedPort);
    }
    else
    {
        // The request failed.  Try again if there are attempts left.
        beginNextAttempt();
    }
}

PortForwarder::PortForwarder(QObject *pParent, const QString &clientId)
    : QObject{pParent},
      _requestUrl{requestUrlPrefix + clientId},
      _connectionState{State::Disconnected},
      _forwardingEnabled{false},
      _pCurrentRequest{}
{
}

void PortForwarder::updateConnectionState(State connectionState)
{
    if(connectionState == _connectionState)
        return; // No change, nothing to do

    if(connectionState == State::ConnectedSupported)
    {
        // Invariant - cleared in any state other than ConnectedSupported
        Q_ASSERT(!_pCurrentRequest);
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
        _pCurrentRequest.reset();
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
        // If we're connected, forwarding is enabled, and no port has been
        // requested yet, then inform the user they must reconnect to request a port.
        // (_pCurrentRequest could be set
        // already if port forwarding was toggled off and on again while already
        // connected.)
        if(_forwardingEnabled && !_pCurrentRequest)
            emit portForwardUpdated(PortForwardState::Inactive, true);
        break;
    case State::ConnectedUnsupported:
        // If we're connected to a region that doesn't support port forwarding,
        // update the state to either Inactive or Unavailable.  (This shows or
        // hides the notice in the client.)  Do this dynamically (unlike in the
        // ConnectedSupported state where we continue to show the port once it's
        // forwarded even if the setting is turned off).

        // Class invariant - can only be set in ConnectedSupported state.
        // (Guarantees that we're not about to overwrite a valid port forward.)
        Q_ASSERT(!_pCurrentRequest);
        emit portForwardUpdated(_forwardingEnabled ? PortForwardState::Unavailable : PortForwardState::Inactive);
        break;
    }
}

void PortForwarder::requestPort()
{
    _pCurrentRequest.reset(new PortRequester{_requestUrl});
    connect(_pCurrentRequest.data(), &PortRequester::portForwardComplete, this,
            [this](int port)
            {
                emit portForwardUpdated(port ? port : PortForwardState::Failed);
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
