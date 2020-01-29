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
#line SOURCE_FILE("networktaskwithretry.cpp")

#include "networktaskwithretry.h"
#include "apinetwork.h"
#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>

namespace
{
    // Timeout for all API requests
    const std::chrono::seconds requestTimeout{5};

    const QByteArray authHeaderName{QByteArrayLiteral("Authorization")};

    // Set the authorization header on a QNetworkRequest
    void setAuth(QNetworkRequest &request, const QByteArray &authHeaderVal)
    {
        request.setRawHeader(authHeaderName, authHeaderVal);
    }
}

NetworkTaskWithRetry::NetworkTaskWithRetry(QNetworkAccessManager::Operation verb,
                                           ApiBase &apiBaseUris,
                                           QString resource,
                                           std::unique_ptr<ApiRetry> pRetryStrategy,
                                           const QJsonDocument &data,
                                           QByteArray authHeaderVal)
    : _verb{std::move(verb)}, _baseUriSequence{apiBaseUris.beginAttempt()},
      _pRetryStrategy{std::move(pRetryStrategy)}, _resource{std::move(resource)},
      _data{(data.isNull() ? QByteArray() : data.toJson())},
      _authHeaderVal{std::move(authHeaderVal)},
      _worstRetriableError{Error::Code::ApiNetworkError}
{
    Q_ASSERT(_pRetryStrategy);
    // Only GET and HEAD are supported right now
    Q_ASSERT(_verb == QNetworkAccessManager::Operation::GetOperation ||
             _verb == QNetworkAccessManager::Operation::PostOperation ||
             _verb == QNetworkAccessManager::Operation::HeadOperation);

    scheduleNextAttempt();
}

NetworkTaskWithRetry::~NetworkTaskWithRetry()
{

}

void NetworkTaskWithRetry::scheduleNextAttempt()
{
    Q_ASSERT(_pRetryStrategy);  // Class invariant

    nullable_t<std::chrono::milliseconds> nextDelay = _pRetryStrategy->beginNextAttempt(_resource);
    if(!nextDelay)
    {
        qWarning() << "Request for resource" << _resource
            << "failed, returning error" << _worstRetriableError;
        reject({HERE, _worstRetriableError});
        return;
    }

    QTimer::singleShot(nextDelay->count(), this, &NetworkTaskWithRetry::executeNextAttempt);
}

void NetworkTaskWithRetry::executeNextAttempt()
{
    // Handle the request
    sendRequest()
            ->notify(this, [this](const Error& error, const QByteArray& body) {

                // Release this task; it's no longer needed
                _pNetworkReply.reset();

                // Check for errors
                if (error)
                {
                    // Auth errors can't be retried.
                    if (error.code() == Error::ApiUnauthorizedError)
                    {
                        reject(error);
                        return;
                    }

                    // A rate limiting error is worse than a network error - set the worst
                    // retriable error, but keep trying in case another API endpoint gives us
                    // 200 or 401.
                    // (Otherwise, leave the worst error alone, it might already be set to a
                    // rate limiting error by a prior attempt.)
                    if (error.code() == Error::ApiRateLimitedError)
                        _worstRetriableError = Error::Code::ApiRateLimitedError;

                    qWarning() << "Attempt for" << _resource
                        << "failed with error" << error;

                    // Retry if we still have attempts left.
                    scheduleNextAttempt();
                }
                else
                {
                    _baseUriSequence.attemptSucceeded();
                    resolve(body);
                }
            });
}

