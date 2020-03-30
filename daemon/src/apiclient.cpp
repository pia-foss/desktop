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
#line SOURCE_FILE("apiclient.cpp")

#include "apiclient.h"
#include "testshim.h"
#include "networktaskwithretry.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <chrono>
#include <array>

namespace
{
    // Auth header parts (see ApiClient::setAuth())
    const QByteArray authPasswordDelim{QByteArrayLiteral(":")};
    const QByteArray authBasicPrefix{QByteArrayLiteral("Basic ")};
    const QByteArray authTokenPrefix{QByteArrayLiteral("Token ")};

    // Number of times we will attempt any retryable API request.
    // Note that these retries are spread among the possible API base URLs, so
    // it should normally be at least apiBaseUris.size()
    const int apiAttempts{4};

    static inline QJsonDocument parseJsonBody(const QByteArray& body)
    {
        QJsonParseError parseError;
        auto json = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError || json.isNull())
        {
            qError() << "Couldn't parse API response due to error" << parseError.error << "at position" << parseError.offset << "in document:" << body;
            throw Error(HERE, Error::ApiBadResponseError);
        }
        return json;
    }
}

ApiClient::ApiClient()
    : _nextApiBaseUrl{0}
{
}

// Create tasks to issue a request with retries and return its body.
Async<QByteArray> ApiClient::requestRetry(QNetworkAccessManager::Operation verb,
                                          ApiBase &apiBaseUris,
                                          QString resource, unsigned maxAttempts,
                                          const QJsonDocument &data, QByteArray auth)
{
    return requestRetry(verb, apiBaseUris, std::move(resource),
                        ApiRetries::counted(maxAttempts), data,
                        std::move(auth));
}

Async<QByteArray> ApiClient::requestRetry(QNetworkAccessManager::Operation verb,
                                          ApiBase &apiBaseUris,
                                          QString resource,
                                          std::unique_ptr<ApiRetry> pRetryStrategy,
                                          const QJsonDocument &data, QByteArray auth)
{
    // Create a retriable task to fetch the response body
    return Async<NetworkTaskWithRetry>::create(verb, apiBaseUris,
                                               resource,
                                               std::move(pRetryStrategy),
                                               data,
                                               std::move(auth));
}

Async<QJsonDocument> ApiClient::get(QString resource, QByteArray auth)
{
    // Just use a max attempt count of 1 to do a GET with no retries.
    return requestRetry(QNetworkAccessManager::Operation::GetOperation,
                        ApiBases::piaApi, std::move(resource), 1,
                        {}, std::move(auth))
            ->then(parseJsonBody);
}

Async<QJsonDocument> ApiClient::getRetry(QString resource, QByteArray auth)
{
    return requestRetry(QNetworkAccessManager::Operation::GetOperation,
                        ApiBases::piaApi, std::move(resource),
                        apiAttempts, {}, std::move(auth))
            ->then(parseJsonBody);
}

Async<QJsonDocument> ApiClient::getRetry(ApiBase &apiBaseUris, QString resource,
                                         QByteArray auth)
{
    return requestRetry(QNetworkAccessManager::Operation::GetOperation,
                        apiBaseUris, std::move(resource), apiAttempts, {},
                        std::move(auth))
            ->then(parseJsonBody);
}

Async<QJsonDocument> ApiClient::getIp(QString resource, QByteArray auth)
{
    // Like get(), max of 1 attempt
    return requestRetry(QNetworkAccessManager::Operation::GetOperation,
                        ApiBases::piaIpAddrApi, std::move(resource), 1,
                        {}, std::move(auth))
            ->then(parseJsonBody);
}

Async<QJsonDocument> ApiClient::getIpRetry(QString resource, QByteArray auth)
{
    return requestRetry(QNetworkAccessManager::Operation::GetOperation,
                        ApiBases::piaIpAddrApi, std::move(resource),
                        apiAttempts, {}, std::move(auth))
            ->then(parseJsonBody);
}

Async<QJsonDocument> ApiClient::getVpnIpRetry(QString resource, QByteArray auth)
{
    return requestRetry(QNetworkAccessManager::Operation::GetOperation,
                        ApiBases::piaIpAddrApi, std::move(resource),
                        ApiRetries::timed(std::chrono::seconds{5},
                                          std::chrono::minutes{2}),
                        {}, std::move(auth))
            ->then(parseJsonBody);
}

