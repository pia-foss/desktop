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
#line HEADER_FILE("openssl.h")

#ifndef OPENSSL_H
#define OPENSSL_H
#pragma once

#include <QByteArray>
#include <QCryptographicHash>
#include <QList>
#include <QSslCertificate>

bool COMMON_EXPORT verifySignature(const QByteArray& publicKeyPem, const QByteArray& signature, const QByteArray& data);

// PrivateCA can be used to validate a certificate chain issued from a private
// CA certificate.
class COMMON_EXPORT PrivateCA
{
private:
    struct data;

public:
    // Create PrivateCA with the PEM content of the cert file
    PrivateCA(const QByteArray &caCertPem);
    ~PrivateCA();

public:
    // Verify that an HTTPS certificate is valid for the peer name given, and
    // was issued by this CA. The allowExpired parameter permits an expired
    // certificate while still validating that it was issued by the correct
    // CA, and that it should only be used for unit tests so that the unit
    // test certificates don't have to be rotated
    bool verifyHttpsCertificate(const QList<QSslCertificate> &certChain,
                                const QString &peerName,
                                bool allowExpired = false);

private:
    std::unique_ptr<data> _pData;
};

enum
{
    // Required buffer size for public / private keys in genCurve25519KeyPair()
    Curve25519KeySize = 32,
};
bool COMMON_EXPORT genCurve25519KeyPair(unsigned char *pPubkey, unsigned char *pPrivkey);

#endif // OPENSSL_H
