// Copyright (c) 2020 Private Internet Access, Inc.
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
#line HEADER_FILE("win/win_firewall.h")

#ifndef WIN_FIREWALL_H
#define WIN_FIREWALL_H
#pragma once

#include "win.h"
#include "win/win_linkreader.h"
#include "win/win_util.h"
#include <QHostAddress>
#include <QObject>
#include <QStringView>
#include <optional>

class FirewallFilter;
class Callout;
class ProviderContext;

extern GUID zeroGuid;

// Identifier types for different WFP objects.
// WFP uses GUID for everything, so having separate types for these objects
// helps ensure that we know what type of object they refer to, prevents us from
// mixing them up, etc.  Most of the operations are different for different
// object types; in particular the "delete" functions are all separate.
//
// These are different from the FirewallFilter, Callout, and ProviderContext
// types; those are definitions for WFP objects that we can create - these
// represent actual WFP objects that have been created.
//
// In the future, these might provide object-specific operations (like "remove"
// or the "activate/deactivate" macros), but for now they just contain the
// identifiers.
class WfpFilterObject : public GUID {};
class WfpCalloutObject : public GUID {};
class WfpProviderContextObject : public GUID {};

/**
 * @brief The FirewallEngine class is the main interface to WFP and lets
 * us add or remove various FirewallFilter rules. We can keep the same
 * instance throughout the lifetime of the daemon.
 */
class FirewallEngine : public QObject
{
    Q_OBJECT
public:
    explicit FirewallEngine(QObject* parent = nullptr);
    ~FirewallEngine();

    bool open();

    bool installProvider();
    bool uninstallProvider();

    WfpFilterObject add(const FirewallFilter& filter);
    WfpCalloutObject add(const Callout& mCallout);
    WfpProviderContextObject add(const ProviderContext& providerContext);
    bool remove(const WfpFilterObject &filter);
    bool remove(const WfpCalloutObject &callout);
    bool remove(const WfpProviderContextObject &providerContext);

    bool removeAll();
    bool removeAll(const GUID& layerKey);

private:
    template<class ObjectT, class TemplateT, class CreateEnumHandleFuncT,
             class EnumFuncT, class DestroyEnumHandleFuncT,
             class ActionFuncT>
    bool enumObjects(const TemplateT &search,
                     CreateEnumHandleFuncT createEnumHandleFunc,
                     EnumFuncT enumFunc, DestroyEnumHandleFuncT destroyFunc,
                     ActionFuncT actionFunc);
    template<class ActionFuncT>
    bool enumFilters(const GUID &layerKey, ActionFuncT actionFunc);
    bool removeFilters(const GUID& layerKey);
    template<class ActionFuncT>
    bool enumCallouts(const GUID &layerKey, ActionFuncT actionFunc);
    bool removeCallouts(const GUID& layerKey);
    template<class ActionFuncT>
    bool enumProviderContexts(ActionFuncT actionFunc);
    bool removeProviderContexts();

public:
    // Verify that no WFP objects were leaked; used by destructor for
    // diagnostics.
    void checkLeakedLayerObjects(const GUID &layerKey);
    void checkLeakedObjects();

public:
    HANDLE _handle;

private:
    friend class FirewallTransaction;
};


/**
 * @brief The FirewallTransaction class is a thin RAII wrapper around a WFP
 * transaction; changes made to a FirewallEngine instance during a transaction
 * behave atomically and can be aborted.
 */
class FirewallTransaction
{
public:
    FirewallTransaction(FirewallEngine* firewall);
    ~FirewallTransaction();

    void commit();
    void abort();

private:
    HANDLE _handle;
};


/**
 * @brief The FirewallFilter class encapsulates a single WFP filter,
 * and is usually instantiated via a subclass.
 */
struct FirewallFilter : public FWPM_FILTER
{
public:
    typedef UINT64 Id;
public:
    FirewallFilter();
    FirewallFilter(const FirewallFilter&) = delete;
    FirewallFilter(FirewallFilter&&) = delete;
    operator const GUID&() const { return this->filterKey; }
    operator const Id&() const { return this->filterId; }
};

