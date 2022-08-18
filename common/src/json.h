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

#pragma once
#include "common.h"
#include <kapps_core/src/corejson.h>

#include <cmath>
#include <exception>
#include <limits>
#include <vector>
#include <deque>
#include <unordered_map>
#include <set>

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLinkedList>
#include <QList>
#include <QMap>
#include <QMetaObject>
#include <QMetaProperty>
#include <QObject>
#include <QSet>
#include <QSharedPointer>
#include <QVector>


// Clang issues this warning spuriously for generic lambdas.  Argh.
// It also emits this at the point where the template is instantiated, so we
// leave the warning disabled in general instead of just disabling it for this
// file.  ARRRRGGGHHH.
// https://bugs.llvm.org/show_bug.cgi?id=31815
QT_WARNING_DISABLE_CLANG("-Wunused-lambda-capture")

// Newer version of clang emit this warning:
//   implicit conversion from 'unsigned long' to 'double' changes value from 18446744073709551615 to 18446744073709551616
//
// This occurs when instantiating the integer json_cast<>() with a 64-bit
// integer type, the maximum 64-bit value that it checks against can't be
// represented as a double.
//
// This is technically an issue, because we test '... > max()', and due to the
// max increasing, if we got max+1, we would try to convert it to a 64-bit
// integer even though it can't actually be represented.  This should be
// addressed, but right now the mass of warnings is hiding other more important
// issues.
#if __clang_major__ >= 11
QT_WARNING_DISABLE_CLANG("-Wimplicit-const-int-float-conversion")
#endif

// Tracing a QJsonObject or QJsonDocument renders JSON.
inline std::ostream &operator<<(std::ostream &os, const QJsonDocument &doc)
{
    return os << doc.toJson(QJsonDocument::JsonFormat::Compact).data();
}
inline std::ostream &operator<<(std::ostream &os, const QJsonObject &obj)
{
    return os << QJsonDocument{obj};
}
inline std::ostream &operator<<(std::ostream &os, const QJsonArray &arr)
{
    return os << QJsonDocument{arr};
}
COMMON_EXPORT std::ostream &operator<<(std::ostream &os, const QJsonValue &val);

// Forward declaration
class COMMON_EXPORT NativeJsonObject;

// Implementations of json_value(from, to) for basic types

// Identity cast
static inline bool json_cast(const QJsonValue& from, QJsonValue& to) { to = from; return true; }

// Trivial QJsonValue-to-actual-type casts
COMMON_EXPORT bool json_cast(const QJsonValue& from, bool& to);
COMMON_EXPORT bool json_cast(const QJsonValue& from, double& to);
COMMON_EXPORT bool json_cast(const QJsonValue& from, QString& to);
COMMON_EXPORT bool json_cast(const QJsonValue& from, QJsonArray& to);
COMMON_EXPORT bool json_cast(const QJsonValue& from, QJsonObject& to);
COMMON_EXPORT bool json_cast(const QJsonValue& from, const std::nullptr_t& to);

// Trivial actual-type-to-QJsonValue casts
COMMON_EXPORT bool json_cast(bool from, QJsonValue& to);
COMMON_EXPORT bool json_cast(double from, QJsonValue& to);
COMMON_EXPORT bool json_cast(const QString& from, QJsonValue& to);
COMMON_EXPORT bool json_cast(const QJsonArray& from, QJsonValue& to);
COMMON_EXPORT bool json_cast(const QJsonObject& from, QJsonValue& to);
COMMON_EXPORT bool json_cast(const std::nullptr_t& from, QJsonValue& to);
COMMON_EXPORT bool json_cast(QJsonValue::Type from, QJsonValue& to);

// Cast to float (the reverse is transparently handled by the double-to-QJsonValue cast).
bool COMMON_EXPORT json_cast(const QJsonValue& from, float& to);

// Cast to integer (fails if a contained number is not an integer)
template<typename Integer> std::enable_if_t<std::is_integral<Integer>::value, bool> json_cast(const QJsonValue& from, Integer& to)
{
    if (!from.isDouble()) return false;
    double number = from.toDouble(), i;
    if (std::modf(number, &i) != 0.0) return false;
    if (number > std::numeric_limits<Integer>::max() || number < std::numeric_limits<Integer>::min()) return false;
    to = static_cast<Integer>(number);
    return true;
}
// Cast from integer
template<typename Integer> std::enable_if_t<std::is_integral<Integer>::value, bool> json_cast(const Integer& from, QJsonValue& to)
{
    return json_cast(static_cast<double>(from), to);
}

