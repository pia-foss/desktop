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
#line SOURCE_FILE("environment.cpp")

#include "environment.h"
#include "path.h"
#include "metaserviceapibase.h"
#include "brand.h"
#include <QFile>
#include <QSslCertificate>
#include <QSslKey>
#include <QJsonDocument>
#include <QJsonArray>

const QByteArray Environment::defaultRegionsListPublicKey = QByteArrayLiteral(
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAzLYHwX5Ug/oUObZ5eH5P\n"
    "rEwmfj4E/YEfSKLgFSsyRGGsVmmjiXBmSbX2s3xbj/ofuvYtkMkP/VPFHy9E/8ox\n"
    "Y+cRjPzydxz46LPY7jpEw1NHZjOyTeUero5e1nkLhiQqO/cMVYmUnuVcuFfZyZvc\n"
    "8Apx5fBrIp2oWpF/G9tpUZfUUJaaHiXDtuYP8o8VhYtyjuUu3h7rkQFoMxvuoOFH\n"
    "6nkc0VQmBsHvCfq4T9v8gyiBtQRy543leapTBMT34mxVIQ4ReGLPVit/6sNLoGLb\n"
    "gSnGe9Bk/a5V/5vlqeemWF0hgoRtUxMtU1hFbe7e8tSq1j+mu0SHMyKHiHd+OsmU\n"
    "IQIDAQAB\n"
    "-----END PUBLIC KEY-----"
);

Environment::Environment(const DaemonState &state)
    : _state{state}
{
    reload();
}

QByteArray Environment::readFile(const QString &path)
{
    QFile file{path};
    if(!file.open(QIODevice::OpenModeFlag::ReadOnly | QIODevice::OpenModeFlag::Text))
        return {};
    return file.readAll();
}

void Environment::loadCertificateAuthority(const QString &handshakeSetting,
                                           const QString &filename)
{
    const QString &resourceName = handshakeSetting + QStringLiteral(" CA");

    auto overrideFilePath = Path::DaemonSettingsDir / QStringLiteral("ca_override") / filename;
    if(QFile::exists(overrideFilePath))
    {
        QByteArray fileData = readFile(overrideFilePath);
        // Strip CR - normalize line endings to LF.
        fileData.replace('\r', QByteArray{});
        // Make sure the file is a valid cert, even though we store the raw PEM
        QSslCertificate testCert{fileData};
        if(testCert.isNull())
        {
            qWarning() << "Override certificate" << overrideFilePath
                << "can't be parsed";
            emit overrideFailed(resourceName);
            // Proceed to load default certificate
        }
        else
        {
            qInfo() << "Overriding" << handshakeSetting << "CA with:"
                << overrideFilePath;
            emit overrideActive(resourceName);
            _authorities.emplace(handshakeSetting, std::move(fileData));
            return;
        }
    }

    // No override, or it couldn't be parsed, load the default.
    QByteArray defaultData = readFile(QStringLiteral(":/ca/") + filename);
    defaultData.replace('\r', QByteArray{});
    _authorities.emplace(handshakeSetting, std::move(defaultData));
}

void Environment::loadRegionsListPublicKey()
{
    auto overrideFilePath = Path::DaemonSettingsDir / QStringLiteral("regions_key_override.pem");

    if(QFile::exists(overrideFilePath))
    {
        QByteArray fileData = readFile(overrideFilePath);
        // Make sure it's a valid public key, even though we store the raw PEM
        QSslKey testKey{fileData, QSsl::KeyAlgorithm::Rsa,
                        QSsl::EncodingFormat::Pem, QSsl::KeyType::PublicKey};
        if(testKey.isNull())
        {
            qWarning() << "Override regions list key" << overrideFilePath
                << "can't be parsed";
            emit overrideFailed(QStringLiteral("regions list key"));
            // Proceed to load default
        }
        else
        {
            qInfo() << "Overriding regions list key with:" << overrideFilePath;
            emit overrideActive(QStringLiteral("regions list key"));
            _regionsListPublicKey = fileData;
            return;
        }
    }

    // Load default
    _regionsListPublicKey = defaultRegionsListPublicKey;
}

void Environment::applyApiBaseOverride(bool overridePresent,
                                       const QJsonDocument &apiOverride,
                                       std::shared_ptr<ApiBase> &pApiBase,
                                       const QString &jsonKey,
                                       const QString &resourceName)
{
    // Check failure conditions first to trace specific failures

    // File present but couldn't be loaded - emit overrideFailed()
    if(overridePresent && apiOverride.isNull())
    {
        // Don't need to trace, failure to load the JSON was traced separately
        emit overrideFailed(resourceName);
        return;
    }

    auto overrideBasesValue = apiOverride[jsonKey];
    // Unexpected value type.  null, undefined, or an empty array are normal,
    // these indicate not to override this particular API base.
    if(!overrideBasesValue.isNull() && !overrideBasesValue.isUndefined() &&
       !overrideBasesValue.isArray())
    {
        qWarning() << "Can't override" << resourceName << "- incorrect value for"
            << jsonKey << "- expected array, got"
            << overrideBasesValue;
        emit overrideFailed(resourceName);
        return;
    }

    // If it's null, undefined, or empty, that's normal, don't override.
    // (null or undefined produce an empty array when calling
    // QJsonValue::toArray()).
    auto overrideBases = overrideBasesValue.toArray();
    if(overrideBases.isEmpty())
    {
        // No trace - this is the normal case
        return;
    }

    // Ensure all values are strings
    std::vector<QString> overrideBaseStrs;
    overrideBaseStrs.reserve(overrideBases.size());
    int pos = 0;   // Just for tracing
    for(const auto &value : overrideBases)
    {
        // Non-string or empty values aren't allowed
        overrideBaseStrs.push_back(value.toString());
        if(overrideBaseStrs.back().isEmpty())
        {
            qWarning() << "Can't override" << resourceName
                << "- incorrect value at position" << pos << "-" << value;
            emit overrideFailed(resourceName);
            return;
        }
        ++pos;
    }

    qInfo() << "Overriding" << resourceName << "with" << overrideBaseStrs;
    pApiBase = std::make_shared<FixedApiBase>(overrideBaseStrs);
    emit overrideActive(resourceName);
}

