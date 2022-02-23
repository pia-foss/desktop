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

#ifndef LINUX_NLCACHE_H
#define LINUX_NLCACHE_H

#include "common.h"
#include <memory>
#include <unordered_map>
#include "linux_libnl.h"

// Exception object for a libnl error
class LibnlError : public std::exception
{
public:
    // Check a pointer return value from libnl - if it's nullptr, throw a
    // LibnlError with NLE_FAILURE.
    // Ptr_t can be anything explicitly convertible to 'bool', including raw
    // pointers and smart pointers.
    template<class Ptr_t>
    static void checkPtr(const Ptr_t &ptr, CodeLocation location, const char *what)
    {
        if(!ptr)
            throw LibnlError{NLE_FAILURE, std::move(location), what};
    }

    // Check a return value from libnl - if it's negative, throw a LibnlError
    // with that error code.
    static void checkRet(int ret, CodeLocation location, const char *what);

    // Check both a pointer and a return code - a few libnl APIs return both
    // (like nl_cache_alloc_name()).
    template<class Ptr_t>
    static void checkPtrRet(const Ptr_t &ptr, int ret, CodeLocation location,
                            const char *what)
    {
        if(!ptr && ret >= 0)
            ret = NLE_FAILURE;  // Default error code for nullptr if an error wasn't returned
        checkRet(ret, std::move(location), what);
    }

    // Verify that some condition holds; throw a LibnlError with NLE_FAILURE
    // otherwise.
    static void verify(bool condition, CodeLocation location, const char *what);

public:
    // Most libnl functions return negated error codes.  This can be passed to
    // LibnlError directly (nl_geterror() takes the absolute value of the code).
    //
    // 'what' must be a string literal, the string is not copied.
    LibnlError(int libnlCode, CodeLocation location, const char *what);
    // The libnl code defaults to NLE_FAILURE if not given, use this for a few
    // libnl APIs that just return nullptr, etc. with no error code.
    LibnlError(CodeLocation location, const char *what);

public:
    const CodeLocation &location() const {return _location;}
    // Get the libnl error code - always returns a positive code
    int libnlCode() const {return std::abs(_libnlCode);}
    virtual const char *what() const noexcept override;

    const char *libnlMsg() const;

private:
    CodeLocation _location;
    int _libnlCode;
    const char *_what;
};

inline QDebug &operator<<(QDebug &d, const LibnlError &err)
{
    QDebugStateSaver saver{d};
    return d.nospace() << err.what() << " - libnl error " << err.libnlCode()
        << " (" << err.libnlMsg() << ") (at " << err.location() << ")";
}

// Deleter for libnl types
class NlDeleter
{
public:
    void operator()(libnl::nl_cache *pObj) const {libnl::nl_cache_free(pObj);}
    void operator()(libnl::nl_sock *pObj) const {libnl::nl_socket_free(pObj);}
    void operator()(libnl::nl_msg *pObj) const {libnl::nlmsg_free(pObj);}
    void operator()(libnl::rtnl_link *pObj) const {libnl::rtnl_link_put(pObj);}
};

// Owning pointer for libnl types (using NlDeleter)
template<class NlPtr_t>
using NlUniquePtr = std::unique_ptr<NlPtr_t, NlDeleter>;

// Netlink request socket - just allocates a socket and connects it to the given
// netlink family.
class LinuxNlReqSock
{
public:
    // If the socket can't be created or connected, this throws.
    LinuxNlReqSock(int family);

public:
    libnl::nl_sock *get() {return _pSock.get();}

private:
    NlUniquePtr<libnl::nl_sock> _pSock;
};

class LinuxNlCache;

// LinuxNlNtfSock opens a Netlink notification socket.  This is an abstract base
// that operates the socket and receives notifications.
//
// handleMsg() should be implemented to handle incoming messages.
class LinuxNlNtfSock
{
public:
    // Create LinuxNlNtfSock with the netlink family, such as NETLINK_ROUTE.
    //
    // If the netlink socket can't be allocated or connected, this throws -
    // LinuxNlNtfSock can't be constructed.
    explicit LinuxNlNtfSock(int netlinkFamily);

