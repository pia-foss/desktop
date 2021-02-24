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

#include "common.h"
#line HEADER_FILE("apinetwork.h")

#ifndef APINETWORK_H
#define APINETWORK_H

#include <QNetworkAccessManager>
#include <QNetworkConfigurationManager>

// ApiNetwork keeps track of the local network address that we need to use for
// API requests (such as server lists, web API, port forwarding/MACE).
//
// When an address is set, it creates a local proxy that we use for outgoing
// requests, in order to bind outgoing connections to that interface.
// (QNetworkAccessManager does not provide any way to bind its outgoing
// connections.)
class COMMON_EXPORT ApiNetwork : public QObject, public AutoSingleton<ApiNetwork>
{
    Q_OBJECT

public:
    // Initially, ApiNetwork uses any network interface.
    ApiNetwork();

public:
    // Use the specified proxy for future network requests.
    void setProxy(QNetworkProxy proxy);
    // Stop using a proxy for future requests.
    void clearProxy();

    // Get the shared QNetworkAccessManager.  This object remains valid until
    // static destruction.
    QNetworkAccessManager &getAccessManager() const;

private:
    // The QNetworkAccessManager used for all connections.  Dynamically
    // allocated so it can be mocked in unit tests.
    std::unique_ptr<QNetworkAccessManager> _pAccessManager;
};

#endif