Async<QJsonDocument> ApiClient::post(QString resource, const QJsonDocument &data, QByteArray auth)
{
    return requestRetry(QNetworkAccessManager::Operation::PostOperation,
                        ApiBases::piaApi, std::move(resource), 1,
                        data, std::move(auth))
            ->then(parseJsonBody);
}

Async<QJsonDocument> ApiClient::postRetry(QString resource, const QJsonDocument &data, QByteArray auth)
{
    return requestRetry(QNetworkAccessManager::Operation::PostOperation,
                        ApiBases::piaApi, std::move(resource), apiAttempts,
                        data, std::move(auth))
            ->then(parseJsonBody);
}

Async<void> ApiClient::head(QString resource, QByteArray auth)
{
    // Like get(), just use a max attempt count of 1.
    return requestRetry(QNetworkAccessManager::Operation::HeadOperation,
                        ApiBases::piaApi, std::move(resource), 1,
                        {}, std::move(auth));
}

Async<void> ApiClient::headRetry(QString resource, QByteArray auth)
{
    return requestRetry(QNetworkAccessManager::Operation::HeadOperation,
                        ApiBases::piaApi, std::move(resource),
                        apiAttempts, {}, std::move(auth));
}

Async<QJsonDocument> ApiClient::getForwardedPort(QString resource, QByteArray auth)
{
    // The port forward request doesn't go to the normal web API, it uses a
    // special ApiBase.  The "resource" from PortForwarder is the query string.
    return requestRetry(QNetworkAccessManager::Operation::GetOperation,
                        ApiBases::piaPortForwardApi,
                        std::move(resource),
                        ApiRetries::timed(std::chrono::seconds{5},
                                          std::chrono::seconds{30}),
                        {}, std::move(auth))
            ->then(parseJsonBody);
}


// Qt doesn't provide any help encoding Authentication headers, so we have
// to do it manually.  As mentioned in the ApiClient definition,
// QNetworkAccessManager can ask for credentials with the
// authenticationRequired signal and would encode the header for us then,
// but it's much simpler and more robust to just attach the header in the
// first place.  (It also probably only asks for credentials when the
// server challenges with a WWW-Authenticate header, which PIA's API does
// not do.)

// Encode the base64 authorization header value, which is "user:password"
// Base64-encoded.
//
// The user could enter a literal colon in the username field, which would
// cause the encoded string to split incorrectly, but there are no security
// implications here since the user would have to know a valid user/password
// combination anyway to authenticate this way (and they could just as
// easily enter the creds correctly in that case).  The frontend could check
// this just for good UX if desired.
//
// This encoding method isn't super efficient (we allocate multiple buffers,
// concatenate into a new buffer, then throw the temporaries away), but Qt
// doesn't provide a way to get the UTF-8-encoded length ahead of time.  It
// shouldn't really make much difference anyway.

QByteArray ApiClient::passwordAuth(const QString& username, const QString& password)
{
    // For username+password authentication, the proper header value is the
    // "username:password" string (in UTF-8) Base64-encoded, prepended with
    // the string "Basic ".
    //
    // The HTTP Basic Authentication scheme does not allow literal colons in
    // usernames, and if the user enters a colon in the username field the
    // encoded string would split incorrectly.  However, there are no security
    // implications since the user would have to know a valid username/password
    // combination anyway to authenticate this way.  The frontend could check
    // for invalid usernames just in case for good UX.

    QByteArray usernameAndPassword;
    usernameAndPassword += username.toUtf8();
    usernameAndPassword += authPasswordDelim;
    usernameAndPassword += password.toUtf8();
    return authBasicPrefix + usernameAndPassword.toBase64();
}

QByteArray ApiClient::tokenAuth(const QString& token)
{
    // For our token authentication, the intended header value is simply
    // the token hex string prepended with the string "Token ".

    return authTokenPrefix + token.toLatin1();
}

QByteArray ApiClient::autoAuth(const QString& username, const QString& password, const QString& token)
{
    // For automatic authentication, tokens take precedence over passwords.

    if (!token.isEmpty())
        return tokenAuth(token);
    else if (!username.isEmpty() && !password.isEmpty())
        return passwordAuth(username, password);
    else
        return {};
}
