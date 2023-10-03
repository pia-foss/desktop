// Copyright (c) 2023 Private Internet Access, Inc.
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
#line SOURCE_FILE("json.cpp")

#include "json.h"
#include "builtin/path.h"
#include "builtin/util.h"

#include <QJsonDocument>
#include <QFile>
#include <nlohmann/json.hpp>

std::ostream &operator<<(std::ostream &os, const QJsonValue &val)
{
    switch(val.type())
    {
        default:
            // Have to break this up to avoid the ?-?-) trigraph
            os << "QJsonValue(??" ")";
            break;
        case QJsonValue::Type::Undefined:
            os << "QJsonValue(undefined)";
            break;
        case QJsonValue::Type::Null:
            os << "QJsonValue(null)";
            break;
        case QJsonValue::Type::Bool:
            os << "QJsonValue(bool, " << val.toBool() << ')';
            break;
        case QJsonValue::Type::Double:
            os << "QJsonValue(double, " << val.toDouble() << ')';
            break;
        case QJsonValue::Type::String:
            os << "QJsonValue(string, " << val.toString() << ')';
            break;
        case QJsonValue::Type::Array:
            os << "QJsonValue(array, " << val.toArray() << ')';
            break;
        case QJsonValue::Type::Object:
            os << "QJsonValue(object, " << val.toObject() << ')';
            break;
    }
    return os;
}

bool json_cast(const QJsonValue &from, bool &to) { return from.isBool() && ((to = from.toBool()), true); }
bool json_cast(const QJsonValue &from, double &to) { return from.isDouble() && ((to = from.toDouble()), true); }
bool json_cast(const QJsonValue &from, QString &to) { return from.isString() && ((to = from.toString()), true); }
bool json_cast(const QJsonValue &from, QJsonArray &to) { return from.isArray() && ((to = from.toArray()), true); }
bool json_cast(const QJsonValue &from, QJsonObject &to) { return from.isObject() && ((to = from.toObject()), true); }
bool json_cast(const QJsonValue &from, const std::nullptr_t&) { return from.isNull() || from.isUndefined(); }

bool json_cast(bool from, QJsonValue &to) { to = from; return true; }
bool json_cast(double from, QJsonValue &to) { to = from; return true; }
bool json_cast(const QString &from, QJsonValue &to) { to = from; return true; }
bool json_cast(const QJsonArray &from, QJsonValue &to) { to = from; return true; }
bool json_cast(const QJsonObject &from, QJsonValue &to) { to = from; return true; }
bool json_cast(const std::nullptr_t& from, QJsonValue& to) { to = QJsonValue(QJsonValue::Null); return true; }
bool json_cast(QJsonValue::Type from, QJsonValue& to) { to = QJsonValue(from); return true; }

bool json_cast(const QJsonValue &from, float &to)
{
    return from.isDouble() && ((void)(to = static_cast<float>(from.toDouble())), true);
}


NativeJsonObject::NativeJsonObject(UnknownPropertyBehavior unknownPropertyBehavior, QObject *parent)
    : QObject(parent),
      _saveUnknownProperties(unknownPropertyBehavior == SaveUnknownProperties),
      _pDeferredChanges{nullptr}
{
}

void NativeJsonObject::setError(Error error)
{
    _error = std::move(error);
}

void NativeJsonObject::clearError()
{
    _error = nullptr;
}

bool NativeJsonObject::validate(const QString& value, const std::initializer_list<const char*>& valid)
{
    for (const auto& v : valid)
    {
        if (value.compare(QLatin1String(v), Qt::CaseSensitive) == 0) return true;
    }
    return false;
}
bool NativeJsonObject::validate(const QString &value, const QStringList &valid)
{
    return valid.contains(value, Qt::CaseSensitive);
}

QStringList NativeJsonObject::choices(const QString*, const std::initializer_list<const char*> &valid)
{
    QStringList result;
    result.reserve(valid.size());
    for(const auto &value : valid)
        result.push_back(QString{value});
    return result;
}

template<typename T>
QJsonValue NativeJsonObject::getInternal(const char* asciiName, const T& name) const
{
    auto m = this->metaObject();
    auto pi = m->indexOfProperty(asciiName);
    if (pi >= m->propertyOffset())
    {
        auto p = m->property(pi);
        return QJsonValue::fromVariant(p.read(this));
    }
    else
    {
        return _other.value(name);
    }
}
QJsonValue NativeJsonObject::get(const char *name) const
{
    return getInternal(name, QLatin1String(name));
}
QJsonValue NativeJsonObject::get(const QLatin1String &name) const
{
    return getInternal(name.data(), name);
}
QJsonValue NativeJsonObject::get(const QString &name) const
{
    return getInternal(qUtf8Printable(name), name);
}

