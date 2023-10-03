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

#include "displaytext.h"
#include <kapps_core/src/logger.h>
#include <kapps_core/src/corejson.h>
#include <nlohmann/json.hpp>

namespace kapps::regions {

Bcp47Tag::Bcp47Tag(core::StringSlice tag)
    : Bcp47Tag{"en", {}, {}}    // Default until we parse the tag and then assign
{
    // Split the tag into one, two, or three parts delimited by hyphens
    core::StringSlice p1, p2, p3;

    p1 = tag;
    auto hyphen = p1.find('-');
    if(hyphen != core::StringSlice::npos)
    {
        // Split on the hyphen
        p2 = p1.substr(hyphen+1);
        p1 = p1.substr(0, hyphen);

        hyphen = p2.find('-');
        if(hyphen != core::StringSlice::npos)
        {
            p3 = p2.substr(hyphen+1);
            p2 = p2.substr(0, hyphen);

            hyphen = p3.find('-');
            if(hyphen != core::StringSlice::npos)
            {
                // 4 or more parts; don't understand this tag
                KAPPS_CORE_ERROR() << "Invalid language tag:" << tag;
                throw std::runtime_error{"Invalid language tag in Bcp47Tag"};
            }
        }
    }

    // All we need to do at this point is shift p2 to p3 if it's actually a
    // region code with an omitted script.  The construction below will validate
    // everything else.
    if(p2.size() == 2 && p3.empty())    // Region with omitted script
    {
        p3 = p2;
        p2 = {};
    }

    try
    {
        *this = Bcp47Tag{p1, p2, p3};
    }
    catch(const std::exception &ex)
    {
        // The constructor doesn't have the whole language tag to trace
        KAPPS_CORE_ERROR() << "Invalid language tag:" << tag << "-" << ex.what();
        throw;
    }
}

Bcp47Tag::Bcp47Tag(core::StringSlice language, core::StringSlice script,
                   core::StringSlice region)
    : _language{language.to_string()}, _script{script.to_string()},
      _region{region.to_string()}
{
    // std::isalpha/toupper/tolower are influenced by locale; not desired here
    auto isAlpha = [](char c){return ('c'>='a' && 'c'<='z') || ('c'>='A' && 'c'<='Z');};
    auto allAlpha = [&](const auto &s){return std::all_of(s.begin(), s.end(), isAlpha);};
    auto toLower = [](char c) -> char
    {
        if(c >= 'A' && c <= 'Z')
            return c - 'A' + 'a';
        return c;
    };
    auto toUpper = [](char c) -> char
    {
        if(c >= 'a' && c <= 'z')
            return c - 'a' + 'A';
        return c;
    };

    // Validate each part
    if(_language.size() != 2 || !allAlpha(_language))
    {
        KAPPS_CORE_ERROR() << "Invalid language code" << _language << "in language tag";
        throw std::runtime_error{"Invalid language code"};
    }
    if(!_script.empty() && (_script.size() != 4 || !allAlpha(_script)))
    {
        KAPPS_CORE_ERROR() << "Invalid script" << _script << "in language tag";
        throw std::runtime_error{"Invalid script"};
    }
    if(!_region.empty() && (_region.size() != 2 || !allAlpha(_region)))
    {
        KAPPS_CORE_ERROR() << "Invalid region" << _region << "in language tag";
        throw std::runtime_error{"Invalid region"};
    }

    // Normalize case
    _language[0] = toLower(_language[0]);
    _language[1] = toLower(_language[1]);
    if(!_script.empty())
    {
        _script[0] = toUpper(_script[0]);
        _script[1] = toLower(_script[1]);
        _script[2] = toLower(_script[2]);
        _script[3] = toLower(_script[3]);
    }
    if(!_region.empty())
    {
        _region[0] = toUpper(_region[0]);
        _region[1] = toUpper(_region[1]);
    }
}

std::size_t Bcp47Tag::hash() const
{
    return kapps::core::hashFields(_language, _script, _region);
}

core::StringSlice DisplayText::getLanguageText(const Bcp47Tag &lang) const
{
    auto itMatch = _texts.find(lang);
    if(itMatch != _texts.end())
        return itMatch->second;
    itMatch = _texts.find({lang.language(), lang.script(), {}});
    if(itMatch != _texts.end())
        return itMatch->second;
    itMatch = _texts.find({lang.language(), {}, lang.region()});
    if(itMatch != _texts.end())
        return itMatch->second;
    itMatch = _texts.find({lang.language(), {}, {}});
    if(itMatch != _texts.end())
        return itMatch->second;
    itMatch = _texts.find({"en", {}, "US"});
    if(itMatch != _texts.end())
        return itMatch->second;
    return {};
}

void DisplayText::readJson(const nlohmann::json &j)
{
    _texts.clear();
    _texts.reserve(j.size());
    for(const auto &[key, val] : j.items())
    {
        // If an item somehow has an invalid Bcp47Tag, it's ignored - this
        // allows us to add disambiguations not yet supported by Bcp47Tag in the
        // future if needed.
        try
        {
            Bcp47Tag lang{key};
            _texts.emplace(std::move(lang), val.get<std::string>());
        }
        catch(const std::exception &ex)
        {
            KAPPS_CORE_WARNING() << "Ignoring translation" << key << "-" << val
                << "due to error:" << ex.what();
        }
    }
}

}