struct ProviderContext : public FWPM_PROVIDER_CONTEXT
{
    FWP_BYTE_BLOB blob;
public:
    typedef UINT64 Id;
public:
    ProviderContext(void *contextData, UINT32 dataSize);
    ProviderContext(const ProviderContext&) = delete;
    ProviderContext(ProviderContext&&) = delete;
    operator const GUID&() const { return this->providerContextKey; }
    operator const Id&() const { return this->providerContextId; }
};

/** The Callout class is a user-mode representation of a Callout driver.
 *  The calloutKey parameter must match the calloutKey found in the kernel-mode driver
 *  The applicableLayer parameter determines at which ALE layer this driver will be available
 */
 struct Callout : public FWPM_CALLOUT
{
public:
    typedef UINT32 Id;
public:
    Callout(const GUID& applicableLayer, const GUID& calloutKey);
    Callout(const Callout&) = delete;
    Callout(Callout&&) = delete;
    operator const GUID&() const { return this->calloutKey; }
    operator const Id&() const { return this->calloutId; }
};

// Implementation details for template filter classes below
namespace impl {

template<FWP_DATA_TYPE TYPE> struct FWP_DATA_Accessor { };
#define DECLARE_VALUE_OVERLOAD(id, member) template<> struct FWP_DATA_Accessor<id> { typedef decltype(FWP_CONDITION_VALUE::member) type; static inline type& get(FWP_CONDITION_VALUE& value) { return value.member; } };
DECLARE_VALUE_OVERLOAD(FWP_UINT8, uint8)
DECLARE_VALUE_OVERLOAD(FWP_UINT16, uint16)
DECLARE_VALUE_OVERLOAD(FWP_UINT32, uint32)
DECLARE_VALUE_OVERLOAD(FWP_UINT64, uint64)
DECLARE_VALUE_OVERLOAD(FWP_INT8, int8)
DECLARE_VALUE_OVERLOAD(FWP_INT16, int16)
DECLARE_VALUE_OVERLOAD(FWP_INT32, int32)
DECLARE_VALUE_OVERLOAD(FWP_INT64, int64)
DECLARE_VALUE_OVERLOAD(FWP_FLOAT, float32)
DECLARE_VALUE_OVERLOAD(FWP_DOUBLE, double64)
DECLARE_VALUE_OVERLOAD(FWP_BYTE_ARRAY16_TYPE, byteArray16)
DECLARE_VALUE_OVERLOAD(FWP_BYTE_BLOB_TYPE, byteBlob)
DECLARE_VALUE_OVERLOAD(FWP_SID, sid)
DECLARE_VALUE_OVERLOAD(FWP_SECURITY_DESCRIPTOR_TYPE, sd)
DECLARE_VALUE_OVERLOAD(FWP_TOKEN_INFORMATION_TYPE, tokenInformation)
DECLARE_VALUE_OVERLOAD(FWP_TOKEN_ACCESS_INFORMATION_TYPE, tokenAccessInformation)
DECLARE_VALUE_OVERLOAD(FWP_UNICODE_STRING_TYPE, unicodeString)
DECLARE_VALUE_OVERLOAD(FWP_BYTE_ARRAY6_TYPE, byteArray6)
DECLARE_VALUE_OVERLOAD(FWP_V4_ADDR_MASK, v4AddrMask)
DECLARE_VALUE_OVERLOAD(FWP_V6_ADDR_MASK, v6AddrMask)
DECLARE_VALUE_OVERLOAD(FWP_RANGE_TYPE, rangeValue)
#undef DECLARE_VALUE_OVERLOAD

}

// Represents the data for a WFP filter condition
template <FWP_DATA_TYPE dataType>
struct Condition
{
    using ValueType = typename impl::FWP_DATA_Accessor<dataType>::type;

    GUID fieldKey;
    FWP_MATCH_TYPE matchType;
    ValueType value;

    // Available at compile-time
    static constexpr FWP_DATA_TYPE _dataType = dataType;
};

