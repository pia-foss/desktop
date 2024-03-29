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

#include "corejson.h"
#include "ipaddress.h"
#include "logger.h"
#include <nlohmann/json.hpp>

namespace kapps::core {

const nlohmann::json &jsonObject(const nlohmann::json &j)
{
    if(!j.is_object())
    {
        KAPPS_CORE_WARNING() << "Expected JSON object, got value" << j;
        throw std::runtime_error{"Expected JSON object"};
    }
    return j;
}

const nlohmann::json &jsonArray(const nlohmann::json &j)
{
    if(!j.is_array())
    {
        KAPPS_CORE_WARNING() << "Expected JSON array, got value" << j;
        throw std::runtime_error{"Expected JSON array"};
    }
    return j;
}

}