// Cast to a nullable_t<T> to also handle QJsonValue::Null
template<typename T> bool json_cast(const QJsonValue& from, nullable_t<T>& to)
{
    if (from.isNull() || from.isUndefined())
    {
        to = nullptr;
        return true;
    }
    else
    {
        // Ensure 'to' has a value before reading into it
        return json_cast(from, to.defaultConstructIfNull());
    }
}
// Cast from a nullable_t<T>, where nullptr values become QJsonValue::Null
template<typename T> bool json_cast(const nullable_t<T>& from, QJsonValue& to)
{
    if (from == nullptr)
    {
        to = QJsonValue::Null;
        return true;
    }
    else
    {
        return json_cast(from.get(), to);
    }
}
// Cast from a pointer, where nullptr values become QJsonValue::Null
template<typename T> bool json_cast(const T* from, QJsonValue& to)
{
    if (from == nullptr)
    {
        to = QJsonValue::Null;
        return true;
    }
    else
    {
        return json_cast(*from, to);
    }
}
// Cast from a QSharedPointer<T>, where nullptr values become QJsonValue::Null
template<typename T> bool json_cast(const QSharedPointer<T>& from, QJsonValue& to)
{
    if (from.isNull())
    {
        to = QJsonValue::Null;
        return true;
    }
    else
    {
        return json_cast(*from, to);
    }
}
// Cast to a QSharedPointer<T>, instantiating a new T
template<typename T> bool json_cast(const QJsonValue& from, QSharedPointer<T>& to)
{
    if (from.isNull())
    {
        to.reset();
        return true;
    }
    else
    {
        auto result = QSharedPointer<std::remove_cv_t<T>>::create();
        if (!json_cast(from, *result))
            return false;
        to = std::move(result);
        return true;
    }
}

// Helper to cast between standard API lists/arrays/vectors
template<typename From, typename To> bool json_cast_array(const From& from, To& to)
{
    To result;
    for (const auto& item : from)
    {
        typename To::value_type converted;
        if (!json_cast(item, converted)) return false;
        result.insert(result.end(), std::move(converted));
    }
    to = std::move(result);
    return true;
}

// Get the key and value from either an STL-style associative container
// iterator, or a Qt-style associative container iterator
template<class IteratorT>
auto associativeIteratorKey(typename std::enable_if<std::is_member_function_pointer<decltype(&IteratorT::key)>::value, const IteratorT &>::type it)
{
    return it.key();
}
template<class IteratorT>
auto associativeIteratorValue(typename std::enable_if<std::is_member_function_pointer<decltype(&IteratorT::value)>::value, const IteratorT &>::type it)
{
    return it.value();
}
template<class IteratorT>
auto associativeIteratorKey(typename std::enable_if<std::is_pointer<decltype(&std::declval<IteratorT>()->first)>::value, const IteratorT &>::type it)
{
    return it->first;
}
template<class IteratorT>
auto associativeIteratorValue(typename std::enable_if<std::is_pointer<decltype(&std::declval<IteratorT>()->second)>::value, const IteratorT &>::type it)
{
    return it->second;
}

// Helper to cast between standard API maps/hashmaps/objects
template<typename From, typename To> bool json_cast_map(const From& from, To& to)
{
    To result;
    for (auto it = from.begin(); it != from.end(); ++it)
    {
        typename To::mapped_type converted;
        if (!json_cast(associativeIteratorValue<typename From::const_iterator>(it), converted))
            return false;
        result[associativeIteratorKey<typename From::const_iterator>(it)] = std::move(converted);
    }
    to = std::move(result);
    return true;
}

#define IMPLEMENT_JSON_CAST_ARRAY(...) \
    template<typename T> bool json_cast(const QJsonValue& from, __VA_ARGS__& to) \
    { \
        if (!from.isArray()) return false; \
        return json_cast_array(from.toArray(), to); \
    } \
    template<typename T> bool json_cast(const __VA_ARGS__& from, QJsonValue& to) \
    { \
        QJsonArray result; \
        if (!json_cast_array(from, result)) return false; \
        to = std::move(result); \
        return true; \
    }

