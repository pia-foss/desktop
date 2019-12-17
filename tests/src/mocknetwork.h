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
#line HEADER_FILE("mocknetwork.h")

#ifndef MOCKNETWORK_H
#define MOCKNETWORK_H

#include "util.h"
#include <QNetworkReply>
#include <QBuffer>
#include <QTimer>
#include <QPointer>
#include <deque>

// A mock network reply that can be retruned by MockNetworkManager.  This
// stitches together a QNetworkReply with a QBuffer (an IO device backed by a
// QByteArray) to serve fake content as network replies.
//
// This implementation is probably not complete, but it is sufficient for unit
// tests.
class MockNetworkReply : public QNetworkReply
{
    Q_OBJECT

public:
    MockNetworkReply(QByteArray body)
        : _body{std::move(body)}, _buffer{&_body}
    {
        open(ReadOnly | Unbuffered);
        _buffer.open(ReadOnly);
    }

public:
    virtual void abort() override {}    // Nothing to do, already completed
    virtual qint64 bytesAvailable() const override {return _buffer.bytesAvailable();}
    virtual bool isSequential() const override {return true;}

    // Queue up a finished() signal.
    // The test can also emit finished() synchronously if desired.
    void queueFinished()
    {
        QTimer::singleShot(0, this, &MockNetworkReply::finished);
    }

protected:
    virtual qint64 readData(char *data, qint64 maxlen) override
    {
        return _buffer.read(data, maxlen);
    }

private:
    QByteArray _body;
    QBuffer _buffer;
};

// Mock network reply that doesn't provide any data - used for timeouts, rate
// limiting errors, etc.  It could probably also be used for empty GETs or
// HEADs, etc.
//
// Initially, the reply is open and acts as if it is waiting on data.
//  - abort() causes the the reply to abort (usually called by the code under
//    test).
//  - The various 'finish' methods end the reply with various other errors.
//  - finishRateLimit() causes the reply to end with a rate-limiting error
//  - finishAuth() causes the reply to end in an auth error
//  - finishNetError() causes the reply to end in a network error
//  - finishError() causes the reply to end in some other error
class MockNetworkReplyDataless : public QNetworkReply
{
    Q_OBJECT

public:
    MockNetworkReplyDataless();

public:
    virtual void abort() override {finishError(NetworkError::OperationCanceledError);}

    // If the stream is still open, act as if there's no data available, but
    // we're still waiting.  If it has been closed, return -1, since the stream
    // won't read any more data.
    virtual qint64 readData(char *, qint64) override {return isOpen() ? 0 : -1;}

    // End the reply with a rate limiting error.
    void finishRateLimit();
    // End the reply with an auth error.
    void finishAuthError() {finishError(QNetworkReply::NetworkError::AuthenticationRequiredError);}
    // End the reply with a generic network error.
    void finishNetError() {finishError(QNetworkReply::NetworkError::UnknownNetworkError);}
    // End the reply with any QNetworkReply error code.
    void finishError(QNetworkReply::NetworkError code);
};

// Class used to define the MockNetworkManager::_replyConsumed() signal.
// (StaticSignal can't be used because the Qt moc doesn't support template
// classes derived from QObject.)
class ReplyConsumedSignal : public QObject
{
    Q_OBJECT
signals:
    void signal(const QNetworkRequest &consumingReq);
};

class MockNetworkManager : public QNetworkAccessManager
{
    Q_OBJECT

private:
    // Outstanding replies to be consumed by consumeNextReply().
    static std::deque<std::unique_ptr<QNetworkReply>> _replyQueue;

public:
    // This signal can be observed to detect when a queued reply is consumed by
    // the code under test.
    static ReplyConsumedSignal _replyConsumed;

private:
    // Implementation of the various setNextReply*() methods
    template<class ReplyT, class... ArgsT>
    static QPointer<ReplyT> enqueueReplyImpl(ArgsT&&... args);

public:
    // Check whether a reply is currently queued.
    static bool hasNextReply();
    // Remove any queued replies.
    static void clearQueuedReplies();
    // Enqueue a byte array reply.  This creates a MockNetworkReply that will
    // respond with the data given.
    // A pointer to the new reply is returned, so the caller can choose when to
    // emit its 'finished()' signal, observe when it is destroyed, etc.
    // Keep in mind that the caller does not own the reply; it will be destroyed
    // when the code under test consumes and destroys it (or by
    // MockNetworkManager if it is replaced before being consumed).
    static QPointer<MockNetworkReply> enqueueReply(QByteArray body);
    // Enqueue a data-less reply.  This creates a MockNetworkReplyDataless,
    // which can be used to simulate a timeout or error result.
    static QPointer<MockNetworkReplyDataless> enqueueReply();

public:
    // createRequest() is overridden to take the QNetworkReply from pNextReply
    // and return it.  This implements the various operations
    virtual QNetworkReply *createRequest(QNetworkAccessManager::Operation op,
                                         const QNetworkRequest &originalReq,
                                         QIODevice *outgoingData) override;
};

#endif
