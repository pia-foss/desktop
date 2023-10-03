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

#ifndef LINUX_GENLFAMILIES_H
#define LINUX_GENLFAMILIES_H

#include <common/src/common.h>
#include "linux_nlcache.h"
#include "linux_libnl.h"
#include <cstdint>

// Cache generic netlink families (including updates).
//
// libnl offers a genl/family cache, but it isn't very useful
// - it ignores updates, we'd have to force a re-dump on every update
// - there's no API to get multicast group IDs from the cache - the value is
//   there, we just can't get to it through the libnl API
class LinuxGenlFamilies : private LinuxNlNtfSock
{
public:
    // Data for a generic Netlink family.
    struct Family
    {
        // Protocol ID for this family (Netlink message type).
        std::uint16_t _protocol;
        // Version for this family
        std::uint32_t _version;
        // Multicast groups for this family.
        // LinuxGenlFamilies does not currently process updates to multicast
        // groups.
        std::unordered_map<QString, std::uint32_t> _multicastGroups;
    };

public:
    LinuxGenlFamilies();

private:
    // Read the value of an NLA_STRING attribute as a QString.  The terminating
    // null character (if present) is removed.
    QString readAttrString(libnl::nlattr &attr);
    void handleNewFamily(libnl::nlmsghdr &msgHeader);
    void handleDelFamily(libnl::nlmsghdr &msgHeader);
    virtual int handleMsg(libnl::nl_msg *pMsg) override;

    virtual int handleFinish(libnl::nl_msg *) override;

public:
    using LinuxNlNtfSock::getFd;
    using LinuxNlNtfSock::receive;

    // Get info for a generic family.  If the family exists, returns a pointer
    // to the family data (owned by LinuxGenlFamilies); otherwise returns
    // nullptr.
    const Family *getFamily(const QString &name);

    // Check whether the initial family dump has completed - if this hasn't been
    // set yet, we don't necessarily know about all families that might exist
    // yet.
    bool ready() const {return _ready;}

private:
    // Cache of families
    std::unordered_map<QString, Family> _families;
    // Whether the initial family dump has completed
    bool _ready;
    // Whether we've added the "notify" multicast group to the socket yet
    bool _addedNotifyGroup;
};

#endif
