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

#pragma once
#include <kapps_core/src/winapi.h>
#include <kapps_core/src/win/win_linkreader.h>
#include <kapps_core/src/ipaddress.h>
#include "../firewall.h"
#include "appidkey.h"
#include <kapps_core/core.h>
#include <kapps_net/net.h>
#include <kapps_core/logger.h>
#include <optional>

namespace kapps { namespace net {

class FirewallFilter;
class Callout;
class ProviderContext;

extern const GUID zeroGuid;
extern FWPM_SUBLAYER g_wfpSublayer;

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
class FirewallEngine
{
public:
    // Create FirewallEngine with the BrandInfo from the firewall configuration;
    // the layer GUIDs and display data will be used to create all firewall
    // rules.
    FirewallEngine(const BrandInfo &brandInfo);
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

struct CalloutFilter : public FirewallFilter
{
    CalloutFilter(const GUID &calloutKey, const GUID &layerKey, uint8_t weight = 10)
    {
        this->layerKey = layerKey;
        this->action.type = FWP_ACTION_CALLOUT_TERMINATING;
        this->action.calloutKey = calloutKey;
        this->subLayerKey = g_wfpSublayer.subLayerKey;
        this->weight.uint8 = weight;
    }
};

struct IpInboundFilter : public CalloutFilter
{
    IpInboundFilter(const GUID &calloutKey, const GUID &provCtxt, uint8_t weight = 10) : CalloutFilter(calloutKey, FWPM_LAYER_INBOUND_IPPACKET_V4, weight)
    {
        if(provCtxt != zeroGuid)
        {
            flags |= FWPM_FILTER_FLAG_HAS_PROVIDER_CONTEXT;
            providerContextKey = provCtxt;
        }
    }
};

struct IpOutboundFilter : public CalloutFilter
{
    IpOutboundFilter(const GUID &calloutKey, const GUID &provCtxt, uint8_t weight = 10) : CalloutFilter(calloutKey, FWPM_LAYER_OUTBOUND_IPPACKET_V4, weight)
    {
        if(provCtxt != zeroGuid)
        {
            flags |= FWPM_FILTER_FLAG_HAS_PROVIDER_CONTEXT;
            providerContextKey = provCtxt;
        }
    }
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
        assert(_conditionIndex < conditions.size());

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

    IPSubnetFilter(const core::Ipv4Address& addr, int prefix, uint8_t weight) : ConditionalFirewallFilter(weight)
    {
        address.addr = addr.address();
        address.mask = ~0UL << (32 - prefix);
        setCondition<FWP_V4_ADDR_MASK>(FWPM_CONDITION_IP_REMOTE_ADDRESS, FWP_MATCH_EQUAL, &address);
    }
    IPSubnetFilter(const std::string& subnet, uint8_t weight) : ConditionalFirewallFilter(weight)
    {
        // For consistency with prior QHostAddress implementation, treat
        // unparseable subnets as 0/32
        core::Ipv4Subnet subnetValue{{}, 32};
        try
        {
            subnetValue = core::Ipv4Subnet{subnet};
        }
        catch(const std::exception &ex)
        {
            KAPPS_CORE_WARNING() << "Unable to parse subnet" << subnet << "-"
                << ex.what();
        }
        address.addr = subnetValue.address().address();
        address.mask = ~0UL << (32 - subnetValue.prefix());
        setCondition<FWP_V4_ADDR_MASK>(FWPM_CONDITION_IP_REMOTE_ADDRESS, FWP_MATCH_EQUAL, &address);
    }
};

template<FWP_ACTION_TYPE action, FWP_DIRECTION direction>
struct IPSubnetFilter<action, direction, FWP_IP_VERSION_V6> : ConditionalFirewallFilter<1, action, direction, FWP_IP_VERSION_V6>
{
    FWP_V6_ADDR_AND_MASK address;

    IPSubnetFilter(const core::Ipv6Address& addr, int prefix, uint8_t weight) : ConditionalFirewallFilter(weight)
    {
        const auto &addrBytes{addr.address()};
        std::copy(std::begin(addrBytes), std::end(addrBytes),
            std::begin(address.addr));
        address.prefixLength = prefix;
        setCondition<FWP_V6_ADDR_MASK>(FWPM_CONDITION_IP_REMOTE_ADDRESS, FWP_MATCH_EQUAL, &address);
    }
    IPSubnetFilter(const std::string& subnet, uint8_t weight) : ConditionalFirewallFilter(weight)
    {
        // For consistency with prior QHostAddress implementation, treat
        // unparseable subnets as 0/128
        core::Ipv6Subnet subnetValue{{}, 128};
        try
        {
            subnetValue = core::Ipv6Subnet{subnet};
        }
        catch(const std::exception &ex)
        {
            KAPPS_CORE_WARNING() << "Unable to parse subnet" << subnet << "-"
                << ex.what();
        }
        const auto &addrBytes{subnetValue.address().address()};
        std::copy(std::begin(addrBytes), std::end(addrBytes),
            std::begin(address.addr));
        address.prefixLength = subnetValue.prefix();
        setCondition<FWP_V6_ADDR_MASK>(FWPM_CONDITION_IP_REMOTE_ADDRESS, FWP_MATCH_EQUAL, &address);
    }
};

// Filter to allow or block a single IP address
template<FWP_ACTION_TYPE action, FWP_DIRECTION direction, FWP_IP_VERSION ipVersion>
struct IPAddressFilter : public IPSubnetFilter<action, direction, ipVersion>
{
    IPAddressFilter(const core::Ipv4Address& addr, uint8_t weight = 10) : IPSubnetFilter(addr, ipVersion == FWP_IP_VERSION_V6 ? 128 : 32, weight) {}
    IPAddressFilter(const core::Ipv6Address& addr, uint8_t weight = 10) : IPSubnetFilter(addr, ipVersion == FWP_IP_VERSION_V6 ? 128 : 32, weight) {}