// Base class for a block/allow incoming/outgoing IPv4/IPv6 filter
template<FWP_ACTION_TYPE actionType, FWP_DIRECTION direction, FWP_IP_VERSION ipVersion>
struct BasicFirewallFilter : public FirewallFilter
{
    BasicFirewallFilter(uint8_t weight = 10)
    {
        if (direction == FWP_DIRECTION_INBOUND)
            this->layerKey = (ipVersion == FWP_IP_VERSION_V6) ? FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6 : FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4;
        else if (direction == FWP_DIRECTION_OUTBOUND)
            this->layerKey = (ipVersion == FWP_IP_VERSION_V6) ? FWPM_LAYER_ALE_AUTH_CONNECT_V6 : FWPM_LAYER_ALE_AUTH_CONNECT_V4;
        this->action.type = actionType;
        this->weight.uint8 = weight;
    }
};

// Base class for a basic filter with a number of conditions on it
template<UINT32 conditionCount, FWP_ACTION_TYPE action, FWP_DIRECTION direction, FWP_IP_VERSION ipVersion>
struct ConditionalFirewallFilter : BasicFirewallFilter<action, direction, ipVersion>
{
    std::vector<FWPM_FILTER_CONDITION> conditions;

    template <typename... ConditionTypes>
    ConditionalFirewallFilter(uint8_t weight = 10, ConditionTypes&& ...inlineConditions)
        : _conditionIndex{0},
        BasicFirewallFilter(weight)
    {
        auto totalConditionCount{conditionCount + sizeof...(inlineConditions)};
        conditions.resize(totalConditionCount);
        processInlineConditions(std::forward<ConditionTypes>(inlineConditions)...);

        this->numFilterConditions = totalConditionCount;
        this->filterCondition = conditions.empty() ? nullptr : conditions.data();
    }

    template<FWP_DATA_TYPE dataType>
    void setCondition(const GUID& fieldKey, FWP_MATCH_TYPE matchType, const typename impl::FWP_DATA_Accessor<dataType>::type& value)
    {
        Q_ASSERT(_conditionIndex < conditions.size());

        conditions[_conditionIndex].fieldKey = fieldKey;
        conditions[_conditionIndex].matchType = matchType;
        conditions[_conditionIndex].conditionValue.type = dataType;
        impl::FWP_DATA_Accessor<dataType>::get(conditions[_conditionIndex].conditionValue) = value;
        ++_conditionIndex;
    }

    private:

    template <typename T, typename...ConditionTypes>
    void processInlineConditions(T&& firstArg, ConditionTypes&& ...args)
    {
        setCondition<firstArg._dataType>(firstArg.fieldKey, firstArg.matchType, firstArg.value);
        processInlineConditions(std::forward<ConditionTypes>(args)...);
    }

    // Base case for variadic recursion
    void processInlineConditions() {}

    // Keep track of current condition index
    UINT32 _conditionIndex;
};

// Filter to allow or block a certain IP range
template<FWP_ACTION_TYPE action, FWP_DIRECTION direction, FWP_IP_VERSION ipVersion> struct IPSubnetFilter;
template<FWP_ACTION_TYPE action, FWP_DIRECTION direction>
struct IPSubnetFilter<action, direction, FWP_IP_VERSION_V4> : ConditionalFirewallFilter<1, action, direction, FWP_IP_VERSION_V4>
{
    FWP_V4_ADDR_AND_MASK address;

    IPSubnetFilter(const QHostAddress& addr, int prefix = 32, uint8_t weight = 10) : ConditionalFirewallFilter(weight)
    {
        address.addr = addr.toIPv4Address();
        address.mask = ~0UL << (32 - prefix);
        setCondition<FWP_V4_ADDR_MASK>(FWPM_CONDITION_IP_REMOTE_ADDRESS, FWP_MATCH_EQUAL, &address);
    }
    IPSubnetFilter(const QString& subnet, uint8_t weight = 10) : ConditionalFirewallFilter(weight)
    {
        QPair<QHostAddress, int> pair = QHostAddress::parseSubnet(subnet);
        address.addr = pair.first.toIPv4Address();
        address.mask = ~0UL << (32 - pair.second);
        setCondition<FWP_V4_ADDR_MASK>(FWPM_CONDITION_IP_REMOTE_ADDRESS, FWP_MATCH_EQUAL, &address);
    }
};

