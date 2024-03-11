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
#include <kapps_core/src/util.h>
#include <kapps_net/net.h>

namespace kapps { namespace net {

// NOTE: since iproute2 6.7.0 they changed the fallback file to /usr/share/iproute2/rt_tables rather than
// /usr/lib/iproute2/rt_tables
//
// This class is responsible for creating and appending our routing tables to the rt_tables file on Linux.
// There is some complexity now as iproute2 recently changed to a 'stateless configuration' approach
// whereby /etc/iproute2/rt_tables (which we were previously using) will not exist until a user needs to
// make changes to rt_tables. Prior to that /usr/lib/iproute2/rt_tables will be used instead and should
// never be modified.
// When modifications are necessary write to /etc/iproute2/rt_tables (ensuring the iproute2/ directory first exists)
// and then make changes to that file. Tables added to /etc/iproute2/rt_tables are then applied on top of the tables that
// already exist in /usr/lib/iproute2/rt_tables
// For the rationale behind the changes to iproute2 - see this link to the mailing list:
// https://git.kernel.org/pub/scm/network/iproute2/iproute2.git/commit/?id=0a0a8f12fa1b03dd0ccbebf5f85209d1c8a0f580
// Full text description from link above provided here:
// > Add support for the so called "stateless" configuration pattern (read
// > from /etc, fall back to /usr), giving system administrators a way to
// > define local configuration without changing any distro-provided files.
// > In practice this means that each configuration file FOO is loaded
// > from /usr/lib/iproute2/FOO unless /etc/iproute2/FOO exists.
// > Signed-off-by: Gioele Barabucci <gioele@svario.it>
// > Signed-off-by: Stephen Hemminger <stephen@networkplumber.org>
class KAPPS_NET_EXPORT RtTablesInitializer
{
public:
    struct KAPPS_NET_EXPORT RtLocations
    {
        // Used if it exists
        std::string etcPath;
        // Otherwise search in order for a fallback
        // and copy that fallback into etcPath
        // Current fallback paths are: /usr/share/iproute2/rt_tables
        // and /usr/lib/iproute2/rt_tables and should be searched in that order.
        std::vector<std::string> fallbackPaths;
    };

public:
    RtTablesInitializer(const std::string &brandPrefix,
        RtLocations rtLocations);

public:
    // Install our routing tables in the rt_tables file
    bool install() const;
    // Return our routing table names (based on the brandPrefix)
    std::vector<std::string> tableNames() const { return _tableNames; }

private:
    // Insert our routing tables at the end of rt_tables file
    void appendRoutingTables(int availableIndex) const;
    // The next available index based on the passed-in rt_tables file
    int nextAvailableTableIndexFromFile(const std::string &rtPath) const;
    // The start index will use for our custom routing tables in /etc/iproute2/rt_tables
    // this index is either calculated from /etc/iproute2/rt_tables itself or (if that file is empty)
    // from one of the fallback files
    int nextAvailableIndex() const;
    // This method checks for the presence of /etc/iproute2/rt_tables.
    // If not found, it ensures the parent directory (/etc/iproute2) exists
    // and then creates the /etc/iproute2/rt_tables file
    bool prepareRtLocation() const;
    // Whether the specific routing table is already installed
    bool isRoutingTableInstalled(const std::string &tableName) const;

private:
    // Common error message
    std::string _errorEpilogue;
    // The routing tables to insert
    std::vector<std::string> _tableNames;
    // The locations of the rt_tables file
    RtLocations _rtLocations;
};

}}
