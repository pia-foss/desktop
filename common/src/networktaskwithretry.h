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
#line HEADER_FILE("networktaskwithretry.h")

#ifndef NETWORKTASKWITHRETRY_H
#define NETWORKTASKWITHRETRY_H

#include "async.h"
#include "apibase.h"
#include "apiretry.h"
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <memory>

// NetworkTaskWithRetry executes an API request until either it succeeds or
// the maximum attempt count is reached.  It uses a NetworkReplyHandler for each
// attempt.
class COMMON_EXPORT NetworkTaskWithRetry : public Task<QByteArray>
{
    CLASS_LOGGING_CATEGORY("apiclient")

public:
    // Create NetworkTaskWithRetry with the verb and request that will be used
    // for each attempt.
    //
    // An ApiBase is passed to specify the base URIs to use for this request.
    // Each attempt uses the next base URI from the ApiBase.
    //
    // A retry strategy is passed to control the attempt count / duration /
    // delays for the request.  NetworkTaskWithRetry takes ownership of the
    // ApiRetry object.
    //
    // The attempts are spread across the base URIs (for example, with 2 base
    // URIs and 4 max attempts, each URI could be tried twice).
    //
    // If authHeaderVal is not empty, it is applied as an authorization header
    // to each request.
    NetworkTaskWithRetry(QNetworkAccessManager::Operation verb,
                         ApiBase &apiBaseUris, QString resource,
                         std::unique_ptr<ApiRetry> pRetryStrategy,
                         const QJsonDocument &data, QByteArray authHeaderVal,
                         QSharedPointer<QNetworkAccessManager> pNetworkManager);
    ~NetworkTaskWithRetry();

private:
    // Schedule an attempt, or reject if all attempts have been used.
    void scheduleNextAttempt();

    // Execute an attempt (used by scheduleNextAttempt())
    void executeNextAttempt();

    // Create task to issue a single request and return its body.
    Async<QByteArray> sendRequest();

private:
    QNetworkAccessManager::Operation _verb;
    ApiBaseSequence _baseUriSequence;
    std::unique_ptr<ApiRetry> _pRetryStrategy;
    QString _resource;
    QByteArray _data;
    QByteArray _authHeaderVal;
    QSharedPointer<QNetworkAccessManager> _pNetworkManager;
    Async<QByteArray> _pNetworkReply;
    // ApiRateLimitedError is retriable but causes us to return that instead of
    // the generic error if we don't encounter an auth error.
    // This field keeps track of the worst retriable error we have seen, if we
    // fail due to all attempts failing, this is the error we return.
    Error::Code _worstRetriableError;
};

#endif