template<FWP_ACTION_TYPE action, FWP_DIRECTION direction>
struct IPSubnetFilter<action, direction, FWP_IP_VERSION_V6> : ConditionalFirewallFilter<1, action, direction, FWP_IP_VERSION_V6>
{
    FWP_V6_ADDR_AND_MASK address;

    IPSubnetFilter(const QHostAddress& addr, int prefix = 128, uint8_t weight = 10) : ConditionalFirewallFilter(weight)
    {
        *(Q_IPV6ADDR*)address.addr = addr.toIPv6Address();
        address.prefixLength = prefix;
        setCondition<FWP_V6_ADDR_MASK>(FWPM_CONDITION_IP_REMOTE_ADDRESS, FWP_MATCH_EQUAL, &address);
    }
    IPSubnetFilter(const QString& subnet, uint8_t weight = 10) : ConditionalFirewallFilter(weight)
    {
        QPair<QHostAddress, int> pair = QHostAddress::parseSubnet(subnet);
        *(Q_IPV6ADDR*)address.addr = pair.first.toIPv6Address();
        address.prefixLength = pair.second;
        setCondition<FWP_V6_ADDR_MASK>(FWPM_CONDITION_IP_REMOTE_ADDRESS, FWP_MATCH_EQUAL, &address);
    }
};

// Filter to allow or block a single IP address
template<FWP_ACTION_TYPE action, FWP_DIRECTION direction, FWP_IP_VERSION ipVersion>
struct IPAddressFilter : public IPSubnetFilter<action, direction, ipVersion>
{
    IPAddressFilter(const QHostAddress& addr, uint8_t weight = 10) : IPSubnetFilter(addr, ipVersion == FWP_IP_VERSION_V6 ? 128 : 32, weight) {}
    IPAddressFilter(const QString& addr, uint8_t weight = 10) : IPSubnetFilter(QHostAddress(addr), ipVersion == FWP_IP_VERSION_V6 ? 128 : 32, weight) {}
};

// Filter to allow or block loopback traffic
template<FWP_ACTION_TYPE action, FWP_DIRECTION direction, FWP_IP_VERSION ipVersion>
struct LocalhostFilter : public IPAddressFilter<action, direction, ipVersion>
{
    LocalhostFilter(uint8_t weight = 10) : IPAddressFilter(ipVersion == FWP_IP_VERSION_V6 ? QHostAddress::LocalHostIPv6 : QHostAddress::LocalHost, weight) {}
};

// Filter to allow or block DHCP traffic
template<FWP_ACTION_TYPE action, FWP_IP_VERSION ipVersion> struct DHCPFilter;
template<FWP_ACTION_TYPE action>
struct DHCPFilter<action, FWP_IP_VERSION_V4> : public ConditionalFirewallFilter<3, action, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V4>
{
    FWP_V4_ADDR_AND_MASK address;

    DHCPFilter(uint8_t weight = 10) : ConditionalFirewallFilter(weight)
    {
        setCondition<FWP_UINT16>(FWPM_CONDITION_IP_LOCAL_PORT, FWP_MATCH_EQUAL, 68);
        setCondition<FWP_UINT16>(FWPM_CONDITION_IP_REMOTE_PORT, FWP_MATCH_EQUAL, 67);
        // 255.255.255.255/32
        address.addr = 0xFFFFFFFFu;
        address.mask = 0xFFFFFFFFu;
        setCondition<FWP_V4_ADDR_MASK>(FWPM_CONDITION_IP_REMOTE_ADDRESS, FWP_MATCH_EQUAL, &address);
    }
};
template<FWP_ACTION_TYPE action>
struct DHCPFilter<action, FWP_IP_VERSION_V6> : public ConditionalFirewallFilter<3, action, FWP_DIRECTION_OUTBOUND, FWP_IP_VERSION_V6>
{
    FWP_V6_ADDR_AND_MASK address;

