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
#line HEADER_FILE("apibase.h")

#ifndef APIBASE_H
#define APIBASE_H

#include "openssl.h"
#include <QSharedPointer>
#include <vector>
#include <initializer_list>

// ApiBase is used to describe the different endpoints where various APIs can be
// reached.  PIA Desktop uses several different APIs, each of which have
// different lists of API bases (the major APIs and bases are set up in
// environment.cpp, a few one-time-use APIs are set up elsewhere, such as the
// WireGuard key push API in wireguardmethod.cpp).
//
// In particular, ApiBase must be able to describe the endpoints for:
// - The web API, reachable via the PIA origin or a proxy
// - IP address APIs, reachable via the PIA origin only (the proxy can't be
//   used, it gives the IP of the proxy)
// - The PIA WireGuard key push API and modern port forward API, where the
//   certificate is signed by a PIA CA using a CN from the servers list, and the
//   IP addresses are determined at connection time
// - The modern servers list API, reachable via servers described in the servers
//   list itself (for the Meta service).  These servers are grouped into
//   regions and may have more than one endpoint per region.
//
// API bases can also be overridden for testing; see environment.cpp.

// BaseUri describes a single API endpoint.  A BaseUri is selected for each
// attempt by calling ApiBaseSequence::beginAttempt().
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
    // Create an ApiBaseData with any combination of base URIs
    ApiBaseData(std::vector<BaseUri> baseUris);

private:
    // Append a base URI, and ensure that it ends with '/' - used by constructors
    void addBaseUri(const QString &uri, std::shared_ptr<PrivateCA> pCA,
                    const QString &peerVerifyName);

public:
    unsigned getNextStartIndex() const {return _nextStartIndex;}
    unsigned getUriCount() const {return _baseUris.size();}
    // The base URI is returned by value, since the ApiBaseData can be destroyed
    // if the API base is updated.  This contains QStrings and a shared_ptr, so
    // copies aren't too expensive.
    BaseUri getUri(unsigned index);
    void attemptSucceeded(unsigned successIndex);

private:
    std::vector<BaseUri> _baseUris;
    unsigned _nextStartIndex;
};

// ApiBaseSequence keeps track of the base URIs being used for a particular
// request.  If an attempt is successful, it updates the last successful URI in
// the ApiBaseData to speed up later requests using the same API base.
class COMMON_EXPORT ApiBaseSequence
{
public:
    ApiBaseSequence(QSharedPointer<ApiBaseData> pData);

public:
    BaseUri getNextUri();
    void attemptSucceeded();

private:
    const QSharedPointer<ApiBaseData> _pData;
    // Index of the base URI currently being attempted
    unsigned _currentBaseUri;
};

// ApiBase describes the base APIs for a resource; it provides a way to get an
// ApiBaseSequence containing the sequence of base URIs to try for a particular
// request.
//
// Many resources use a FixedApiBase, which has a fixed set of URIs, and keeps
// track of the last successful one to speed up later requests.  Some resources
// use dynamically built API bases; the base URIs are generated from the servers
// list each time a request occurs.
class COMMON_EXPORT ApiBase
{
public:
    virtual ~ApiBase() = default;

    // Get an ApiBaseSequence for a new request attempt.  It will start with the
    // URI base that most recently succeeded.  If a request succeeds, it updates
    // the last successful URI base for later attempts.
    virtual ApiBaseSequence beginAttempt() = 0;
    // Get the number of attempts we should make when using a counted retry
    // strategy with this API base, for the preferred attempts per base given.
    //
    // Normally this is <number of bases> * <attempts per base>, but a
    // dynamically built ApiBase might not always know the exact number of
    // bases; it'll use an estimate here to determine the attempt count.
    virtual unsigned getAttemptCount(unsigned attemptsPerBase) = 0;
};

// ApiBase using a fixed set of base URIs.
class COMMON_EXPORT FixedApiBase : public ApiBase
{
public:
    template<class... Arg>
    FixedApiBase(Arg &&... args) : _pData{new ApiBaseData{std::forward<Arg>(args)...}} {}

public:
    virtual ApiBaseSequence beginAttempt() override;
    virtual unsigned getAttemptCount(unsigned attemptsPerBase) override;

private:
    const QSharedPointer<ApiBaseData> _pData;
};

#endif
