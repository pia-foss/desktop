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
#line SOURCE_FILE("apinetwork.cpp")

#include "apinetwork.h"
#include <testshim.h>
#include <algorithm>

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

void ApiNetwork::setProxy(const QNetworkProxy &proxy)
{
    getAccessManager().setProxy(proxy);
}

QNetworkAccessManager &ApiNetwork::getAccessManager() const
{
    Q_ASSERT(_pAccessManager);  // Class invariant

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
    _pAccessManager->clearConnectionCache();

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
