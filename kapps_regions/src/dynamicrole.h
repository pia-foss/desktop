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

#pragma once
#include "displaytext.h"
#include <kapps_core/src/corejson.h>
#include <kapps_core/src/retainshared.h>

namespace kapps::regions {

class KAPPS_REGIONS_EXPORT DynamicRole : public core::JsonReadable<DynamicRole>,
    public core::RetainSharedFromThis<DynamicRole>
{
public:
    DynamicRole() = default;
    DynamicRole(std::string id, DisplayText name, std::string resource,
                 std::string winIcon)
        : _id{std::move(id)}, _name{std::move(name)},
          _resource{std::move(resource)}, _winIcon{std::move(winIcon)}
    {}

    bool operator==(const DynamicRole &other) const
    {
        return id() == other.id() && name() == other.name() &&
            resource() == other.resource() && winIcon() == other.winIcon();
    }

public:
    core::StringSlice id() const {return _id;}
    const DisplayText &name() const {return _name;}
    core::StringSlice resource() const {return _resource;}
    core::StringSlice winIcon() const {return _winIcon;}

    void readJson(const nlohmann::json &j);

private:
    std::string _id;
    DisplayText _name;
    std::string _resource;
    std::string _winIcon;
};

}
