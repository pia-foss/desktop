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
#include "displaytext.h"
#include <kapps_core/src/corejson.h>
#include <kapps_core/src/retainshared.h>

namespace kapps::regions {

class KAPPS_REGIONS_EXPORT CountryDisplay
    : public core::JsonReadable<CountryDisplay>,
      public core::RetainSharedFromThis<CountryDisplay>
{
public:
    CountryDisplay() = default;
    CountryDisplay(std::string code, DisplayText name, DisplayText prefix)
        : _code{std::move(code)}, _name{std::move(name)},
            _prefix{std::move(prefix)}
    {}

    bool operator==(const CountryDisplay &other) const
    {
        return code() == other.code() && name() == other.name() &&
            prefix() == other.prefix();
    }

public:
    core::StringSlice code() const {return _code;}
    const DisplayText &name() const {return _name;}
    const DisplayText &prefix() const {return _prefix;}

    void readJson(const nlohmann::json &j);

private:
    std::string _code;
    DisplayText _name;
    DisplayText _prefix;
};

}
