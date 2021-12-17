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

#ifndef LINUX_LIBNL_H
#define LINUX_LIBNL_H

#include "common.h"
#include <QLibrary>
#include <linux/genetlink.h>
#include <netlink/genl/genl.h>
#include <netlink/cache.h>
#include <netlink/route/addr.h>
#include <netlink/route/route.h>
#include <netlink/genl/mngt.h>
#include <netlink/genl/genl.h>
#include <linux/nl80211.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <netlink/cache.h>
#include <netlink/route/link.h>

// Pointer to a function from a library loaded at runtime.
template<class R, class...Args>
class RuntimeFunc
{
public:
    RuntimeFunc(const char *pSymbol) : _pFunc{}, _pSymbol{pSymbol} {}

public:
    const char *symbol() const {return _pSymbol;}

    // Attempt to load the function.  Returns false if it can't be loaded.
    bool load(QLibrary &lib)
    {
        _pFunc = reinterpret_cast<R(*)(Args...)>(lib.resolve(_pSymbol));
        if(!_pFunc)
        {
            qWarning() << "Could not find" << _pSymbol << "in" << lib.fileName();
            return false;
        }
        return true;
    }

    // Call the function.  Throws if the function hasn't been loaded.
    template<class...CallArgs>
    R operator()(CallArgs... args) const
    {
        if(!_pFunc)
            throw Error{HERE, Error::Code::LibraryUnavailable};
        return _pFunc(std::forward<CallArgs>(args)...);
    }

private:
    R (*_pFunc)(Args...);
    const char *_pSymbol;
};

// Deduce the argument types for RuntimeFunc from the type of a declared
// function.  (C++14 lacks deduction guides.)
//
// For example:
// namespace fancy_lib
// {
//     // Dynamic binding to ::create_fancy_obj() from fancy_lib.so
//     auto create_fancy_obj = runtimeFunc(decltype(&::create_fancy_obj){}, "create_fancy_obj");
// }
//
// Meanwhile:
//     QLibrary fancyLib{...};
//     // load fancy_lib.so
//     fancy_lib::create_fancy_obj.load(fancyLib);
template<class R, class...Args>
RuntimeFunc<R, Args...> runtimeFunc(R(*)(Args...), const char *pSymbol){return {pSymbol};}

// The libnl libraries are loaded at runtime so the daemon can gracefully
// degrade if they're not present.  This layer is just a shim over the libnl
// API; further abstractions are built on top of this.
//
// These dependencies were added in an update - the installer will install them
// for known platforms, but we don't know the packages for all platforms.
// Avoiding a hard dependency also provides a fallback if the library in some
// distribution is not compatible.
//
// The libnl dev packages (and kernel headers) are still required at build time.
// Types from those headers are used here (and aliased into the libnl namespace
// for consistency).
namespace libnl
{
    // Ensure libnl is loaded.  The first time this is called, attempts to load
    // all entry points from libnl-3.so, libnl-route-3.so.200, and
    // libnl-genl-3.so.200.  (Subsequent calls just return the result of the
    // initial load.)
    //
    // Returns true if all entry points were loaded.
    bool load();

    // Types from the kernel or libnl headers
    using ::nlattr;
    using ::nlmsghdr;
    using ::nl_addr;
    using ::nl_cache;
    using ::nl_dump_params;
    using ::nl_msg;
    using ::nl_object;
    using ::nl_sock;
    using ::rtnl_addr;
    using ::rtnl_link;
    using ::rtnl_nexthop;
    using ::rtnl_route;
    using ::genlmsghdr;

    // Declare a binding in this namespace to a libnl function in the global
    // namespace - these are defined with runtimeFunc()
#define LIBNL_FUNC(name)    extern decltype(runtimeFunc(decltype(&::name){}, #name)) name;
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

#endif
