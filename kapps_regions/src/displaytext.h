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
#include <kapps_regions/regions.h>
#include <kapps_core/src/stringslice.h>
#include <kapps_core/src/corejson.h>
#include <unordered_map>

namespace kapps::regions {

// Limited subset of a BCP-47 language tag - supports the language, script, and
// region parts only.
class KAPPS_REGIONS_EXPORT Bcp47Tag : public core::OStreamInsertable<Bcp47Tag>
{
public:
    // Construct a Bcp47Tag from a string tag.  If the string is not a valid
    // BCP-47 tag, or has any components not supported by Bcp47Tag, this throws.
    Bcp47Tag(core::StringSlice tag);

    // Construct a Bcp47Tag from individual parts.
    // - language must be two or three alpha chars; it is lower-cased
    // - script must be empty or four alpha chars; it is title-cased
    // - region must be empty or two alpha chars (digits are not supported); it
    //   is upper-cased
    //
    // If any of the parts are invalid, this throws.
    Bcp47Tag(core::StringSlice language, core::StringSlice script,
             core::StringSlice region);

    bool operator==(const Bcp47Tag &other) const
    {
        return _language == other._language && _script == other._script &&
            _region == other._region;
    }

public:
    std::size_t hash() const;

    void trace(std::ostream &os) const
    {
        os << language();
        if(!script().empty())
            os << "-" << script();
        if(!region().empty())
            os << "-" << region();
    }

    const std::string &language() const {return _language;}
    const std::string &script() const {return _script;}
    const std::string &region() const {return _region;}

    std::string toString() const
    {
        std::string result{_language};
        if(!script().empty())
        {
            result += '-';
            result += script();
        }
        if(!region().empty())
        {
            result += '-';
            result += region();
        }
        return result;
    }

private:
    // Storing each part on the heap would be pretty inefficient given how many
    // Bcp47Tags will be constructed by the region metadata, but since all the
    // parts are small (<= 4 chars), we rely on std::string supporting the
    // small-string optimization rather than doing anything fancy with manual
    // buffers.
    std::string _language, _script, _region;
};

}

KAPPS_CORE_STD_HASH(kapps::regions::Bcp47Tag);

namespace kapps::regions {

// DisplayText just holds a set of translations identified by language tags.
class KAPPS_REGIONS_EXPORT DisplayText : public core::JsonReadable<DisplayText>
{
public:
    DisplayText() = default;
    DisplayText(std::unordered_map<Bcp47Tag, std::string> texts)
        : _texts{std::move(texts)}
    {}

    bool operator==(const DisplayText &other) const
    {
        return texts() == other.texts();
    }

public:
    // Get the best display text for lang - this implements the following
    // fallback order:
    // - Exact match
    // - <lang>-<Script>
    // - <lang>-<RGN>
    // - <lang>
    // - en-US
    core::StringSlice getLanguageText(const Bcp47Tag &lang) const;

    // All the texts can be enumerated this way, but this is rarely useful
    // outside of diagnostics.
    const std::unordered_map<Bcp47Tag, std::string> &texts() const {return _texts;}

    void readJson(const nlohmann::json &j);

private:
    std::unordered_map<Bcp47Tag, std::string> _texts;
};

}
