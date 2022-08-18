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
#line SOURCE_FILE("networktaskwithretry.cpp")

#include "networktaskwithretry.h"
#include "apinetwork.h"
#include "openssl.h"
#include <common/src/builtin/util.h>
#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>

namespace
{
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

    scheduleNextAttempt(std::chrono::milliseconds{0});
}

NetworkTaskWithRetry::~NetworkTaskWithRetry()
{

}

void NetworkTaskWithRetry::scheduleNextAttempt(std::chrono::milliseconds nextDelay)
{
    Q_ASSERT(_pRetryStrategy);  // Class invariant

    QTimer::singleShot(msec(nextDelay), this, &NetworkTaskWithRetry::executeNextAttempt);
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
                    // Auth and "payment required" (expired account) errors can't be retried.
                    if (error.code() == Error::ApiUnauthorizedError ||
                        error.code() == Error::ApiPaymentRequiredError ||
                        error.code() == Error::ApiRateLimitedError)
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
                    Q_ASSERT(_pRetryStrategy);  // Class invariant
                    auto nextDelay = _pRetryStrategy->attemptFailed(_resource);
                    if(!nextDelay)
                    {
                        qWarning() << "Request for resource" << _resource
                            << "failed, returning error" << _worstRetriableError;
                        reject({HERE, _worstRetriableError});
                        return;
                    }
                    else
                        scheduleNextAttempt(*nextDelay);
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


    const BaseUri &nextBase = _baseUriSequence.getNextUri();
    ApiResource requestResource{nextBase.uri + _resource};
    QUrl requestUri{requestResource};
    QNetworkRequest request(requestUri);
    if (!_authHeaderVal.isEmpty())
        setAuth(request, _authHeaderVal);

    // The URL for each request is logged to indicate if there is trouble with
    // specific API URLs, etc.  Query parameters are redacted by ApiResource.
    if(nextBase.pCA && !nextBase.peerVerifyName.isEmpty())
    {
        qDebug() << "requesting:" << requestResource
            << "using peer name" << nextBase.peerVerifyName;
        // Since we're using a custom CA and peer name, do not use the default
        // CAs.  Copy the SSL configuration and explicitly set an empty CA list.
        //
        // We can't just apply the custom CA in the configuration.   Qt 5.12
        // lacks QNetworkRequest::setPeerVerifyName(), so we have to validate
        // the cert ourselves.
        QSslConfiguration sslConfig{request.sslConfiguration()};
        sslConfig.setCaCertificates({});
        request.setSslConfiguration(sslConfig);
    }
    else
    {
        qDebug() << "requesting:" << requestResource;
    }

    // Permit same-origin redirects.  Qt does not follow redirects by default,
    // which has resulted in some near-misses in the past when load balancers,
    // meta proxies, etc. have been reconfigured.
    //
    // Only same-origin redirects are allowed; there's no reason to allow
    // cross-origin redirects.  Do this with a user handler since Qt normally
    // distinguishes `https://host/` and `https://host:443/`; treat these as the
    // same.
    request.setAttribute(QNetworkRequest::Attribute::RedirectPolicyAttribute,
                         QNetworkRequest::RedirectPolicy::UserVerifiedRedirectPolicy);

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
    Q_ASSERT(replyPtr); // Postcondition of QNetworkAccessManager::get/post/head

    // Wrap the reply in a shared pointer. Use deleteLater as the deleter
    // since the finished signal is not always safe to destroy ourselves
    // in (e.g. abort->finished->delete is not currently safe). This way
    // we don't have to delay the entire finished signal to stay safe.
    QSharedPointer<QNetworkReply> reply(replyPtr, &QObject::deleteLater);

    // Abort the request if it doesn't complete within a certain interval
    Q_ASSERT(_pRetryStrategy);  // Class invariant
    QTimer::singleShot(msec(_pRetryStrategy->beginAttempt(_resource)), reply.get(), &QNetworkReply::abort);

    // Handle redirects by permitting same-origin HTTPS redirects only
    connect(reply.get(), &QNetworkReply::redirected, this,
        [reply, requestUri](const QUrl &url)
        {
            // Resolve the redirect URL if it's relative.  Typical relative
            // paths as URLs won't affect the scheme/host/port and will be
            // accepted since they are unchanged, but if something odd like a
            // protocol-relative URL shows up, this will handle it properly.
            const auto &targetResolved = requestUri.resolved(url);
            if(targetResolved.scheme() == QStringLiteral("https") &&
                targetResolved.host() == requestUri.host() &&
                targetResolved.port(443) == requestUri.port(443))
            {
                qInfo() << "Accepted redirect from"
                    << ApiResource{requestUri.toString()} << "to"
                    << ApiResource{url.toString()} << "(resolved:"
                    << ApiResource{targetResolved.toString()} << ")";
                reply->redirectAllowed();
            }
            else
            {
                qInfo() << "Rejected redirect from"
                    << ApiResource{requestUri.toString()} << "to"
                    << ApiResource{url.toString()} << "(resolved:"
                    << ApiResource{targetResolved.toString()} << ")";
                reply->abort();
            }
        });

    // If a custom CA and peer name are specified, handle SSL errors by
    // validating the cert manually
    if(nextBase.pCA && !nextBase.peerVerifyName.isEmpty())
    {
        connect(reply.get(), &QNetworkReply::sslErrors, this,
            [this, reply, nextBase](const QList<QSslError> &errors)
            {
                checkSslCertificate(*reply, nextBase, errors);
            });
    }

    // Create a network task that resolves to the result of the request
    auto networkTask = Async<QByteArray>::create();
    ApiResource resource = _resource;
    connect(reply.get(), &QNetworkReply::finished, networkTask.get(), [networkTask = networkTask.get(), reply, resource]
    {
        auto keepAlive = networkTask->sharedFromThis();

        // Log the status just for supportability.
        const auto &statusCode = reply->attribute(QNetworkRequest::Attribute::HttpStatusCodeAttribute);
        const auto &statusMsg = reply->attribute(QNetworkRequest::Attribute::HttpReasonPhraseAttribute);

        auto replyError = reply->error();
        qInfo() << "Request for" << resource << "-" << statusCode.toInt()
            << statusMsg.toByteArray().data() << "- error code:" << replyError;

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
            QByteArray header = reply->rawHeader("Retry-After");
            // Default retry delay is 59 seconds
            int retryDelay = 59;
            if(!header.isEmpty())
            {
                bool ok{false};
                int val = header.toInt(&ok);
                if(ok)
                {
                    retryDelay = val;
                }
                else
                {
                    qWarning() << "Invalid Retry-After value, got: " << QString{header};
                }
            }
            else
            {
                qWarning() << "Retry-After header not found in 429 response";
            }

            qWarning() << "Could not request" << resource << "due to rate limiting; "
                       << "Retry after " << retryDelay << " seconds.";
            // A rate limiting error is worse than a network error - set the worst
            // retriable error, but keep trying in case another API endpoint gives us
            // 200 or 401.
            // (Otherwise, leave the worst error alone, it might already be set to a
            // rate limiting error by a prior attempt.)
            networkTask->reject(Error(HERE, Error::ApiRateLimitedError,
                                  QDateTime::currentDateTime().addSecs(retryDelay)));
            return;
        }

        if (reply->attribute(QNetworkRequest::Attribute::HttpStatusCodeAttribute).toInt() == 402)
        {
            // 402 is used by our client API to indicate an account subscription has expired
            qWarning() << "Could not request" << resource << "due to payment required";
            networkTask->reject(Error(HERE, Error::ApiPaymentRequiredError));
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

void NetworkTaskWithRetry::traceLeafCert(const QSslCertificate &leafCert) const
{
    // In general, there can be any number of each of these fields
    const auto &commonNames = leafCert.subjectInfo(QSslCertificate::SubjectInfo::CommonName);
    const auto &serialNumbers = leafCert.subjectInfo(QSslCertificate::SubjectInfo::SerialNumber);
    const auto &altNames = leafCert.subjectAlternativeNames();
    qInfo() << "Certificate for" << _resource << "has" << commonNames.size()
        << "common names," << serialNumbers.size() << "serial numbers, and"
        << altNames.size() << "subject alternative names";
    for(const auto &cn : commonNames)
    {
        qInfo() << " - CN:" << cn;
    }
    for(const auto &serial : serialNumbers)
    {
        qInfo() << " - Serial:" << serial;
    }
    for(auto itSan = altNames.begin(); itSan != altNames.end(); ++itSan)
    {
        qInfo() << " - SAN:" << *itSan << "- type:" << itSan.key();
    }
}

void NetworkTaskWithRetry::checkSslCertificate(QNetworkReply &reply,
                                               const BaseUri &baseUri,
                                               const QList<QSslError> &errors)
{
    // This shouldn't happen, we don't connect this slot if pCA or peerName are
    // not set, but check for robustness - do not risk possibly accepting any
    // peer name
    if(!baseUri.pCA || baseUri.peerVerifyName.isEmpty())
    {
        qWarning() << "Not ignoring" << errors.size()
            << "SSL errors in request for" << _resource
            << "- CA or peer name is not known";
        return;
    }

    const auto &certChain = reply.sslConfiguration().peerCertificateChain();
    if(certChain.isEmpty())
    {
        qWarning() << "No certificate provided by server for"
            << baseUri.peerVerifyName;
        return;
    }

    if(baseUri.pCA->verifyHttpsCertificate(certChain,
                                           baseUri.peerVerifyName))
    {
        qInfo() << "Accepted certificate for" << baseUri.peerVerifyName;
        traceLeafCert(certChain.first());
        reply.ignoreSslErrors();
    }
    else
    {
        qWarning() << "Rejected certificate for" << baseUri.peerVerifyName;
        traceLeafCert(certChain.first());
    }
}