    virtual ~LinuxNlNtfSock() = default;

private:
    // libnl callback for valid message
    int onValidMsg(libnl::nl_msg *pMsg);
    // libnl callback for finishing a multipart message (called for the
    // NLMSG_DONE message)
    int onFinishMsg(libnl::nl_msg *pMsg);

public:
    // Get the notification socket descriptor for use in a blocking wait.
    // When data becomes available, call receive().
    int getFd() const;

    // Receive data on the notification socket.  Pass the revents from the
    // poll() call; if an error of any kind was signaled LinuxNlCache throws.
    void receive(int revents);

    // Check if we're currently in the middle of receiving a dump (we have
    // received at least one multipart message, but haven't received NLMSG_DONE
    // yet).
    //
    // Dump requests shouldn't be sent when we're already receiving a dump -
    // this may vary by protocol but is known not to work with nl80211 (see
    // LinuxNl80211Cache).
    bool inDump() const {return _receivingDump;}

protected:
    // Implement handleMsg() to receive incoming messages.  This can return an
    // error code, which is returned to libnl.  Negative values indicate
    // failure; nonnegative values indicate success.
    virtual int handleMsg(libnl::nl_msg *pMsg) = 0;

    // Implement handleFinish() to receive notifications that a multipart dump
    // has completed.  Like handleMsg(), this can return an error code.
    virtual int handleFinish(libnl::nl_msg *pMsg) = 0;

    // Take actions after a batch of messages have been received.
    // Whenever receive() is called, it calls this after all messages have been
    // processed (if they are processed successfully).
    // This isn't called if any error occurs (including exceptions from
    // handleMsg()).
    // Note that this refers to a batch of messages received at once in the
    // event loop, not a multipart message - we _can_ be in the middle of a
    // multipart message at this point (see inDump() / handleFinish()).
    virtual void handleMsgBatchEnd() {}

    // Add or remove a notification group membership
    void addMembership(int group);
    void dropMembership(int group);

    // Send a message.  This uses ::nl_send_auto(), which completes the message
    // if necessary.
    void sendAuto(libnl::nl_msg *pMsg);

private:
    NlUniquePtr<libnl::nl_sock> _pNtfSock;
    // Whether we're currently in the middle of a dump - enabled when we receive
    // any multipart message, disabled on NLMSG_DONE.
    bool _receivingDump;
};

// LinuxNlCacheSock opens a netlink socket that can receive notifications for
// any number of libnl caches sharing the same netlink family.
//
// This socket only works with cacheable types provided by libnl.  Since libnl
// doesn't provide a public API to register additional cacheable types, custom
// types must implement LinuxNlCacheSock::handleMsg() differently.
class LinuxNlCacheSock : public LinuxNlNtfSock
{
public:
    // Create LinuxNlCacheSock with the netlink family, such as NETLINK_ROUTE.
    explicit LinuxNlCacheSock(int netlinkFamily);

private:
    // Handle a cache update with nl_cache_include.  Finds the appropriate cache
    // by object type.
    void applyUpdate(libnl::nl_object *pObj);

    // handleMsg() parses a message and applies it to the appropriate cache.
    virtual int handleMsg(libnl::nl_msg *pMsg) override;

