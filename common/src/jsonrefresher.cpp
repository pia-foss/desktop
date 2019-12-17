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
#line SOURCE_FILE("jsonrefresher.cpp")

#include "jsonrefresher.h"
#include "networktaskwithretry.h"
#include "openssl.h"
#include <QNetworkReply>
#include <QDir>

JsonRefresher::JsonRefresher(QString name, ApiBase &apiBaseUris,
                             QString resource,
                             std::chrono::milliseconds initialInterval,
                             std::chrono::milliseconds refreshInterval,
                             QByteArray signatureKey)
    : _name{std::move(name)}, _apiBaseUris{apiBaseUris},
      _resource{std::move(resource)},
      _initialInterval{std::move(initialInterval)},
      _refreshInterval{std::move(refreshInterval)},
      _pNetworkManager{TestShim::create<QNetworkAccessManager>()},
      _signatureKey{std::move(signatureKey)}
{
    connect(&_refreshTimer, &QTimer::timeout, this,
            &JsonRefresher::refreshTimerElapsed);
    _refreshTimer.setInterval(static_cast<int>(_initialInterval.count()));
}

JsonRefresher::~JsonRefresher()
{
}

void JsonRefresher::refreshTimerElapsed()
{
    if(_pFetchTask)
    {
        qWarning() << "Refresh timer for" << _name << "elapsed, but fetch is still ongoing, skipping this interval";
        return;
    }

    // Fetch the resource.  Try each possible base URI one time.
    Async<QByteArray> pBodyTask = Async<QByteArray>{new NetworkTaskWithRetry{
                                        QNetworkAccessManager::GetOperation,
                                        _apiBaseUris, _resource,
                                        ApiRetries::counted(_apiBaseUris.getUriCount()),
                                        {}, {}, _pNetworkManager}};
    // Use next() instead of notify() so we can abandon the task (if the
    // JsonRefresher is stopped) by dropping our reference to the outermost
    // task.
    // Note that the stored task refers to the void result of our callback, not
    // to the QByteArray result of the body task.
    _pFetchTask = pBodyTask->next(this,
            [this](const Error& error, const QByteArray& body)
            {
                // We shouldn't get this signal if we're not running; we abandon
                // tasks when stopped.
                // Note that just ignoring calls when !isRunning() is not
                // sufficient, because we could be stopped and quickly
                // restarted, then still receive calls from the prior running
                // interval.
                Q_ASSERT(isRunning());

                // No longer needed
                _pFetchTask.reset();
                if (error)
                {
                    qWarning() << "Could not retrieve" << _name << "due to error:" << error;
                }
                else
                {
                    emitReply(body);
                }
            });
}

QJsonDocument JsonRefresher::readReply(QByteArray responsePayload) const
{
    // The response can optionally contain a GPG signature appended to the
    // end after a double newline. If one exists, verify that it matches
    // the supplied public key. For additional robustness though, we actually
    // split after the detected end of the JSON content (the last instance of
    // '}' or ']').

    QByteArray signature;
    if (!responsePayload.isEmpty() && (responsePayload.at(0) == '{' || responsePayload.at(0) == '['))
    {
        const char endCh = responsePayload.at(0) + 2; // ==('}'-'{')==(']'-'[')
        int end = responsePayload.lastIndexOf(endCh);
        if (end >= 0 && end != responsePayload.length() - 1)
        {
            if (end + 2 >= responsePayload.length() || responsePayload.at(end + 1) != '\n' || responsePayload.at(end + 2) != '\n')
                qWarning() << "Nonstandard appended data found after JSON response for" << _name;
            signature = QByteArray::fromBase64(responsePayload.mid(end + 1));
            responsePayload.truncate(end + 1);
        }
    }

    // If a key was supplied, check that there is a valid signature.
    if (!_signatureKey.isNull())
    {
        if (signature.isEmpty())
        {
            qError() << "Missing signature in response for" << _name;
            return {};
        }
        if (!verifySignature(_signatureKey, signature, responsePayload))
        {
            // Urgh; piaproxy.net alters content in-transit without re-signing it...
            // Make a single educated guess what the original content was.
            if (!verifySignature(_signatureKey, signature, QByteArray(responsePayload).replace(".piaproxy.net", ".privateinternetaccess.com")))
            {
                qError() << "Invalid signature in response for" << _name;
                return {};
            }
        }
        qInfo() << "Verified signature in response for" << _name;
    }
    else if (!signature.isEmpty())
    {
        qWarning() << "Unexpected signature found in response for" << _name;
    }

    // Parse the JSON response
    QJsonParseError parseError;
    const auto &jsonDoc = QJsonDocument::fromJson(responsePayload,
                                                         &parseError);
    if(jsonDoc.isNull())
    {
        qWarning() << "Could not parse" << _name << "due to error:"
            << parseError.error << "at position" << parseError.offset;
        qWarning() << "Retrieved JSON:" << responsePayload;
        return {};
    }

    // Got a result
    return jsonDoc;
}

