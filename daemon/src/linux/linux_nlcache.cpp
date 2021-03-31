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
#line SOURCE_FILE("linux_nl.cpp")

#include "linux_nlcache.h"
#include "posix/posix_objects.h"
#include "linux_libnl.h"

void LibnlError::checkRet(int ret, CodeLocation location, const char *what)
{
    if(ret < 0)
        throw LibnlError{ret, std::move(location), what};
}

void LibnlError::verify(bool condition, CodeLocation location, const char *what)
{
    if(!condition)
        throw LibnlError{NLE_FAILURE, std::move(location), what};
}

LibnlError::LibnlError(int libnlCode, CodeLocation location, const char *what)
    : _location{std::move(location)}, _libnlCode{libnlCode}, _what{what}
{
    Q_ASSERT(what);
}

LibnlError::LibnlError(CodeLocation location, const char *what)
    : LibnlError{NLE_FAILURE, std::move(location), what}
{
}

const char *LibnlError::what() const noexcept
{
    return _what;
}

const char *LibnlError::libnlMsg() const
{
    return libnl::nl_geterror(_libnlCode);
}

LinuxNlReqSock::LinuxNlReqSock(int family)
    : _pSock{libnl::nl_socket_alloc()}
{
    LibnlError::checkPtr(_pSock, HERE, "Unable to allocate request socket");

    auto libnlErr = libnl::nl_connect(_pSock.get(), family);
    LibnlError::checkRet(libnlErr, HERE, "Unable to connect request socket");

    qInfo() << "Created socket for family" << family << "with local port"
        << libnl::nl_socket_get_local_port(_pSock.get());
}

LinuxNlNtfSock::LinuxNlNtfSock(int netlinkFamily)
    : _receivingDump{false}
{
    // Create the notification socket
    _pNtfSock.reset(libnl::nl_socket_alloc());
    LibnlError::checkPtr(_pNtfSock, HERE, "Could not allocate netlink socket");

    auto libnlErr = libnl::nl_connect(_pNtfSock.get(), netlinkFamily);
    LibnlError::checkRet(libnlErr, HERE, "Could not connect netlink socket");

    // Nonblocking mode and no sequence checks (since we receive notifications)
    libnlErr = libnl::nl_socket_set_nonblocking(_pNtfSock.get());
    LibnlError::checkRet(libnlErr, HERE, "Could not set netlink socket to nonblocking mode");

    libnl::nl_socket_disable_seq_check(_pNtfSock.get());

    qInfo() << "Created socket for family" << netlinkFamily << "with local port"
        << libnl::nl_socket_get_local_port(_pNtfSock.get());

    auto validCb = [](libnl::nl_msg *pMsg, void *pArg)
    {
        LinuxNlNtfSock *pThis = reinterpret_cast<LinuxNlNtfSock*>(pArg);
        return pThis->onValidMsg(pMsg);
    };

    // Set up the callback for the notification socket
    libnlErr = libnl::nl_socket_modify_cb(_pNtfSock.get(), NL_CB_VALID, NL_CB_CUSTOM,
                                          validCb, this);
    LibnlError::checkRet(libnlErr, HERE, "Could not set valid message callback");

    // Set up the "finish" callback that indicates the end of a dump
    auto finishCb = [](libnl::nl_msg *pMsg, void *pArg)
    {
        LinuxNlNtfSock *pThis = reinterpret_cast<LinuxNlNtfSock*>(pArg);
        return pThis->onFinishMsg(pMsg);
    };
    libnlErr = libnl::nl_socket_modify_cb(_pNtfSock.get(), NL_CB_FINISH, NL_CB_CUSTOM,
                                          finishCb, this);
    LibnlError::checkRet(libnlErr, HERE, "Could not set multipart finish callback");
}

int LinuxNlNtfSock::onValidMsg(libnl::nl_msg *pMsg)
{
    try
    {
        libnl::nlmsghdr *pHeader = libnl::nlmsg_hdr(pMsg);
        if(!_receivingDump && pHeader->nlmsg_flags & NLM_F_MULTI)
        {
            qInfo() << "Beginning multipart message";
            _receivingDump = true;
        }
        return handleMsg(pMsg);
    }
    catch(LibnlError &ex)
    {
        // Eat exception, we can't throw out of libnl callbacks
        qWarning() << "Error applying cache update:" << ex;
        // We can still return the error code, which will throw again when
        // the receive result is checked, but this loses the original
        // exception context.
        return -ex.libnlCode();
    }
}

