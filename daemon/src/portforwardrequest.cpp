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
#line SOURCE_FILE("portforwardrequest.cpp")

#include "portforwardrequest.h"
#include <chrono>

namespace
{
    // std::chrono::days requires C++20
    std::chrono::hours days(int count) {return std::chrono::hours{count*24};}

    // Minimum remaining validity time to reuse a PF token.  This threshold is
    // decided by the client.
    auto modernPfMinimumReuseValidity = days(7);

    // How often we need to bind the port to keep it alive.
    std::chrono::minutes modernPfBindInterval{15};

    // Wait for the auth token to be available.  If we hadn't obtained a token
    // before connecting, we need to wait for the token to be obtained before
    // attempting to use port forwarding in the Modern infrastructure.  This
    // task never fails, a timeout should be used to abandon it if the token is
    // never obtained.
    class AuthTokenTask : public Task<QString>
    {
    public:
        AuthTokenTask(DaemonAccount &account)
            : _account{account}
        {
            checkToken();
            if(isPending())
            {
                connect(&account, &DaemonAccount::tokenChanged, this,
                        &AuthTokenTask::checkToken);
            }
        }

    private:
        void checkToken()
        {
            if(!_account.token().isEmpty())
                resolve(_account.token());
        }

    public:
        DaemonAccount &_account;
    };
}


PortForwardRequestModern::PortForwardRequestModern(ApiClient &apiClient,
                                                   DaemonAccount &account,
                                                   DaemonState &state,
                                                   const Environment &environment)
    : _apiClient{apiClient}, _account{account},
      _pfApiBase{QStringLiteral("https://") + state.tunnelDeviceRemoteAddress() + QStringLiteral(":19999"),
                 environment.getRsa4096CA(),
                 state.connectedServer().isNull() ? QString{} : state.connectedServer()->commonName()},
      _canRetry{true}
{
    if(!checkAccountPfToken())
    {
        // Reason traced by checkAccountPfToken()
        _account.portForwardPayload({});
        _account.portForwardSignature({});
        // Get a new token
        _canRetry = false;  // If the token fails immediately, do not retry
        // Wait for the auth token to become available for up to 1 minute
        Async<AuthTokenTask>::create(account)
            .timeout(std::chrono::minutes{1})
            // Then kick off an API request to get a PF token
            ->then(this, [this](const QString &token)
                {
                    QString resource = QStringLiteral("getSignature?token=") +
                        QString::fromLatin1(QUrl::toPercentEncoding(token));
                    return _apiClient.getRetry(_pfApiBase, resource, {});
                })
            ->then(this, &PortForwardRequestModern::handleGetSigResult)
            // Then, bind the port.  Fail if we couldn't get the token for any
            // reason
            ->notify(this, [this](const Error &err, const PfToken &token)
                {
                    if(err)
                    {
                        qWarning() << "Unable to obtain PF token:" << err;
                        // Failed entirely, this is not retriable.
                        emit stateUpdated(State::Failed, 0);
                    }
                    else
                    {
                        // Obtained token, save it and bind the port
                        _account.portForwardPayload(token._payload);
                        _account.portForwardSignature(token._signature);
                        qInfo() << "Obtained token, bind port on this server";
                        bindWithToken(token, true);
                    }
                });
    }
    else
    {
        // Use the existing token
        qInfo() << "Already have existing token, bind port on this server";
        bindWithToken({_account.portForwardPayload(),
                       _account.portForwardSignature()}, true);
    }

    // Rebind periodically to keep the forwarded port alive
    _bindTimer.setInterval(msec(modernPfBindInterval));
    connect(&_bindTimer, &QTimer::timeout, this, [this]()
    {
        bindWithToken({_account.portForwardPayload(),
                       _account.portForwardSignature()}, false);
    });
}

QJsonDocument PortForwardRequestModern::parsePfPayload()
{
    auto portForwardJson = QByteArray::fromBase64(_account.portForwardPayload().toUtf8());
    QJsonParseError parseErr;
    auto portForwardDoc = QJsonDocument::fromJson(portForwardJson, &parseErr);
    if(portForwardDoc.isNull())
        qWarning() << "Unable to parse PF payload:" << parseErr.errorString();
    return portForwardDoc;
}

