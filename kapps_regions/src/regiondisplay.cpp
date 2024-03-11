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

#include "regiondisplay.h"
#include <nlohmann/json.hpp>

namespace kapps::regions {

void RegionDisplay::readJson(const nlohmann::json &j)
{
    _id = j.at("id").get<std::string>();
    _country = j.at("country").get<std::string>();
    _geoLatitude = j.at("geo").at(0).get<double>();
    _geoLongitude = j.at("geo").at(1).get<double>();
    _name = j.at("display").get<DisplayText>();
}

}