#define IMPLEMENT_JSON_CAST_MAP(...) \
    template<typename T> bool json_cast(const QJsonValue& from, __VA_ARGS__& to) \
    { \
        if (!from.isObject()) return false; \
        return json_cast_map(from.toObject(), to); \
    } \
    template<typename T> bool json_cast(const __VA_ARGS__& from, QJsonValue& to) \
    { \
        QJsonObject result; \
        if (!json_cast_map(from, result)) return false; \
        to = std::move(result); \
        return true; \
    }

IMPLEMENT_JSON_CAST_ARRAY(QList<T>)
IMPLEMENT_JSON_CAST_ARRAY(QLinkedList<T>)
IMPLEMENT_JSON_CAST_ARRAY(QVector<T>)
IMPLEMENT_JSON_CAST_ARRAY(std::vector<T>)
IMPLEMENT_JSON_CAST_ARRAY(std::deque<T>)
IMPLEMENT_JSON_CAST_ARRAY(std::set<T>)
IMPLEMENT_JSON_CAST_ARRAY(std::set<T, std::greater<T>>)

IMPLEMENT_JSON_CAST_MAP(std::unordered_map<QString, T>)

#undef IMPLEMENT_JSON_CAST_ARRAY
#undef IMPLEMENT_JSON_CAST_MAP

// Cast to a NativeJsonObject (if you get an error here, you are probably
// trying to copy NativeJsonObjects around; wrap them in a QSharedPointer
// instead)
COMMON_EXPORT bool json_cast(const QJsonValue& from, NativeJsonObject& to);
// Cast from a NativeJsonObject
COMMON_EXPORT bool json_cast(const NativeJsonObject& from, QJsonValue& to);


// Cast between a QJsonValue and a specific type; throws json_cast_exception
// if the stored value is not a compatible type. Add additional conversions
// for custom types with extra json_cast(from, to) function overloads.
//
// Pass HERE additionally to improve error diagnostics, this causes any thrown
// error to reference the json_cast() call rather than a line in this function.
// (Important for parsing JSON data from APIs, indicates which field was invalid
// if an error occurs.)
template<typename To, typename From> static inline To json_cast(const From& from,
                                                                const CodeLocation &loc)
{
    To to;
    if (!json_cast(from, to))
        throw Error{loc, Error::Code::JsonCastError};
    return to;
}


// Get a printable version of any QJsonValue.
//
COMMON_EXPORT QString jsonValueString(const QJsonValue& value);

// Base class for a QJsonObject-like class with fields accessible natively
// as well as via Qt properties. All properties must be convertible to/from
// QJsonValue via json_cast, and the reflected Qt properties are always
// of type QJsonValue as well. Use the JsonField and ValidatedJsonField
// macros to declare the native fields in a derived subclass.
//
class COMMON_EXPORT NativeJsonObject : public QObject
{
    Q_OBJECT
private:
    // When a NativeJsonObject is being updated with assign(), it assigns all
    // properties atomically - it does not emit any change signals until all
    // changes have been applied.  (This preserves invariants in related
    // properties from the daemon.)
    //
    // This occurs by deferring change signals during assign().  Each change
    // signal that would have been emitted is stored in a DeferredChange.
    struct DeferredChange
    {
        // Emits the property-specific change signal.  If this is an unknown
        // property, this is nullptr, and unknownPropertyChanged() is emitted
        // instead
        std::function<void()> _specificSignal;
        const QString _name; // for propertyChanged()
    };
protected:
    enum UnknownPropertyBehavior { DiscardUnknownProperties, SaveUnknownProperties };

    explicit NativeJsonObject(UnknownPropertyBehavior unknownPropertyBehavior = DiscardUnknownProperties, QObject* parent = nullptr);

    // Helper function to report assignment/type errors.
    void setError(Error error);
    void clearError();