void JsonRefresher::emitReply(QByteArray responsePayload)
{
    QJsonDocument doc{readReply(std::move(responsePayload))};
    if(!doc.isNull())
        emit contentLoaded(doc);
}

// This member function is a variant of start() but allows for initial data
// to be provided. This data can be useful to get things going if the user
// is experiencing network difficulties or the resource endpoint is otherwise
// unavailable.
void JsonRefresher::start(const QByteArray &initialData)
{
    // first process the provided data
    emitReply(initialData);

    // then start as usual
    start();
}

void JsonRefresher::startOrOverride(const QString &overridePath,
                                    const QString &bundledPath, bool haveCache)
{
    QFile overrideRegionFile{overridePath};

    // an override file exists, use it instead of the endpoint
    if (overrideRegionFile.open(QFile::ReadOnly))
    {
        qInfo() << "Loading" << _name << "from override file";
        QJsonParseError parseError;
        const auto &jsonDoc = QJsonDocument::fromJson(overrideRegionFile.readAll(), &parseError);
        if(parseError.error == QJsonParseError::NoError)
        {
            emit contentLoaded(jsonDoc);
            qInfo() << "Override for" << _name << "loaded successfully";
            return; // Don't start refreshes since regions are overridden
        }

        // The parse error is traced as a numeric code.
        // QJsonParseError::ParseError can't be bothered to provide Q_ENUM()...
        // This is just meant for internal use anyway.
        qInfo() << "Could not parse" << overrideRegionFile.fileName() << "for"
            << _name << "- error" << parseError.error << "at offset"
            << parseError.offset;
        // Regions are not overridden, so do normal initialization with the
        // servers.json and refreshes below.
    }

    QFile bundledRegionFile{bundledPath};
    // If a bundled file is present, and we don't have any cache yet, use it
    // as the initial data
    if (bundledRegionFile.open(QFile::ReadOnly) && !haveCache)
    {
        qInfo() << "Loading initial" << _name << "from bundled file";
        start(bundledRegionFile.readAll());
    }
    // just use the endpoint to fetch the regions
    else
    {
        qInfo() << "Loading regions from endpoint";
        start();
    }
}

void JsonRefresher::start()
{
    if(!isRunning())
    {
        // Issue a request for the resource right now.
        refreshTimerElapsed();
        // Start refreshing periodically.
        _refreshTimer.start();
    }
}

void JsonRefresher::stop()
{
    if(isRunning())
    {
        _refreshTimer.stop();
        // If there's an ongoing request right now, abandon it.
        if(_pFetchTask)
        {
            _pFetchTask->abandon();
            _pFetchTask.reset();
        }
    }
}

bool JsonRefresher::isRunning() const
{
    return _refreshTimer.isActive();
}

void JsonRefresher::refresh()
{
    // If the timer is running, restart it with the new interval.
    // QTimer doesn't say what exactly happens to the remaining time if we just
    // change the timer interval.  Here, we specifically want to issue a request
    // now and then wait the full _initialInterval - calling start() this way
    // is documented as restarting the timer if it was running before.
    if(isRunning())
    {
        // Issue a new request now
        refreshTimerElapsed();
        // Restart with the short interval
        _refreshTimer.start(static_cast<int>(_initialInterval.count()));
    }
    else
    {
        // Not running - don't start now, just change to the short interval if
        // the timer is restarted later.
        _refreshTimer.setInterval(static_cast<int>(_initialInterval.count()));
    }
}

void JsonRefresher::loadSucceeded()
{
    //A load succeeded.  If we were still using the shorter initial
    //interval, switch to the longer refresh interval.
    if(std::chrono::milliseconds{_refreshTimer.interval()} != _refreshInterval)
    {
        _refreshTimer.setInterval(static_cast<int>(_refreshInterval.count()));
    }
}
