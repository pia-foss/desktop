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

#include "wfp_firewall.h"
#include <kapps_core/src/win/win_error.h>
#include <kapps_core/src/uuid.h>

#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "rpcrt4.lib")

namespace kapps { namespace net {

// TODO - Move these into FirewallEngine and rework the way filter objects
// are made:
// - eliminate activate/deactivate macros, make these proper owning objects
// - build invalidation into the objects, invalidation logic in WinFirewall
//   is fragile and complex
namespace
{
    const DWORD DEFAULT_FILTER_FLAGS = FWPM_FILTER_FLAG_PERSISTENT | (IsWindows8OrGreater() ? FWPM_FILTER_FLAG_INDEXED : 0);

    FWPM_PROVIDER g_wfpProvider = {
        GUID{}, { nullptr, nullptr },   // Initialized by WinFirewall::WinFirewall
        FWPM_PROVIDER_FLAG_PERSISTENT,
        { 0, NULL },
        NULL
    };

    FWPM_DISPLAY_DATA g_wfpProviderCtxDisplay{};
    FWPM_DISPLAY_DATA g_wfpCalloutDisplay{};
}

// TODO - zeroGuid and g_wfpSublayer are declared in wfp_firewall.h due to use
// from inline ctors, move all this too
const GUID zeroGuid = {0};
FWPM_SUBLAYER g_wfpSublayer = {
    GUID{}, { nullptr, nullptr },   // Initialized by WinFirewall::WinFirewall
    FWPM_SUBLAYER_FLAG_PERSISTENT,
    &g_wfpProvider.providerKey,
    { 0, NULL },
    9000
};


FirewallFilter::FirewallFilter()
{
    memset(static_cast<FWPM_FILTER*>(this), 0, sizeof(FWPM_FILTER));
    UuidCreate(&filterKey);
    displayData = g_wfpSublayer.displayData;
    flags = DEFAULT_FILTER_FLAGS;
    providerKey = &g_wfpProvider.providerKey;
    subLayerKey = g_wfpSublayer.subLayerKey;
    weight.type = FWP_UINT8;
}

ProviderContext::ProviderContext(void *pContextData, UINT32 dataSize)
{
    memset(static_cast<FWPM_PROVIDER_CONTEXT*>(this), 0, sizeof(FWPM_PROVIDER_CONTEXT));
    UuidCreate(&this->providerContextKey);
    displayData = g_wfpProviderCtxDisplay;
    providerKey = &g_wfpProvider.providerKey;
    blob.data = static_cast<UINT8*>(pContextData);
    blob.size = dataSize;

    flags = FWPM_PROVIDER_CONTEXT_FLAG_PERSISTENT;
    type = FWPM_GENERAL_CONTEXT;
    dataBuffer = &blob;
}

Callout::Callout(const GUID& applicableLayer, const GUID& calloutKey)
{
    memset(static_cast<FWPM_CALLOUT*>(this), 0, sizeof(FWPM_CALLOUT));
    displayData = g_wfpCalloutDisplay;
    providerKey = &g_wfpProvider.providerKey;
    this->applicableLayer = applicableLayer;
    this->calloutKey = calloutKey;
    flags = FWPM_CALLOUT_FLAG_PERSISTENT | FWPM_CALLOUT_FLAG_USES_PROVIDER_CONTEXT;
}

FirewallEngine::FirewallEngine(const BrandInfo &brandInfo)
    : _handle{}
{
    g_wfpProvider.providerKey = brandInfo.wfpBrandProvider;
    g_wfpProvider.displayData.name = brandInfo.pWfpFilterName;
    g_wfpProvider.displayData.description = brandInfo.pWfpFilterDescription;

    g_wfpSublayer.subLayerKey = brandInfo.wfpBrandSublayer;
    g_wfpSublayer.displayData.name = brandInfo.pWfpFilterName;
    g_wfpSublayer.displayData.description = brandInfo.pWfpFilterDescription;

    g_wfpProviderCtxDisplay.name = brandInfo.pWfpProviderCtxName;
    g_wfpProviderCtxDisplay.description = brandInfo.pWfpProviderCtxDescription;

    g_wfpCalloutDisplay.name = brandInfo.pWfpCalloutName;
    g_wfpCalloutDisplay.description = brandInfo.pWfpCalloutDescription;
}

FirewallEngine::~FirewallEngine()
{
    if (_handle)
    {
        FwpmEngineClose(_handle);
        _handle = NULL;
    }
}

bool FirewallEngine::open()
{
    if (DWORD error = FwpmEngineOpen(NULL, RPC_C_AUTHN_DEFAULT, NULL, NULL, &_handle))
    {
        KAPPS_CORE_ERROR() << core::WinErrTracer{error};
        return false;
    }
    return true;
}

bool FirewallEngine::installProvider()
{
    FWPM_PROVIDER* provider;
    switch (DWORD error = FwpmProviderGetByKey(_handle, &g_wfpProvider.providerKey, &provider))
    {
    case ERROR_SUCCESS:
        break;
    case FWP_E_PROVIDER_NOT_FOUND:
        if (error = FwpmProviderAdd(_handle, &g_wfpProvider, NULL))
        {
            KAPPS_CORE_ERROR() << core::WinErrTracer{error};
            return false;
        }
        KAPPS_CORE_INFO() << "Installed WFP provider";
        break;
    default:
        KAPPS_CORE_ERROR() << core::WinErrTracer{error};
        return false;
    }
    FWPM_SUBLAYER* sublayer;
    switch (DWORD error = FwpmSubLayerGetByKey(_handle, &g_wfpSublayer.subLayerKey, &sublayer))
    {
    case ERROR_SUCCESS:
        break;
    case FWP_E_SUBLAYER_NOT_FOUND:
        if (error = FwpmSubLayerAdd(_handle, &g_wfpSublayer, NULL))
        {
            KAPPS_CORE_ERROR() << core::WinErrTracer{error};
            return false;
        }
        KAPPS_CORE_INFO() << "Installed WFP sublayer";
        break;
    default:
        KAPPS_CORE_ERROR() << core::WinErrTracer{error};
        return false;
    }
    return true;
}

bool FirewallEngine::uninstallProvider()
{
    bool result = true;
    switch (DWORD error = FwpmSubLayerDeleteByKey(_handle, &g_wfpSublayer.subLayerKey))
    {
    case ERROR_SUCCESS: KAPPS_CORE_INFO() << "Removed WFP sublayer"; break;
    case FWP_E_SUBLAYER_NOT_FOUND: break;
    default: KAPPS_CORE_ERROR() << core::WinErrTracer{error}; result = false; break;
    }
    switch (DWORD error = FwpmProviderDeleteByKey(_handle, &g_wfpProvider.providerKey))
    {
    case ERROR_SUCCESS: KAPPS_CORE_INFO() << "Removed WFP provider"; break;
    case FWP_E_PROVIDER_NOT_FOUND: break;
    default: KAPPS_CORE_ERROR() << core::WinErrTracer{error}; result = false; break;
    }
    return result;
}

WfpFilterObject FirewallEngine::add(const FirewallFilter& filter)
{
    UINT64 id = 0;
    if (DWORD error = FwpmFilterAdd(_handle, &filter, NULL, &id))
    {
        KAPPS_CORE_ERROR() << core::WinErrTracer{error};
        return {zeroGuid};
    }
    return {filter.filterKey};
}

WfpCalloutObject FirewallEngine::add(const Callout& mCallout)
{
    UINT32 id = 0;
    if (DWORD error = FwpmCalloutAdd(_handle, &mCallout, NULL, &id))
    {
        KAPPS_CORE_ERROR() << core::WinErrTracer{error};
        return {zeroGuid};
    }
    return {mCallout.calloutKey};
}

WfpProviderContextObject FirewallEngine::add(const ProviderContext& providerContext)
{
    UINT64 id = 0;
    if (DWORD error = FwpmProviderContextAdd(_handle, &providerContext, NULL, &id))
    {
        KAPPS_CORE_ERROR() << core::WinErrTracer{error};
        return {zeroGuid};
    }
    return {providerContext.providerContextKey};
}

bool FirewallEngine::remove(const WfpFilterObject &filter)
{
    if(DWORD error = FwpmFilterDeleteByKey(_handle, &filter))
    {
        KAPPS_CORE_ERROR() << core::WinErrTracer{error};
        return false;
    }
    return true;
}

bool FirewallEngine::remove(const WfpCalloutObject &callout)
{
    if(DWORD error = FwpmCalloutDeleteByKey(_handle, &callout))
    {
        KAPPS_CORE_ERROR() << core::WinErrTracer{error};
        return false;
    }
    return true;
}

bool FirewallEngine::remove(const WfpProviderContextObject &providerContext)
{
    if(DWORD error = FwpmProviderContextDeleteByKey(_handle, &providerContext))
    {
        KAPPS_CORE_ERROR() << core::WinErrTracer{error};
        return false;
    }
    return true;
}

bool FirewallEngine::removeAll()
{
    bool result = true;
    if (!removeAll(FWPM_LAYER_ALE_AUTH_CONNECT_V4)) result = false;
    if (!removeAll(FWPM_LAYER_ALE_AUTH_CONNECT_V6)) result = false;
    if (!removeAll(FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4)) result = false;
    if (!removeAll(FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6)) result = false;
    if (!removeAll(FWPM_LAYER_ALE_BIND_REDIRECT_V4)) result = false;
    if (!removeAll(FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4)) result = false;
    if (!removeAll(FWPM_LAYER_ALE_CONNECT_REDIRECT_V4)) result = false;
    if (!removeAll(FWPM_LAYER_INBOUND_IPPACKET_V4)) result = false;
    if (!removeAll(FWPM_LAYER_OUTBOUND_IPPACKET_V4)) result = false;
    if (!removeProviderContexts()) result = false;
    return result;
}

bool FirewallEngine::removeAll(const GUID &layerKey)
{
    bool result = true;
    if(!removeFilters(layerKey)) result = false;
    if(!removeCallouts(layerKey)) result = false;
    return result;
}

// Enumerate all WFP objects of a particular type.
//
// The WFP object enumeration APIs are all nearly identical, this function
// implements the enumeration algorithm.
//
// - ObjectT - the FWPM type representing the object
// - const TemplateT &search - the search template structure to use when creating the enum handle
// - CreateEnumHandleFuncT createEnumHandleFunc - WFP API to create the enum handle
// - EnumFuncT enumFunc - WFP API to enumerate objects
// - DestroyEnumHandleFuncT - WFP API to enumerate objects
// - ActionFuncT - Called for each object - functor that takes a const ObjectT &, returns bool
//
// Returns false if any actionFunc invocation returned false, true otherwise.
template<class ObjectT, class TemplateT, class CreateEnumHandleFuncT,
         class EnumFuncT, class DestroyEnumHandleFuncT,
         class ActionFuncT>
bool FirewallEngine::enumObjects(const TemplateT &search,
                                   CreateEnumHandleFuncT createEnumHandleFunc,
                                   EnumFuncT enumFunc,
                                   DestroyEnumHandleFuncT destroyFunc,
                                   ActionFuncT actionFunc)
{
    bool result = true;

    HANDLE enumHandle = NULL;
    if (DWORD error = createEnumHandleFunc(_handle, &search, &enumHandle))
    {
        KAPPS_CORE_ERROR() << core::WinErrTracer{error};
        return false;
    }

    ObjectT** entries;
    UINT32 count;
    do
    {
        if (DWORD error = enumFunc(_handle, enumHandle, 100, &entries, &count))
        {
            KAPPS_CORE_ERROR() << core::WinErrTracer{error};
            result = false;
            break;
        }
        for (UINT32 i = 0; i < count; i++)
        {
            if (entries[i] && !actionFunc(*entries[i]))
                result = false;
        }
        FwpmFreeMemory(reinterpret_cast<void**>(&entries));
    } while (count == 100);

    destroyFunc(_handle, enumHandle);

    return result;
}

template<class ActionFuncT>
bool FirewallEngine::enumFilters(const GUID& layerKey, ActionFuncT actionFunc)
{
    FWPM_FILTER_ENUM_TEMPLATE search = { 0 };
    search.providerKey = &g_wfpProvider.providerKey;
    search.layerKey = layerKey;
    search.enumType = FWP_FILTER_ENUM_OVERLAPPING;
    search.actionMask = 0xFFFFFFFF;

    return enumObjects<FWPM_FILTER>(search, &::FwpmFilterCreateEnumHandle,
                                    &::FwpmFilterEnum,
                                    &::FwpmFilterDestroyEnumHandle,
                                    std::move(actionFunc));
}

bool FirewallEngine::removeFilters(const GUID &layerKey)
{
    return enumFilters(layerKey, [this](const FWPM_FILTER &filter)
    {
        return remove(WfpFilterObject{filter.filterKey});
    });
}

template<class ActionFuncT>
bool FirewallEngine::enumCallouts(const GUID& layerKey, ActionFuncT actionFunc)
{
    FWPM_CALLOUT_ENUM_TEMPLATE search{};
    search.providerKey = &g_wfpProvider.providerKey;
    search.layerKey = layerKey;

    return enumObjects<FWPM_CALLOUT>(search, &::FwpmCalloutCreateEnumHandle,
                                     &::FwpmCalloutEnum,
                                     &::FwpmCalloutDestroyEnumHandle,
                                     std::move(actionFunc));
}

bool FirewallEngine::removeCallouts(const GUID& layerKey)
{
    return enumCallouts(layerKey, [this](const FWPM_CALLOUT &callout)
    {
        return remove(WfpCalloutObject{callout.calloutKey});
    });
}

template<class ActionFuncT>
bool FirewallEngine::enumProviderContexts(ActionFuncT actionFunc)
{
    FWPM_PROVIDER_CONTEXT_ENUM_TEMPLATE search{};
    search.providerKey = &g_wfpProvider.providerKey;

    return enumObjects<FWPM_PROVIDER_CONTEXT>(search,
                                              &::FwpmProviderContextCreateEnumHandle,
                                              &::FwpmProviderContextEnum,
                                              &::FwpmProviderContextDestroyEnumHandle,
                                              std::move(actionFunc));
}

bool FirewallEngine::removeProviderContexts()
{
    return enumProviderContexts([this](const FWPM_PROVIDER_CONTEXT &providerContext)
    {
        return remove(WfpProviderContextObject{providerContext.providerContextKey});
    });
}

void FirewallEngine::checkLeakedLayerObjects(const GUID &layerKey)
{
    enumFilters(layerKey, [&](const FWPM_FILTER &filter)
    {
        KAPPS_CORE_WARNING() << "WFP filter leaked:" << core::Uuid{filter.filterKey}
            << "in" << core::Uuid{layerKey};
        return true;
    });
    enumCallouts(layerKey, [&](const FWPM_CALLOUT &callout)
    {
        KAPPS_CORE_WARNING() << "WFP callout leaked:"
            << core::Uuid{callout.calloutKey} << "in" << core::Uuid{layerKey};
        return true;
    });
}

void FirewallEngine::checkLeakedObjects()
{
    checkLeakedLayerObjects(FWPM_LAYER_ALE_AUTH_CONNECT_V4);
    checkLeakedLayerObjects(FWPM_LAYER_ALE_AUTH_CONNECT_V6);
    checkLeakedLayerObjects(FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4);
    checkLeakedLayerObjects(FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6);
    checkLeakedLayerObjects(FWPM_LAYER_ALE_BIND_REDIRECT_V4);
    checkLeakedLayerObjects(FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4);
    checkLeakedLayerObjects(FWPM_LAYER_ALE_CONNECT_REDIRECT_V4);
    checkLeakedLayerObjects(FWPM_LAYER_INBOUND_IPPACKET_V4);
    checkLeakedLayerObjects(FWPM_LAYER_OUTBOUND_IPPACKET_V4);
    enumProviderContexts([&](const FWPM_PROVIDER_CONTEXT &providerContext)
    {
        KAPPS_CORE_WARNING() << "WFP provider context leaked:"
            << core::Uuid{providerContext.providerContextKey};
        return true;
    });

    KAPPS_CORE_INFO() << "Finished enumerating WFP objects";
}

FirewallTransaction::FirewallTransaction(FirewallEngine* firewall)
    : _handle(firewall ? firewall->_handle : NULL)
{
    if (_handle)
    {
        if (DWORD error = FwpmTransactionBegin(_handle, 0))
        {
            KAPPS_CORE_ERROR() << core::WinErrTracer{error};
            _handle = NULL;
        }
    }
}

FirewallTransaction::~FirewallTransaction()
{
    abort();
}

void FirewallTransaction::commit()
{
    if (_handle)
    {
        if (DWORD error = FwpmTransactionCommit(_handle))
        {
            KAPPS_CORE_ERROR() << core::WinErrTracer{error};
        }
        else
            _handle = NULL;
    }
}

void FirewallTransaction::abort()
{
    if (_handle)
    {
        if (DWORD error = FwpmTransactionAbort(_handle))
        {
            KAPPS_CORE_ERROR() << core::WinErrTracer{error};
        }
        _handle = NULL;
    }
}

}}