template<typename T>
bool NativeJsonObject::setInternal(const char* asciiName, const T& name, const QJsonValue& value)
{
    clearError();
    auto m = this->metaObject();
    auto pi = m->indexOfProperty(asciiName);
    if (pi >= m->propertyOffset())
    {
        auto p = m->property(pi);
        if (!p.write(this, value.toVariant()))
        {
            _error = JsonFieldError(HERE, name, p.typeName(), jsonValueString(value));
            return false;
        }
        return error() == nullptr;
    }
    else if (_saveUnknownProperties)
    {
        auto it = _other.find(name);
        if (it == _other.end())
            _other.insert(name, std::move(value));
        else if (*it != value)
            *it = std::move(value);
        else
            return true; // Return without emitting signals
        emitPropertyChange({nullptr, name});
        return true;
    }
    else
    {
        _error = JsonFieldError(HERE, name, QString(), jsonValueString(value));
        return false;
    }
}
bool NativeJsonObject::set(const char *name, const QJsonValue &value)
{
    return setInternal(name, QLatin1String(name), value);
}
bool NativeJsonObject::set(const QLatin1String &name, const QJsonValue &value)
{
    return setInternal(name.data(), name, value);
}
bool NativeJsonObject::set(const QString &name, const QJsonValue &value)
{
    return setInternal(qUtf8Printable(name), name, value);
}

bool NativeJsonObject::isKnownProperty(const char *name) const
{
    auto m = this->metaObject();
    return m->indexOfProperty(name) >= m->propertyOffset();
}
bool NativeJsonObject::isKnownProperty(const QLatin1String &name) const
{
    return isKnownProperty(name.data());
}
bool NativeJsonObject::isKnownProperty(const QString &name) const
{
    auto m = this->metaObject();
    return m->indexOfProperty(qUtf8Printable(name)) >= m->propertyOffset();
}

bool NativeJsonObject::assign(const QJsonObject &properties)
{
    // Defer change signals during assign() so the entire change is observed
    // atomically
    QVector<DeferredChange> changes;
    changes.reserve(properties.count());
    // If assign() is somehow called recursively, let the changes accumulate
    // together in the outer group
    if(!_pDeferredChanges)
        _pDeferredChanges = &changes;

    clearError();
    Optional<Error> error;
    for (QJsonObject::const_iterator it = properties.constBegin(), end = properties.constEnd(); it != end; ++it)
    {
        auto key = it.key();
        auto value = it.value();
        setInternal(qUtf8Printable(key), key, value);
        if (!error && _error) error = std::move(_error);
    }

    if(_pDeferredChanges == &changes)
        _pDeferredChanges = nullptr;
    // Emit all changes that were queued
    for(auto &change : changes)
        emitPropertyChange(std::move(change));

    if (error)
    {
        // Restore the first error
        _error = std::move(error);
        return false;
    }
    return true;
}

void NativeJsonObject::reset()
{
    clearError();
    auto m = this->metaObject();
    for (int i = m->propertyOffset(), c = m->propertyCount(); i < c; i++)
    {
        auto p = m->property(i);
        p.reset(this);
    }
    QJsonObject empty;
    _other.swap(empty);
    for (auto it = empty.begin(); it != empty.end(); ++it)
    {
        emit unknownPropertyChanged(it.key());
        emit propertyChanged(it.key());
    }
}
template<typename T>
void NativeJsonObject::resetInternal(const char* asciiName, const T& name)
{
    clearError();
    auto m = this->metaObject();
    auto pi = m->indexOfProperty(asciiName);
    if (pi >= m->propertyOffset())
    {
        auto p = m->property(pi);
        p.reset(this);
    }
    else
    {
        auto it = _other.find(name);
        if (it != _other.end())
        {
            _other.erase(it);
            emit unknownPropertyChanged(name);
            emit propertyChanged(name);
        }
    }
}

void NativeJsonObject::emitPropertyChange(DeferredChange change)
{
    if(_pDeferredChanges)
    {
        // Deferring during assign(), store it
        _pDeferredChanges->push_back(std::move(change));
    }
    else
    {
        // Not in assign(), emit now.
        if(change._specificSignal)
            change._specificSignal();
        else
            emit unknownPropertyChanged(change._name);
        emit propertyChanged(change._name);
    }
}

