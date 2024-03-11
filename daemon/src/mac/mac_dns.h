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

#include <common/src/common.h>
#line HEADER_FILE("mac_dns.h")

#ifndef MAC_DNS_H
#define MAC_DNS_H

#include "mac_dynstore.h"
#include "mac_objects.h"

// MacDns is used to monitors for changes to the DNS configuration on MacOS and
// restores PIA's configuration.  (MacOS tends to overwrite the DNS
// configuration, especially on 10.15.4+ apparently.)  This is used by both the
// OpenVPN and WireGuard methods.
class MacDns : public QObject
{
    Q_OBJECT

public:
    // The monitor is initially disabled
    MacDns();

public:
    // Enable or disable the configuration change monitor
    void enableMonitor();
    void disableMonitor();

private:
    MacDynamicStore _dynStore;
    MacArray _monitorKeyRegexes;
};

#endif
