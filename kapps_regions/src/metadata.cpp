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

#include "metadata.h"
#include <kapps_core/src/corejson.h>
#include <nlohmann/json.hpp>

namespace kapps::regions {

namespace
{
    template<class T, class GetKeyT, class JsonT>
    void readSharedElements(const JsonT &j, GetKeyT getKey,
        std::unordered_map<core::StringSlice, std::shared_ptr<const T>> &elementsById)
    {
        elementsById.clear();

        elementsById.reserve(j.size());
        core::readJsonArrayTolerant<T>(j,[&](T value)
            {
                if(elementsById.count(getKey(value)))
                {
                    KAPPS_CORE_WARNING() << "Duplicate" << core::typeName<T>()
                        << "ID:" << getKey(value);
                }
                else
                {
                    auto pValue = std::make_shared<T>(std::move(value));
                    elementsById.emplace(getKey(*pValue), std::move(pValue));
                }
            });
    }

    template<class T>
    void buildFlatVector(const std::unordered_map<core::StringSlice, std::shared_ptr<const T>> &elementsById,
        std::vector<const T*> &elements)
    {
        elements.clear();
        elements.reserve(elementsById.size());
        for(const auto &[id, group] : elementsById)
            elements.push_back(group.get());
    }
}

Metadata::Metadata(core::StringSlice metadataJson,
                   core::ArraySlice<const DedicatedIp> dips,
                   core::ArraySlice<const ManualRegion> manual)
{
    auto json = nlohmann::json::parse(metadataJson);

    // dynamic_groups is optional
    auto itDynGroups = json.find("dynamic_roles");
    if(itDynGroups != json.end())
    {
        readSharedElements<DynamicRole>(*itDynGroups,
            [](const DynamicRole &value){return value.id();},
            _dynamicGroupsById);
    }

    readSharedElements<CountryDisplay>(json.at("countries"),
        [](const CountryDisplay &value){return value.code();},
        _countryDisplaysById);
    readSharedElements<RegionDisplay>(json.at("regions"),
        [](const RegionDisplay &value){return value.id();},
        _regionDisplaysById);

    copyDipRegionDisplays(dips);
    buildManualRegionDisplays(manual);

    buildFlatVector(_dynamicGroupsById, _dynamicGroups);
    buildFlatVector(_countryDisplaysById, _countryDisplays);
    buildFlatVector(_regionDisplaysById, _regionDisplays);
}

Metadata::Metadata(core::StringSlice regionsv6Json, core::StringSlice metadatav2Json,
                   core::ArraySlice<const DedicatedIp> dips,
                   core::ArraySlice<const ManualRegion> manual)
{
    auto regions = nlohmann::json::parse(regionsv6Json);
    auto metadata = nlohmann::json::parse(metadatav2Json);

    // The legacy metadata v2 format provides relatively primitive data - there
    // are no "region" or "country" objects, just a collection of translations
    // for display strings (of any kind) and some extra country code / GPS data.
    //
    // Read the regions from regions v6 first to start building region/country
    // displays.  Group by country so we can undo the "country"/"ct - city"
    // logic that is applied to display names in metadata v2.
    struct RegionIdName
    {
        core::StringSlice id;
        core::StringSlice name;
    };
    std::unordered_multimap<core::StringSlice, RegionIdName> regionsByCountry;
    for(const auto &region : core::jsonArray(regions.at("regions")))
    {
        try
        {
            RegionIdName idName{};
            idName.id = region.at("id").get<core::StringSlice>();
            idName.name = region.at("name").get<core::StringSlice>();
            regionsByCountry.emplace(region.at("country").get<core::StringSlice>(),
                                     idName);
        }
        catch(const std::exception &ex)
        {
            KAPPS_CORE_WARNING() << "Unable to read region metadata from region"
                << region;
            // Ignore this region
        }
    }

    auto itCountryFirst = regionsByCountry.begin();
    while(itCountryFirst != regionsByCountry.end())
    {
        auto itCountryEnd = regionsByCountry.equal_range(itCountryFirst->first).second;

        // Impossible since itCountryFirst->first is in this container, ensures
        // we can increment itCountryFirst
        assert(itCountryFirst != itCountryEnd);

        auto itCountrySecond = itCountryFirst;
        ++itCountrySecond;
        if(itCountrySecond == itCountryEnd)
        {
            // There is one region in this country.  The region's name is the
            // country name.  We'll also use it as the city name even though
            // this isn't quite right, because we don't have any alternative,
            // and because the city name shouldn't be used by the UI in this
            // case for products using the legacy format.  The country prefix
            // is empty.
            buildPiav2SingleCountryDisplay(metadata,
                itCountryFirst->first.to_string(),
                itCountryFirst->second.name);
            buildPiav2SingleRegionDisplay(metadata,
                itCountryFirst->second.id.to_string(),
                itCountryFirst->first.to_string(),
                itCountryFirst->second.name.to_string());
        }
        else
        {
            // There is more than one region in this country, so the name is
            // "<ct> <city>", like "US Chicago" or "DE Frankfurt".  Find the
            // length of the common country prefix so we can separate it (for
            // all languages).
            auto prefixMap = buildPiav2PrefixMap(metadata,
                itCountryFirst->second.name);
            auto itPrefixSearchRegion = itCountrySecond;
            while(itPrefixSearchRegion != itCountryEnd)
            {
                updatePiav2PrefixMap(metadata, prefixMap,
                    itPrefixSearchRegion->second.name);
                ++itPrefixSearchRegion;
            }
            finishPiav2PrefixMap(prefixMap);

            // We now know the country prefix lengths for all languages, so we
            // can build all displays.
            buildPiav2MultipleCountryDisplay(metadata,
                itCountryFirst->first.to_string(), prefixMap);
            auto itAddRegion = itCountryFirst;
            while(itAddRegion != itCountryEnd)
            {
                buildPiav2MultipleRegionDisplay(metadata,
                    itAddRegion->second.id,
                    itCountryFirst->first,
                    itAddRegion->second.name,
                    prefixMap);

                ++itAddRegion;
            }
        }

        // Advance to the next country range (or end() if this was the last)
        itCountryFirst = itCountryEnd;
    }

    copyDipRegionDisplays(dips);
    buildManualRegionDisplays(manual);

    buildFlatVector(_countryDisplaysById, _countryDisplays);
    buildFlatVector(_regionDisplaysById, _regionDisplays);
}

template<class StringT>
auto Metadata::buildPiav2DisplayText(const nlohmann::json &metadata,
    core::StringSlice name)
    -> std::unordered_map<Bcp47Tag, StringT>
{
    std::unordered_map<Bcp47Tag, StringT> result;
    // If no en-US translation is found (or no translations can be loaded at
    // all), use 'name' from the regions list as a default
    result.insert({Bcp47Tag{"en-US"}, StringT{name.data(), name.size()}});
    try
    {
        const auto &translations = metadata.at("translations").at(name.to_string());
        for(const auto &[lang, text] : core::jsonObject(translations).items())
        {
            // Individual language text failures don't fail the entire display
            // text
            try
            {
                Bcp47Tag langTag{lang};
                StringT textValue{text.get<StringT>()};
                // Be sure not to insert empty values if this should happen
                // somehow, this would impede the language fallback
                if(!textValue.empty())
                    result.insert({langTag, std::move(textValue)});
            }
            catch(const std::exception &ex)
            {
                KAPPS_CORE_WARNING() << "Ignoring translation" << lang
                    << "of" << name << "-" << ex.what();
            }
        }
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Can't build display text for name" << name
            << "-" << ex.what();
    }
    return result;
}

std::pair<double, double> Metadata::findPiav2RegionCoords(const nlohmann::json &metadata,
    core::StringSlice regionId)
{
    try
    {
        const auto &gpsArray = metadata.at("gps").at(regionId.to_string());
        if(core::jsonArray(gpsArray).size() != 2)
            throw std::runtime_error{"GPS coordinates must be array of 2 elements"};

        // The coordinates in metadata v2 are stringified numbers for whatever
        // reason
        std::string value;
        char *pNumEnd{};

        value = gpsArray[0].get<std::string>();
        double lat = std::strtod(value.c_str(), &pNumEnd);
        if(pNumEnd != value.c_str() + value.size())
            throw std::runtime_error{"latitude is not a valid number"};

        value = gpsArray[1].get<std::string>();
        double lon = std::strtod(value.c_str(), &pNumEnd);
        if(pNumEnd != value.c_str() + value.size())
            throw std::runtime_error{"longitude is not a valid number"};

        return {lat, lon};
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Could not find geo coords for region"
            << regionId << "-" << ex.what();
    }
    return {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()};
}

void Metadata::buildPiav2SingleCountryDisplay(const nlohmann::json &metadata,
    core::StringSlice countryCode, core::StringSlice countryName)
{
    try
    {
        // There are no prefixes known for a country containing one region, it's
        // simply not present in metadata v2.
        auto pCountryDisplay = std::make_shared<CountryDisplay>(countryCode.to_string(),
            DisplayText{buildPiav2DisplayText<std::string>(metadata, countryName)},
            DisplayText{});
        _countryDisplaysById.emplace(pCountryDisplay->code(), std::move(pCountryDisplay));
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Ignoring country" << countryCode << "-"
            << ex.what();
    }
}

void Metadata::buildPiav2SingleRegionDisplay(const nlohmann::json &metadata,
        core::StringSlice regionId, core::StringSlice countryCode,
        core::StringSlice regionName)
{
    try
    {
        auto coords = findPiav2RegionCoords(metadata, regionId);
        auto pRegionDisplay = std::make_shared<RegionDisplay>(regionId.to_string(),
            countryCode.to_string(), coords.first, coords.second,
            DisplayText{buildPiav2DisplayText<std::string>(metadata, regionName)});
        _regionDisplaysById.emplace(pRegionDisplay->id(), std::move(pRegionDisplay));
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Ignoring region" << regionId << "-"
            << ex.what();
    }
}

auto Metadata::buildPiav2PrefixMap(const nlohmann::json &metadata,
    core::StringSlice firstRegionName)
    -> std::unordered_map<Bcp47Tag, core::StringSlice>
{
    return buildPiav2DisplayText<core::StringSlice>(metadata, firstRegionName);
}

void Metadata::updatePiav2PrefixMap(const nlohmann::json &metadata,
    std::unordered_map<Bcp47Tag, core::StringSlice> &prefixMap,
    core::StringSlice regionName)
{
    const auto &regionNames = buildPiav2DisplayText<core::StringSlice>(metadata,
        regionName);
    // Find the common prefix between the existing region names/prefixes and
    // the next one.
    //
    // This isn't perfect, but it works for "good" data (complete translations
    // with matching country prefixes) and tolerates major errors:
    // - names with only an en-US value (missing all translations) are handled
    //   well
    // - translations with mismatched prefixes ("EE.UU."/"EEUU", for example)
    //   are handled well
    //
    // The model breaks down in some cases, but this should not affect the
    // current data provided in metadata v2, possibly with fixes if needed.
    // The new v3 format addresses these issues:
    // - If only one region in a multi-country region actually has a
    //   translation, the whole name becomes a "prefix", which is no good.  The
    //   solution is to add the missing translations.
    // - It's possible for the prefix to be mis-identified if all regions start
    //   with the same letter, etc. - say if "US Chicago" and "US California"
    //   were the only US regions, the detected prefix would be "US C".  This
    //   can be worked around by inserting a zero-width space after "US " in one
    //   of the regions to break the prefix, and this is backward compatible.
    //
    // Iterate through the region's text so:
    // - we can add languages missing from the prefix map
    // - we don't generate blank prefixes if the region is missing a translation
    //   (we want the common prefix from whatever translations are provided)
    for(const auto &[lang, regionText] : regionNames)
    {
        // Ignore empty region texts, usually this would just be omitted, but
        // ignore it if it would happen.
        if(regionText.empty())
            continue;

        auto itPrefixEntry = prefixMap.find(lang);
        // If this language isn't present in the region names yet (a region in
        // this country lacked this language), we need to add the new prefix.
        if(itPrefixEntry == prefixMap.end())
            prefixMap.insert({lang, regionText});
        else
            itPrefixEntry->second = itPrefixEntry->second.common_prefix(regionText);
    }
}

void Metadata::finishPiav2PrefixMap(std::unordered_map<Bcp47Tag, core::StringSlice> &prefixMap)
{
    // Check a position N bytes from the end of a UTF-8 string slice to see if
    // it was a truncated codepoint.  correctBits/correctMask indicate the
    // correct lead byte for a code point of exactly N bytes.
    //
    // Returns 'false' for a continuation byte - keep searching the next bytes.
    // Returns 'true' to stop searching - either we found a complete code point
    // (no truncation needed), or we found a partial code point and removed it.
    auto checkTruncatePos = [](core::StringSlice &text, std::size_t fromEnd,
                        char correctBits, char correctMask)
        -> bool
    {
        // If the text doesn't even have this many characters, either it is
        // empty or consists only of continuation bytes.  There's nothing
        // reasonable we can do with only continuation bytes, the input was
        // corrupt.
        if(text.size() < fromEnd)
            return true;    // Done searching, nothing we can do

        // If it's a continuation byte, keep looking up the string
        char c{text[text.size()-fromEnd]};
        if((c & 0b1100'0000) == 0b1000'0000)
            return false;   // Keep searching

        // It's not a continuation byte - we are done searching at this
        // byte, just determine whether to truncate.
        if((c & correctMask) != correctBits)
        {
            // Not the correct length for this position.  Assume it is too
            // long (truncated sequence) and remove the partial sequence.
            // (If it was actually too short, this means the input was
            // invalid UTF-8 - it had extra continuation bytes.  We don't
            // bother handling this since truncating one extra code point is
            // a reasonable degradation for invalid UTF-8.)
            text = text.substr(0, text.size()-fromEnd);
        }
        return true;
    };
    // Prefixes can never end on a partial code point, which is possible at this
    // point of the region names all began with a common code unit sequence
    // (but not a complete code point).  Back up to the end of the prior code
    // point if needed.
    for(auto &[lang, text] : prefixMap)
    {
        // Check 1-4 bytes from the end.  If checkPos(4, ...) still returns
        // false, the text ended with 4 continuation bytes, which is not valid;
        // just leave it alone.
        checkTruncatePos(text, 1, 0b0000'0000, 0b1000'0000) ||
            checkTruncatePos(text, 2, 0b1100'0000, 0b1110'0000) ||
            checkTruncatePos(text, 3, 0b1110'0000, 0b1111'0000) ||
            checkTruncatePos(text, 4, 0b1111'0000, 0b1111'1000);
    }
}


DisplayText Metadata::buildPiav2MultipleCountryName(const nlohmann::json &metadata,
    core::StringSlice countryCode)
{
    // For a country group, look in country_groups for the country's en-US
    // name, then look up the translations for that.  If there aren't any,
    // use the country code as a default.
    try
    {
        // For whatever reason, country codes are lowercase here; they are uppercase
        // everywhere else
        std::string codeLower{countryCode.to_string()};
        for(auto &c : codeLower)
        {
            if(c >= 'A' && c <= 'Z')
                c = c - 'A' + 'a';
        }

        const auto &countryName = metadata.at("country_groups").at(codeLower).get<core::StringSlice>();
        return {buildPiav2DisplayText<std::string>(metadata, countryName)};
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Couldn't find name for country" << countryCode;
    }
    // Use the country code followed by a space
    return {{{Bcp47Tag{"en-US"}, countryCode.to_string() + ' '}}};
}

void Metadata::buildPiav2MultipleCountryDisplay(const nlohmann::json &metadata,
    core::StringSlice countryCode,
    const std::unordered_map<Bcp47Tag, core::StringSlice> &prefixMap)
{
    try
    {
        // The prefix map contains the prefixes (it's copied to owning
        // std::strings here).  Look up the country name and its translations.
        std::unordered_map<Bcp47Tag, std::string> ownedPrefixes;
        for(const auto &[lang, text] : prefixMap)
            ownedPrefixes.insert({lang, text.to_string()});
        auto pCountryDisplay = std::make_shared<CountryDisplay>(countryCode.to_string(),
            buildPiav2MultipleCountryName(metadata, countryCode),
            DisplayText{std::move(ownedPrefixes)});
        _countryDisplaysById.emplace(pCountryDisplay->code(), std::move(pCountryDisplay));
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Ignoring country" << countryCode << "-"
            << ex.what();
    }
}

void Metadata::buildPiav2MultipleRegionDisplay(const nlohmann::json &metadata,
    core::StringSlice regionId, core::StringSlice countryCode,
    core::StringSlice regionName,
    const std::unordered_map<Bcp47Tag, core::StringSlice> &prefixMap)
{
    try
    {
        // For a region in a country with multiple regions, we need to strip off
        // the common country prefix.
        const auto &display = buildPiav2DisplayText<core::StringSlice>(metadata, regionName);
        std::unordered_map<Bcp47Tag, std::string> regionNames;
        for(auto &[lang, text] : display)
        {
            auto itPrefix = prefixMap.find(lang);
            std::size_t prefixLen = (itPrefix == prefixMap.end()) ? 0 : itPrefix->second.size();
            regionNames.insert({lang, text.substr(prefixLen).to_string()});
        }
        auto coords = findPiav2RegionCoords(metadata, regionId);
        auto pRegionDisplay = std::make_shared<RegionDisplay>(regionId.to_string(),
            countryCode.to_string(), coords.first, coords.second,
            DisplayText{std::move(regionNames)});
        _regionDisplaysById.emplace(pRegionDisplay->id(), std::move(pRegionDisplay));
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Ignoring region" << regionId << "-"
            << ex.what();
    }
}

void Metadata::copyDipRegionDisplays(core::ArraySlice<const DedicatedIp> dips)
{
    for(const auto &dip : dips)
    {
        auto pCorrespondingRegion = getRegionDisplay(dip.correspondingRegionId);
        if(!pCorrespondingRegion)
        {
            KAPPS_CORE_WARNING() << "Can't find corresponding region"
                << dip.correspondingRegionId << "for DIP region"
                << dip.dipRegionId;
        }
        else
        {
            auto pDipRegionDisplay = std::make_shared<RegionDisplay>(
                dip.dipRegionId.to_string(),
                pCorrespondingRegion->country().to_string(),
                pCorrespondingRegion->geoLatitude(),
                pCorrespondingRegion->geoLongitude(),
                pCorrespondingRegion->name());
            _regionDisplaysById.emplace(pDipRegionDisplay->id(),
                std::move(pDipRegionDisplay));
        }
    }
}

// Build a region and country display for manual regions
void Metadata::buildManualRegionDisplays(core::ArraySlice<const ManualRegion> manualRegions)
{
    // Don't build the dummy country if there are no manual regions
    if(manualRegions.empty())
        return;

    auto buildRegionName = [](const ManualRegion &manual) -> std::string
    {
        return manual.commonName.to_string() + ' ' + manual.address.toString();
    };

    std::string manualCountryName;
    // If there is exactly one manual region, give the "manual country" the
    // single manual region's name.  The UI might not display the "city" in this
    // case, and it's really helpful for this dev tool to see the exact IP/CN.
    if(manualRegions.size() == 1)
        manualCountryName = buildRegionName(manualRegions.front());
    else
    {
        // The region names will probably be shown if there is more than one
        // region in this "country"
        manualCountryName = "Manual";
    }

    // Build dummy country "ZZ" to represent manual regions.  "ZZ" is a reserved
    // code for user assignment.
    auto pManualCountry = std::make_shared<CountryDisplay>("ZZ",
            DisplayText{{{Bcp47Tag{"en-US"}, std::move(manualCountryName)}}},
            DisplayText{{{Bcp47Tag{"en-US"}, "ZZ "}}});
    _countryDisplaysById.emplace(pManualCountry->code(), std::move(pManualCountry));

    for(const auto &manual : manualRegions)
    {
        auto pManualRegion = std::make_shared<RegionDisplay>(
            manual.manualRegionId.to_string(),
            "ZZ", std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(),
            DisplayText{{{Bcp47Tag{"en-US"}, buildRegionName(manual)}}});
        _regionDisplaysById.emplace(pManualRegion->id(),
            std::move(pManualRegion));
    }
}

const DynamicRole *Metadata::getDynamicRole(core::StringSlice id) const
{
    auto it = _dynamicGroupsById.find(id);
    if(it != _dynamicGroupsById.end())
        return it->second.get();
    return {};
}

const CountryDisplay *Metadata::getCountryDisplay(core::StringSlice id) const
{
    auto it = _countryDisplaysById.find(id);
    if(it != _countryDisplaysById.end())
        return it->second.get();
    return {};
}

const RegionDisplay *Metadata::getRegionDisplay(core::StringSlice id) const
{
    auto it = _regionDisplaysById.find(id);
    if(it != _regionDisplaysById.end())
        return it->second.get();
    return {};
}

}
