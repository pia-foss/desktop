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
    return *_pAccessManager;
}