    DHCPFilter(uint8_t weight = 10) : ConditionalFirewallFilter(weight)
    {
        setCondition<FWP_UINT16>(FWPM_CONDITION_IP_LOCAL_PORT, FWP_MATCH_EQUAL, 546);
        setCondition<FWP_UINT16>(FWPM_CONDITION_IP_REMOTE_PORT, FWP_MATCH_EQUAL, 547);
        // ff00::/8
        ZeroMemory(&address.addr, sizeof(address.addr));
        address.addr[0] = 0xFFu;
        address.prefixLength = 8;
        setCondition<FWP_V6_ADDR_MASK>(FWPM_CONDITION_IP_REMOTE_ADDRESS, FWP_MATCH_EQUAL, &address);
    }
};

// Filter to allow or block DNS traffic
template<FWP_ACTION_TYPE action, FWP_IP_VERSION ipVersion>
struct DNSFilter : public ConditionalFirewallFilter<1, action, FWP_DIRECTION_OUTBOUND, ipVersion>
{
    DNSFilter(uint8_t weight = 10) : ConditionalFirewallFilter(weight)
    {
        setCondition<FWP_UINT16>(FWPM_CONDITION_IP_REMOTE_PORT, FWP_MATCH_EQUAL, 53);
    }
};

// Filter to allow or block an interface
template<FWP_ACTION_TYPE action, FWP_DIRECTION direction, FWP_IP_VERSION ipVersion>
struct InterfaceFilter : public ConditionalFirewallFilter<1, action, direction, ipVersion>
{
    UINT64 interfaceLuid;

    InterfaceFilter(UINT64 interfaceLuid, uint8_t weight = 10) : ConditionalFirewallFilter(weight)
    {
        this->interfaceLuid = interfaceLuid;
        setCondition<FWP_UINT64>(FWPM_CONDITION_IP_LOCAL_INTERFACE, FWP_MATCH_EQUAL, &this->interfaceLuid);
    }
};

// Filter to allow or block an application
template<FWP_ACTION_TYPE action, FWP_DIRECTION direction, FWP_IP_VERSION ipVersion>
struct ApplicationFilter : public ConditionalFirewallFilter<1, action, direction, ipVersion>
{
    FWP_BYTE_BLOB* applicationBlob;

    template <typename...ConditionTypes>
    ApplicationFilter(const QString& applicationPath, uint8_t weight = 10, ConditionTypes&& ...inlineConditions) : ConditionalFirewallFilter(weight, std::forward<ConditionTypes>(inlineConditions)...)
    {
        if (DWORD error = FwpmGetAppIdFromFileName(qUtf16Printable(applicationPath), &applicationBlob))
        {
            qCritical() << SystemError(HERE, error);
            applicationBlob = NULL;
            // Rely on the filter addition failing later
        }
        setCondition<FWP_BYTE_BLOB_TYPE>(FWPM_CONDITION_ALE_APP_ID, FWP_MATCH_EQUAL, applicationBlob);
    }
    ~ApplicationFilter()
    {
        if (applicationBlob)
        {
            FwpmFreeMemory((void**)&applicationBlob);
        }
    }
};

class WinLinkReader;

// A WFP app ID that can be used as a key in containers.
class AppIdKey
{
public:
    AppIdKey() : _pBlob{} {} // Empty by default
    // Load the app ID for an app, which can be a shortcut or executable path.
    // Results in an empty AppIdKey if it can't be loaded.
    // The caller must try to load WinLinkReader and provide it if it was
    // loaded.  If pReader is nullptr, AppIdKey won't be able to load the app ID
    // for a shortcut.
    // The target executable path can optionally be returned; see reset().
    explicit AppIdKey(WinLinkReader *pReader, const QString &appPath, std::wstring *pTarget = nullptr)
        : AppIdKey{} {reset(pReader, appPath, pTarget);}
    AppIdKey(AppIdKey &&other) : AppIdKey{} {*this = std::move(other);}
    ~AppIdKey() {clear();}

public:
    AppIdKey &operator=(AppIdKey &&other)
    {
        std::swap(_pBlob, other._pBlob);
        return *this;
    }

