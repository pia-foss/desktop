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

#include <common/src/common.h>
#line SOURCE_FILE("linux_libnl.cpp")

#include "linux_libnl.h"

namespace libnl
{
    namespace
    {
        bool attempted = false, loaded = false;
    }

    bool load()
    {
        if(attempted)
            return loaded;

        attempted = true;

        // Default to successful load, clear it if anything fails.  This lets us
        // continue through to try to load everything and fail at the end, which
        // produces better diagnostic information.
        loaded = true;
        QLibrary libnl{QStringLiteral("libnl-3.so.200")};
        QLibrary librtnl{QStringLiteral("libnl-route-3.so.200")};
        QLibrary libgenl{QStringLiteral("libnl-genl-3.so.200")};

        if(!libnl.load())
        {
            loaded = false;
            qWarning() << "Can't find" << libnl.fileName();
        }
        if(!librtnl.load())
        {
            loaded = false;
            qWarning() << "Can't find" << librtnl.fileName();
        }
        if(!libgenl.load())
        {
            loaded = false;
            qWarning() << "Can't find" << libgenl.fileName();
        }

        // Don't try to load entry points if we already failed, would just spew
        // lots of unhelpful failures
        if(!loaded)
            return false;

        // Load libnl entry points
        auto loadFromLib = [](QLibrary &lib, auto &func)
        {
            if(!func.load(lib))
            {
                loaded = false;
                qWarning() << "Can't find" << func.symbol() << "in" << lib.fileName();
            }
        };
        auto loadLibnl = [&libnl, &loadFromLib](auto &func){loadFromLib(libnl, func);};
        auto loadLibrtnl = [&librtnl, &loadFromLib](auto &func){loadFromLib(librtnl, func);};
        auto loadLibgenl = [&libgenl, &loadFromLib](auto &func){loadFromLib(libgenl, func);};

        // Entry points from libnl-3.so
        loadLibnl(nl_addr_get_binary_addr);
        loadLibnl(nl_addr_get_prefixlen);
        loadLibnl(nl_addr_get_family);
        loadLibnl(nl_addr_get_len);
        loadLibnl(nl_cache_alloc_name);
        loadLibnl(nl_cache_dump);
        loadLibnl(nl_cache_free);
        loadLibnl(nl_cache_get_first);
        loadLibnl(nl_cache_get_next);
        loadLibnl(nl_cache_include);
        loadLibnl(nl_cache_mngt_provide);
        loadLibnl(nl_cache_mngt_unprovide);
        loadLibnl(nl_cache_nitems);
        loadLibnl(nl_cache_refill);
        loadLibnl(nl_connect);
        loadLibnl(nl_geterror);
        loadLibnl(nl_msg_parse);
        loadLibnl(nl_object_get_cache);
        loadLibnl(nl_object_get_msgtype);
        loadLibnl(nl_recvmsgs_default);
        loadLibnl(nl_send_auto);
        loadLibnl(nl_socket_add_membership);
        loadLibnl(nl_socket_alloc);
        loadLibnl(nl_socket_disable_seq_check);
        loadLibnl(nl_socket_drop_membership);
        loadLibnl(nl_socket_free);
        loadLibnl(nl_socket_get_fd);
        loadLibnl(nl_socket_get_local_port);
        loadLibnl(nl_socket_modify_cb);
        loadLibnl(nl_socket_set_nonblocking);
        loadLibnl(nla_data);
        loadLibnl(nla_get_u16);
        loadLibnl(nla_get_u32);
        loadLibnl(nla_put_u32);
        loadLibnl(nla_len);
        loadLibnl(nla_next);
        loadLibnl(nla_ok);
        loadLibnl(nla_parse_nested);
        loadLibnl(nlmsg_alloc);
        loadLibnl(nlmsg_free);
        loadLibnl(nlmsg_hdr);
        loadLibnl(nlmsg_parse);

        // Entry points from libnl-route-3.so.200
        loadLibrtnl(rtnl_addr_get_ifindex);
        loadLibrtnl(rtnl_addr_get_local);
        loadLibrtnl(rtnl_link_get);
        loadLibrtnl(rtnl_link_get_mtu);
        loadLibrtnl(rtnl_link_get_name);
        loadLibrtnl(rtnl_link_put);
        loadLibrtnl(rtnl_route_get_dst);
        loadLibrtnl(rtnl_route_get_family);
        loadLibrtnl(rtnl_route_get_metric);
        loadLibrtnl(rtnl_route_get_priority);
        loadLibrtnl(rtnl_route_get_nnexthops);
        loadLibrtnl(rtnl_route_get_table);
        loadLibrtnl(rtnl_route_nexthop_n);
        loadLibrtnl(rtnl_route_nh_get_gateway);
        loadLibrtnl(rtnl_route_nh_get_ifindex);

        // Entry points from libnl-genl-3.so.200
        loadLibgenl(genlmsg_hdr);
        loadLibgenl(genlmsg_put);

        return loaded;
    }

