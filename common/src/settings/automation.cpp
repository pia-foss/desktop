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

#include "../common.h"
#include "automation.h"

void AutomationRuleAction::trace(std::ostream &os) const
{
    os << "{connection: " << connection() << "}";
}

void AutomationRuleCondition::trace(std::ostream &os) const
{
    bool wroteAnything = false;
    os << "{";

    if(ruleType() != QString(""))
    {
        os << "ruleType: " << ruleType();
        wroteAnything = true;
    }

    if(ssid() != QString(""))
    {
        if(wroteAnything)
            os << ", ";
        os << "ssid: " << ssid();
        wroteAnything = true;
    }

    if(!wroteAnything)
        os << "<none>";    // No criteria in this condition
    os << "}";
}

void AutomationRule::trace(std::ostream &os) const
{
    os << "{condition: " << condition() << ", action: " << action() << "}";
}
