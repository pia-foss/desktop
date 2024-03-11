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
#include "common.h"
#include <kapps_core/src/corejson.h>
#include <kapps_core/src/coresignal.h>
#include <unordered_map>

// JsonState represents a set of state properties using JSON.  The properties
// can individually signal changes, and they can be individually serialized.
//
// JsonState represents runtime state - it cannot be read from JSON, and as a
// result the properties do not need to be readable from JSON.  This allows the
// properties to use representations with strong invariants.
//
// JsonState can also use a non-default serializer, such as a "client state
// serializer" that only serializes information that is actually relevant to
// clients.  This can be useful to put (for example) a
// kapps::regions::RegionList into the state without serializing all of the
// individual servers, and without having to define "wrapper" types for
// serialization only.
//
// To define a JsonState object, derive from JsonState, then define any number
// of properties with JsonProperty().  (The macro mainly duplicates the name to
// both the variable name and JSON property name.  There's no way to reflect on
// on this in C++17, so we can nearly escape the macro, but not quite.)
//
// For example:
//
// class ServiceState : public JsonState
// {
// public:
//     // Default constructor and assignment are fine.  Copy must be delegated
//     // to construction and assignment (due to a limitation in JsonState), but
//     // you don't have to list all the properties:
//     ServiceState(const ServiceState &other) : ServiceState{} {*this = other;}
//
// public:
//     // Declare any number of fields using JsonProperty:
//     JsonProperty(int, numberOfServers);  // Defaults to T{}, here 0
//     JsonProperty(std::string, connectionState, "Disconnected");  // Defaults to value listed
//     JsonProperty(std::vector<std::string>, recentServers);
// };
//
// Then, later, set property values as they change:
//     // class declared member `ServiceState _serviceState`
//     _serviceState.numberOfServers(100);  // we found some servers
//     _serviceState.connectionState("Connected");  // we are connected
//     _serviceState.recentServers({"newyork405", "newyork421"});
//
// To observe any property change, connect to the JsonState::propertyChanged()
// signal.  Then use JsonState::getJsonObject() to get the entire object, or use
// JsonState::getProperty() to get individual properties for an incremental
// change:
//     auto stateObj = _serviceState.getJsonObject();   // entire state
//     // assume we observed changes in numberOfServers and recentServers
//     nlohmann::json incrementalChange{};
//     incrementalChange.emplace("numberOfServers",
//         _serviceState.getProperty("numberOfServers"));
//     incrementalChange.emplace("recentServers",
//         _serviceState.getProperty("recentServers"));
//
// To observe a specific property change, connect to that property's changed
// signal:
//     _serviceState.numberOfServers.changed = [this]{ /*number of servers changed*/ };

