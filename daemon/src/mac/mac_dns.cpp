// Copyright (c) 2021 Private Internet Access, Inc.
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

#include "common.h"
#line SOURCE_FILE("mac_dns.cpp")

#include "mac_dns.h"
#include "path.h"
#include "exec.h"

MacDns::MacDns()
{
    connect(&_dynStore, &MacDynamicStore::keysChanged, this,
        []()
        {
            qInfo() << "System configuration changed, invoke DNS helper to re-check";
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("script_type", "watch-notify");
            Exec::cmdWithEnv(Path::OpenVPNUpDownScript, {}, env);
        });

    // In addition to DNS state, the helper script also detects the primary
    // IPv4 interface from the global state, and it checks whether that
    // interface's IP addresses have changed.
    CFTypeRef helperKeyRegexes[3]{CFSTR("^State:/Network/Service/[^/]+/DNS$"),
                                  CFSTR("^State:/Network/Global/IPv4$"),
                                  CFSTR("^State:/Network/Service/[^/]+/IPv4$")};
    _monitorKeyRegexes = MacArray{::CFArrayCreate(nullptr, helperKeyRegexes,
                                                  3, &kCFTypeArrayCallBacks)};
}

void MacDns::enableMonitor()
{
    _dynStore.setNotificationKeys(nullptr, _monitorKeyRegexes.get());
}

void MacDns::disableMonitor()
{
    _dynStore.setNotificationKeys(nullptr, nullptr);
}
