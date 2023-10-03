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

#pragma once
#include <common/src/json.h>
#include <common/src/settings/locations.h>
#include <kapps_regions/src/metadata.h>
#include <nlohmann/json.hpp>

// The Daemon JSON models (StateModel, etc.) use client-specific serializers to
// send the data to clients.  Clients don't need all properties of all objects:
// - Region objects store all servers in the region, which the daemon needs but
//   the client does not (the servers would greatly increase the JSON size of
//   StateModel::availableLocations)
// - Some fields of DaemonAccount are privileged and must not be sent
//
// Additionally, this allows us to directly use types such as kapps::regions
// types in the state models that do not provide JSON serialization, or may
// provide it differently.  We can define specific serializers here for the
// behavior we want when sending (for example) a kapps::regions::Region to
// clients.
namespace clientjson
{

// Use regular ADL serializers by default.  Specialize serializer<> to override
// the client representation.
//
// Note that once we "cross over" to a regular serializer, any contained types
// will also use the regular serializer.  So if a Region contains a Server, and
// only serializer<Server> is specialized, the Server inside a Region still uses
// the normal serializer.  This isn't usually something we need to deal with,
// normally we need to specialize at the top level anyway.
//
// (The second template parameter allows hanging SFINAE conditions on a
// specialization.)
template<class T, class SFINAE = void>
struct serializer
{
    template<class JsonT, class ArgT>
    static void to_json(JsonT &j, ArgT &&arg)
    {
        nlohmann::to_json(j, std::forward<ArgT>(arg));
    }
};

// nlohmann::basic_json specialization using our purpose-specific serializers.
// All of the parameters are the same as nlohmann::json except for the
// serializer.
using json = nlohmann::basic_json<
        std::map,
        std::vector,
        std::string,
        bool,
        std::int64_t,
        std::uint64_t,
        double,
        std::allocator,
        serializer,
        std::vector<std::uint8_t>
    >;

// Though Location doesn't serialize its servers for clients, there is a
// StateModel::connectedServer property that serializes one server to clients
template<>
struct serializer<Server>
{
    static void to_json(json &j, const Server &s)
    {
        j = {
            {"ip", s.ip()},
            {"commonName", s.commonName()}
        };
    }
};

// Clients don't need the individual servers in a location, this saves a huge
// amount of JSON that we would otherwise send for no reason.
template<>
struct serializer<Location>
{
    static void to_json(json &j, const Location &l)
    {
        j = {
            {"id", l.id()},
            {"portForward", l.portForward()},
            {"geoLocated", l.geoLocated()},
            {"autoSafe", l.autoSafe()},
            {"latency", l.latency()},
            {"dedicatedIp", l.dedicatedIp()},
            {"offline", l.offline()},
            {"hasShadowsocks", l.hasShadowsocks()}
        };
    }
};

template<>
struct serializer<CountryLocations>
{
    static void to_json(json &j, const CountryLocations &cl)
    {
        j = {
            {"code", cl.code()},
            {"locations", cl.locations()}
        };
    }
};

template<>
struct serializer<ServiceLocations>
{
    static void to_json(json &j, const ServiceLocations &sl)
    {
        j = {
            {"chosenLocation", sl.chosenLocation()},
            {"bestLocation", sl.bestLocation()},
            {"nextLocation", sl.nextLocation()}
        };
    }
};

// Serializers for kapps::regions::Metadata and contained types.  This allows us
// to put a kapps::regions::Metadata directly into StateModel::regionsMetadata.
template<>
struct serializer<kapps::regions::DisplayText>
{
    static void to_json(json &j, const kapps::regions::DisplayText &dt)
    {
        j = json::object();
        for(const auto &[lang, text] : dt.texts())
            j.emplace(lang.toString(), text);
    }
};

template<>
struct serializer<kapps::regions::DynamicRole>
{
    static void to_json(json &j, const kapps::regions::DynamicRole &dg)
    {
        // id isn't needed; it appears in the dynamicGroups key
        j = {
            {"name", dg.name()},
            {"resource", dg.resource()},
            {"winIcon", dg.winIcon()}
        };
    }
};

template<>
struct serializer<kapps::regions::CountryDisplay>
{
    static void to_json(json &j, const kapps::regions::CountryDisplay &cd)
    {
        // code isn't needed; it appears in the countryDisplays key
        j = {
            {"name", cd.name()},
            {"prefix", cd.prefix()}
        };
    }
};

template<>
struct serializer<kapps::regions::RegionDisplay>
{
    static void to_json(json &j, const kapps::regions::RegionDisplay &rd)
    {
        // id isn't needed; it appears in the regionDisplays key
        j = {
            {"country", rd.country()},
            {"geoLatitude", rd.geoLatitude()},
            {"geoLongitude", rd.geoLongitude()},
            {"name", rd.name()}
        };
    }
};

template<>
struct serializer<kapps::regions::Metadata>
{
    template<class ValueT>
    static json keyedArrayJson(kapps::core::ArraySlice<const ValueT * const> values,
        kapps::core::StringSlice (ValueT::*keyMethod)() const)
    {
        json j = json::object();
        for(const auto &pValue : values)
        {
            if(!pValue)
                continue;
            j.emplace((pValue->*keyMethod)().to_string(), *pValue);
        }
        return j;
    }
    static void to_json(json &j, const kapps::regions::Metadata &m)
    {
        j = {
            {"dynamicRoles", keyedArrayJson(m.dynamicGroups(), &kapps::regions::DynamicRole::id)},
            {"countryDisplays", keyedArrayJson(m.countryDisplays(), &kapps::regions::CountryDisplay::code)},
            {"regionDisplays", keyedArrayJson(m.regionDisplays(), &kapps::regions::RegionDisplay::id)}
        };
    }
};

}
