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
#include "displaytext.h"
#include <kapps_core/src/corejson.h>
#include <kapps_core/src/retainshared.h>

namespace kapps::regions {

class KAPPS_REGIONS_EXPORT RegionDisplay
    : public core::JsonReadable<RegionDisplay>,
      public core::RetainSharedFromThis<RegionDisplay>
{
public:
    RegionDisplay() = default;
    RegionDisplay(std::string id, std::string country, double geoLatitude,
                  double geoLongitude, DisplayText name)
        : _id{std::move(id)}, _country{std::move(country)},
          _geoLatitude{geoLatitude}, _geoLongitude{geoLongitude},
          _name{std::move(name)}
    {}

    bool operator==(const RegionDisplay &other) const
    {
        return id() == other.id() && country() == other.country() &&
            geoLatitude() == other.geoLatitude() &&
            geoLongitude() == other.geoLongitude() && name() == other.name();
    }

public:
    core::StringSlice id() const {return _id;}
    core::StringSlice country() const {return _country;}
    double geoLatitude() const {return _geoLatitude;}
    double geoLongitude() const {return _geoLongitude;}
    const DisplayText &name() const {return _name;}

    void readJson(const nlohmann::json &j);

private:
    std::string _id;
    std::string _country;
    double _geoLatitude;
    double _geoLongitude;
    DisplayText _name;
};

}