int LinuxNlNtfSock::onFinishMsg(libnl::nl_msg *pMsg)
{
    // _receivingDump might still be false if this dump was empty, in that case
    // we just get a "done" message.
    _receivingDump = false;
    return handleFinish(pMsg);
}

int LinuxNlNtfSock::getFd() const
{
    Q_ASSERT(_pNtfSock);    // Class invariant

    return libnl::nl_socket_get_fd(_pNtfSock.get());
}

void LinuxNlNtfSock::receive(int revents)
{
    Q_ASSERT(_pNtfSock);    // Class invariant

    // Any other events (potentially in combination with POLLIN) indicate the
    // socket is hosed.
    if(revents != POLLIN)
    {
        qInfo() << "Netlink socket is lost; revents:" << revents;
        throw LibnlError{HERE, "Lost netlink socket"};
    }

    auto receiveResult = libnl::nl_recvmsgs_default(_pNtfSock.get());
    LibnlError::checkRet(receiveResult, HERE, "Error receiving into cache");

    handleMsgBatchEnd();
}

void LinuxNlNtfSock::addMembership(int group)
{
    Q_ASSERT(_pNtfSock);    // Class invariant
    auto addErr = libnl::nl_socket_add_membership(_pNtfSock.get(), group);
    LibnlError::checkRet(addErr, HERE, "Could not add group membership");
}

void LinuxNlNtfSock::dropMembership(int group)
{
    Q_ASSERT(_pNtfSock);    // Class invariant
    auto addErr = libnl::nl_socket_drop_membership(_pNtfSock.get(), group);
    LibnlError::checkRet(addErr, HERE, "Could not drop group membership");
}

void LinuxNlNtfSock::sendAuto(libnl::nl_msg *pMsg)
{
    Q_ASSERT(_pNtfSock);    // Class invariant
    auto sendErr = libnl::nl_send_auto(_pNtfSock.get(), pMsg);
    LibnlError::checkRet(sendErr, HERE, "Could not send message");
}

LinuxNlCacheSock::LinuxNlCacheSock(int netlinkFamily)
    : LinuxNlNtfSock{netlinkFamily}, _netlinkFamily{netlinkFamily}
{
}

void LinuxNlCacheSock::applyUpdate(libnl::nl_object *pObj)
{
    if(!pObj)
        return;

    int objType = libnl::nl_object_get_msgtype(pObj);
    auto itObjCache = objCaches.find(objType);

    if(itObjCache == objCaches.end())
    {
        qWarning() << "Received unexpected object of type" << objType;
        return;
    }

    Q_ASSERT(itObjCache->second);   // Class invariant, no nullptrs in map
    itObjCache->second->cache_include(pObj);
}

int LinuxNlCacheSock::handleMsg(libnl::nl_msg *pMsg)
{
    auto parseCb = [](libnl::nl_object *pObj, void *pArg)
    {
        LinuxNlCacheSock *pThis = reinterpret_cast<LinuxNlCacheSock*>(pArg);
        try
        {
            pThis->applyUpdate(pObj);
        }
        catch(LibnlError &ex)
        {
            // Eat exception, we can't throw out of libnl callbacks
            qWarning() << "Error applying cache update:" << ex;
        }
    };

    return libnl::nl_msg_parse(pMsg, parseCb, this);
}

