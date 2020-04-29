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

#include "openssl.h"
#include <QSharedPointer>
#include <vector>
#include <initializer_list>

struct COMMON_EXPORT BaseUri
{
    // The URI prefix to use in an API request (typically like
    // "https://<host>/", may have a partial URI path)
    QString uri;
    // An optional PrivateCA that will be used to validate the certificate
    // instead of the default.  If specified, peerVerifyName must also be set.
    std::shared_ptr<PrivateCA> pCA;
    // The peer verify name to use when checking the host's certificate.
    // Requires pCA, and must be set if pCA is set.
    QString peerVerifyName;
};

// Data used by both ApiBase and ApiBaseSequence - the actual base URIs and the
// last successful one.
// Note that this is not currently thread-safe; all API requests of any kind are
// handled on the main thread.
class COMMON_EXPORT ApiBaseData
{
public:
    // Create ApiBaseData with plain base URIs - no overridden peer names.
    ApiBaseData(const std::initializer_list<QString> &baseUris);
    ApiBaseData(const std::vector<QString> &baseUris);
    // Create ApiBaseData with one host, with an optional CA and peer name.
    ApiBaseData(const QString &uri, std::shared_ptr<PrivateCA> pCA,
                const QString &peerVerifyName);

private:
    // Append a base URI, and ensure that it ends with '/' - used by constructors
    void addBaseUri(const QString &uri, std::shared_ptr<PrivateCA> pCA,
                    const QString &peerVerifyName);

public:
    unsigned getNextStartIndex() const {return _nextStartIndex;}
    unsigned getUriCount() const {return _baseUris.size();}
    const BaseUri &getUri(unsigned index);
    void attemptSucceeded(unsigned successIndex);

private:
    std::vector<BaseUri> _baseUris;
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
    const BaseUri &getNextUri();
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
    template<class... Arg>
    ApiBase(Arg &&... args) : _pData{new ApiBaseData{std::forward<Arg>(args)...}} {}

public:
    // Get an ApiBaseSequence for a new request attempt.  It will start with the
    // URI base that most recently succeeded.  If a request succeeds, it updates
    // the last successful URI base for later attempts.
    ApiBaseSequence beginAttempt();
    unsigned getUriCount() const;

private:
    const QSharedPointer<ApiBaseData> _pData;
};

#endif
