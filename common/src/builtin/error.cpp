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
#line SOURCE_FILE("builtin/error.cpp")

#include "error.h"
#include "util.h"

#include <QJsonArray>
#include <QJsonObject>


Error::Error(const QJsonObject& errorObject)
{
    _code = (Code)errorObject.value(QLatin1String("code")).toInt(0);
#ifdef QT_DEBUG
    if (qEnumToString(_code).isEmpty())
        qWarning() << "Unknown error code received";
#endif

    QJsonObject data = errorObject.value(QLatin1String("data")).toObject();
    _systemCode = data.value(QLatin1String("systemCode")).toInt();
    QJsonArray params = data.value(QLatin1String("params")).toArray();
    _params.reserve(params.size());
    for (const QJsonValue& param : params)
        _params.append(param.toString());

    QJsonObject location = data.value(QLatin1String("location")).toObject();
    _location.category = nullptr;
    _location.file = (_storedFile = location.value(QLatin1String("file")).toString().toUtf8()).data();
    _location.line = location.value(QLatin1String("line")).toInt();
}

QString Error::errorString() const
{
    switch (_code)
    {
    case Unknown: return [](auto&& a, auto&& b) { return b.isEmpty() ? a : QStringLiteral("%1: %2").arg(a, b); }(tr("Unknown error"), _params.value(0));
    case System: return [](auto&& code, auto&& desc, auto&& op) { return op.isEmpty() ? tr("System error %1: %2").arg(code, desc) : tr("System error %1 inside %3: %2").arg(code, desc, op); }(_params.value(0), _params.value(1), _params.value(2));
    default: return tr("Unknown error code %1").arg(_code);
    }
}

QString Error::errorDescription() const
{
    switch (_code)
    {
    default: return tr("No additional information available.");
    }
}

QJsonObject Error::toJsonObject() const
{
    QJsonObject location;
    if (const char* categoryName = _location.categoryName())
        location.insert(QStringLiteral("category"), QLatin1String(categoryName));
    if (_location.file)
        location.insert(QStringLiteral("file"), QLatin1String(_location.file));
    if (_location.line)
        location.insert(QStringLiteral("line"), _location.line);

    QJsonObject data;
    QLatin1String name = qEnumToString(_code);
    if (!name.isEmpty())
        data.insert(QStringLiteral("name"), std::move(name));
    if (_systemCode || _code == System)
        data.insert(QStringLiteral("systemCode"), (double)_systemCode);
    if (_params.size())
        data.insert(QStringLiteral("params"), QJsonArray::fromStringList(_params));
    if (!location.isEmpty())
        data.insert(QStringLiteral("location"), std::move(location));

    return QJsonObject {
        { QStringLiteral("code"), _code },
        { QStringLiteral("message"), errorString() },
        { QStringLiteral("description"), errorDescription() },
        { QStringLiteral("data"), std::move(data) },
    };
}

QDebug operator<<(QDebug d, const Error& e)
{
    QDebugStateSaver saver(d);
    return d.noquote() << e.errorString() << e.location();
}

#ifdef UNIT_TEST
// In unit tests, report unhandled errors by logging them.
void reportError(Error error)
{
    qCritical(error);
}
#endif


#ifdef QT_DEBUG
static QString formatOperation(const char* operation)
{
    if (operation)
    {
        while (*operation == ':') ++operation;
        const char* end = operation;
        while (*end && *end != '(') ++end;
        return QString::fromLatin1(operation, end - operation);
    }
    else
        return QString();
}
#endif

static QString formatSystemErrorString(Error::SystemCode errorCode)
{
    auto err = std::system_error(errorCode, std::system_category());
    const char* begin = err.what();
    if (!begin) return QString();
    const char* end = begin + qstrlen(begin);
    while (end > begin && isspace(end[-1])) --end;
    return QString::fromLocal8Bit(begin, end - begin);
}

static QString formatSystemErrorCode(Error::SystemCode code)
{
#ifdef Q_OS_WIN
    if ((code & 0xFF000000u) && !(code & 0x00FF0000u))
    {
        return QStringLiteral("0x%1").arg(code, 8, 16, QLatin1Char('0'));
    }
#endif
    return QString::number(code);
}

SystemError::SystemError(ErrorLocation&& location, SystemCode errorCode, _D(const char* operation)_R(const std::nullptr_t&))
    : Error(std::move(location), System, errorCode, { formatSystemErrorCode(errorCode), formatSystemErrorString(errorCode), _D(formatOperation(operation))_R(QString()) })
{

}
