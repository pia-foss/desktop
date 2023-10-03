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

#ifndef KAPPS_NET_API_FIREWALL_H
#define KAPPS_NET_API_FIREWALL_H

#include "net.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Public C-linkage API to the firewall component
typedef struct KANFirewallConfig
{
    void(*pAboutToApplyRules)();
    void(*pDidApplyRules)();
} KANFirewallConfig;

KAPPS_NET_EXPORT void KANConfigureFirewall(const struct KANFirewallConfig *pConfig);
KAPPS_NET_EXPORT void KANApplyFirewallRules();

#ifdef __cplusplus
}
#endif

#endif
