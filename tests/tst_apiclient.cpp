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

#include "daemon/src/apiclient.h"
#include "testshim.h"
#include "src/mocknetwork.h"
#include <QtTest>

namespace TestData {

const QString user = QStringLiteral("p00000000");
const QString password = QStringLiteral("password");

// Password auth value using test data
QByteArray passwordAuth() {return ApiClient::passwordAuth(user, password);}

const QString status = QStringLiteral("status");

// Generic 'successful' JSON
const int successPort = 55947;
const QByteArray success{R"(
{
    "port": 55947
}
)"};

// Check that a QSignalSpy has exactly one signal; print a message if it doesn't.
bool checkSingle(const QSignalSpy &spy)
{
    if(spy.size() != 1)
    {
        qWarning() << "Got" << spy.size() << "signals, expected 1";
        return false;
    }

    return true;
}

// Check the error code of an ApiClient result.
bool checkError(const QSignalSpy &resultSpy, Error::Code code)
{
    if(!checkSingle(resultSpy))
        return false;

    // Verify that the Error code is correct.
    //
    // This is also used for success to check that the error code is Success.
    // QVariant::value() returns a default constructed value if the value isn't
    // of the expected type, so make sure it's really an Error to ensure that an
    // unexpected type results in a failure in that case.
    if(!resultSpy[0][0].canConvert<Error>())
    {
        qWarning() << "Result wasn't an error, expected code" << code << "- got"
            << resultSpy[0][0];
        return false;
    }

    if(resultSpy[0][0].value<Error>().code() != code)
    {
        qWarning() << "Expected error code" << code << "- got"
            << resultSpy[0][0].value<Error>();
        return false;
    }

    return true;
}

// Check a success result in a completed signal spy
bool checkSuccess(const QSignalSpy &resultSpy)
{
    // Verify that the Error is Success.
    if(!checkError(resultSpy, Error::Code::Success))
        return false;

    QJsonDocument resultJson = resultSpy[0][1].value<QJsonDocument>();
    // Check the arbitrary value in the response
    const auto &replyPort = resultJson[QStringLiteral("port")].toInt();
    if(replyPort != successPort)
    {
        qWarning() << "Unexpected JSON result:" << resultJson.toJson();
        return false;
    }

    return true;
}

// Check that a mock reply was consumed with the right hostname (used when
// verifying the API base memory)
bool checkConsumedHost(const QSignalSpy &consumedSpy, const QString &host)
{
    if(!checkSingle(consumedSpy))
        return false;

    const auto &url = consumedSpy[0][0].value<QNetworkRequest>().url();
    if(url.host() != host)
    {
        qWarning() << "Request was sent to" << url.host() << "- expected" << host;
        return false;
    }

    return true;
}

}

// Adapt a callback to a QObject signal so it can be spied in unit tests; used
// to implement CallbackSpy.
class CallbackAdapter : public QObject
{
    Q_OBJECT

public:
    // The callback functor returned by callback() - this is movable/copiable;
    // when called it causes CallbackAdapter::called() to be emitted
    class Callback
    {
    public:
        Callback(CallbackAdapter &adapter) : _pAdapter{&adapter} {}
        // Callback for non-void results
        template<class ResultT>
        void operator()(const Error &err, const ResultT &result) const
        {
            if(_pAdapter)
                emit _pAdapter->called(err, QVariant::fromValue(result));
        }
        // Callback for void results
        void operator()(const Error &err) const
        {
            if(_pAdapter)
                emit _pAdapter->called(err, {});
        }
    private:
        QPointer<CallbackAdapter> _pAdapter;
    };

signals:
    // Spy on the 'called' signal to observe when a callback is called.
    // QObject doesn't support templates, so the result here is in a QVariant.
    // This is normally used with QSignalSpy, so this doesn't really impact
    // usability.
    void called(const Error &err, const QVariant &result);

public:
    // Pass the object returned by callback() to a method expecting a callback
    // functor
    Callback callback() {return {*this};}
};

// Spy on a callback.
// Use callback() to get a callback functor to pass to an API, and use spy() to
// get the QSignalSpy hooked up to that callback.
// Also a QObject so it can be used as the owning object for callback.
class CallbackSpy : public QObject
{
public:
    CallbackSpy()
        : _adapter{}, _spy{&_adapter, &CallbackAdapter::called}
    {}

public:
    auto callback() {return _adapter.callback();}
    QSignalSpy &spy() {return _spy;}

private:
    CallbackAdapter _adapter;
    QSignalSpy _spy;
};

// An ApiClient with its own Environment.
class TestApiClient : public DaemonState, public Environment, public ApiClient
{
public:
    TestApiClient()
        : Environment{static_cast<DaemonState&>(*this)}
    {}
};