    // Create a binding in this namespace to a libnl function in the global
    // namespace - fills in the name of the binding object and symbol name from
    // the function name
#define LIBNL_FUNC(name)    auto name = runtimeFunc(decltype(&::name){}, #name)
    // Entry points from libnl-3.so
    LIBNL_FUNC(nl_addr_get_binary_addr);
    LIBNL_FUNC(nl_addr_get_prefixlen);
    LIBNL_FUNC(nl_addr_get_family);
    LIBNL_FUNC(nl_addr_get_len);
    LIBNL_FUNC(nl_cache_alloc_name);
    LIBNL_FUNC(nl_cache_dump);
    LIBNL_FUNC(nl_cache_free);
    LIBNL_FUNC(nl_cache_get_first);
    LIBNL_FUNC(nl_cache_get_next);
    LIBNL_FUNC(nl_cache_include);
    LIBNL_FUNC(nl_cache_mngt_provide);
    LIBNL_FUNC(nl_cache_mngt_unprovide);
    LIBNL_FUNC(nl_cache_nitems);
    LIBNL_FUNC(nl_cache_refill);
    LIBNL_FUNC(nl_connect);
    LIBNL_FUNC(nl_geterror);
    LIBNL_FUNC(nl_msg_parse);
    LIBNL_FUNC(nl_object_get_cache);
    LIBNL_FUNC(nl_object_get_msgtype);
    LIBNL_FUNC(nl_recvmsgs_default);
    LIBNL_FUNC(nl_send_auto);
    LIBNL_FUNC(nl_socket_add_membership);
    LIBNL_FUNC(nl_socket_alloc);
    LIBNL_FUNC(nl_socket_disable_seq_check);
    LIBNL_FUNC(nl_socket_drop_membership);
    LIBNL_FUNC(nl_socket_free);
    LIBNL_FUNC(nl_socket_get_fd);
    LIBNL_FUNC(nl_socket_get_local_port);
    LIBNL_FUNC(nl_socket_modify_cb);
    LIBNL_FUNC(nl_socket_set_nonblocking);
    LIBNL_FUNC(nla_data);
    LIBNL_FUNC(nla_get_u16);
    LIBNL_FUNC(nla_get_u32);
    LIBNL_FUNC(nla_put_u32);
    LIBNL_FUNC(nla_len);
    LIBNL_FUNC(nla_next);
    LIBNL_FUNC(nla_ok);
    LIBNL_FUNC(nla_parse_nested);
    LIBNL_FUNC(nlmsg_alloc);
    LIBNL_FUNC(nlmsg_free);
    LIBNL_FUNC(nlmsg_hdr);
    LIBNL_FUNC(nlmsg_parse);

    // Entry points from libnl-route-3.so.200
    LIBNL_FUNC(rtnl_addr_get_ifindex);
    LIBNL_FUNC(rtnl_addr_get_local);
    LIBNL_FUNC(rtnl_link_get);
    LIBNL_FUNC(rtnl_link_get_mtu);
    LIBNL_FUNC(rtnl_link_get_name);
    LIBNL_FUNC(rtnl_link_put);
    LIBNL_FUNC(rtnl_route_get_dst);
    LIBNL_FUNC(rtnl_route_get_family);
    LIBNL_FUNC(rtnl_route_get_metric);
    LIBNL_FUNC(rtnl_route_get_priority);
    LIBNL_FUNC(rtnl_route_get_nnexthops);
    LIBNL_FUNC(rtnl_route_get_table);
    LIBNL_FUNC(rtnl_route_nexthop_n);
    LIBNL_FUNC(rtnl_route_nh_get_gateway);
    LIBNL_FUNC(rtnl_route_nh_get_ifindex);

    // Entry points from libnl-genl-3.so.200
    LIBNL_FUNC(genlmsg_hdr);
    LIBNL_FUNC(genlmsg_put);
#undef LIBNL_FUNC

}