    // Unused
    //IPAddressFilter(const std::string& addr, uint8_t weight = 10) : IPSubnetFilter(QHostAddress(addr), ipVersion == FWP_IP_VERSION_V6 ? 128 : 32, weight) {}
};

// Filter to allow or block loopback traffic
template<FWP_ACTION_TYPE action, FWP_DIRECTION direction, FWP_IP_VERSION ipVersion>
struct LocalhostFilter;
template<FWP_ACTION_TYPE action, FWP_DIRECTION direction>
struct LocalhostFilter<action, direction, FWP_IP_VERSION_V4>
    : public IPAddressFilter<action, direction, FWP_IP_VERSION_V4>
{
    LocalhostFilter(uint8_t weight = 10) : IPAddressFilter(core::Ipv4Address{127,0,0,1}, weight) {}
};
template<FWP_ACTION_TYPE action, FWP_DIRECTION direction>
struct LocalhostFilter<action, direction, FWP_IP_VERSION_V6>
    : public IPAddressFilter<action, direction, FWP_IP_VERSION_V6>
{
    LocalhostFilter(uint8_t weight = 10) : IPAddressFilter(core::Ipv6Address{0,0,0,0,0,0,0,1}, weight) {}
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

// Filter to invoke a callout for DNS traffic
template<FWP_IP_VERSION ipVersion>
struct CalloutDNSFilter : public DNSFilter<FWP_ACTION_CALLOUT_TERMINATING, ipVersion>
{
    CalloutDNSFilter(const GUID &calloutKey, const GUID &layerKey, uint8_t weight = 10)
        : DNSFilter{weight}
    {
        this->layerKey = layerKey;
        this->action.type = FWP_ACTION_CALLOUT_TERMINATING;
        this->action.calloutKey = calloutKey;
        this->subLayerKey = g_wfpSublayer.subLayerKey;
        this->weight.uint8 = weight;
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
    AppIdKey _appId;

    template <typename...ConditionTypes>
    // Ensure caller passes in wide strings
    ApplicationFilter(const std::wstring& applicationPath, uint8_t weight = 10,
                      ConditionTypes&& ...inlineConditions)
        : ConditionalFirewallFilter(weight, std::forward<ConditionTypes>(inlineConditions)...),
          _appId{applicationPath}
    {
        setCondition<FWP_BYTE_BLOB_TYPE>(FWPM_CONDITION_ALE_APP_ID, FWP_MATCH_EQUAL, _appId.data());
    }
};

// Filter to allow or block an application
template<FWP_ACTION_TYPE action, FWP_DIRECTION direction, FWP_IP_VERSION ipVersion>
struct AppDNSFilter : public ConditionalFirewallFilter<2, action, direction, ipVersion>
{
    template <typename...ConditionTypes>
    AppDNSFilter(const AppIdKey &appId, uint8_t weight = 10, ConditionTypes&& ...inlineConditions) : ConditionalFirewallFilter(weight, std::forward<ConditionTypes>(inlineConditions)...)
    {
        assert(!appId.empty());
        setCondition<FWP_UINT16>(FWPM_CONDITION_IP_REMOTE_PORT, FWP_MATCH_EQUAL, 53);
        setCondition<FWP_BYTE_BLOB_TYPE>(FWPM_CONDITION_ALE_APP_ID, FWP_MATCH_EQUAL, appId.data());
    }
};

template<FWP_IP_VERSION ipVersion, FWP_ACTION_TYPE action=FWP_ACTION_PERMIT>
struct AppIdFilter : public ConditionalFirewallFilter<1, action, FWP_DIRECTION_OUTBOUND, ipVersion>
{
    // Create an AppIdFilter with a valid app ID (appId must not be empty)
    AppIdFilter(const AppIdKey &appId, uint8_t weight = 10) : ConditionalFirewallFilter(weight)
    {
        assert(!appId.empty());
        setCondition<FWP_BYTE_BLOB_TYPE>(FWPM_CONDITION_ALE_APP_ID, FWP_MATCH_EQUAL, appId.data());
    }
};

template<FWP_IP_VERSION ipVersion>
struct SplitFilter : public AppIdFilter<ipVersion>
{
    // The split filter rule causes the callout to be invoked for the specified
    // app.  Provide the GUID of the callout object that will be invoked.
    SplitFilter(const AppIdKey &appId, const GUID &calloutKey,
                const GUID &layerKey, const GUID &provCtxt,
                FWP_ACTION_TYPE action, uint8_t weight)
        : AppIdFilter(appId, weight)
    {
        this->layerKey = layerKey;
        this->action.type = action;
        this->action.calloutKey = calloutKey;
        this->subLayerKey = g_wfpSublayer.subLayerKey;
        this->flags |= FWPM_FILTER_FLAG_HAS_PROVIDER_CONTEXT;

        this->providerContextKey = provCtxt;
    }
};

// Filter to allow or block everything
template<FWP_ACTION_TYPE action, FWP_DIRECTION direction, FWP_IP_VERSION ipVersion>
using EverythingFilter = BasicFirewallFilter<action, direction, ipVersion>;

}}
