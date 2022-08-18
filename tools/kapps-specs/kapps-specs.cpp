// Copyright (c) 2022 Private Internet Access, Inc.
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

#include <kapps_core/logger.h>
#include <kapps_net/firewall.h>
#include <kapps_net/src/firewall.h>
#include <iostream>
#include <cassert>
#include <kapps_core/src/newexec.h>
#include <kapps_core/src/coreprocess.h>

void cppFirewall()
{
#if defined(KAPPS_CORE_FAMILY_DESKTOP)
    kapps::net::FirewallConfig config;
    config.brandInfo.code = "acme";
    config.brandInfo.identifier = "com.privateinternetaccess.vpn";
#if defined(KAPPS_CORE_OS_LINUX)
    config.bypassFile = "bypass/file";
    config.vpnOnlyFile = "vpnOnly/file";
    config.defaultFile = "default/file";
    config.brandInfo.cgroupBase = 1383;
    config.brandInfo.fwmarkBase = 12817;
#endif

    kapps::net::Firewall fw{config};

    fw.applyRules({});
#endif
}

int main()
{
    //configureFirewall();
    cppFirewall();
/*     Process_readAllStandardOutputAndError();
    Exec_cmdWithOutput();
    Exec_bashWithOutput();
 */
    return 0;
}
