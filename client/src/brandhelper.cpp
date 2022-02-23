// Copyright (c) 2022 Private Internet Access, Inc.
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
#line SOURCE_FILE("brandhelper.cpp")

#include "brandhelper.h"

QString BrandHelper::getBrandParam(const QString &code)
{
    // If param was already found, return from cache
    if(_paramsCache.contains(code)) {
        return _paramsCache.value(code);
    }


    QJsonDocument params = QJsonDocument::fromJson(QStringLiteral(BRAND_PARAMS).toUtf8());

    if(params.isNull()) {
        qWarning () << "Unable to read brand JSON";
        return QStringLiteral("");
    }

    if(params.isObject()) {
        QJsonObject paramsObject = params.object();
        if(paramsObject.keys().contains(code)) {
            QString value = paramsObject.value(code).toString();
            _paramsCache.insert(code, value);
            return value;
        }

        qWarning () << "Unable to find param " << code;
        return QStringLiteral("");
    }

    qWarning () << "BRAND_PARAMS is not a valid JSON object.";
    return QStringLiteral("");
}
