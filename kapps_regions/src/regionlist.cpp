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

#include "regionlist.h"
#include <kapps_core/src/logger.h>
#include <nlohmann/json.hpp>

namespace kapps::regions {

RegionList::PIAv6_t RegionList::PIAv6{};

RegionList::RegionList(core::StringSlice regionsJson,
                       core::StringSlice shadowsocksJson,
                       core::ArraySlice<const DedicatedIp> dips,
                       core::ArraySlice<const ManualRegion> manual)
{
    auto json = nlohmann::json::parse(regionsJson);
    auto groups = readJsonServiceGroups(json);

    // pubdns is optional; empty array if not provided.  If provided, all values
    // must be IPv4 addresses.
    auto itPubDns = json.find("pubdns");
    if(itPubDns != json.end())
        _publicDnsServers = itPubDns->get<std::vector<core::Ipv4Address>>();

    // If a Shadowsocks list was given, read Shadowsocks servers so we can
    // include them in the regions list.  Allow an empty shadowsocksJson for
    // clients that don't use it.
    ShadowsocksServers shadowsocksServers;
    nlohmann::json shadowsocksJsonObj;
    if(!shadowsocksJson.empty())
    {
        shadowsocksJsonObj = nlohmann::json::parse(shadowsocksJson);
        shadowsocksServers = readShadowsocksServers(shadowsocksJsonObj);
    }

    // The regions and servers don't use plain JSON conversions due to the
    // "service group name" -> "service group" translation, which requires the
    // service group map.  Read them manually.
    const auto &jsonRegions = json.at("regions");
    _regionsById.reserve(jsonRegions.size() + dips.size() + manual.size());

    readJsonRegions(jsonRegions, groups, shadowsocksServers);
    StdRegionsById stdRegions;
    stdRegions.reserve(_regionsById.size());
    for(const auto &[id, pRegion] : _regionsById)
        stdRegions.insert({id, pRegion.get()});

    // Add Dedicated IP regions.  These need to reference standard regions for
    // some details like port forwarding, etc., so they are added after the
    // standard regions.
    buildDipRegions(dips, groups, stdRegions);
    // Add manual regions too.
    buildManualRegions(manual, groups, stdRegions);

    // Set up _regions now that we've fully built _regionsById
    _regions.reserve(_regionsById.size());
    for(const auto &[id, pRegion] : _regionsById)
        _regions.push_back(pRegion.get());
}

RegionList::RegionList(PIAv6_t, core::StringSlice regionsJson,
                       core::StringSlice shadowsocksJson,
                       core::ArraySlice<const DedicatedIp> dips,
                       core::ArraySlice<const ManualRegion> manual)
{
    auto json = nlohmann::json::parse(regionsJson);
    // Read service groups.  The v6 service groups are just an array of services
    // (the "name" is the property key in "groups"), and the format is nearly
    // identical to v7 - we can use the normal logic to parse the services
    // arrays.
    //
    // Unlike v7, the v6 format does not indicate OpenVPN NCP support in the
    // service group; it's specified on each server instead.  To handle that,
    // we create two copies of each service group - one with NCP=true, and one
    // with NCP=false, then select the appropriate one on a per-server basis.
    ServiceGroups ncpGroups, pssGroups; // "NCP" or "pia-signal-settings" group variants
    const auto &jsonGroups = json.at("groups");
    ncpGroups.reserve(jsonGroups.size());
    pssGroups.reserve(jsonGroups.size());
    for(const auto &[groupKey, groupValue] : core::jsonObject(json.at("groups")).items())
    {
        try
        {
            // Ignore duplicates
            if(ncpGroups.count(groupKey) || pssGroups.count(groupKey))
            {
                KAPPS_CORE_WARNING() << "Duplicate service group" << groupKey
                    << "in regions list";
                throw std::runtime_error{"Duplicate service group in regions list"};
            }

            ServiceGroup newGroup{};
            newGroup.readPiav6JsonServicesArray(groupValue);
            // The group has NCP since v6 lacks an "ncp" attribute; create a
            // pia-signal-settings variation of the group too.  (Do this even if
            // the group doesn't actually have any OpenVPN services, so we can
            // still look in either map when reading servers.)
            pssGroups.emplace(groupKey,
                std::make_shared<ServiceGroup>(
                    newGroup.openVpnUdpPorts().to_vector(), false,
                    newGroup.openVpnTcpPorts().to_vector(), false,
                    newGroup.wireGuardPorts().to_vector(), newGroup.ikev2(),
                    newGroup.shadowsocksPorts().to_vector(),
                    newGroup.shadowsocksKey().to_string(),
                    newGroup.shadowsocksCipher().to_string(),
                    newGroup.metaPorts().to_vector()));
            ncpGroups.emplace(groupKey,
                std::make_shared<ServiceGroup>(std::move(newGroup)));
        }
        catch(const std::exception &ex)
        {
            KAPPS_CORE_WARNING() << "Unable to read service group" << groupKey << "-"
                << ex.what();
        }
    }

    // The v6 format does not provide pubdns.

    // If a Shadowsocks list was given, read Shadowsocks servers.
    ShadowsocksServers shadowsocksServers;
    nlohmann::json shadowsocksJsonObj;
    if(!shadowsocksJson.empty())
    {
        shadowsocksJsonObj = nlohmann::json::parse(shadowsocksJson);
        shadowsocksServers = readShadowsocksServers(shadowsocksJsonObj);
    }

    const auto &jsonRegions = json.at("regions");
    _regionsById.reserve(jsonRegions.size() + dips.size() + manual.size());

    readPiav6JsonRegions(jsonRegions, ncpGroups, pssGroups, shadowsocksServers);
    StdRegionsById stdRegions;
    stdRegions.reserve(_regionsById.size());
    for(const auto &[id, pRegion] : _regionsById)
        stdRegions.insert({id, pRegion.get()});

    // DIP and manual regions are assumed not to support NCP (use pssGroups).
    // Manual has a specific override for this; DIP currently does not.
    buildDipRegions(dips, pssGroups, stdRegions);
    buildManualRegions(manual, pssGroups, stdRegions);

    // Set up _regions now that we've fully built _regionsById
    _regions.reserve(_regionsById.size());
    for(const auto &[id, pRegion] : _regionsById)
        _regions.push_back(pRegion.get());
}

auto RegionList::readJsonServiceGroups(const nlohmann::json &json)
    -> ServiceGroups
{
    ServiceGroups groups;
    const auto &jsonGroups = json.at("service_configs");
    groups.reserve(jsonGroups.size());
    for(const auto &jsonGroup : core::jsonArray(jsonGroups))
    {
        // Malformed service groups, servers, regions, etc. are ignored
        // individually; we still parse as much of the regions list as we can
        core::StringSlice name;
        try
        {
            name = jsonGroup.at("name").get<core::StringSlice>();

            // If it's a duplicate, ignore it.
            if(groups.count(name))
            {
                KAPPS_CORE_WARNING() << "Duplicate service group" << name
                    << "in regions list";
                throw std::runtime_error{"Duplicate service group in regions list"};
            }

            auto group = jsonGroup.get<ServiceGroup>();
            groups.emplace(name, std::make_shared<ServiceGroup>(std::move(group)));
        }
        catch(const std::exception &ex)
        {
            KAPPS_CORE_WARNING() << "Unable to read service group" << name << "-"
                << ex.what();
        }
    }

    return groups;
}

void RegionList::readJsonRegions(const nlohmann::json &jsonRegions,
    const ServiceGroups &groups,
    const ShadowsocksServers &shadowsocksServers)
{
    for(const auto &jsonRegion : core::jsonArray(jsonRegions))
    {
        core::StringSlice id;
        try
        {
            id = jsonRegion.at("id").get<core::StringSlice>();
            if(_regionsById.count(id))
            {
                KAPPS_CORE_WARNING() << "Duplicate region" << id
                    << "in regions list";
                throw std::runtime_error("Duplicate region ID");
            }

            auto autoRegion = jsonRegion.at("auto_region").get<bool>();
            auto portForward = jsonRegion.at("port_forward").get<bool>();
            auto geo = jsonRegion.at("geo").get<bool>();

            auto servers = readJsonRegionServers(jsonRegion, id, groups);
            // Add Shadowsocks if the region isn't offline
            if(!servers.empty())
                addShadowsocksServer(id, servers, shadowsocksServers);

            auto pRegion = std::make_shared<Region>(id.to_string(), autoRegion, portForward,
                    geo, std::string{}, std::move(servers));
            _regionsById.emplace(pRegion->id(), std::move(pRegion));
        }
        catch(const std::exception &ex)
        {
            KAPPS_CORE_WARNING() << "Unable to read region" << id << "-"
                << ex.what();
        }
    }
}

auto RegionList::readJsonRegionServers(const nlohmann::json &jsonRegion,
    core::StringSlice id, const ServiceGroups &groups)
    -> std::vector<std::shared_ptr<const Server>>
{
    const auto &jsonServers = jsonRegion.at("servers");
    std::vector<std::shared_ptr<const Server>> servers;
    servers.reserve(jsonServers.size());
    int serverIdx{};    // Just for diagnostics
    for(const auto &jsonServer : core::jsonArray(jsonServers))
    {
        try
        {
            auto ip = jsonServer.at("ip").get<core::Ipv4Address>();
            auto cn = jsonServer.at("cn").get<std::string>();
            std::string fqdn;
            // FQDN is optional; only used for IKEv2 on some platforms
            auto itFqdn = jsonServer.find("fqdn");
            if(itFqdn != jsonServer.end())
                fqdn = itFqdn->get<std::string>();
            auto group = jsonServer.at("service_config").get<core::StringSlice>();

            // Find the group
            auto itGroup = groups.find(group);
            if(itGroup == groups.end())
            {
                KAPPS_CORE_WARNING() << "Unable to find service config" << group
                    << "for server" << serverIdx << "in region" << id;
            }
            // Otherwise, it existed - if it had at least one service,
            // store this server.  (If it had no known services, ignore
            // this server.)
            else if(itGroup->second && itGroup->second->hasAnyService())
            {
                servers.push_back(std::make_shared<Server>(ip, std::move(cn),
                    std::move(fqdn), itGroup->second));
            }
        }
        catch(const std::exception &ex)
        {
            KAPPS_CORE_WARNING() << "Unable to read server" << serverIdx
                << "of region" << id;
        }

        ++serverIdx;
    }

    return servers;
}

// Map from legacy Shadowsocks IDs (from legacy infrastructure) to
// corresponding modern region IDs.  Legacy IDs from the list are replaced with
// the new IDs.
const std::unordered_map<core::StringSlice, core::StringSlice> ssLegacyIds
{
    {"us_dal", "us_south_west"},    // "US Texas"
    {"us_sea", "us_seattle"},
    {"us_nyc", "us-newjersey"},     // "US East"
    {"jp", "japan"},
    {"uk", "uk-london"},
    {"nl", "nl_amsterdam"}
};

auto RegionList::readShadowsocksServers(const nlohmann::json &json) const
    -> ShadowsocksServers
{
    ShadowsocksServers servers;
    int idx{0}; // For tracing
    for(const auto &ssRegion : core::jsonArray(json))
    {
        try
        {
            core::StringSlice id{ssRegion.at("region").get<core::StringSlice>()};
            // If this is a legacy ID, use the new ID instead
            auto itLegacyId = ssLegacyIds.find(id);
            if(itLegacyId != ssLegacyIds.end())
                id = itLegacyId->second;

            // Create a service group - no attempt is made to actually deduplicate
            // servers with identical configuration
            auto pServiceGroup = std::make_shared<ServiceGroup>(
                std::vector<std::uint16_t>{}, false,
                std::vector<std::uint16_t>{}, false,
                std::vector<std::uint16_t>{}, false,
                std::vector<std::uint16_t>{ssRegion.at("port").get<std::uint16_t>()},
                ssRegion.at("key").get<std::string>(),
                ssRegion.at("cipher").get<std::string>(),
                std::vector<std::uint16_t>{});
            // Then make a server.  No common name is known for these servers,
            // Shadowsocks doesn't need it
            auto pServer = std::make_shared<Server>(
                ssRegion.at("host").get<core::Ipv4Address>(),
                std::string{}, std::string{}, std::move(pServiceGroup));
            servers.emplace(id, std::move(pServer));
        }
        catch(const std::exception &ex)
        {
            KAPPS_CORE_WARNING() << "Couldn't read Shadowsocks location"
                << idx << "-" << ex.what();
        }
        ++idx;
    }
    return servers;
}

void RegionList::addShadowsocksServer(core::StringSlice id,
    std::vector<std::shared_ptr<const Server>> &servers,
    const ShadowsocksServers &shadowsocksServers) const
{
    auto itSs = shadowsocksServers.find(id);
    if(itSs != shadowsocksServers.end())
        servers.push_back(itSs->second);
}

void RegionList::readPiav6JsonRegions(const nlohmann::json &jsonRegions,
    const ServiceGroups &ncpGroups, const ServiceGroups &pssGroups,
    const ShadowsocksServers &shadowsocksServers)
{
    for(const auto &jsonRegion : core::jsonArray(jsonRegions))
    {
        core::StringSlice id;
        try
        {
            id = jsonRegion.at("id").get<core::StringSlice>();
            if(_regionsById.count(id))
            {
                KAPPS_CORE_WARNING() << "Duplicate region" << id
                    << "in regions list";
                throw std::runtime_error("Duplicate region ID");
            }

            auto autoRegion = jsonRegion.at("auto_region").get<bool>();
            auto portForward = jsonRegion.at("port_forward").get<bool>();
            auto geo = jsonRegion.at("geo").get<bool>();
            // * v6 doesn't have a 'new' flag.
            // * 'name' and 'country' are used by Metadata, not RegionList (moved
            //   to metadata in regions v7 / metadata v3)
            // * 'dns' is not used.
            auto offline = jsonRegion.at("offline").get<bool>();

            std::vector<std::shared_ptr<const Server>> servers;
            // v6 has an explicit 'offline' flag for each region.  v7 just
            // indicates offline regions by providing no servers.  The 'offline'
            // flag was never used in lieu of just providing no servers, but
            // just in case it would be set, skip reading the servers so the
            // region does become 'offline'.
            if(!offline)
                servers = readPiav6JsonRegionServers(jsonRegion, id, ncpGroups, pssGroups);
            if(!servers.empty())
                addShadowsocksServer(id, servers, shadowsocksServers);

            auto pRegion = std::make_shared<Region>(id.to_string(), autoRegion,
                    portForward, geo, std::string{}, std::move(servers));
            _regionsById.emplace(pRegion->id(), std::move(pRegion));
        }
        catch(const std::exception &ex)
        {
            KAPPS_CORE_WARNING() << "Unable to read region" << id << "-"
                << ex.what();
        }
    }
}

auto RegionList::readPiav6JsonRegionServers(const nlohmann::json &jsonRegion,
    core::StringSlice id,
    const ServiceGroups &ncpGroups, const ServiceGroups &pssGroups)
    -> std::vector<std::shared_ptr<const Server>>
{
    const auto &jsonServers = jsonRegion.at("servers");
    std::vector<std::shared_ptr<const Server>> servers;
    int serverIdx{};    // Just for diagnostics
    // In v6, the servers are listed as an object containing a property for each
    // service group.  That in turn contains an array of servers for that
    // service group.
    //
    // This means it's not possible to provide a totally-ordered list of
    // servers, because the service groups can't be mixed together.  v7 corrects
    // this by providing servers as a single array, with each naming a service
    // group.
    //
    // To adapt the v6 format, just read the service groups in the order they're
    // listed, and append to the totally-ordered servers.
    for(const auto &[groupName, groupServers] : core::jsonObject(jsonServers).items())
    {
        servers.reserve(servers.size() + groupServers.size());
        for(const auto &jsonServer : core::jsonArray(groupServers))
        {
            try
            {
                auto ip = jsonServer.at("ip").get<core::Ipv4Address>();
                auto cn = jsonServer.at("cn").get<std::string>();
                // * v6 does not have 'fqdn'.
                // * "van" is optional:
                //   - false indicates that the server requires pia-signal-settings
                //   - true or absent indicates that the servers supports NCP

                bool ncp{true};
                auto itVanProperty = jsonServer.find("van");
                if(itVanProperty != jsonServer.end())
                    ncp = itVanProperty->get<bool>();

                // Find the group
                const ServiceGroups &groups{ncp ? ncpGroups : pssGroups};
                auto itGroup = groups.find(groupName);
                if(itGroup == groups.end())
                {
                    KAPPS_CORE_WARNING() << "Unable to find group" << groupName
                        << "for server" << serverIdx << "in region" << id;
                }
                // Otherwise, it existed - if it had at least one service,
                // store this server.  (If it had no known services, ignore
                // this server.)
                else if(itGroup->second && itGroup->second->hasAnyService())
                {
                    servers.push_back(std::make_shared<Server>(ip, std::move(cn),
                        std::string{}, itGroup->second));
                }
            }
            catch(const std::exception &ex)
            {
                KAPPS_CORE_WARNING() << "Unable to read server" << serverIdx
                    << "of region" << id;
            }

            ++serverIdx;
        }
    }

    return servers;
}

void RegionList::buildDipRegions(const core::ArraySlice<const DedicatedIp> &dips,
    const ServiceGroups &groups,
    const StdRegionsById &stdRegions)
{
    for(const auto &dip : dips)
    {
        // Find the corresponding region
        if(_regionsById.count(dip.dipRegionId))
        {
            KAPPS_CORE_WARNING() << "Duplicate region ID" << dip.dipRegionId;
            continue;
        }
        auto itCorrespondingRegion = stdRegions.find(dip.correspondingRegionId);
        if(itCorrespondingRegion == stdRegions.end() ||
            !itCorrespondingRegion->second)
        {
            KAPPS_CORE_WARNING() << "Cannot find corresponding region"
                << dip.correspondingRegionId << "for DIP region"
                << dip.dipRegionId;
            // We can't recover from this since we lack essential display
            // information for the DIP region.  Ignore this region and continue
            continue;
        }

        // Build a Server for the service groups
        std::vector<std::shared_ptr<const Server>> servers;
        servers.reserve(dip.serviceGroups.size());
        for(const auto &serviceGroup : dip.serviceGroups)
        {
            // Find the service group
            auto itServiceGroup = groups.find(serviceGroup);
            if(itServiceGroup == groups.end() || !itServiceGroup->second)
            {
                KAPPS_CORE_WARNING() << "Cannot find service group"
                    << serviceGroup << "for DIP region"
                    << dip.dipRegionId;
                // We can't add a server, we'll still add the region so it shows up
                // "offline".
            }
            else
            {
                servers.push_back(std::make_shared<Server>(dip.address,
                    dip.commonName.to_string(), dip.fqdn.to_string(),
                    itServiceGroup->second));
            }
        }

        const Region &correspondingRegion{*itCorrespondingRegion->second};
        auto pRegion = std::make_shared<Region>(dip.dipRegionId.to_string(),
                                     false,  // DIP regions are not selected automatically
                                     correspondingRegion.portForward(),
                                     correspondingRegion.geoLocated(),
                                     dip.address,
                                     std::move(servers));
        _regionsById.emplace(pRegion->id(), std::move(pRegion));
    }
}

void RegionList::buildManualRegions(const core::ArraySlice<const ManualRegion> &manualRegions,
    const ServiceGroups &groups,
    const StdRegionsById &stdRegions)
{
    for(const auto &manual : manualRegions)
    {
        // Find the corresponding region
        if(_regionsById.count(manual.manualRegionId))
        {
            KAPPS_CORE_WARNING() << "Duplicate region ID" << manual.manualRegionId;
            continue;
        }
        // Find the corresponding region.  This is optional for manual regions
        auto itCorrespondingRegion = stdRegions.find(manual.correspondingRegionId);
        const Region *pCorrespondingRegion{};
        if(itCorrespondingRegion != stdRegions.end())
            pCorrespondingRegion = itCorrespondingRegion->second;

        // Hack the service groups to apply overrides specified for this server
        ServiceGroups effectiveGroups;
        for(const auto &groupEntry : groups)
        {
            assert(groupEntry.second); // Ensured by RegionList ctor
            const auto &existing = *groupEntry.second;
            auto openVpnUdpPorts = existing.openVpnUdpPorts().to_vector();
            auto openVpnUdpNcp = existing.openVpnUdpNcp();
            auto openVpnTcpPorts = existing.openVpnTcpPorts().to_vector();
            auto openVpnTcpNcp = existing.openVpnTcpNcp();

            if(manual.forceOpenVpnNcp)
            {
                openVpnUdpNcp = true;
                openVpnTcpNcp = true;
            }
            if(!manual.openVpnUdpOverridePorts.empty())
                openVpnUdpPorts = manual.openVpnUdpOverridePorts.to_vector();
            if(!manual.openVpnTcpOverridePorts.empty())
                openVpnTcpPorts = manual.openVpnTcpOverridePorts.to_vector();

            // We could optimize this to avoid creating extra copies if the
            // group is unchanged, but it's not really worth it since this is a
            // dev tool, and we usually only have at most 1 manual server
            // anyway.
            effectiveGroups[groupEntry.first] = std::make_shared<ServiceGroup>(
                    std::move(openVpnUdpPorts), openVpnUdpNcp,
                    std::move(openVpnTcpPorts), openVpnTcpNcp,
                    existing.wireGuardPorts().to_vector(),
                    existing.ikev2(),
                    existing.shadowsocksPorts().to_vector(), existing.shadowsocksKey().to_string(),
                    existing.shadowsocksCipher().to_string(),
                    existing.metaPorts().to_vector());
        }

        // Build a Server for the service group
        std::vector<std::shared_ptr<const Server>> servers;
        // We only include meta servers from the corresponding region if it was
        // given, but use the full count to reserve for simplicity
        servers.reserve(manual.serviceGroups.size() +
            (pCorrespondingRegion ? pCorrespondingRegion->servers().size() : 0));
        for(const auto &serviceGroup : manual.serviceGroups)
        {
            // Find the service group
            auto itServiceGroup = groups.find(serviceGroup);
            if(itServiceGroup == groups.end() || !itServiceGroup->second)
            {
                KAPPS_CORE_WARNING() << "Cannot find service group"
                    << serviceGroup << "for manual region"
                    << manual.manualRegionId;
                // We can't add the server; add the region anyway so it shows up
                // "offline"
            }
            else
            {
                servers.push_back(std::make_unique<Server>(manual.address,
                    manual.commonName.to_string(), manual.fqdn.to_string(),
                    itServiceGroup->second));
            }
        }

        // If a corresponding region was given, copy meta servers so this
        // region also provides the meta service
        if(pCorrespondingRegion)
        {
            for(const auto &pServer : pCorrespondingRegion->servers())
            {
                if(pServer && pServer->hasMeta())
                {
                    // Make a service group containing only the meta service;
                    // don't copy other services.  This also makes unnecessary
                    // duplicates if there's more than one meta server, but
                    // again it's not significant for manual servers.
                    auto pMetaGroup = std::make_shared<ServiceGroup>(
                        std::vector<std::uint16_t>{}, true,
                        std::vector<std::uint16_t>{}, true,
                        std::vector<std::uint16_t>{}, false,
                        std::vector<std::uint16_t>{},
                        std::string{}, std::string{},
                        pServer->metaPorts().to_vector());
                    servers.push_back(std::make_shared<Server>(pServer->address(),
                        pServer->commonName().to_string(), pServer->fqdn().to_string(),
                        pMetaGroup));
                }
            }
        }

        // Most of the flags for a manual region can be defaulted since this is
        // a dev tool
        auto pRegion = std::make_shared<Region>(manual.manualRegionId.to_string(),
                                     false,  // Manual regions are not selected automatically
                                     true,   // Always has port forwarding
                                     false,  // Never geo-located
                                     std::string{},     // Not DIP
                                     std::move(servers));
        _regionsById.emplace(pRegion->id(), std::move(pRegion));
    }
}

const Region *RegionList::getRegion(core::StringSlice id) const
{
    auto itRegion = _regionsById.find(id);
    if(itRegion != _regionsById.end())
        return itRegion->second.get();
    return {};
}

}
