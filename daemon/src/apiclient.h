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
#line HEADER_FILE("apiclient.h")

#ifndef APICLIENT_H
#define APICLIENT_H

#include "async.h"
#include "apibase.h"
#include "apiretry.h"
#include "environment.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QPointer>
#include <QSharedPointer>
#include <memory>

// ApiClient is used to make requests to the PrivateInternetAccess client API.
// This API includes requests such as connection status detection, geolocation,
// and problem reports.
class ApiClient : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("apiclient")

public:
    explicit ApiClient(Environment &environment);

private:
    // Create tasks to issue a request with retries and return its body (with
    // a counted retry strategy)
    Async<QByteArray> requestRetry(QNetworkAccessManager::Operation verb,
                                   ApiBase &apiBaseUris,
                                   QString resource, unsigned maxAttempts,
                                   const QJsonDocument &data, QByteArray auth);

    // Create tasks to issue a request with retries and return its body (with
    // any retry strategy)
    Async<QByteArray> requestRetry(QNetworkAccessManager::Operation verb,
                                   ApiBase &apiBaseUris,
                                   QString resource,
                                   std::unique_ptr<ApiRetry> pRetryStrategy,
                                   const QJsonDocument &data, QByteArray auth);

public:
    // Get an API resource, such as "geo" or "status".
    // - resource is the path to the resource (under /api/client/), such as
    //   "geo" or "status"
    // - auth is the content of the Authorization header - use passwordAuth(),
    //   tokenAuth(), or autoAuth() to generate this.
    //
    // NOTE: Some APIs, like "status", do not actually check the credentials
    // passed in the authorization header, although they *do* still check that
    // the header is present.  It's probably best to still pass valid
    // credentials for forward compatibility, but this means a valid response
    // does not necessarily mean that the credentials were valid for all
    // requests.
    Async<QJsonDocument> get(QString resource, QByteArray auth = {});
    // GET with retry.
    Async<QJsonDocument> getRetry(QString resource, QByteArray auth = {});
    // GET with retry using arbitrary API base.  Usually used for requests that
    // have to be made to a specific VPN server.
    Async<QJsonDocument> getRetry(ApiBase &apiBaseUris, QString resource,
                                  QByteArray auth = {});

    // Do a GET request for a particular API resource that returns the user's
    // IP address.  (Doesn't allow API proxies.)
    Async<QJsonDocument> getIp(QString resource, QByteArray auth = {});
    // GET for an IP address with retries.
    Async<QJsonDocument> getIpRetry(QString resource, QByteArray auth = {});

    // GET for the VPN IP address - uses a timed retry strategy tuned for the
    // VPN IP address (with a relatively long 10-minute max retry time).
    Async<QJsonDocument> getVpnIpRetry(QString resource, QByteArray auth = {});

    // Do a POST request for a particular resource with optional parameter
    // data in JSON format.
    Async<QJsonDocument> post(QString resource, const QJsonDocument &data = {}, QByteArray auth = {});
    // POST with retry.
    Async<QJsonDocument> postRetry(QString resource, const QJsonDocument &data = {}, QByteArray auth = {});

    // Do a HEAD request for a particular resource.  This is basically only
    // useful to validate user credentials.
    Async<void> head(QString resource, QByteArray auth = {});
    // HEAD with retry.
    Async<void> headRetry(QString resource, QByteArray auth = {});

    // Get for the forwarded port - specific API base, and timed retry strategy
    Async<QJsonDocument> getForwardedPort(QString resource, QByteArray auth = {});

    // Generate an authentication header from a username and a password.
    static QByteArray passwordAuth(const QString& username, const QString& password);
    // Generate an authentication header from a token.
    static QByteArray tokenAuth(const QString& token);
    // Generate an authentication header from a token, or a username and
    // password if a token is not available.
    static QByteArray autoAuth(const QString& username, const QString& password, const QString& token);

private:
    // Environment used by this ApiClient for API bases (managed by Daemon)
    Environment &_environment;
};


#endif // APICLIENT_H