    // The overloads of validate() implement the optional validation arguments
    // of JsonField().
    template<typename T> static inline bool validate(const T& value) { (void)value; return true; }
    template<typename T, typename U> static inline bool validate(const T& value, const std::initializer_list<U>& valid);
    template<typename T, typename Func> static inline auto validate(const T& value, Func&& valid) -> decltype(valid(value), true) { return valid(value); }
    static bool validate(const QString& value, const std::initializer_list<const char*>& valid);
    static bool validate(const QString& value, const QStringList& valid);

public:
    // The overloads of choices() implement the choices_name() method of
    // JsonField().  These take the same parameters as the overloads of
    // validate(), but some of these cannot actually be instantiated (we can't
    // provide choices_name() for functor validators, etc.)
    //
    // These don't use the const T* parameter, but they're still overloaded with
    // T to handle QString fields the same way as validate().
    template<typename T> static inline int choices(const T*);   // Not instantiable, no choices given
    template<typename T, typename U> static inline std::vector<U> choices(const T*, const std::initializer_list<U> &valid) {return valid;}
    template<typename T, typename Func> static inline int choices(const T*, Func &&); // Not instantiable, functor validator
    static QStringList choices(const QString*, const std::initializer_list<const char*> &valid);
    static QStringList choices(const QString*, const QStringList &valid) {return valid;}

private:
    template<typename T> QJsonValue getInternal(const char* asciiName, const T& name) const;
    template<typename T> bool setInternal(const char* asciiName, const T& name, const QJsonValue& value);
    template<typename T> void resetInternal(const char* asciiName, const T& name);

protected:
    // Used by JsonField to either emit a change now or store it during assign()
    void emitPropertyChange(DeferredChange change);

public:
    // Get any property by name (as a QJsonValue).
    QJsonValue get(const char* name) const;
    QJsonValue get(const QLatin1String& name) const;
    QJsonValue get(const QString& name) const;

    // Get any property by name (as a QJsonValue).
    const QJsonValue operator[](const char* name) const { return get(name); }
    const QJsonValue operator[](const QLatin1String& name) const { return get(name); }
    const QJsonValue operator[](const QString& name) const { return get(name); }

    // Set any property by name (with a QJsonValue).
    bool set(const char* name, const QJsonValue& value);
    bool set(const QLatin1String& name, const QJsonValue& value);
    bool set(const QString& name, const QJsonValue& value);

    // Check if a given property name is a known property.
    bool isKnownProperty(const char* name) const;
    bool isKnownProperty(const QLatin1String& name) const;
    bool isKnownProperty(const QString& name) const;

    // Assign a set of name/value pairs from a QJsonObject; returns true
    // if all properties were assigned successfully.
    bool assign(const QJsonObject& properties);

    // Reset all properties to their default values.
    void reset();
    // Reset a named property to its default value.
    void reset(const char* name);
    void reset(const QLatin1String& name);
    void reset(const QString& name);
    // Reset a subset of properties to their default values.
    void reset(const QStringList& properties);

    // Convert the properties to a QJsonObject (for serialization).
    QJsonObject toJsonObject() const;
    // Reset and read all properties from a QJsonObject (for serialization);
    // returns true if all properties were assigned successfully.
    bool readJsonObject(const QJsonObject& obj);

    // Return the last error, or nullptr if there was no error.
    const Error* error() const;

signals:
    void propertyChanged(const QString& name);
    void unknownPropertyChanged(const QString& name);

protected:
    nullable_t<Error> _error;   // Set by JsonField
private:
    QJsonObject _other;
    const bool _saveUnknownProperties;
    // When set, change signals are being deferred during a call to assign()
    QVector<DeferredChange> *_pDeferredChanges;
};