void NativeJsonObject::reset(const char* name)
{
    resetInternal(name, QLatin1String(name));
}
void NativeJsonObject::reset(const QLatin1String& name)
{
    resetInternal(name.data(), name);
}
void NativeJsonObject::reset(const QString& name)
{
    resetInternal(qUtf8Printable(name), name);
}
void NativeJsonObject::reset(const QStringList& properties)
{
    for (const QString& name : properties)
    {
        resetInternal(qUtf8Printable(name), name);
    }
}

QJsonObject NativeJsonObject::toJsonObject() const
{
    QJsonObject result = _other;
    auto m = this->metaObject();
    for (int i = m->propertyOffset(), c = m->propertyCount(); i < c; i++)
    {
        auto p = m->property(i);
        result.insert(QLatin1String(p.name()), QJsonValue::fromVariant(p.read(this)));
    }
    return result;
}

bool NativeJsonObject::readJsonObject(const QJsonObject &obj)
{
    reset();
    return assign(obj);
}

const Error *NativeJsonObject::error() const
{
    return _error.ptr();
}


QString jsonValueString(const QJsonValue &value)
{
    auto str = QString::fromUtf8(QJsonDocument(QJsonArray { value }).toJson(QJsonDocument::Compact));
    return str.mid(1, str.length() - 2);
}

bool json_cast(const QJsonValue &from, NativeJsonObject &to)
{
    if (!from.isObject()) return false;
    return to.readJsonObject(from.toObject());
}

bool json_cast(const NativeJsonObject &from, QJsonValue &to)
{
    to = from.toJsonObject();
    return true;
}

bool readProperties(NativeJsonObject& object, const Path &settingsDir,
                    const char* filename)
{
    SCOPE_LOGGING_CATEGORY("json.settings");
    bool readExistingFile = false;

    QFile file(settingsDir / filename);
    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        qWarning() << "Unable to read from" << filename;
        return readExistingFile;
    }
    QJsonParseError error;
    QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError)
        qWarning() << "File" << filename << "is not a valid JSON document:" << error.errorString();
    else if (!json.isObject())
        qWarning() << "File" << filename << "did not contain settings";
    else if (!object.assign(json.object())) {
        // Even if all properties could not be assigned
        // we still did read from an existing file.
        readExistingFile = true;
        qWarning() << "Not all properties from" << filename << "could be assigned";
    }
    else {
        readExistingFile = true;
        qDebug() << "Successfully read" << filename;
    }

    return readExistingFile;
}

void writeProperties(const QJsonObject& object, const Path &settingsDir,
                     const char* filename)
{
    SCOPE_LOGGING_CATEGORY("json.settings");

    QFile file(settingsDir.mkpath() / filename);
    if (file.open(QFile::WriteOnly | QFile::Text)
            && 0 < file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)))
        qDebug() << "Successfully wrote" << filename;
    else
        qCritical() << "Unable to write" << filename;
}

QJsonObject adaptJsonTextToQJsonObject(kapps::core::StringSlice jsonText)
{
    // QJsonDocument can only parse from a QByteArray, which normally owns
    // its data.  However, QByteArray provides a way to sneak a raw data
    // pointer/size into it without copying the data, which is done here.
    auto jsonByteArrayView = QByteArray::fromRawData(jsonText.data(), jsonText.size());
    QJsonParseError parseError{};
    auto qtJsonDoc = QJsonDocument::fromJson(jsonByteArrayView, &parseError);
    if(!qtJsonDoc.isObject())
    {
        // Didn't get an object back - this would indicate some sort of
        // interchange error between nlohmann::json and Qt.  Likely, either:
        // - qtJsonDoc is "null" - nlohmann::json produced something that
        //   Qt did not understand at all.  parseError should have info.
        // - qtJsonDoc is non-null but not an object - somehow the JSON was
        //   interpreted as an array (Qt does not support anything other
        //   than object/array at the top level).  parseError won't contain
        //   anything.
        const char *qtJsonDocType = "unknown";
        if(qtJsonDoc.isArray())
            qtJsonDocType = "array";
        else if(qtJsonDoc.isNull())
            qtJsonDocType = "null";
        // QJsonDocument strangely has "isEmpty" too, this probably means
        // either an empty array or empty object, but lots of Qt objects
        // have strange combinations of undefined/empty states, so check it
        // too if we didn't match the above.
        else if(qtJsonDoc.isEmpty())
            qtJsonDocType = "empty";
        KAPPS_CORE_WARNING() << "Can't read object JSON - Qt returned type"
            << qtJsonDocType << "and parse error" << parseError.errorString();
        throw std::runtime_error{"object JSON value could not be read"};
    }
    return qtJsonDoc.object();
}
