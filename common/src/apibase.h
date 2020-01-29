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
#line HEADER_FILE("apibase.h")

#ifndef APIBASE_H
#define APIBASE_H

#include <QSharedPointer>
#include <vector>

// Data used by both ApiBase and ApiBaseSequence - the actual base URIs and the
// last successful one.
// Note that this is not currently thread-safe; all API requests of any kind are
// handled on the main thread.
class COMMON_EXPORT ApiBaseData
{
public:
    ApiBaseData(std::vector<QString> baseUris);

public:
    unsigned getNextStartIndex() const {return _nextStartIndex;}
    unsigned getUriCount() const {return _baseUris.size();}
    const QString &getUri(unsigned index);
    void attemptSucceeded(unsigned successIndex);

private:
    std::vector<QString> _baseUris;
    unsigned _nextStartIndex;
};

// ApiBaseSequence keeps track of which base URI in an ApiBase was last used
// and returns each URI in sequence for subsequent attempts.
// If a request succeeds, it updates the last successful URI in ApiBase.
class COMMON_EXPORT ApiBaseSequence
{
public:
    ApiBaseSequence(QSharedPointer<ApiBaseData> pData);

public:
    const QString &getNextUri();
    void attemptSucceeded();

private:
    const QSharedPointer<ApiBaseData> _pData;
    // Index of the base URI currently being attempted
    unsigned _currentBaseUri;
};

// ApiBase describes a set of API base URIs, and it keeps track of the last one
// that was successful.
class COMMON_EXPORT ApiBase
{
public:
    ApiBase(std::vector<QString> baseUris);

public:
    // Get an ApiBaseSequence for a new request attempt.  It will start with the
    // URI base that most recently succeeded.  If a request succeeds, it updates
    // the last successful URI base for later attempts.
    ApiBaseSequence beginAttempt();
    unsigned getUriCount() const;

private:
    const QSharedPointer<ApiBaseData> _pData;
};

namespace ApiBases
{
    // Base URIs for normal API requests
    extern COMMON_EXPORT ApiBase piaApi;
    // Base URI for API requests that fetch the user's IP address.
    // This excludes API proxies because the IP address isn't fetched correctly when
    // a proxy is used.
    extern COMMON_EXPORT ApiBase piaIpAddrApi;
    // Base URIs for update metadata.
    // This is part of the PIA web API for the PIA brand, but for other brands
    // it is provided by that brand.
    extern COMMON_EXPORT ApiBase piaUpdateApi;
}

#endif
