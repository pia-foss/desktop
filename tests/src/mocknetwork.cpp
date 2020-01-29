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
#line SOURCE_FILE("mocknetwork.cpp")

#include "mocknetwork.h"

MockNetworkReplyDataless::MockNetworkReplyDataless()
{
    open(ReadOnly | Unbuffered);
}

void MockNetworkReplyDataless::finishRateLimit()
{
    setAttribute(QNetworkRequest::Attribute::HttpStatusCodeAttribute,
                 QVariant::fromValue(429));
    // QNetworkReply uses UnknownContentError in this case
    finishError(QNetworkReply::NetworkError::UnknownContentError);
}

void MockNetworkReplyDataless::finishError(QNetworkReply::NetworkError code)
{
    close();
    setError(code, QStringLiteral("Unit test error: %1").arg(qEnumToString(code)));
    emit finished();
}

std::deque<std::unique_ptr<QNetworkReply>> MockNetworkManager::_replyQueue;
ReplyConsumedSignal MockNetworkManager::_replyConsumed;

template<class ReplyT, class... ArgsT>
QPointer<ReplyT> MockNetworkManager::enqueueReplyImpl(ArgsT&&... args)
{
    auto pOwnedReply = std::make_unique<ReplyT>(std::forward<ArgsT>(args)...);
    QPointer<ReplyT> pReply{pOwnedReply.get()};
    _replyQueue.push_back(std::move(pOwnedReply));
    return pReply;
}

bool MockNetworkManager::hasNextReply()
{
    return !_replyQueue.empty();
}

void MockNetworkManager::clearQueuedReplies()
{
    _replyQueue.clear();
}

QPointer<MockNetworkReply> MockNetworkManager::enqueueReply(QByteArray body)
{
    return enqueueReplyImpl<MockNetworkReply>(std::move(body));
}

QPointer<MockNetworkReplyDataless> MockNetworkManager::enqueueReply()
{
    return enqueueReplyImpl<MockNetworkReplyDataless>();
}

QNetworkReply *MockNetworkManager::createRequest(QNetworkAccessManager::Operation op,
                                                 const QNetworkRequest &originalReq,
                                                 QIODevice *outgoingData)
{
    Q_UNUSED(op);
    Q_UNUSED(originalReq);
    Q_UNUSED(outgoingData);

    // Returning nullptr here would violate an invariant of
    // QNetworkAccessManager - the unit test has to queue up all replies
    // that are needed.
    Q_ASSERT(hasNextReply());

    std::unique_ptr<QNetworkReply> pConsumedReply = std::move(_replyQueue.front());
    // Class invariant - no nullptrs in _replyQueue
    Q_ASSERT(pConsumedReply);
    _replyQueue.pop_front();
    emit _replyConsumed.signal(originalReq);
    return pConsumedReply.release();
}