// Define a native field in a NativeJsonObject-derived class; it may then
// be manipulated with native getter/setter functions named <name>() and
// <name>(newValue). The <name>Changed() signal lets you listen for changes.
//
// The default value specifies the initial value of the field, as well as
// what the field will be set to when the reset() function is called.
//
// The validator is an optional fourth parameter that validates assigned
// values, raising errors if the check fails. This parameter can be either:
//
// - a callable taking the value being assigned as a parameter
// - an initializer list of allowed values
// - for QString fields, an initializer list of const char* literals
// - for QString fields, a QStringList of allowed values
//
// The methods defined are:
// - name() / name(const type&) - typed getter/setter
// - get_name() / set_name() - JSON getter/setter
// - default_name() / reset_name() - get default value or reset to default
// - choices_name() - possible choices from the validation list, usually used for
//   tests
//   * not provided for functor validators or settings without validators
//   * for QString fields, returns a QStringList (even if an initializer list of
//     const char* was specified, etc.)
//   * for other fields, returns a vector of the initializer list values
//
// (The implementation of choices_name() is awkward because it has to rely on
// SFINAE from a macro - the actual choices are just passed through as a default
// parameter so the choices() template is only instantiated if choices_name() is
// actually called.  This ensures a compile-time error if choices_name() is
// called for a property that shouldn't have it (although not a very good one).)
#define JsonField_detail_property(type, name, defaultValue, ...) \
    public: const type& name() const { return _##name; } \
    public: void name(const type& value) { clearError(); if (_##name != value) { if (validate(value,##__VA_ARGS__)) { _##name = value; emitPropertyChange({[this](){emit name##Changed();}, QStringLiteral(#name)}); } else { _error = JsonFieldError(HERE, QStringLiteral(#name), QStringLiteral(#type)); } } } \
    signals: Q_SIGNAL void name##Changed(); \
    public: static type default_##name() { return defaultValue; } \
    public: void reset_##name() { type value = default_##name(); if (_##name != value) { _##name = std::move(value); emitPropertyChange({[this](){emit name##Changed();}, QStringLiteral(#name)}); } } \
    private: type _##name = default_##name(); \
    public: static auto choices_##name(decltype(choices(static_cast<type*>(nullptr),##__VA_ARGS__)) c = choices(static_cast<type*>(nullptr),##__VA_ARGS__)) {return c;} \
    Q_PROPERTY(QJsonValue name READ get_##name WRITE set_##name NOTIFY name##Changed RESET reset_##name FINAL)

#define JsonField_detail_setDecl(name) \
    public: void set_##name(const QJsonValue &value);

#define JsonField_detail_setImpl(name, qual) \
    void qual set_##name(const QJsonValue& value) \
    { \
        clearError(); \
        decltype(_##name) actual; \
        if (!json_cast(value, actual)) \
        { \
            _error = JsonFieldError{HERE, \
                QStringLiteral(#name), \
                qs::toQString(kapps::core::typeName<decltype(_##name)>()), \
                jsonValueString(value)}; \
        } \
        else \
            name(actual); \
    }

#define JsonField_detail_getDecl(name) \
    public: QJsonValue get_##name() const;

#define JsonField_detail_getImpl(name, qual) \
    QJsonValue qual get_##name() const \
    { \
        QJsonValue value; \
        if (!json_cast(name(), value)) \
        { \
            qCritical() << "Unable to convert field " #name " to JSON"; \
        } \
        return value; \
    }

// Declare and implement a NativeJsonObject property inline
#define JsonField(type, name, defaultValue, ...) \
    public: JsonField_detail_setImpl(name, ) \
    public: JsonField_detail_getImpl(name, ) \
    JsonField_detail_property(type, name, defaultValue,##__VA_ARGS__)

// Declare a NativeJsonObject property and implement it out-of-line
#define JsonFieldDecl(type, name, defaultValue, ...) \
    JsonField_detail_setDecl(name) \
    JsonField_detail_getDecl(name) \
    JsonField_detail_property(type, name, defaultValue,##__VA_ARGS__)
#define JsonFieldImpl(scope, name) \
    JsonField_detail_setImpl(name, scope::) \
    JsonField_detail_getImpl(name, scope::)

// For object-valued fields, JsonObjectField() provides a mutable name()
// accessor, so the object's properties can be manipulated with
// name().prop(value).
// The enclosing NativeJsonObject must hook up the object's properties to the
// change signals for the object property itself (Client and Daemon do not
// monitor nested properties).
#define JsonObjectField(type, name, defaultValue, ...) \
    JsonField(type, name, defaultValue,##__VA_ARGS__) \
    public: type& name() { return _##name; }

// Inline functions definitions

template<typename T, typename U>
inline bool NativeJsonObject::validate(const T& value, const std::initializer_list<U>& valid)
{
    for (const auto& v : valid)
    {
        if (value == v) return true;
    }
    return false;
}

namespace impl {
    template<size_t Size, size_t Align, typename... Types>
    class JsonVariantImpl;

    template<size_t Size, size_t Align, typename First, typename... Rest>
    class JsonVariantImpl<Size, Align, First, Rest...> : public JsonVariantImpl<(sizeof(First) > Size ? sizeof(First) : Size), (Q_ALIGNOF(First) > Align ? Q_ALIGNOF(First) : Align), Rest...>
    {
        typedef JsonVariantImpl<(sizeof(First) > Size ? sizeof(First) : Size), (Q_ALIGNOF(First) > Align ? Q_ALIGNOF(First) : Align), Rest...> base;
        enum { ID = sizeof...(Rest) + 1 };
        First& data() { return *reinterpret_cast<First*>(this->_data); }
        const First& data() const { return *reinterpret_cast<const First*>(this->_data); }
    public:
        using base::get;
        using base::set;
        using base::equals;
        using base::is;
        using base::as;

        bool get(First& to) const
        {
            if (this->_type == ID)
            {
                to = data();
                return true;
            }
            return false;
        }
        void set(const First& copy) { new(this->_data) First(copy); this->_type = ID; }
        void set(First&& move) { new(this->_data) First(std::move(move)); this->_type = ID; }
        bool setJson(const QJsonValue& value)
        {
            new(this->_data) First();
            if (json_cast(value, data()))
            {
                this->_type = ID;
                return true;
            }
            data().~First();
            return base::setJson(value);
        }
        void reset()
        {
            if (this->_type == ID) { data().~First(); this->_type = 0; }
            else base::reset();
        }
        bool is(First*) const
        {
            return this->_type == ID;
        }
        First as(First*) const
        {
            if (this->_type == ID)
                return data();
            return {};
        }
        template<typename Func>
        void apply(Func&& fn) const
        {
            if (this->_type == ID) fn(data());
            else base::apply(std::forward<Func>(fn));
        }
        template<typename Func, typename Result>
        Result apply(Func&& fn, Result&& defaultValue) const
        {
            if (this->_type == ID) return fn(data());
            else return base::apply(std::forward<Func>(fn), std::forward<Result>(defaultValue));
        }
        bool equals(const JsonVariantImpl& other) const
        {
            if (this->_type == ID && this->_type == other._type) return data() == other.data();
            else return base::equals(other);
        }
        bool equals(const First& value) const
        {
            return this->_type == ID && data() == value;
        }
    };

    template<size_t Size, size_t Align>
    class JsonVariantImpl<Size, Align>
    {
        enum NoSuchOverload {};
    public:
        void get(NoSuchOverload) const {}
        void set(NoSuchOverload) {}
        bool setJson(const QJsonValue&) { return false; }
        void reset() { _type = 0; }
        constexpr bool is(NoSuchOverload) const { return false; }
        constexpr bool as(NoSuchOverload) const { return false; }
        template<typename Func> void apply(Func&&) const {}
        template<typename Func, typename Result> Result apply(Func&&, Result&& defaultValue) const { return std::forward<Result>(defaultValue); }
        bool equals(const JsonVariantImpl& other) const { return !_type && !other._type; }
        bool equals(NoSuchOverload) const {}
    protected:
        Q_DECL_ALIGN(Align) char _data[Size];
        char _type = 0;
    };
}

// Class encapsulating one of several possible native types, all storable in
// a single QJsonValue for serialization. Earlier types take priority when
// deserialized.
//
template<typename... Types>
class JsonVariant : private impl::JsonVariantImpl<0, 0, Types...>
{
    typedef impl::JsonVariantImpl<0, 0, Types...> base;
public:
    JsonVariant() {}
    JsonVariant(const JsonVariant& copy) { copy.apply([this](auto&& value) { base::set(value); }); }
    JsonVariant(JsonVariant&& move) { move.apply([this](auto&& value) { base::set(value); }); }
    template<typename T>
    JsonVariant(T&& value) { base::set(std::forward<T>(value)); }
    ~JsonVariant() { base::reset(); }

    JsonVariant& operator=(const JsonVariant& copy) { if (&copy != this) { base::reset(); copy.apply([this](auto&& value) { base::set(value); }); } return *this; }
    JsonVariant& operator=(JsonVariant&& move) { if (&move != this) { base::reset(); move.apply([this](auto&& value) { base::set(value); }); } return *this; }
    template<typename T>
    JsonVariant& operator=(T&& value) { base::reset(); base::set(std::forward<T>(value)); return *this; }
    bool setJson(const QJsonValue& value) { base::reset(); return base::setJson(value); }

    bool operator==(const JsonVariant& other) const { return base::equals(other); }
    template<typename T>
    bool operator==(T&& value) const { return base::equals(std::forward<T>(value)); }

    template<typename T>
    bool operator!=(T&& value) const { return !operator==(std::forward<T>(value)); }

    template<typename T>
    bool is() const { return base::is(reinterpret_cast<T*>(nullptr)); }
    template<typename T>
    T as() const { return base::as(reinterpret_cast<T*>(nullptr)); }

    using base::get;
    using base::apply;
};

template<typename... Types>
static inline bool json_cast(const QJsonValue& from, JsonVariant<Types...>& to)
{
    return to.setJson(from);
}

template<typename... Types>
static inline bool json_cast(const JsonVariant<Types...>& from, QJsonValue& to)
{
    return from.apply([&](auto&& value) { return json_cast(value, to); }, false);
}


class Path;
// Read the properties from a JSON file.
// Returns false if a new file was created, 'true' if an existing file is found
COMMON_EXPORT bool readProperties(NativeJsonObject &object, const Path &settingsDir,
                                  const char *filename);
COMMON_EXPORT void writeProperties(const QJsonObject &object, const Path &settingsDir,
                                   const char *filename);

// Bridge between Qt and kapps::core JSON support:
// - Provide conversions between Qt types and nlohmann::json
// - Provide conversions between kapps::core::Json{Writable,Readable} and
//   QJsonValue
//
// The goal is to eventually move the daemon entirely to nlohmann::json.  The
// client will probably continue to use Qt's JSON library (as it integrates with
// QML), but since it is intended to move to a dynamic model instead of the
// static models in DaemonState/etc., it shouldn't need any bridging.

// Get a QJsonObject from an nlohman::json that is expected to have an object
// value.  This is relatively complex because:
// - Qt can only parse object- or array-valued JSON, not other types as
//   permitted by RFC8259.
// - Qt's error handling is, as usual, really cumbersome.  Qt doesn't use
//   exceptions, so we have to manually check for lots of error conditions and
//   extract relevant information for tracing.
//
// If the conversion is not possible for any reason (including if the
// nlohmann::json is not an object value), this throws.
//
// There is no helper for the reverse (QJsonObject to nlohmann::json), because
// it's trivial - just QJsonDocument{o}.toJson() followed by
// nlohmann::json::parse(), which throws good exceptions for errors.
COMMON_EXPORT QJsonObject adaptJsonTextToQJsonObject(kapps::core::StringSlice jsonText);
template<class JsonT>
QJsonObject adaptNljToQt(const JsonT &j)
{
    if(!j.is_object())
    {
        KAPPS_CORE_WARNING() << "Can't read JSON of type" << j.type()
            << "- expected object";
        throw std::runtime_error{"object JSON value expected"};
    }

    auto jsonText = j.dump();
    return adaptJsonTextToQJsonObject(jsonText);
}

// Convert QSharedPointer<T> to/from JSON.  nullptr is represented as a JSON null.
template<class T, class JsonT>
void to_json(JsonT &j, const QSharedPointer<T> &p)
{
    if(!p)
    {
        // Parentheses initializion is required here, {value_t::null} may be
        // interpreted as "array of null" due to the initializer_list constructor
        // taking precedence
        j = JsonT(JsonT::value_t::null);
    }
    else
        j = *p;
}
template<class T, class JsonT>
void from_json(const JsonT &j, QSharedPointer<T> &p)
{
    if(j.is_null())
        p.clear();
    else
    {
        auto pValue = QSharedPointer<T>::create();
        from_json(j, *pValue);
        p = std::move(pValue);
    }
}

// Convert between QString and nlohmann::json
template<class JsonT>
void to_json(JsonT &j, const QString &s)
{
    // Creates a std::string containing the content of s in UTF-8, which is then
    // moved into the json value
    j = s.toStdString();
}
template<class JsonT>
void from_json(const JsonT &j, QString &s)
{
    // This throws as intended if the value is not a string
    auto valueStr = j.template get<kapps::core::StringSlice>();
    s = QString::fromUtf8(valueStr.data(), valueStr.size());
}

// Convert between NativeJsonObject and nlohmann::json
template<class JsonT>
void to_json(JsonT &j, const NativeJsonObject &o)
{
    QByteArray jsonText = QJsonDocument{o.toJsonObject()}.toJson();
    j = JsonT::parse(jsonText);
}

template<class JsonT>
void from_json(const JsonT &j, NativeJsonObject &o)
{
    QJsonObject qtJson = adaptNljToQt(j);
    // Normally we try to avoid mutating the result object if any part of the
    // conversion fails, but that's not possible here - we don't know the
    // concrete type of the NativeJsonObject, and it's not assignable to
    // avoid slicing.
    if(!o.readJsonObject(qtJson))
    {
        // There really should be an error if assign() failed
        if(o.error())
        {
            KAPPS_CORE_WARNING() << "Can't read value from JSON data:"
                << o.error();
            throw *o.error();
        }
        KAPPS_CORE_WARNING() << "Can't read value from JSON data: unknown error";
        throw std::runtime_error{"unknown error from NativeJsonObject"};
    }
}

// Convert QJsonValue to kapps::core::JsonReadable.  This has to serialize the
// JSON to transport between Qt and nlohmann::json.
//
// Qt can only serialize JSON for an object or array value (not any JSON
// value, as is now allowed by RFC8259).  This implementation assumes that T
// wants an object value, and it rejects other types.  (We could still relax
// this by putting 'from' into an object for serialization, but no existing
// types need this.)
template<class T, class JsonT = nlohmann::json>
bool json_cast(const QJsonValue &from, kapps::core::JsonReadable<T> &to)
{
    try
    {
        if(!from.isObject())
        {
            KAPPS_CORE_WARNING() << "Can't read" << kapps::core::typeName<T>()
                << "value from JSON of type" << from.type()
                << "- expected object";
            return false;
        }

        QJsonDocument doc{from.toObject()};
        QByteArray docJson = doc.toJson();
        auto nljDoc = JsonT::parse(docJson.begin(), docJson.end());
        from_json(nljDoc, to);
        return true;
    }
    catch(const std::exception &ex)
    {
        // We have to eat the exception, this variation of json_cast is not
        // supposed to throw
        KAPPS_CORE_WARNING() << "Can't read" << kapps::core::typeName<T>()
            << "value from JSON:" << ex.what();
    }
    return false;
}

// kapps::core::JsonReadable<T> values can't be read from JSON, but currently
// NativeJsonObject requires both conversions to exist.  This conversion always
// fails.
template<class T>
bool json_cast(const kapps::core::JsonReadable<T> &, QJsonValue&)
{
    KAPPS_CORE_ERROR() << "Can't convert readable type"
        << kapps::core::typeName<T>() << "to JSON";
    return false;
}

// Convert kapps::core::JsonWritable to a QJsonValue.  Like the conversion from
// kapps::core::JsonReadable, this has to serialize the JSON to transport
// between the libraries.
//
// Again, since Qt can only deserialize object- or array-valued JSON documents,
// this assumes that T wants an object value.
template<class T, class JsonT = nlohmann::json>
bool json_cast(const kapps::core::JsonWritable<T> &from, QJsonValue& to)
{
    try
    {
        JsonT nljDoc;
        to_json(nljDoc, from);
        to = adaptNljToQt(nljDoc);
        return true;
    }
    catch(const std::exception &ex)
    {
        // We have to eat the exception, this variation of json_cast is not
        // supposed to throw
        KAPPS_CORE_WARNING() << "Can't render JSON from"
            << kapps::core::typeName<T>() << "value:" << ex.what();
    }
    return false;
}

// kapps::core::JsonWritable<T> values can't be read from JSON, but currently
// NativeJsonObject requires both conversions to exist.  This conversion always
// fails.
template<class T>
bool json_cast(const QJsonValue &, kapps::core::JsonWritable<T> &)
{
    KAPPS_CORE_ERROR() << "Can't convert writable type"
        << kapps::core::typeName<T>() << "from JSON";
    return false;
}

// kapps::core::JsonConvertible<T> is both writable and readable, use the
// correct operation for each (resolves ambiguity since we had to define bogus
// operations above for JsonWritable/JsonReadable)
template<class T>
bool json_cast(const QJsonValue &from, kapps::core::JsonConvertible<T> &to)
{
    kapps::core::JsonReadable<T> &toReadable{to};
    return json_cast(from, toReadable);
}
template<class T>
bool json_cast(const kapps::core::JsonConvertible<T> &from, QJsonValue& to)
{
    const kapps::core::JsonWritable<T> &fromWritable{from};
    return json_cast(fromWritable, to);
}