    bool operator==(const AppIdKey &other) const;
    bool operator!=(const AppIdKey &other) const {return !(*this == other);}
    bool operator<(const AppIdKey &other) const;

    // Test if AppIdKey is equal to a copy made with copyData().  Note that both
    // "null" and "empty" AppIdKeys could equal the same QByteArray.
    bool operator==(const QByteArray &value) const;

    explicit operator bool() const {return _pBlob;}
    bool operator!() const {return empty();}

    void swap(AppIdKey &other) {std::swap(_pBlob, other._pBlob);}

    bool empty() const {return !_pBlob;}
    // data() returns a mutable FWP_BYTE_BLOB* since the corresponding member of
    // FWP_CONDITION_VALUE is not const
    FWP_BYTE_BLOB *data() const {return _pBlob;}
    void clear();
    // Load the app ID for an app.  If it can't be loaded, AppIdKey becomes
    // empty.
    // Like the constructor, the caller must try to load WinLinkReader and
    // provide it if it was loaded.
    // Optionally, if the target executable path is needed, a std::wstring can
    // be passed to pTarget.  If an app ID is found, this is populated with the
    // target executable path (the link target for a link or appPath otherwise).
    void reset(WinLinkReader *pReader, const QString &appPath, std::wstring *pTarget = nullptr);

    // Copy the data from this app ID to a QByteArray.
    QByteArray copyData() const;

    // App ID as a QStringView that can be traced.  Note that this usually
    // includes a terminating null character, since WFP usually includes one.
    // (The final null is preserved to avoid introducing ambiguity in traces.)
    QStringView traceString() const;

    // QStringView of the data as a string that can be inserted into a WFP
    // command, etc.  (This strips any trailing null characters.)
    QStringView printableString() const;

    std::size_t hash() const;

    void trace(QDebug &dbg) const;

private:
    FWP_BYTE_BLOB *_pBlob;
};

namespace std
{
    template<>
    struct hash<AppIdKey>
    {
        std::size_t operator()(const AppIdKey &appId) const {return appId.hash();}
    };
}

inline QDebug &operator<<(QDebug &dbg, const AppIdKey &appId)
{
    appId.trace(dbg);
    return dbg;
}

template<FWP_IP_VERSION ipVersion, FWP_ACTION_TYPE action=FWP_ACTION_PERMIT>
struct AppIdFilter : public ConditionalFirewallFilter<1, action, FWP_DIRECTION_OUTBOUND, ipVersion>
{
    // Create an AppIdFilter with a valid app ID (appId must not be empty)
    AppIdFilter(const AppIdKey &appId, uint8_t weight = 10) : ConditionalFirewallFilter(weight)
    {
        Q_ASSERT(!appId.empty());
        setCondition<FWP_BYTE_BLOB_TYPE>(FWPM_CONDITION_ALE_APP_ID, FWP_MATCH_EQUAL, appId.data());
    }
};

template<FWP_IP_VERSION ipVersion>
struct SplitFilter : public AppIdFilter<ipVersion>
{
    // The split filter rule causes the callout to be invoked for the specified
    // app.  Provide the GUID of the callout object that will be invoked.
    SplitFilter(const AppIdKey &appId, const GUID &calloutKey,
                const GUID &layerKey, const GUID &provCtxt, uint8_t weight = 10)
        : AppIdFilter(appId, weight)
    {
        this->layerKey = layerKey;
        this->action.type = FWP_ACTION_CALLOUT_TERMINATING;
        this->action.calloutKey = calloutKey;
        this->subLayerKey = FWPM_SUBLAYER_UNIVERSAL; // should we introduce our own sublayer?
        this->flags |= FWPM_FILTER_FLAG_HAS_PROVIDER_CONTEXT;

        this->providerContextKey = provCtxt;
    }
};

// Filter to allow or block everything
template<FWP_ACTION_TYPE action, FWP_DIRECTION direction, FWP_IP_VERSION ipVersion>
using EverythingFilter = BasicFirewallFilter<action, direction, ipVersion>;

#endif // WIN_FIREWALL_H
