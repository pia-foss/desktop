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

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "common.h"
#include "util.h"
#include "apibase.h"
#include "openssl.h"
#include "settings/daemonstate.h"
#include <unordered_map>

// Environment loads constant data defining the production environment, with
// possible overrides to test with a staging or local environment.  This
// includes certificate authorities, API bases, and signing keys.
//
// Override files all require root/Administrator access to create or modify.
// (Users with root/Administrator privileges could also alter the executables
// directly, so this introduce any new security considerations, it just
// facilitates testing.)
class Environment : public QObject
{
    Q_OBJECT

public:
    static const QByteArray defaultRegionsListPublicKey;

public:
    Environment(const DaemonState &state);

private:
    // Read a file - can be an actual disk file or a Qt resource.
    QByteArray readFile(const QString &path);

    // Load a certificate authority, either from the override file if present or
    // from the default resource.
    void loadCertificateAuthority(const QString &handshakeSetting,
                                  const QString &filename);

    // Load the regions list public key, either from the override or the
    // default.
    void loadRegionsListPublicKey();

    // Apply the override value to an ApiBase if it's present and valid (used
    // by loadApiBase())
    void applyApiBaseOverride(bool overridePresent,
                              const QJsonDocument &apiOverride,
                              std::shared_ptr<ApiBase> &pApiBase,
                              const QString &jsonKey,
                              const QString &resourceName);

    // Load a fixed API base - applies the override or uses the defaults given
    void loadApiBase(bool overridePresent, const QJsonDocument &apiOverride,
                     std::shared_ptr<ApiBase> &pApiBase, const QString &jsonKey,
                     const QString &resourceName,
                     const std::initializer_list<QString> &defaults);
    // Load a dynamic API base - applies the override, or uses a
    // MetaServiceApiBase to utilize both Meta service proxies and fixed
    // fallbacks
    void loadDynamicApiBase(bool overridePresent, const QJsonDocument &apiOverride,
                            std::shared_ptr<ApiBase> &pApiBase, const QString &jsonKey,
                            const QString &resourceName, const QString &dynamicBasePath,
                            const std::initializer_list<QString> &defaults);

    // Load the API bases
    void loadApiBases();

public:
    // Reload the environment and re-check overrides.  Emits overrideActive()
    // for each override that is loaded, or overrideFailed() for each override
    // that is present but couldn't be loaded.
    void reload();

    // Get a certificate authority.  The default is returned if the type is not
    // known.  (PEM format; line endings are normalized to '\n' for consistency
    // in the OpenVPN config file, even on Windows.)
    QByteArray getCertificateAuthority(const QString &type) const;

    // Get a shared PrivateCA for the RSA-4096 CA.
    const std::shared_ptr<PrivateCA> &getRsa4096CA() const {return _pRsa4096CA;}

    // Get the regions list public key (PEM format)
    QByteArray getRegionsListPublicKey() const {return _regionsListPublicKey;}

    // Get the API bases.  These always return a non-nullptr shared_ptr.
    // Base URIs for normal API requests.
    const std::shared_ptr<ApiBase> &getApiv1() {Q_ASSERT(_pApiv1); return _pApiv1;}
    // Base URIs for authv2.  This has the same defaults as the PIA API, but it
    // also can leverage meta services in next-gen.
    const std::shared_ptr<ApiBase> &getApiv2() {Q_ASSERT(_pApiv2); return _pApiv2;}
    // Base URIs for the modern regions list.
    const std::shared_ptr<ApiBase> &getModernRegionsListApi() {Q_ASSERT(_pModernRegionsListApi); return _pModernRegionsListApi;}
    // Base URI for API requests that fetch the user's IP address.
    // This excludes API proxies because the IP address isn't fetched correctly when
    // a proxy is used.
    const std::shared_ptr<ApiBase> &getIpAddrApi() {Q_ASSERT(_pIpAddrApi); return _pIpAddrApi;}
    // Base URI for API requests using the proxy only.  This complements the
    // IP address API - we'll still check the proxy to determine Internet
    // connectivity, but the IP and status reported will not be used.
    const std::shared_ptr<ApiBase> &getIpProxyApi() {Q_ASSERT(_pIpProxyApi); return _pIpProxyApi;}
    // Base URIs for update metadata.
    // This is part of the PIA web API for the PIA brand, but for other brands
    // it is provided by that brand.
    const std::shared_ptr<ApiBase> &getUpdateApi() {Q_ASSERT(_pUpdateApi); return _pUpdateApi;}

signals:
    // An override was loaded by reload().
    void overrideActive(const QString &resource);
    // An override is present during reload(), but couldn't be loaded.
    void overrideFailed(const QString &resource);

private:
    const DaemonState &_state;
    std::unordered_map<QString, QByteArray> _authorities;
    std::shared_ptr<PrivateCA> _pRsa4096CA;
    QByteArray _regionsListPublicKey;
    // API bases - these are always valid.  They're in shared_ptrs so callers
    // using them can keep the object alive even if we replace them.
    std::shared_ptr<ApiBase> _pApiv1, _pApiv2, _pModernRegionsListApi,
        _pIpAddrApi, _pIpProxyApi, _pUpdateApi;
};

#endif
