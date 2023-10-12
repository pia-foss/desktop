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

#include "rt_tables_initializer.h"
#include <fstream>
#include <kapps_core/src/newexec.h>
#include <kapps_core/src/fs.h>

namespace kapps { namespace net {

// For convenience
namespace fs = core::fs;

RtTablesInitializer::RtTablesInitializer(const std::string &brandPrefix, RtTablesInitializer::RtLocations rtLocations)
    : _errorEpilogue{"The VPN may not work as expected!"}
    , _rtLocations(std::move(rtLocations))
{
    std::string serviceName = qs::format("%vpn", brandPrefix);
    _tableNames = std::vector<std::string>{
        qs::format("%rt", serviceName),     // piavpnrt     - bypass routing table
        qs::format("%Onlyrt", serviceName), // piavpnOnlyrt - vpnOnly routing table
        qs::format("%Wgrt", serviceName),   // piavpnWgRt   - WireGuard routing table
        qs::format("%Fwdrt", serviceName),  // piavpnFwdrt  - Forwarded packets routing table (i.e docker)
    };
}

bool RtTablesInitializer::install() const
{
    assert(_tableNames.size() > 0); // Invariant

    KAPPS_CORE_INFO() << "Preparing to setup routing tables.";

    // Setup the _routingTablePath
    // This really means preparing the etcPath for rt_tables on disk
    // so that it's ready to be written to.
    // If this step fails we cannot proceed - tracing happens inside prepareRtLocation()
    if(!prepareRtLocation())
        return false;

    // Get the next available routing table index
    int availableIndex = nextAvailableTableIndex();
    // No index was found - this should never be the case .
    // There should always be routing tables in the file - so this
    // is an error we cannot recover from.
    if(availableIndex == -1)
    {
        KAPPS_CORE_WARNING() << "Could not find a routing table index to use."
            << _errorEpilogue;
        return false;
    }

    // This function cannot fail - the wrost it can do
    // is not append anything (if the tables already exist in the file)
    appendRoutingTables(availableIndex);

    return true;
}

void RtTablesInitializer::appendRoutingTables(int availableIndex) const
{
    // Append our routing tables to end of file
    std::ofstream rtFile{_rtLocations.etcPath, std::ios::app};
    int index{availableIndex};
    for(const auto &tableName : _tableNames)
    {
        if(!isRoutingTableInstalled(tableName))
        {
            // An entry looks like, e.g "100  piavpnrt"
            rtFile << index << "\t" << tableName << "\n";
            ++index;

            KAPPS_CORE_INFO() << qs::format("Added % routing table to %", tableName, _rtLocations.etcPath);
        }
    }

    if(availableIndex == index)
        KAPPS_CORE_INFO() << "No routing tables needed to be added.";
}

bool RtTablesInitializer::isRoutingTableInstalled(const std::string &tableName) const
{
    return 0 == core::Exec::bash(qs::format("grep % %", tableName, _rtLocations.etcPath), true);
}

int RtTablesInitializer::nextAvailableTableIndex() const
{
    // Routing tables are formatted like this in rt_tables:
    // 100  myTable1
    // 101  myTable2
    // 99   myTable3
    // 70   myTable4
    // 200  myTable5
    // Note that the index (on the left) is not guaranteed to increment linearly or in order
    // The awk snippet below finds the highest valued index.
    // TODO: rewrite this in pure C++
    std::string highestIndex = core::Exec::bashWithOutput(qs::format("awk '/^[0-9]/{print $1}' \"%\" | sort -n | tail -1", _rtLocations.etcPath));

    if(highestIndex.empty())
        return -1; // Indicate no index was found

    // Add 1 for the next available index
    return std::stoi(highestIndex) + 1;
}

bool RtTablesInitializer::prepareRtLocation() const
{
    // If the etcPath already exists we're ready to go.
    if(fs::exists(_rtLocations.etcPath))
    {
        KAPPS_CORE_INFO() << "Found" << _rtLocations.etcPath;
        return true;
    }

    // Since etcPath doesn't exist we have to create it, and possibly its
    // containing directory.
    else if(fs::exists(_rtLocations.libPath))
    {
        KAPPS_CORE_INFO() << "Did not find" << _rtLocations.etcPath << "- Looking for"
            << _rtLocations.libPath;

        const auto etcDirName{fs::dirName(_rtLocations.etcPath)};
        // This is also successful if the directory already exists.
        if(!fs::mkDir_p(etcDirName))
        {
            KAPPS_CORE_WARNING() << "Could not create the" << etcDirName << "directory"
                << _errorEpilogue << core::ErrnoTracer{};
            return false;
        }
        // At this point the containing directory should exist
        // So we copy the rt_tables file from the libPath
        if(!fs::copyFile(_rtLocations.libPath, _rtLocations.etcPath))
        {
            KAPPS_CORE_WARNING() << "Could not copy rt_tables from" << _rtLocations.libPath
                << "to" << _rtLocations.etcPath << _errorEpilogue;
            return false;
        }
        // Adjust file permissions to match the original rt_tables
        if(!fs::copyFilePermissions(_rtLocations.libPath, _rtLocations.etcPath))
        {
            KAPPS_CORE_WARNING() << "Could not copy permissions from" << _rtLocations.libPath
                << "to" << _rtLocations.etcPath << "- Will continue" << _errorEpilogue;
        }

        KAPPS_CORE_INFO() << "Successfully copied" << _rtLocations.libPath
            << "to" << _rtLocations.etcPath;

        // We succesfully copied the file
        return true;
    }
    else
    {
        KAPPS_CORE_WARNING() << "Could not find location of rt_tables!"
            << _errorEpilogue;
    }

    return false;
}
}}