class tst_apiclient : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        TestShim::installMock<QNetworkAccessManager, MockNetworkManager>();
    }

    // Test rate limiting errors.
    // Rate limiting errors still allow retries, but they cause the ultimate
    // error emitted to be a rate limiting error if all retries fail.
    void testRateLimiting()
    {
        auto pReply1 = MockNetworkManager::enqueueReply();
        TestApiClient client;
        QSignalSpy consumeSpy{&MockNetworkManager::_replyConsumed, &ReplyConsumedSignal::signal};

        // Spy on completion
        CallbackSpy resultSpy;
        client.getRetry(*client.getApiv1(), TestData::status, TestData::passwordAuth())
            ->notify(&resultSpy, resultSpy.callback());
        QVERIFY(consumeSpy.wait(100));
        QVERIFY(!MockNetworkManager::hasNextReply());

        // Exhaust all retries with a mixture of rate limit and network errors
        auto pReply2 = MockNetworkManager::enqueueReply();
        pReply1->finishNetError();
        QVERIFY(consumeSpy.wait(100));
        QVERIFY(!MockNetworkManager::hasNextReply());
        auto pReply3 = MockNetworkManager::enqueueReply();
        pReply2->finishRateLimit();
        QVERIFY(consumeSpy.wait(100));
        QVERIFY(!MockNetworkManager::hasNextReply());
        auto pReply4 = MockNetworkManager::enqueueReply();
        pReply3->finishNetError();
        QVERIFY(consumeSpy.wait(100));
        QVERIFY(!MockNetworkManager::hasNextReply());
        pReply4->finishNetError();

        // Make sure we got a rate limiting error.
        QVERIFY(TestData::checkError(resultSpy.spy(), Error::Code::ApiRateLimitedError));
    }

    // Test an auth error.
    // Auth errors end immediately (no further retries) and return a specific
    // error code.
    void testAuthError()
    {
        auto pReply1 = MockNetworkManager::enqueueReply();
        TestApiClient client;
        QSignalSpy consumeSpy{&MockNetworkManager::_replyConsumed, &ReplyConsumedSignal::signal};

        // Spy on completion
        CallbackSpy resultSpy;
        client.getRetry(*client.getApiv1(), TestData::status, TestData::passwordAuth())
            ->notify(&resultSpy, resultSpy.callback());
        QVERIFY(consumeSpy.wait(100));
        QVERIFY(!MockNetworkManager::hasNextReply());

        // Mix a few errors in before the auth error
        auto pReply2 = MockNetworkManager::enqueueReply();
        pReply1->finishNetError();
        QVERIFY(consumeSpy.wait(100));
        QVERIFY(!MockNetworkManager::hasNextReply());
        auto pReply3 = MockNetworkManager::enqueueReply();
        pReply2->finishRateLimit();
        QVERIFY(consumeSpy.wait(100));
        QVERIFY(!MockNetworkManager::hasNextReply());
        // End with an auth error
        pReply3->finishAuthError();

        // Make sure we got a rate limiting error.
        QVERIFY(TestData::checkError(resultSpy.spy(), Error::Code::ApiUnauthorizedError));
    }

    // Test that ApiClient remembers the last successful API base.
    // Note that the API base data is held in the static ApiBase objects, so an
    // early failure in this test could be due to the state being unexpected
    // following a prior test.
    // (ApiBase does this by design; JsonRefresher shares the last-successful
    // state with ApiClient.)
    void testApiBaseMemory()
    {
        TestApiClient client;

        QSignalSpy consumeSpy{&MockNetworkManager::_replyConsumed, &ReplyConsumedSignal::signal};

        // Do a GET request.  Act as if www.privateinternetaccess.com is not
        // reachable, but piaproxy.net is.
        auto pHeadReply1 = MockNetworkManager::enqueueReply();
        CallbackSpy headSpy;
        client.getRetry(*client.getApiv1(), TestData::status, TestData::passwordAuth())
            ->notify(&headSpy, headSpy.callback());
        QVERIFY(consumeSpy.wait(100));
        QVERIFY(TestData::checkConsumedHost(consumeSpy, QStringLiteral("www.privateinternetaccess.com")));
        consumeSpy.clear();

        auto pHeadReply2 = MockNetworkManager::enqueueReply(TestData::success);
        // Host is unreachable
        pHeadReply1->finishNetError();
        QVERIFY(consumeSpy.wait(100));
        QVERIFY(TestData::checkConsumedHost(consumeSpy, QStringLiteral("www.piaproxy.net")));
        consumeSpy.clear();

        // Success on the alternate host
        emit pHeadReply2->finished();
        // Just check that the error code is Success, there's no result for HEAD
        QVERIFY(TestData::checkError(headSpy.spy(), Error::Code::Success));

        // Now, do a GET and verify that it starts on piaproxy.net.
        CallbackSpy getSpy;
        auto pGetReply = MockNetworkManager::enqueueReply(TestData::success);
        client.getRetry(*client.getApiv1(), TestData::status, TestData::passwordAuth())
            ->notify(&getSpy, getSpy.callback());
        QVERIFY(consumeSpy.wait(100));
        QVERIFY(TestData::checkConsumedHost(consumeSpy, QStringLiteral("www.piaproxy.net")));
        consumeSpy.clear();

        emit pGetReply->finished();
        QVERIFY(TestData::checkSuccess(getSpy.spy()));
    }
};

QTEST_GUILESS_MAIN(tst_apiclient)
#include TEST_MOC
