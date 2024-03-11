// Copyright (c) 2024 Private Internet Access, Inc.
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
#line SOURCE_FILE("apinetwork.cpp")

#include "apinetwork.h"
#include "testshim.h"
#include <algorithm>
#include <QNetworkProxyFactory>
#include <atomic>

namespace
{
    // Counter used to vary the proxy username.
    //
    // QNetworkAccessManager caches connections, and we can no longer clear the
    // cache for every attempt like we did in Qt 5.11 - since 5.12 clearing the
    // cache terminates in-flight requests, because it kills the worker thread.
    // (It has to kill the thread, because the caches now are thread-local
    // objects on that thread.)
    //
    // The proxy username is included in the cache key, so we can trick it into
    // never reusing connections by varying the proxy username, at least when
    // the proxy is active.
    //
    // Clearing the cache when we know we're connecting/disconnecting is also
    // beneficial, but this is difficult to time sufficiently well to guarantee
    // that reusing connections is safe, particularly given that some VPN
    // interface setup is done asynchronously (such as configuring the interface
    // IP with WireGuard on Windows).
    //
    // This is atomic in case it might be used from QNAM's HTTP worker thread.
    std::atomic<std::uint32_t> proxyUsernameCounter;

    // A QNetworkProxyFactory that always returns the same proxy, but with a
    // varying username to trick the QNAM connection cache.  See
    // ApiNetwork::setProxy().
    class UsernameCounterProxyFactory : public QNetworkProxyFactory
    {
    public:
        UsernameCounterProxyFactory(QNetworkProxy proxy) : _proxy{std::move(proxy)} {}

    public:
        virtual QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &) override
        {
            QNetworkProxy result{_proxy};
            std::uint32_t counter = proxyUsernameCounter++;
            result.setUser(result.user() + QString::number(counter));
            return {std::move(result)};
        }

    private:
        const QNetworkProxy _proxy;
    };
}

ApiNetwork::ApiNetwork()
{
    // By default, Qt Bearer Management will try to poll network interfaces
    // every 10 seconds.  We don't care about this functionality (it doesn't
    // really even work), so disable it by overriding the poll interval with -1.
    // (This does work, internally a QTimer is set with an interval of -1, which
    // is rejected by QObject::setTimer with a warning.)
    //
    // The bearer management functionality is anecdotally reported to have a
    // negative impact on the system (lots of wakes for the bearer thread,
    // latency spikes during the interface polls, etc.), and it doesn't really
    // work anyway - https://bugreports.qt.io/browse/QTBUG-65586
    //
    // This must be done before creating any QNetworkAccessManager objects.
    qputenv("QT_BEARER_POLL_TIMEOUT", "-1");
    _pAccessManager.reset(TestShim::create<QNetworkAccessManager>());
}

void ApiNetwork::setProxy(QNetworkProxy proxy)
{
    // If we were to set the proxy with QNetworkAccessManager::setProxy(), then
    // it will assume that the proxy can only handle one request at a time.
    // That means that after connecting, when we try to refresh all regions
    // lists (as well as PF, login, etc.), only one request will go through at
    // a time, and the others could time out before they get a chance to use the
    // proxy.
    //
    // Insead, use a proxy factory, which returns a new QNetworkProxy object for
    // every request (with the same proxy configuration every time).  This
    // allows all requests to use the proxy at the same time.
    //
    // Additionally, this proxy factory varies the username in order to trick
    // the QNAM connection cache.
    getAccessManager().setProxyFactory(new UsernameCounterProxyFactory{std::move(proxy)});
    // Clear the connection cache now.  It's possible that ongoing request might
    // actually complete in this case, but since we're starting the proxy we
    // want to abandon them anyway.
    getAccessManager().clearConnectionCache();
}

void ApiNetwork::clearProxy()
{
    getAccessManager().setProxyFactory(nullptr);
    // Clear the connection cache now.  This kills any ongoing requests, but
    // since we're shutting down the proxy, that's fine.
    getAccessManager().clearConnectionCache();
}

QNetworkAccessManager &ApiNetwork::getAccessManager() const
{
    Q_ASSERT(_pAccessManager);  // Class invariant

    // Additionally, Qt 5.15 on Mac added even more network state monitoring
    // beyond the "bearer management" stuff worked around in the constructor.
    // This doesn't work either - it never seems to update the state after the
    // app starts, which means if the network isn't up when the daemon starts,
    // we can never send API requests even after the network comes up.
    //
    // Strangely, it tracks the "accessibility" state reported by
    // QNetworkStatusMonitor in QNetworkAccessManagerPrivate::networkAccessible,
    // which is also used to allow the app to override the network
    // accessibility.  This probably means that manually overriding the network
    // state wouldn't really work, but fortunately we can work around the
    // QNetworkStatusMonitor issues by manually setting the state to Accessible
    // before any request.
    //
    // Setting it once might be OK if QNetworkStatusMonitor never emits any
    // updates, but setting it for each request will also cover us if it does
    // ever emit an update.
    _pAccessManager->setNetworkAccessible(QNetworkAccessManager::NetworkAccessibility::Accessible);

    return *_pAccessManager;
}

template class COMMON_EXPORT AutoSingleton<ApiNetwork>;
