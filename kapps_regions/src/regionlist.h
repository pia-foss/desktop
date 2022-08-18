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
#include "region.h"
#include <kapps_regions/dedicatedip.h>
#include <kapps_core/src/corejson.h>
#include <unordered_map>
#include <vector>

namespace kapps::regions {

// Dedicated IP structure used to indicate DIPs to RegionList.  This is similar
// to the KARDedicatedIP API structure but adapted to internal types.
//
// All members are non-owning; this is only used to describe DIPs to the
// constructor so internal representations can be built.
struct DedicatedIp
{
    // The client's artificial "region ID" for the generated DIP region.
    core::StringSlice dipRegionId;
    // The IPv4 address of the VPN server for this dedicated IP
    core::Ipv4Address address;
    // The certificate common name for this server
    core::StringSlice commonName;
    // FQDN of the server - only used on some platforms for IKEv2, empty if not
    // needed
    core::StringSlice fqdn;
    // The service group(s) from the general regions list to apply to this
    // region - indicates the available protocols, ports, etc.  Represents the
    // deployment configuration applied by Ops.
    core::ArraySlice<const core::StringSlice> serviceGroups;
    // The region ID for the corresponding region - required for DIP regions.
    // Metadata information such as country name, region name, geo flag, etc.
    // are taken from this region, and auxiliary services such as
    // meta/Shadowsocks are used from this region when needed.
    core::StringSlice correspondingRegionId;
};

// Manual region structure used to indicate manual regions to RegionList.  This
// is similar to the KARManualRegion API structure but adapted to internal
// types.
//
// All members are non-owning; this is only used to describe manual regions to
// the constructor so internal representations can be built.
struct ManualRegion
{
    // The client's artifical "region ID" for the manual region.
    core::StringSlice manualRegionId;

    // The IPv4 address of the single VPN server for this region.
    core::Ipv4Address address;
    // The certificate common name for this server
    core::StringSlice commonName;
    // FQDN of the server - only used on some platforms for IKEv2, empty if not
    // needed
    core::StringSlice fqdn;

    // The service group(s) from the general regions list to apply to this
    // region - indicates the available protocols, ports, etc.  Represents the
    // deployment configuration applied by Ops.
    core::ArraySlice<const core::StringSlice> serviceGroups;
    // The region ID for the corresponding region.  Unlike DIPs, this is
    // optional for manual regions.
    core::StringSlice correspondingRegionId;

    // Additional overrides for specific testing

    // Force NCP use instead of legacy pia-signal-settings, if the service group
    // named uses pia-signal-settings.
    bool forceOpenVpnNcp;
    // Override the possible ports for OpenVPN TCP or UDP.  If either list is
    // empty, the ports are taken from the server group.
    core::ArraySlice<const std::uint16_t> openVpnUdpOverridePorts;
    core::ArraySlice<const std::uint16_t> openVpnTcpOverridePorts;
};

class KAPPS_REGIONS_EXPORT RegionList
{
private:
    // Service group map used when building regions
    using ServiceGroups = std::unordered_map<core::StringSlice, std::shared_ptr<ServiceGroup>>;
    // Region map used when building regions.  This _only_ includes standard
    // regions, because DIP/manual regions cannot reference another DIP/manual
    // region as the "corresponding region".
    //
    // There's no need to hold more refs on the regions here since they are
    // held by _regionsById when this is used.
    using StdRegionsById = std::unordered_map<core::StringSlice, const Region*>;
    // Map of Shadowsocks servers from region IDs, used to add them to the
    // regions
    using ShadowsocksServers = std::unordered_map<core::StringSlice, std::shared_ptr<const Server>>;

public:
    // RegionList can be created from the PIA regions list v6 format JSON as
    // well.  To do that, construct a RegionList with:
    //   `RegionList{RegionList::PIAv6, json, dips, manual}`
    static struct PIAv6_t {} PIAv6;

public:
    RegionList() = default; // Empty region list

    RegionList(core::StringSlice regionsJson,
               core::StringSlice shadowsocksJson,
               core::ArraySlice<const DedicatedIp> dips,
               core::ArraySlice<const ManualRegion> manual);

    // Construct from the legacy PIAv6 format; see RegionList::PIAv6 above
    RegionList(PIAv6_t, core::StringSlice regionsJson,
               core::StringSlice shadowsocksJson,
               core::ArraySlice<const DedicatedIp> dips,
               core::ArraySlice<const ManualRegion> manual);