Async<QByteArray> NetworkTaskWithRetry::sendRequest()
{
    // Use ApiNetwork's QNetworkAccessManager, this binds us to the VPN
    // interface when connected (important when we do not route the default
    // gateway into the VPN).
    QNetworkAccessManager &networkManager = ApiNetwork::instance()->getAccessManager();

    // Clear the connection cache for every request.
    //
    // QNetworkManager caches connections for reuse, but if we connect or
    // disconnect from the VPN, these connections break.  If QNetworkManager
    // uses a cached connection that's broken, we end up waiting the full
    // 10 seconds before the request is aborted.
    //
    // The cost of waiting for a bad cached connection to time out is a lot
    // higher than the cost of establishing a new connection, so clear them
    // every time.  (There is no way to disable connection caching in
    // QNetworkAccessManager.)
    //
    // In principle, we could try to do this only when the connection state
    // changes, but it would have to be cleared any time the OpenVPNProcess
    // state changes, which doesn't always cause a state transition in
    // VPNConnection.  The cost of failing to clear the cache is pretty high,
    // but the cost of clearing it an extra time is pretty small, so such an
    // optimization probably would be too complex to make sense.
    networkManager.clearConnectionCache();

    QNetworkRequest request(_baseUriSequence.getNextUri() + _resource);
    if (!_authHeaderVal.isEmpty())
        setAuth(request, _authHeaderVal);

    // The URL for each request is logged to indicate if there is trouble with
    // specific API URLs, etc.  The resources we request don't contain any
    // query parameters, so this won't contain anything identifiable.
    qDebug() << "requesting:" << request.url().toString();

    // Seems like QNetworkAccessManager could provide this, but the closest
    // thing it has is sendCustomRequest().  It looks like that would produce a
    // QNetworkReply that says its operation was "custom" even if the method
    // was a standard one, and there might be other subtleties, so this is more
    // robust.
    QNetworkReply* replyPtr;
    switch (_verb)
    {
        default:
        case QNetworkAccessManager::GetOperation:
            replyPtr = networkManager.get(request);
            break;
        case QNetworkAccessManager::PostOperation:
            request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
            replyPtr = networkManager.post(request, _data);
            break;
        case QNetworkAccessManager::HeadOperation:
            replyPtr = networkManager.head(request);
            break;
    }
    // Wrap the reply in a shared pointer. Use deleteLater as the deleter
    // since the finished signal is not always safe to destroy ourselves
    // in (e.g. abort->finished->delete is not currently safe). This way
    // we don't have to delay the entire finished signal to stay safe.
    QSharedPointer<QNetworkReply> reply(replyPtr, &QObject::deleteLater);

    // Abort the request if it doesn't complete within a certain interval
    QTimer::singleShot(std::chrono::milliseconds(requestTimeout).count(), reply.get(), &QNetworkReply::abort);

    // Create a network task that resolves to the result of the request
    auto networkTask = Async<QByteArray>::create();
    QString resource = _resource;
    connect(reply.get(), &QNetworkReply::finished, networkTask.get(), [networkTask = networkTask.get(), reply, resource]
    {
        auto keepAlive = networkTask->sharedFromThis();

        // Log the status just for supportability.
        const auto &statusCode = reply->attribute(QNetworkRequest::Attribute::HttpStatusCodeAttribute);
        const auto &statusMsg = reply->attribute(QNetworkRequest::Attribute::HttpReasonPhraseAttribute);

        auto replyError = reply->error();
        qInfo() << "Request for" << resource << "-" << statusCode.toInt()
            << statusMsg.toByteArray() << "- error code:" << replyError;

        // Check specifically for an auth error, which indicates that the creds are
        // not valid.
        if (replyError == QNetworkReply::NetworkError::AuthenticationRequiredError)
        {
            qWarning() << "Could not request" << resource << "due to invalid credentials";
            networkTask->reject(Error(HERE, Error::ApiUnauthorizedError));
            return;
        }

        // If the API returned 429, it is rate limiting us, return a specific error.
        // This is still retriable, but it can cause NetworkTaskWithRetry to return
        // a specific error if all retries fail.
        if (reply->attribute(QNetworkRequest::Attribute::HttpStatusCodeAttribute).toInt() == 429)
        {
            qWarning() << "Could not request" << resource << "due to rate limiting";
            // A rate limiting error is worse than a network error - set the worst
            // retriable error, but keep trying in case another API endpoint gives us
            // 200 or 401.
            // (Otherwise, leave the worst error alone, it might already be set to a
            // rate limiting error by a prior attempt.)
            networkTask->reject(Error(HERE, Error::ApiRateLimitedError));
            return;
        }

        if (replyError != QNetworkReply::NetworkError::NoError)
        {
            qWarning() << "Could not request" << resource << "due to error:" << replyError;
            networkTask->reject(Error(HERE, Error::Code::ApiNetworkError));
            return;
        }

        networkTask->resolve(reply->readAll());
    });

    return networkTask;
}
