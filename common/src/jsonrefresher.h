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
#line HEADER_FILE("jsonrefresher.h")

#ifndef JSONREFRESHER_H
#define JSONREFRESHER_H

#include "apibase.h"
#include "async.h"
#include "testshim.h"
#include <QObject>
#include <QJsonDocument>
#include <QByteArray>
#include <QSharedPointer>
#include <QTimer>


// Periodically loads a JSON payload over HTTP(S).
//
// Initially, uses a fast interval until at least one load succeeds.  Then, uses
// a slow interval to periodically refresh the content.
//
// Multiple URIs can be provided as alternates, each attempt will try each
// alternate until one succeeds or all have failed.  If an attempt succeeds,
// that URI will be the first one tried for subsequent attempts.
//
// The JSON payload is expected to have a GPG signature if signatureKey is set.
class COMMON_EXPORT JsonRefresher : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("jsonrefresher")

public:
    // JsonRefresher expects apiBaseUris to outlive this object.  (This is
    // normally a static storage duration object from apibase.cpp.)
    JsonRefresher(QString name, ApiBase &apiBaseUris, QString resource,
                  std::chrono::milliseconds initialInterval,
                  std::chrono::milliseconds refreshInterval,
                  QByteArray signatureKey = {});
    ~JsonRefresher();

private:
    void refreshTimerElapsed();
    // Read a reply payload into a QJsonDocument, including validating the
    // signature if a key is configured on this JsonRefresher.  If the response
    // can't be read for any reason, returns a null QJsonDocument.
    QJsonDocument readReply(QByteArray responsePayload) const;
    // Read a reply, and emit it to contentLoaded() if successful.
    void emitReply(QByteArray responsePayload);

public:
    // Start trying to load the resource.
    void start();

    // Start with initial data, then continue trying to load the resource as in start()
    void start(const QByteArray &initialData);
    // Check for an override file, bundled seed file, and load or start the
    // refresher.  'haveCache' indicates whether the caller has an existing
    // cache from a prior run, if so the bundled seed file will not be used.
    //
    // - If an override file is loaded, it is used and the refresher is not
    //   started.
    // - Otherwise, if no data are cached, and the bundled file can be loaded,
    //   use that and start the refresher.
    // - Otherwise, just start the refresher - there may be no data available
    //   until it loads the resource.
    void startOrOverride(const QString &overridePath,
                         const QString &bundledPath, bool haveCache);
    // Stop refreshing the resource.  If a request was in-flight, it is
    // canceled (contentLoaded() cannot be emitted while stopped).
    void stop();

    bool isRunning() const;

    // Refresh the resource now (we believe it has likely changed, the user
    // asked to refresh, etc.)
    //
    // Switches back to the short interval.  If JsonRefresher is started,
    // issues a request now also.  (If it's not started, it will use the short
    // interval again the next time it is started.)
    void refresh();

    // Call loadSucceeded() to indicate that data were successfully loaded from
    // a result emitted by contentLoaded().  This switches to the long interval
    // if we were using the short interval.
    //
    // This isn't implicitly done when contentLoaded is emitted, because there
    // may be resource-specific validation done on the JSON body.
    void loadSucceeded();

signals:
    // Emitted any time the content of the resource is successfully loaded.
    void contentLoaded(const QJsonDocument &content);

public:
    QString _name;
    ApiBase &_apiBaseUris;
    QString _resource;
    std::chrono::milliseconds _initialInterval, _refreshInterval;
    QTimer _refreshTimer;
    // If a fetch task is ongoing, it's held here.  Dropping this reference
    // abandons the task.
    Async<void> _pFetchTask;
    QByteArray _signatureKey;
};

#endif
