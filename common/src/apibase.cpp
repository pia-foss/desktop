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

#include "apibase.h"
#include "brand.h"

ApiBaseData::ApiBaseData(std::vector<QString> baseUris)
    : _baseUris{std::move(baseUris)}, _nextStartIndex{0}
{
    // Ensure each URI ends with a slash
    for(auto &uri : _baseUris)
    {
        if(!uri.endsWith('/'))
            uri.append('/');
    }
}

const QString &ApiBaseData::getUri(unsigned index)
{
    Q_ASSERT(index < _baseUris.size());   // Guaranteed by caller
    return _baseUris[index];
}

void ApiBaseData::attemptSucceeded(unsigned successIndex)
{
    Q_ASSERT(successIndex < _baseUris.size());  // Guaranteed by caller
    _nextStartIndex = successIndex;
}

ApiBase::ApiBase(std::vector<QString> baseUris)
    : _pData{new ApiBaseData{std::move(baseUris)}}
{
}

ApiBaseSequence ApiBase::beginAttempt()
{
    Q_ASSERT(_pData);   // Class invariant
    return {_pData};
}

unsigned ApiBase::getUriCount() const
{
    Q_ASSERT(_pData);   // Class invariant
    return _pData->getUriCount();
}

ApiBaseSequence::ApiBaseSequence(QSharedPointer<ApiBaseData> pData)
    : _pData{std::move(pData)}
{
    Q_ASSERT(_pData);   // Guaranteed by caller

    // Back up _nextBaseUri relative to the start so getNextUri() can increment
    // it normally
    unsigned startIndex = _pData->getNextStartIndex();
    _currentBaseUri = startIndex ? startIndex-1 : _pData->getUriCount()-1;
}

const QString &ApiBaseSequence::getNextUri()
{
    Q_ASSERT(_pData);   // Class invariant
    _currentBaseUri = (_currentBaseUri + 1) % _pData->getUriCount();
    return _pData->getUri(_currentBaseUri);
}

void ApiBaseSequence::attemptSucceeded()
{
    Q_ASSERT(_pData);   // Class invariant
    _pData->attemptSucceeded(_currentBaseUri);
}

namespace ApiBases
{
    ApiBase piaApi
    {{
        QStringLiteral("https://www.privateinternetaccess.com/"),
        QStringLiteral("https://piaproxy.net/")
    }};

    ApiBase piaIpAddrApi
    {{
        QStringLiteral("https://www.privateinternetaccess.com/")
    }};

    // Update endpoints are determined by branding info, since brands are
    // responsible for hosting their own updates
    ApiBase piaUpdateApi
    {{
        BRAND_UPDATE_APIS
    }};
}