// This macro is used to define a property inside a JsonState-derived object;
// it declares a Property<>.  You can declare a Property<> yourself too, but the
// macro captures the variable name as the JSON property name.
//
// A default value can optionally provided; if omitted it is T{}
#define JsonProperty(T, name, ...) \
    Property<T> name{*this, #name,##__VA_ARGS__}

// The JsonState class keeps track of the defined properties so the complete
// JSON object can be assembled with getJsonObject() and general property
// changes can be observed.
//
// Any nlohmann::basic_json<> specialization can be used as the JSON
// representation, which determines how the properties are serialized.  A
// purpose-specific JSONSerializer can be used instead of
// nlohmann::adl_serializer to serialize the properties in a specific way.
template<class JsonT>
class JsonState
{
public:
    class PropertyBase
    {
    public:
        PropertyBase(JsonState &parent, std::string name)
            : _parent{parent}, _name{std::move(name)}
        {
            parent.addProperty(_name, *this);
        }

        // Not copiable since there is no way to figure out the new parent
        // relationship.  However, PropertyBase can be assigned, which does
        // not affect the parent or name (it remains part of the same object,
        // only the value is assigned).
        PropertyBase(const PropertyBase &) = delete;
        PropertyBase &operator=(const PropertyBase &) {return *this;}

    public:
        // Get the property value as JSON.  This can throw if the conversion
        // fails for any reason.
        virtual JsonT getJson() const = 0;

    protected:
        // Tell JsonState to emit a signal indicating that this property has
        // changed
        void signalChange() { changed(); _parent.propertyChanged(_name); }

    public:
        // This property changed
        kapps::core::Signal<> changed;

    private:
        // The JsonState parent; used to signal changes
        JsonState &_parent;
        // The name of this field when represented in JSON
        std::string _name;
    };

    template<class T>
    class Property : public PropertyBase
    {
    public:
        Property(JsonState &parent, std::string name)
            : PropertyBase{parent, std::move(name)}, _value{}
        {}
        Property(JsonState &parent, std::string name, T value)
            : PropertyBase{parent, std::move(name)}, _value{std::move(value)}
        {}

        // Not copiable due to PropertyBase
        // Assignment is implemented to signal a change if one occurs
        Property &operator=(const Property &other)
        {
            PropertyBase::operator=(other);
            operator()(other());
            return *this;
        }

    public:
        // Get the value as the native type.  There is no mutable access to the
        // value - even if the value is an object, it should not be mutated
        // directly, as this wouldn't be indicated by JsonState.  Create and set
        // a new object with the change instead.
        const T &operator()() const {return _value;}
        // Set the value.  If value is equal to the existing value, this is
        // ignored (no change is generated to avoid sending duplicate data to
        // clients).
        void operator()(T value)
        {
            if(!(value == _value))
            {
                _value = std::move(value);
                this->signalChange();
            }
        }

        // Get the value as JSON.
        virtual JsonT getJson() const override
        {
            // Parentheses initialization to avoid errant initializer-list
            // construction
            return JsonT(_value);
        }

    private:
        T _value;
    };

public:
    // Default construction is fine; initially there are no properties until
    // the Property objects add themselves.
    JsonState() = default;
    // We can't copy-construct a JsonState because we can't determine our own
    // _properties map from other.  (In theory it might be possible with some
    // careful pointer offset manipulation and some assumptions, but this would
    // be fragile, especially if JsonState was accidentally sliced.)  Derived
    // types can implement copy by delegating to their default constructor and
    // assignment.
    JsonState(const JsonState &) = delete;

    // Assignment does _not_ copy the propertyChanged signal or the _properties
    // map, but derived objects can still be default-assigned.  The _properties
    // map is just used to reflect on all properties, so do not copy it.
    // Copying propertyChanged would be surprising to the receiver, who would
    // receive changes about the copied values in this object unexpectedly.
    JsonState &operator=(const JsonState &) {return *this;}

    // Default destruction is fine; note that Property objects referenced in
    // _properties are no longer valid at this point.
    ~JsonState() = default;

private:
    // Used by PropertyBase to connect itself to this JsonState
    void addProperty(std::string name, const PropertyBase &property)
    {
        _properties.emplace(std::move(name), property);
    }

public:
    // Get an individual property as JSON, by name.  Throws if the name does not
    // exist, or if the serialization fails (only possible if the object's
    // serializer can throw).
    JsonT getProperty(const std::string &name) const
    {
        return _properties.at(name).getJson();
    }

    // Get the entire object as a JSON object.  If any individual property cannot be
    // serialized, that property is omitted (exceptions are not propagated).
    JsonT getJsonObject() const
    {
        JsonT obj{};
        for(const auto &[name, property] : _properties)
        {
            try
            {
                obj.emplace(name, property.getJson());
            }
            catch(const std::exception &ex)
            {
                KAPPS_CORE_WARNING() << "Ignoring property" << name
                    << "- could not serialize:" << ex.what();
            }
        }

        return obj;
    }

public:
    // Emitted when a property is modified along with the property's name
    kapps::core::Signal<kapps::core::StringSlice> propertyChanged;

private:
    // All of the properties in this JsonState.  This is used to get the entire
    // object as JSON.
    std::unordered_map<std::string, const PropertyBase &> _properties;
};
