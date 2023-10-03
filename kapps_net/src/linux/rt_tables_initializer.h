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
#include <kapps_core/src/util.h>
#include <kapps_net/net.h>

namespace kapps { namespace net {

// This class is responsible for creating and appending our routing tables to the rt_tables file on Linux.
// There is some complexity now as iproute2 recently changed to a 'stateless configuration' approach
// whereby /etc/iproute2/rt_tables (which we were previously using) will not exist until a user needs to
// make changes to rt_tables. Prior to that /usr/lib/iproute2/rt_tables will be used instead and should
// never be modified.
// When modifications are necessary copy /usr/lib/iproute2/rt_tables to /etc/iproute2/rt_tables (ensuring the iproute2/ directory first exists)
// and then make changes to that file.
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
        std::string etcPath;
        std::string libPath;
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
    // The start index for our custom routing tables
    int nextAvailableTableIndex() const;
    // This method checks for the presence of /etc/iproute2/rt_tables.
    // If not found, it ensures the parent directory (/etc/iproute2) exists
    // and then copies the file from /usr/lib/iproute2/rt_tables to use as a base
    // for our modifications.
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