std::shared_ptr<LinuxNlCache> LinuxNlCacheSock::addCache(const char *libnlObjType,
                                                         std::initializer_list<int> netlinkNtfGroups,
                                                         std::initializer_list<int> netlinkObjTypes)
{
    // libnl requires two separate sockets to populate and monitor an object
    // cache:
    // - a blocking "request" socket used for the initial dump
    // - a nonblocking "notification" socket used for updates
    //
    // This means that the initial dump isn't properly serialized with
    // notifications that could occur at the same time.  We might observe some
    // incorrect transients as we set up the cache, but this will steady out
    // once the notifications are processed, and those transients won't
    // negatively impact network detection.
    //
    // If the transients did cause a problem, we'd probably have to stop using
    // libnl entirely in order to receieve the dump inline with notifications
    // correctly.

    // Join notification groups.  Do this before LinuxNlCache fills the cache,
    // so we don't miss events that occur during the dump.
    auto itGroup = netlinkNtfGroups.begin();
    while(itGroup != netlinkNtfGroups.end())
    {
        // If we can't add a group membership for some reason, this throws.
        // This may leave some group memberships already added, which is fine
        // because the netlink worker shuts down if anything unexpected happens.
        addMembership(*itGroup);
        ++itGroup;
    }

    // If the cache can't be created for any reason, this throws.  As above,
    // this may leave some group memberships already added.
    auto pNewCache = std::make_shared<LinuxNlCache>(_netlinkFamily, libnlObjType);

    // Assign this cache to the specified message types
    for(int type : netlinkObjTypes)
        objCaches[type] = pNewCache;

    return pNewCache;
}

LinuxNlCache::Iterator &LinuxNlCache::Iterator::operator++()
{
    if(_pObj)
    {
        // libnl assumes this; not checking it as we do not manipulate caches
        // during iteration.  (If we needed to check this, we'd also need to
        // retain a ref to the current object to keep it alive.)
        Q_ASSERT(libnl::nl_object_get_cache(_pObj));
        _pObj = libnl::nl_cache_get_next(_pObj);
        // After the last object, ::nl_cache_get_next() returns nullptr, so this
        // becomes an end iterator.
    }
    return *this;
}

LinuxNlCache::LinuxNlCache(int netlinkFamily, const char *libnlObjType)
    : _provided{false}, _netlinkFamily{netlinkFamily}
{
    // Create the cache and fill it
    libnl::nl_cache *pRawCache{nullptr};
    auto libnlErr = libnl::nl_cache_alloc_name(libnlObjType, &pRawCache);
    _pCache.reset(pRawCache);
    LibnlError::checkPtrRet(_pCache, libnlErr, HERE, "Unable to allocate cache");

    qInfo() << "Filling netlink cache" << libnlObjType;
    refill();
}

LinuxNlCache::~LinuxNlCache()
{
    if(_provided)
        libnl::nl_cache_mngt_unprovide(_pCache.get());
}

void LinuxNlCache::cache_include(libnl::nl_object *pObj)
{
    Q_ASSERT(pObj); // Checked by caller
    int libnlErr = libnl::nl_cache_include(_pCache.get(), pObj, nullptr, nullptr);
    LibnlError::checkRet(libnlErr, HERE, "Unable to apply update to cache");
}

void LinuxNlCache::refill()
{
    // Create a request socket for the initial cache fill.  We don't need to
    // keep this, it's just for the initial load.
    LinuxNlReqSock reqSock{_netlinkFamily};

    auto libnlErr = libnl::nl_cache_refill(reqSock.get(), _pCache.get());
    LibnlError::checkRet(libnlErr, HERE, "Unable to fill cache");
}

std::size_t LinuxNlCache::count()
{
    auto count = libnl::nl_cache_nitems(_pCache.get());
    // Shouldn't be negative, but it's returned as a signed value, so sanity
    // check it
    if(count >= 0)
        return static_cast<std::size_t>(count);
    return 0;
}

void LinuxNlCache::provide()
{
    if(!_provided)
    {
        libnl::nl_cache_mngt_provide(_pCache.get());
        _provided = true;
    }
}

void LinuxNlCache::dumpSummary() const
{
    libnl::nl_dump_params dumpParams{};
    dumpParams.dp_type = NL_DUMP_LINE;
    dumpParams.dp_prefix = 2;
    dumpParams.dp_print_index = true;
    dumpParams.dp_dump_msgtype = true;
    dumpParams.dp_cb = [](libnl::nl_dump_params*, char *line){qInfo() << line;};
    libnl::nl_cache_dump(_pCache.get(), &dumpParams);
}
