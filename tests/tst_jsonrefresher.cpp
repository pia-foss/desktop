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

#include "daemon/src/updatedownloader.h"
#include "testshim.h"
#include "src/mocknetwork.h"
#include <QtTest>

/*

=== JsonRefresher tests ===

These tests cover some behaviors of JsonRefresher.  Mainly, some corner cases
and potential race conditions are covered, the usual behavior of JsonRefresher
is generally straightforward to test manually.

Note that ApiBase (used by JsonRefresher) is covered in the ApiClient tests.

*/

namespace TestData {

const QByteArray &successJson = R"({"unit_test":true})";

std::shared_ptr<ApiBase> pUnitTestDummyApi =
    std::make_shared<FixedApiBase>(
        std::initializer_list<QString>{QStringLiteral("https://www.privateinternetaccess.com/")}
    );

}

// JsonRefresher with some dummy parameters filled in.
class TestRefresher : public JsonRefresher
{
public:
    TestRefresher()
        : JsonRefresher{QStringLiteral("Unit test"),
                        QStringLiteral("/unit_test"), std::chrono::seconds(1),
                        std::chrono::seconds(5)}
    {}
};

class tst_jsonrefresher : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        TestShim::installMock<QNetworkAccessManager, MockNetworkManager>();
    }

    // Test stopping a JsonRefresher with a request in-flight, which should
    // abandon and destroy the request.
    void testStopInFlight()
    {
        TestRefresher refresher;
        QSignalSpy consumeSpy{&MockNetworkManager::_replyConsumed, &ReplyConsumedSignal::signal};

        auto pReply = MockNetworkManager::enqueueReply();
        QSignalSpy replyDestroySpy(pReply.data(), &QObject::destroyed);
        refresher.start(TestData::pUnitTestDummyApi);
        // Should consume the reply
        QVERIFY(consumeSpy.wait(100));
        QVERIFY(!MockNetworkManager::hasNextReply());

        // Stop the refresher before completing the reply
        refresher.stop();

        // The reply object is destroyed asynchronously (QNetworkReply requires
        // this).
        QVERIFY(replyDestroySpy.wait());
    }

    // Test stopping a JsonRefresher with a request in-flight, then fulfill the
    // request anyway and verify that it's not signaled by JsonRefresher.
    void testCompleteAfterStop()
    {
        TestRefresher refresher;
        QSignalSpy fetchSpy{&refresher, &JsonRefresher::contentLoaded};
        QSignalSpy consumeSpy{&MockNetworkManager::_replyConsumed, &ReplyConsumedSignal::signal};

        auto pReply = MockNetworkManager::enqueueReply(TestData::successJson);
        refresher.start(TestData::pUnitTestDummyApi);
        // Should consume the reply
        QVERIFY(consumeSpy.wait(100));
        QVERIFY(!MockNetworkManager::hasNextReply());

        // Stop the refresher before completing the reply
        refresher.stop();

        // The reply object still exists right now since it's destroyed
        // asynchronously.  Fulfill it and make sure the JsonRefresher doesn't
        // signal anything.
        pReply->finished();
        // No signal expected (either sync or async)
        QVERIFY(fetchSpy.empty());
        QVERIFY(!fetchSpy.wait(1000));
    }
};

QTEST_GUILESS_MAIN(tst_jsonrefresher)
#include TEST_MOC
