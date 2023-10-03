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

#include <common/src/common.h>
#line SOURCE_FILE("linux_networks.cpp")

#include "linux_networks.h"
#include "linux_nl.h"
#include "linux_libnl.h"
#include <common/src/exec.h>
#include <QRegularExpression>
#include <QProcess>
#include <netlink/cache.h>

class LinuxNetworks : public NetworkMonitor
{
public:
    LinuxNetworks();

private:
    LinuxNl _nlWorker;
};

std::unique_ptr<NetworkMonitor> createLinuxNetworks()
{
    if(!libnl::load())
    {
        qWarning() << "Can't monitor network state, failed to load libnl";
        return {};
    }
    return std::unique_ptr<LinuxNetworks>{new LinuxNetworks{}};
}

LinuxNetworks::LinuxNetworks()
{
    connect(&_nlWorker, &LinuxNl::networksUpdated, this,
            &LinuxNetworks::updateNetworks);
}
