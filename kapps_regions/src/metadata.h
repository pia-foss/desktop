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
#include "dynamicrole.h"
#include "countrydisplay.h"
#include "regiondisplay.h"
#include "regionlist.h"
#include <unordered_set>

namespace kapps::regions {

class KAPPS_REGIONS_EXPORT Metadata
{
public:
    Metadata() = default;
    // Create Metadata from the metadata JSON.  Remove any attached signature if
    // it was present.
    //
    // Dedicated IP and manual region information can also be attached if you
    // want Metadata to generate region displays for those regions.  These can
    // be omitted if you don't need them.
    //
    // - For Dedicated IPs, Metadata duplicates the corresponding region's
    //   RegionDisplay with the DIP region ID, so the client does not have to
    //   manually fall back to the corresponding region.  The presentation
    //   should still include the DIP address, etc.
    // - For manual regions, Metadata fabricates a dummy CountryDisplay and
    //   RegionDisplay.  There are no translations, but there is en-US display
    //   text filled in (for example, the region name becomes "<cn> - <ip>")
    Metadata(core::StringSlice metadataJson,
             core::ArraySlice<const DedicatedIp> dips,
             core::ArraySlice<const ManualRegion> manual);

    // Like RegionList, the legacy PIA metadata v2 format can be loaded also.
    // This requires the regions v6 data too - some of the data were moved from
    // the regions list to metadata in v7, so the legacy support reads the
    // corresponding data from regions v6.
    Metadata(core::StringSlice regionsv6Json, core::StringSlice metadatav2Json,
             core::ArraySlice<const DedicatedIp> dips,
             core::ArraySlice<const ManualRegion> manual);

    // Like RegionList, we need to implement moves to ensure the
    // cross-referenced containers remain valid
    Metadata(Metadata &&other) : Metadata{} {*this = std::move(other);}
    Metadata &operator=(Metadata &&other)
    {
        _dynamicGroupsById = std::move(other._dynamicGroupsById);
        _dynamicGroups = std::move(other._dynamicGroups);
        _countryDisplaysById = std::move(other._countryDisplaysById);
        _countryDisplays = std::move(other._countryDisplays);
        _regionDisplaysById = std::move(other._regionDisplaysById);
        _regionDisplays = std::move(other._regionDisplays);
        other._dynamicGroupsById.clear();
        other._dynamicGroups.clear();
        other._countryDisplaysById.clear();
        other._countryDisplays.clear();
        other._regionDisplaysById.clear();
        other._regionDisplays.clear();
        return *this;
    }

    bool operator==(const Metadata &other) const
    {
        // Test if two maps of pointers hold equivalent values (operator==()
        // would compare the pointers themselves).
        auto ptrMapEqual = [](const auto &self, const auto &other) -> bool
        {
            if(self.size() != other.size())
                return false;

            auto entryPtrValuesEqual = [](const auto &selfEntry, const auto &otherEntry)
                -> bool
            {
                // If one is nullptr, they're equal if they're both nullptr
                if(!selfEntry.second || !otherEntry.second)
                    return !selfEntry.second && !otherEntry.second;
                return *selfEntry.second == *otherEntry.second;
            };

            // For each key range in self, make sure the corresponding range in
            // other matches.  Unordered maps may not iterate in the same order,
            // so we have to pull each range from self and then find it in
            // other.
            auto itSelfPos = self.begin();
            while(itSelfPos != self.end())
            {
                auto selfRange = self.equal_range(itSelfPos->first);
                auto otherRange = other.equal_range(itSelfPos->first);
                if(!std::equal(selfRange.first, selfRange.second,
                    otherRange.first, otherRange.second, entryPtrValuesEqual))
                {
                    return false;   // Ranges were not equal
                }

                // Advance to next range, or to end()
                itSelfPos = selfRange.second;
            }

            // All keys in self had matching items in other.  Since the sizes
            // of self and other are also equal, we know other does not have any
            // extra keys that were not present in self.
            return true;
        };

        // Check that all maps contain the same elements for the same keys.
        // There's no need to check the raw pointer vectors, since these are
        // built from the maps for the API.
        return ptrMapEqual(_dynamicGroupsById, other._dynamicGroupsById) &&
            ptrMapEqual(_countryDisplaysById, other._countryDisplaysById) &&
            ptrMapEqual(_regionDisplaysById, other._regionDisplaysById);
    }

private:
    // Default copy/assign would break references as in RegionList
    Metadata(const Metadata &) = delete;
    Metadata &operator=(const Metadata &) = delete;

private:
    // Build a DisplayText from the translations for a display text in legacy
    // metadata v2.  The name given is not used in the resulting DisplayText,
    // since v2 provides an en_US "translation", which sometimes varies from
    // the name in the regions list.
    //
    // The map can be built with core::StringSlice values (referencing the
    // JSON data, for country prefix detection), or owning std::string values
    // (for building the actual DisplayText).
    template<class StringT>
    auto buildPiav2DisplayText(const nlohmann::json &metadata,
        core::StringSlice name)
        -> std::unordered_map<Bcp47Tag, StringT>;