bool PortForwardRequestModern::checkAccountPfToken()
{
    if(_account.portForwardPayload().isEmpty() && _account.portForwardSignature().isEmpty())
    {
        // No token at all (would be caught by next check, but this improves tracing)
        qInfo() << "Don't have a PF token yet, request one";
        return false;
    }

    // If the payload or signature is missing entirely, discard it and trace.
    if(_account.portForwardPayload().isEmpty() || _account.portForwardSignature().isEmpty())
    {
        qWarning() << "Discarding partial token - has payload:"
            << !_account.portForwardPayload().isEmpty() << "- has signature:"
            << !_account.portForwardSignature().isEmpty();
        return false;
    }

    // Check if we have a valid port forwarding token, and it isn't going to
    // expire soon.
    auto portForwardDoc = parsePfPayload();
    if(portForwardDoc.isNull())
    {
        qWarning() << "Discarding invalid token - unable to parse JSON";
        return false;
    }

    // Find the creation time and expiration time
    auto expireTime = QDateTime::fromString(portForwardDoc["expires_at"].toString(),
                                            Qt::DateFormat::ISODateWithMs);

    // This is the minimum expire time we want to ensure a minimum validity period
    auto minimumExpireTime = QDateTime::currentDateTime().addMSecs(msec(modernPfMinimumReuseValidity));
    if(expireTime < minimumExpireTime)
    {
        qWarning() << "Discarding old token, will expire at" << expireTime
            << "- want minimum expiration" << minimumExpireTime;
        return false;
    }

    // Token is valid, use it.
    return true;
}

auto PortForwardRequestModern::handleGetSigResult(const QJsonDocument &result)
    -> PfToken
{
    const auto &resultStatus = result[QStringLiteral("status")].toString();
    if(resultStatus != QStringLiteral("OK"))
    {
        qWarning() << "Server rejected PF request, status:" << resultStatus;
        qWarning().noquote() << "Server response:" << result.toJson();
        throw Error{HERE, Error::Code::ApiBadResponseError};
    }

    PfToken token{result[QStringLiteral("payload")].toString(),
                  result[QStringLiteral("signature")].toString()};
    if(token._payload.isEmpty() || token._signature.isEmpty())
    {
        qWarning().noquote() << "Server returned invalid PF response:" << result.toJson();
        throw Error{HERE, Error::Code::ApiBadResponseError};
    }

    return token;
}

void PortForwardRequestModern::retryNewToken()
{
    // Discard the existing token (if any) since it failed
    _account.portForwardPayload({});
    _account.portForwardSignature({});
    if(_canRetry)
    {
        qWarning() << "Unable to bind existing token, retry with new token";
        emit stateUpdated(State::Retry, 0);
    }
    else
    {
        qWarning() << "Not retrying, already obtained a new token and PF still failed.";
        emit stateUpdated(State::Failed, 0);
    }
}

void PortForwardRequestModern::bindWithToken(const PfToken &token, bool initial)
{
    QString resource = QStringLiteral("bindPort?payload=") +
        QString::fromLatin1(QUrl::toPercentEncoding(token._payload)) +
        QStringLiteral("&signature=") +
        QString::fromLatin1(QUrl::toPercentEncoding(token._signature));
    _apiClient.getRetry(_pfApiBase, resource, {})
        ->notify(this, [this, initial](const Error &err, const QJsonDocument &bindResult)
        {
            if(err)
            {
                qWarning() << "Bind failed due to error:" << err;
                retryNewToken();
                return;
            }

            const auto &resultStatus = bindResult[QStringLiteral("status")].toString();
            if(resultStatus != QStringLiteral("OK"))
            {
                qWarning() << "Server rejected PF bind, status:" << resultStatus;
                qWarning().noquote() << "Server response:" << bindResult.toJson();
                retryNewToken();
                return;
            }

            qInfo() << "Server accepted PF bind, message:"
                << bindResult[QStringLiteral("message")].toString();

            // If this is the first success, parse the token to find and emit
            // the port.
            // If we have already succeeded, we don't need to emit the port
            // again, we're just keeping the bind alive.
            if(initial)
            {
                // Parse the token to determine the port that was forwarded
                auto portForwardDoc = parsePfPayload();
                // If the document couldn't be parsed, we'll get port 0 and handle
                // it below.  This is unlikely (would mean that the server signed
                // and accepted invalid JSON) but could happen.
                int port = portForwardDoc[QStringLiteral("port")].toInt();
                if(port <= 0 || port > std::numeric_limits<quint16>::max())
                {
                    qWarning() << "Could not determine port number from token, got"
                        << port;
                    retryNewToken();
                    return;
                }

                emit stateUpdated(State::Success, port);

                // Since we successfully bound the port, if it later fails, we
                // can attempt to get a new token, even if this was a new token
                // for this attempt.
                _canRetry = true;
                _bindTimer.start();
            }
        });
}