    // Default copy and assign are fine - _regions and _regionsById in both
    // *this and other will refer to the same objects after the copy.
    RegionList(const RegionList &) = default;
    RegionList &operator=(const RegionList &) = default;

    // Move must be implemented manually - default moves leave the moved-from
    // object in an indeterminate state, so it's possible the moved-from
    // _regions could be empty while _regionsById might hold swapped contents,
    // etc.  Explicitly swapping both is fine.
    RegionList(RegionList &&other) : RegionList{} {*this = std::move(other);}
    RegionList &operator=(RegionList &&other)
    {
        _publicDnsServers = std::move(other._publicDnsServers);
        _regions = std::move(other._regions);
        _regionsById = std::move(other._regionsById);
        // To guarantee that other is in a valid state (not violating its
        // invariant that _regions corresponds to _regionsById); just clear both
        // containers.
        other._regions.clear();
        other._regionsById.clear();
        return *this;
    }

private:
    // Read service groups from the regions list JSON object
    auto readJsonServiceGroups(const nlohmann::json &json)
        -> ServiceGroups;
    // Read a regions from the regions list JSON object using the group map.
    // This reads into _regions.  nullptr entries are added to _regionsById to
    // identify duplicate region IDs.
    void readJsonRegions(const nlohmann::json &json, const ServiceGroups &groups,
        const ShadowsocksServers &shadowsocksServers);
    // Read a region's servers from the region JSON using the group map
    auto readJsonRegionServers(const nlohmann::json &json, core::StringSlice id,
                               const ServiceGroups &groups)
        -> std::vector<std::shared_ptr<const Server>>;

    // Build servers from the Shadowsocks server list for incorporation into
    // regions.  The Shadowsocks list only provides one server per region.
    auto readShadowsocksServers(const nlohmann::json &json) const
        -> ShadowsocksServers;

    // Add a Shadowsocks server to a region's servers if that region has a
    // Shadowsocks server
    void addShadowsocksServer(core::StringSlice id,
        std::vector<std::shared_ptr<const Server>> &servers,
        const ShadowsocksServers &shadowsocksServers) const;

    // Support for legacy PIAv6 format
    void readPiav6JsonRegions(const nlohmann::json &jsonRegions,
        const ServiceGroups &ncpGroups, const ServiceGroups &pssGroups,
        const ShadowsocksServers &shadowsocksServers);
    auto readPiav6JsonRegionServers(const nlohmann::json &jsonRegion,
                                    core::StringSlice id,
                                    const ServiceGroups &ncpGroups,
                                    const ServiceGroups &pssGroups)
        -> std::vector<std::shared_ptr<const Server>>;

    // Build dedicated IP regions from the information given to the constructor
    void buildDipRegions(const core::ArraySlice<const DedicatedIp> &dips,
                         const ServiceGroups &groups,
                         const StdRegionsById &stdRegions);
    // Build manual regions from the information given to the constructor
    void buildManualRegions(const core::ArraySlice<const ManualRegion> &manualRegions,
                            const ServiceGroups &groups,
                            const StdRegionsById &stdRegions);

public:
    // Get the public DNS servers indicated by the regions list, if provided.
    // These use anycast routing, so they can be used from any geographic
    // location - they're intended to be used when not connected to the VPN if
    // DNS is needed.
    //
    // Some brands may not provide this, in which case the list is empty.
    core::ArraySlice<const core::Ipv4Address> publicDnsServers() const {return _publicDnsServers;}

    // Find a region by ID; returns nullptr if not found.
    const Region *getRegion(core::StringSlice id) const;
    // Get all regions
    core::ArraySlice<const Region * const> regions() const {return _regions;}

private:
    std::vector<core::Ipv4Address> _publicDnsServers;

    // Regions are held with shared_ptr so that callers can continue to use them
    // even if the RegionList is destroyed.  This map is keyed by region IDs;
    // the keys refer to string data from the Region.
    std::unordered_map<core::StringSlice, std::shared_ptr<const Region>> _regionsById;
    // This vector of raw region points is held just to provide an ArraySlice
    // from regions().  The Region objects are owned by the shared_ptrs above.
    std::vector<const Region*> _regions;
};

}