    // Find region coordinates from metadata v2.  Returns (NaN, NaN) if they
    // cannot be found, parsed, etc.
    std::pair<double, double> findPiav2RegionCoords(const nlohmann::json &metadata,
        core::StringSlice regionId);

    // Build a country display from legacy v6+v2 data for a country with a
    // single region.
    void buildPiav2SingleCountryDisplay(const nlohmann::json &metadata,
        core::StringSlice countryCode, core::StringSlice countryName);
    // Build a region display from legacy v6+v2 data for a region in a country
    // with a single region.
    void buildPiav2SingleRegionDisplay(const nlohmann::json &metadata,
        core::StringSlice regionId, core::StringSlice countryCode,
        core::StringSlice regionName);

    // Build the "prefix length map" used to determine country display prefixes
    // for countries with more than one region.  This builds the initial map
    // from the first region.  The map keys are language tags; the values
    // are common name prefixes.
    auto buildPiav2PrefixMap(const nlohmann::json &metadata,
        core::StringSlice firstRegionName)
        -> std::unordered_map<Bcp47Tag, core::StringSlice>;
    // Update the "prefix length map" for an additional region in the same
    // country.  Call this for each region beyond the first two.
    void updatePiav2PrefixMap(const nlohmann::json &metadata,
        std::unordered_map<Bcp47Tag, core::StringSlice> &prefixMap,
        core::StringSlice regionName);
    // Finish processing the prefix length map after examining all regions
    void finishPiav2PrefixMap(std::unordered_map<Bcp47Tag, core::StringSlice> &prefixMap);

    // Look up the name for a country group and load its translations.
    DisplayText buildPiav2MultipleCountryName(const nlohmann::json &metadata,
        core::StringSlice countryCode);
    // Build a country display from legacy v6+v2 data for a country with more
    // than one region.
    // The country name is found from the country_groups data in metadata; the
    // country prefix is found from the prefix map.
    void buildPiav2MultipleCountryDisplay(const nlohmann::json &metadata,
        core::StringSlice countryCode,
        const std::unordered_map<Bcp47Tag, core::StringSlice> &prefixMap);
    // Build a region display from legacy v6+v2 data for a region in a country
    // with multiple regions.
    // The country prefixes are removed from the region display names.
    void buildPiav2MultipleRegionDisplay(const nlohmann::json &metadata,
        core::StringSlice regionId, core::StringSlice countryCode,
        core::StringSlice regionName,
        const std::unordered_map<Bcp47Tag, core::StringSlice> &prefixMap);

    // Copy the region displays for DIP regions' corresponding regions, so the
    // client doesn't have to fall back manually
    void copyDipRegionDisplays(core::ArraySlice<const DedicatedIp> dips);

    // Build a region and country display for manual regions
    void buildManualRegionDisplays(core::ArraySlice<const ManualRegion> manualRegions);

public:
    const DynamicRole *getDynamicRole(core::StringSlice id) const;
    core::ArraySlice<const DynamicRole * const> dynamicGroups() const {return _dynamicGroups;}
    const CountryDisplay *getCountryDisplay(core::StringSlice id) const;
    core::ArraySlice<const CountryDisplay * const> countryDisplays() const {return _countryDisplays;}
    const RegionDisplay *getRegionDisplay(core::StringSlice id) const;
    core::ArraySlice<const RegionDisplay * const> regionDisplays() const {return _regionDisplays;}

private:
    // Like RegionList, the maps own the elements in shared_ptrs, and vectors of
    // raw pointers are held just to provide to the API
    std::unordered_map<core::StringSlice, std::shared_ptr<const DynamicRole>> _dynamicGroupsById;
    std::vector<const DynamicRole*> _dynamicGroups;
    std::unordered_map<core::StringSlice, std::shared_ptr<const CountryDisplay>> _countryDisplaysById;
    std::vector<const CountryDisplay*> _countryDisplays;
    std::unordered_map<core::StringSlice, std::shared_ptr<const RegionDisplay>> _regionDisplaysById;
    std::vector<const RegionDisplay*> _regionDisplays;
};

}