void Environment::loadApiBase(bool overridePresent,
                              const QJsonDocument &apiOverride,
                              std::shared_ptr<ApiBase> &pApiBase,
                              const QString &jsonKey,
                              const QString &resourceName,
                              const std::initializer_list<QString> &defaults)
{
    pApiBase.reset();
    // Apply the override if it's set and valid
    applyApiBaseOverride(overridePresent, apiOverride, pApiBase, jsonKey,
                         resourceName);
    // If the override wasn't applied, load the default.  applyApiBaseOverride()
    // traces and emits overrideFailed() as appropriate.
    if(!pApiBase)
        pApiBase = std::make_shared<FixedApiBase>(defaults);
}

void Environment::loadDynamicApiBase(bool overridePresent,
                                     const QJsonDocument &apiOverride,
                                     std::shared_ptr<ApiBase> &pApiBase,
                                     const QString &jsonKey,
                                     const QString &resourceName,
                                     const QString &dynamicBasePath,
                                     const std::initializer_list<QString> &fixedBases)
{
    pApiBase.reset();
    applyApiBaseOverride(overridePresent, apiOverride, pApiBase, jsonKey,
                         resourceName);
    // If it wasn't overridden, use the dynamic API base, which leverages Meta
    // services and the fixed fallbacks
    if(!pApiBase)
    {
        pApiBase = std::make_shared<MetaServiceApiBase>(_state, dynamicBasePath,
                                                        _pRsa4096CA, fixedBases);
    }
}

void Environment::loadApiBases()
{
    QByteArray apiOverrideJson = readFile(Path::DaemonSettingsDir / QStringLiteral("api_override.json"));
    // Flag to indicate whether the override file was present at all - if it was
    // present and couldn't be loaded, this causes each API base to emit
    // overrideFailed().
    bool overridePresent = !apiOverrideJson.isEmpty();
    // Only try to parse if the file was present and non-empty - don't trace
    // bogus errors if the file was not present
    QJsonDocument apiOverride;
    if(overridePresent)
    {
        QJsonParseError parseError;
        apiOverride = QJsonDocument::fromJson(apiOverrideJson, &parseError);
        if(apiOverride.isNull())
        {
            qWarning() << "Can't parse api_override.json:" << parseError.errorString();
        }
    }

    // Load each API base
    loadApiBase(overridePresent, apiOverride, _pApiv1, QStringLiteral("apiv1"),
                QStringLiteral("API v1"), {
                    QStringLiteral("https://www.privateinternetaccess.com/api/client"),
                    QStringLiteral("https://www.piaproxy.net/api/client")
                });
    loadDynamicApiBase(overridePresent, apiOverride, _pApiv2, QStringLiteral("apiv2"),
                QStringLiteral("API v2"), QStringLiteral("/apiv2/"), {
                    QStringLiteral("https://www.privateinternetaccess.com/api/client/v2/"),
                    QStringLiteral("https://www.piaproxy.net/api/client/v2/")
                });

    loadDynamicApiBase(overridePresent, apiOverride, _pModernRegionsListApi,
                QStringLiteral("modern_regions_list_api"),
                QStringLiteral("modern regions list API"), QStringLiteral("/"), {
                    QStringLiteral("https://serverlist.piaservers.net")
                });

    loadApiBase(overridePresent, apiOverride, _pIpAddrApi, QStringLiteral("ip_api"),
                QStringLiteral("IP API"), {
                    QStringLiteral("https://www.privateinternetaccess.com/")
                });
    loadApiBase(overridePresent, apiOverride, _pIpProxyApi, QStringLiteral("ip_proxy_api"),
                QStringLiteral("IP proxy API"), {
                    QStringLiteral("https://www.piaproxy.net/")
                });
    loadApiBase(overridePresent, apiOverride, _pUpdateApi, QStringLiteral("update_api"),
                QStringLiteral("update API"), {
                    BRAND_UPDATE_APIS
                });
}

void Environment::reload()
{
    _authorities.clear();
    loadCertificateAuthority(QStringLiteral("RSA-4096"), QStringLiteral("rsa_4096.crt"));

    _pRsa4096CA = std::make_shared<PrivateCA>(getCertificateAuthority(QStringLiteral("RSA-4096")));

    loadRegionsListPublicKey();

    loadApiBases();
}

QByteArray Environment::getCertificateAuthority(const QString &type) const
{
    auto itAuthority = _authorities.find(type);
    if(itAuthority != _authorities.end())
        return itAuthority->second;
    qWarning() << "Unable to find certificate authority" << type;
    auto itDefault = _authorities.find(QStringLiteral("default"));
    Q_ASSERT(itDefault != _authorities.end());
    return itDefault->second;
}
