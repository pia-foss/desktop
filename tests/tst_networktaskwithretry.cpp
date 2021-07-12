// Copyright (c) 2021 Private Internet Access, Inc.
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

#include "networktaskwithretry.h"
#include "testshim.h"
#include "src/mocknetwork.h"
#include "src/callbackspy.h"
#include <QtTest>

namespace
{
    // Various API bases used to test redirects with different scheme/host/port
    // differences
    FixedApiBase noPortBase{QStringLiteral("https://redir.example.com/")};
    FixedApiBase defPortBase{QStringLiteral("https://redir.example.com:443/")};
    FixedApiBase otherPortBase{QStringLiteral("https://redir.example.com:8443/")};
    
    // A dummy resource and retry strategy used by testGet(), doesn't matter for
    // most tests
    const QString testResource{QStringLiteral("resource")};
    
    Async<QByteArray> testGet(ApiBase &baseUris)
    {
        return Async<NetworkTaskWithRetry>::create(
            QNetworkAccessManager::Operation::GetOperation, baseUris,
            testResource, ApiRetries::counted(1), QJsonDocument{},
            QByteArray{});
    }
    
    QByteArray successJson{QByteArrayLiteral(R"({"unittest":true})")};
    
    bool checkSuccessResponse(const CallbackSpy &spy)
    {
        return spy.checkSuccessValue<QByteArray>([](const QByteArray &result)
            {
                return result == successJson;
            });
    }
    
    void testSuccessRedirect(ApiBase &baseUris, const QString &location)
    {
        qInfo() << "Test successful redirect to" << location;
    
        QSignalSpy consumeSpy{&MockNetworkManager::_replyConsumed, &ReplyConsumedSignal::signal};

        auto pReply1 = MockNetworkManager::enqueueReply(successJson);
        QSignalSpy reply1RedirAllow{pReply1.data(), &QNetworkReply::redirectAllowed};
        CallbackSpy result1Spy;
        testGet(baseUris)->notify(&result1Spy, result1Spy.callback());
        QVERIFY(consumeSpy.wait(100));
        
        // Redirect to another resource, this should be allowed.
        emit pReply1->redirected(location);
        // NetworkTaskWithRetry accepted this redirect
        QCOMPARE(reply1RedirAllow.count(), 1);
        
        // Finish the request
        emit pReply1->finished();
        QVERIFY(checkSuccessResponse(result1Spy));
    }
    
    void testFailRedirect(ApiBase &baseUris, const QString &location)
    {
        qInfo() << "Test rejected redirect to" << location;
    
        QSignalSpy consumeSpy{&MockNetworkManager::_replyConsumed, &ReplyConsumedSignal::signal};

        auto pReply1 = MockNetworkManager::enqueueReply();
        QSignalSpy reply1RedirAllow{pReply1.data(), &QNetworkReply::redirectAllowed};
        CallbackSpy result1Spy;
        testGet(baseUris)->notify(&result1Spy, result1Spy.callback());
        QVERIFY(consumeSpy.wait(100));
        
        // Redirect to another resource, this should be rejected.
        emit pReply1->redirected(location);
        // NetworkTaskWithRetry rejected this redirect
        QCOMPARE(reply1RedirAllow.count(), 0);
        QVERIFY(result1Spy.checkError(Error::Code::ApiNetworkError));
    }
}

class tst_networktaskwithretry : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        TestShim::installMock<QNetworkAccessManager, MockNetworkManager>();
    }
    
    // Test redirects that are accepted by NetworkTaskWithRetry (must have
    // scheme 'https' and same origin)
    void testNoPortMatch()
    {
        testSuccessRedirect(noPortBase, QStringLiteral("https://redir.example.com/redir_resource"));
    }
    void testPortMatch()
    {
        testSuccessRedirect(otherPortBase, QStringLiteral("https://redir.example.com:8443/redir_resource"));
    }
    void testDefToExplicitPortMatch()
    {
        testSuccessRedirect(noPortBase, QStringLiteral("https://redir.example.com:443/redir_resource"));
    }
    void testExplicitToDefPortMatch()
    {
        testSuccessRedirect(defPortBase, QStringLiteral("https://redir.example.com/redir_resource"));
    }
    // Relative URIs are accepted
    void testRelativeMatch()
    {
        testSuccessRedirect(noPortBase, QStringLiteral("/redir_resource"));
    }
    // Scheme-relative URIs are accepted only if the host still matches
    void testSchemeRelativeMatch()
    {
        testSuccessRedirect(noPortBase, QStringLiteral("//redir.example.com/redir_resource"));
    }
    // Excessive "../" in a relative redirect are just ignored, this still
    // matches since the host is unchanged
    void testExcessiveDotsMatch()
    {
        testSuccessRedirect(noPortBase, QStringLiteral("../../../../../redir_resource"));
    }
    
    // Test basic failures where one of the scheme/host/port does not match
    void testSchemeFailureHttp()
    {
        testFailRedirect(noPortBase, QStringLiteral("http://redir.example.com/redir_resource"));
    }
    void testSchemeFailureOther()
    {
        testFailRedirect(noPortBase, QStringLiteral("ftp://redir.example.com/redir_resource"));
    }
    void testSubdomainHostFailure()
    {
        testFailRedirect(noPortBase, QStringLiteral("https://other.redir.example.com/redir_resource"));
    }
    void testUnrelatedHostFailure()
    {
        testFailRedirect(noPortBase, QStringLiteral("https://unrelated.test/redir_resource"));
    }
    void testExplicitPortFailure()
    {
        testFailRedirect(noPortBase, QStringLiteral("https://redir.example.com:444/redir_resource"));
    }
    void testDefaultPortFailure()
    {
        testFailRedirect(otherPortBase, QStringLiteral("https://redir.example.com/redir_resource"));
    }
    void testMismatchPortFailure()
    {
        testFailRedirect(otherPortBase, QStringLiteral("https://redir.example.com:8444/redir_resource"));
    }
    
    // Scheme-relative URIs are rejected if the host or port do not match
    void testRelativeHostFailure()
    {
        testFailRedirect(noPortBase, QStringLiteral("//other.example.com/redir_resource"));
    }
    void testRelativePortFailure()
    {
        testFailRedirect(noPortBase, QStringLiteral("//redir.example.com:444/redir_resource"));
    }
    
    // Test some garbage URIs
    void testNoHostFailure()
    {
        testFailRedirect(noPortBase, QStringLiteral("https:///redir_resource"));
    }
    void testBadUriFailure()
    {
        testFailRedirect(noPortBase, QStringLiteral("https:redir_resource"));
    }
};

QTEST_GUILESS_MAIN(tst_networktaskwithretry)
#include TEST_MOC
