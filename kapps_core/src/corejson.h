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

#pragma once
#include <kapps_core/core.h>
#include "stringslice.h"
#include "logger.h"
#include "util.h"
#include <nlohmann/json_fwd.hpp>

// # JSON conversion
//
// kapps uses "JSON for Modern C++" to interpret and render JSON.
//
// json.h includes nlohmann/json_fwd.hpp to avoid bloating compilation times.
// Include nlohmann/json.hpp when implementing JSON conversions.
//
// Useful reading:
//    * General documentation: https://github.com/nlohmann/json
//    * API for 'nlohmann::json::json': https://nlohmann.github.io/json/api/basic_json/
//
// To allow an object to be read from JSON, derive from JsonReadable<> and
// implement readJson().  For writing to JSON, derive from JsonWritable<> and
// implement writeJson().  For both, derive from JsonConvertible<> and implement
// both methods.
//
// Conventionally, all JSON interpretation should tolerate additional unknown
// fields - we introduce new fields when they can be ignored by older clients.
// Introducing a new field that cannot be ignored, or removing a field, requires
// a new API revision.
//
// For simple objects, this looks like:
//
// class Person : public kapps::core::JsonConvertible<Person>
// {
// public:
//     // A default constructor is needed for basic JSON conversions.
//     Person() = default;
//     // ctors, methods
//
// public:
//     // Read the JSON data from j and apply it to this object.  This should
//     // replace the entire state of the object (i.e. omitted optional fields
//     // should be defaulted, not left in some prior state, etc.)
//     //
//     // Throw if the conversion is not possible (the data are invalid, etc.).
//     //
//     // Be sure to enforce class invariants - if the data would violate an
//     // invariant, throw.
//     void readJson(const nlohmann::json &j);
//
//     // Build a JSON representation of this object.
//     nlohmann::json writeJson() const;
//
// private:
//     std::string _name;
//     unsigned _age;
// };
//
// // Meanwhile, in person.cpp...
// void Person::readJson(const nlohmann::json &)
// {
//     // Person requires both 'name' and 'age', and they must be of the
//     // appropriate type.  If either does not exist or is an incorrect type,
//     // this intentionally throws.  Any other fields are simply ignored.
//     //
//     // There are no other invariants to enforce on this Person, but we could
//     // do that here if needed (say, to enfore non-empty names, age <200,
//     // etc.).  This also means that we don't need to use weak JSON-like types
//     // to store members; for example we could store an Ipv4Address internally
//     // and serialize to/from a string.
//     j.at("name").get_to(_name);
//     j.at("age").get_to(_age);
// }
//
// nlohmann::json Person::writeJson() const
// {
//     // Just return a JSON object with both necessary fields.
//     return {{"name", _name}, {"age", _age}};
// }
//
namespace kapps::core {

// These templates just enable the ADL to_json()/from_json() below.
template<class T>
class JsonReadable{};
template<class T>
class JsonWritable{};
// Shortcut for both readable and writable
template<class T>
class JsonConvertible : public JsonReadable<T>, public JsonWritable<T> {};

template<class T>
void to_json(nlohmann::json &j, const JsonWritable<T> &v)
{
    j = static_cast<const T&>(v).writeJson();
}

template<class T>
void from_json(const nlohmann::json &j, JsonReadable<T> &v)
{
    static_cast<T&>(v).readJson(j);
}

// core::StringSlice can be converted to/from JSON.  Getting a StringSlice
// from JSON results in a StringSlice referring to the `json`'s internal data,
// which is useful for transiently inspecting the data without copying it to
// a std::string.
template<class JsonT>
void to_json(JsonT &j, const StringSlice &v)
{
    // This creates a std::string containing a copy of the slice, which is
    // then moved into the json value.
    j = v.to_string();
}

template<class JsonT>
void from_json(const JsonT &j, StringSlice &v)
{
    // If the json value isn't a string, this throws as intended.  get_ref()
    // gets a reference to the value's internal std::string, so we just
    // reference that data.
    v = j.template get_ref<const std::string&>();
}

// Convert nullable_t to/from JSON.  An empty nullable_t is represented as null
// in JSON.
template<class T, class JsonT>
void to_json(JsonT &j, const nullable_t<T> &v)
{
    if(!v)
    {
        // Parentheses initializion is required here, {value_t::null} may be
        // interpreted as "array of null" due to the initializer_list constructor
        // taking precedence
        j = JsonT(JsonT::value_t::null);
    }
    else
        j = *v;
}
template<class T, class JsonT>
void from_json(const JsonT &j, nullable_t<T> &v)
{
    if(j.is_null())
        v.clear();
    else
    {
        // This currently assumes the object uses the "default-constructible"
        // variation of from_json().
        T value;
        from_json(j, value);
        v.emplace(std::move(value));
    }
}

// Serialize an ArraySlice by converting each element.  It's not possible to
// read an ArraySlice from nlohmann::json, as ArraySlice is non-owning - the
// result needs to own the converted elements, use std::vector, etc. instead.
template<class T, class JsonT>
void to_json(JsonT &j, const ArraySlice<T> &a)
{
    JsonT jsonArray{JsonT::array()};
    for(const auto &element : a)
    {
        JsonT jsonElement;
        to_json(jsonElement, element);
        jsonArray.push_back(std::move(jsonElement));
    }
    j = std::move(jsonArray);
}

// nlohmann::json::begin()/end()/items() silently accept either arrays or
// objects and will even "iterate" a single non-array/object value.  To enforce
// the expected type, use jsonArray() or jsonObject(), like:
//
// for(const auto &element : jsonArray(j.at("elements"))) { ... }
// for(const auto &[key, value] : jsonObject(j.at("properties")).items()) { ... }
//
// These throw if the value is not of the expected type, or return the value
// otherwise.
KAPPS_CORE_EXPORT const nlohmann::json &jsonArray(const nlohmann::json &j);
KAPPS_CORE_EXPORT const nlohmann::json &jsonObject(const nlohmann::json &j);

// Read a JSON array of elements, tracing and tolerating any element that cannot
// be read.  This is used for highly resilient functionality like the regions
// list, which still tries to load valid regions even if some fail.
//
// The functor is invoked for each element read.  This can be used to put the
// elements in a map with some object-specific ID, build a vector, etc.
//
// The JSON type is templated as well even though it's virtually always
// 'nlohmann::json' so we don't have to leak the entire json.hpp from this file;
// it just has to be included by the point of instantiation.
template<class T, class CallbackT, class JsonT>
void readJsonArrayTolerant(const JsonT &j, CallbackT callback)
{
    std::size_t index{0};   // for tracing only
    for(const auto &jsonElement : jsonArray(j))
    {
        try
        {
            callback(jsonElement.template get<T>());
        }
        catch(const std::exception &ex)
        {
            KAPPS_CORE_WARNING() << "Unable to read" << core::typeName<T>()
                << "from array element" << index << "-" << jsonElement;
            // Ignore this value
        }
        ++index;
    }
}


// Read a JSON array into a vector of elements, tracing and tolerating any
// elements that cannot be read.  Wraps readJsonArrayTolerant() when the desired
// output is a vector.
//
// (To read a vector of elements _without_ tolerating individual element errors,
// just use `json.get<std::vector<T>>()`.)
template<class T, class JsonT>
std::vector<T> readJsonVectorTolerant(const JsonT &j)
{
    std::vector<T> result;
    result.reserve(j.size());
    readJsonArrayTolerant<T>(j, [&result](T value){result.push_back(std::move(value));});

    return result;
}

}