    virtual int handleFinish(libnl::nl_msg *) override {return NL_OK;}

public:
    // Add a new cache using this notification socket.
    //
    // Provide the libnl object type (such as "route/addr", the netlink
    // broadcast groups (such as RTNLGRP_IPV4_IFADDR), and the netlink message
    // types (such as RTM_(NEW|DEL|GET)ADDR.
    //
    // These parameter values are listed in libnl (nl_cache_ops::co_name,
    // nl_cache_ops::co_msgtypes, and nl_cache_ops::co_groups), but libnl
    // doesn't provide any API to get those parts of the cache ops, and we need
    // them to route messages correctly.
    //
    // Ownership of the resulting cache is shared with the caller.  If
    // LinuxNlCacheSock is destroyed first, the cache will still work (but won't
    // be able to receive any new data).  If the caller drops its reference to
    // the cache, LinuxNlCacheSock still keeps it alive (but there's no way to
    // get to the data).
    //
    // This can fail, which throws.  Failure can occur due to libnl errors,
    // invalid parameters, failure to allocate a socket, etc.
    std::shared_ptr<LinuxNlCache> addCache(const char *libnlObjType,
                                           std::initializer_list<int> netlinkNtfGroups,
                                           std::initializer_list<int> netlinkObjTypes);

private:
    int _netlinkFamily;
    // Map of object types to caches; used when receiving messages.  A cache may
    // appear more than once if it has more than one object type.
    std::unordered_map<int, std::shared_ptr<LinuxNlCache>> objCaches;
};

// LinuxNlCache maintains a Netlink cache and the sockets to monitor/populate
// it.
class LinuxNlCache
{
public:
    // Iterates a LinuxNlCache as a sequence of `nl_object*` values.
    // The cache _cannot_ be mutated during the iteration:
    // - libnl assumes the object is still in a cache in ::nl_cache_get_next()
    // - Iterator does not retain an extra reference to the current object
    class Iterator
    {
    public:
        Iterator() : _pObj{nullptr} {}  // End iterator
        Iterator(libnl::nl_cache *pCache) : _pObj{libnl::nl_cache_get_first(pCache)} {}

    public:
        bool operator==(const Iterator &other) const {return _pObj == other._pObj;}
        bool operator!=(const Iterator &other) const {return !(*this == other);}

        Iterator &operator++();
        libnl::nl_object *operator*() const {return _pObj;}

    private:
        libnl::nl_object *_pObj;
    };

public:
    // This constructor is normally called by LinuxNlNtfSock::addCache().
    //
    // Create LinuxNlCache with:
    // - the netlink family for the connection (such as NETLINK_ROUTE)
    // - the libnl object type (such as "route/addr")
    //
    // The appropriate broadcast groups should have been added to a
    // notification socket before this call, so updates can be delivered to
    // cache_include().
    //
    // LinuxNlCache will throw if it can't allocate the cache or socket.
    LinuxNlCache(int netlinkFamily, const char *libnlObjType);
    ~LinuxNlCache();

private:
    LinuxNlCache(const LinuxNlCache &) = delete;
    LinuxNlCache &operator=(const LinuxNlCache &) = delete;

public:
    // Apply an update with ::nl_cache_include().  Normally called by
    // LinuxNlNtfSock.  pObj must be valid.
    void cache_include(libnl::nl_object *pObj);

    // Refill the cache - request a new dump from the kernel.  Called
    // automatically in constructor
    void refill();

    // Get the count of items in the cache
    std::size_t count();

    Iterator begin() {Q_ASSERT(_pCache); return {_pCache.get()};}
    Iterator end() {return {};}

    // Provide this cache for shared use in libnl.  This remains in effect until
    // LinuxNlCache is destroyed.
    // Some libnl features require shared caches of specific types.
    void provide();

    // Dump summary of cache to qInfo()
    void dumpSummary() const;

    // Get the raw nl_cache* - for use with type-specific cache operations when
    // you know the type of the cache.
    // The nl_cache* is still owned by LinuxNlCache and isn't retained by this
    // call.
    libnl::nl_cache *get() {Q_ASSERT(_pCache); return _pCache.get();}

private:
    // The libnl cache object - always valid.
    NlUniquePtr<libnl::nl_cache> _pCache;
    // Whether the cache has been provided as a shared cache (with provide())
    bool _provided;
    int _netlinkFamily;
};


#endif
