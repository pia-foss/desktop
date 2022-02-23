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

#include "common.h"
#line SOURCE_FILE("win/win_firewall.cpp")

#include "win_firewall.h"
#include "brand.h"
#include "win.h"
#include "product.h"
#include <QUuid>

GUID zeroGuid = {0};
static wchar_t DEFAULT_FIREWALL_NAME[] = L"" PIA_PRODUCT_NAME " Firewall";
static wchar_t DEFAULT_FIREWALL_DESCRIPTION[] = L"Implements privacy filtering features of " PIA_PRODUCT_NAME ".";
static const DWORD DEFAULT_FILTER_FLAGS = FWPM_FILTER_FLAG_PERSISTENT | (IsWindows8OrGreater() ? FWPM_FILTER_FLAG_INDEXED : 0);

static FWPM_PROVIDER g_wfpProvider = {
    BRAND_WINDOWS_WFP_PROVIDER,
    { DEFAULT_FIREWALL_NAME, DEFAULT_FIREWALL_DESCRIPTION },
    FWPM_PROVIDER_FLAG_PERSISTENT,
    { 0, NULL },
    NULL
};

FWPM_SUBLAYER g_wfpSublayer = {
    BRAND_WINDOWS_WFP_SUBLAYER,
    { DEFAULT_FIREWALL_NAME, DEFAULT_FIREWALL_DESCRIPTION },
    FWPM_SUBLAYER_FLAG_PERSISTENT,
    &g_wfpProvider.providerKey,
    { 0, NULL },
    9000
};


FirewallFilter::FirewallFilter()
{
    memset(static_cast<FWPM_FILTER*>(this), 0, sizeof(FWPM_FILTER));
    UuidCreate(&filterKey);
    displayData.name = DEFAULT_FIREWALL_NAME;
    displayData.description = DEFAULT_FIREWALL_DESCRIPTION;
    flags = DEFAULT_FILTER_FLAGS;
    providerKey = &g_wfpProvider.providerKey;
    subLayerKey = g_wfpSublayer.subLayerKey;
    weight.type = FWP_UINT8;
}

Callout::Callout(const GUID& applicableLayer, const GUID& calloutKey)
{
    memset(static_cast<FWPM_CALLOUT*>(this), 0, sizeof(FWPM_CALLOUT));
    displayData.name = L"PIA WFP Callout";
    displayData.description = L"PIA WFP Callout";
    providerKey = &g_wfpProvider.providerKey;
    this->applicableLayer = applicableLayer;
    this->calloutKey = calloutKey;
    flags = FWPM_CALLOUT_FLAG_PERSISTENT | FWPM_CALLOUT_FLAG_USES_PROVIDER_CONTEXT;
}

ProviderContext::ProviderContext(void *pContextData, UINT32 dataSize)
{
    memset(static_cast<FWPM_PROVIDER_CONTEXT*>(this), 0, sizeof(FWPM_PROVIDER_CONTEXT));
    UuidCreate(&this->providerContextKey);
    displayData.name = L"PIA WFP Provider Context";
    displayData.description = L"PIA WFP Provider Context";
    providerKey = &g_wfpProvider.providerKey;
    blob.data = static_cast<UINT8*>(pContextData);
    blob.size = dataSize;

    flags = FWPM_PROVIDER_CONTEXT_FLAG_PERSISTENT;
    type = FWPM_GENERAL_CONTEXT;
    dataBuffer = &blob;
}

bool AppIdKey::operator==(const AppIdKey &other) const
{
    if(!_pBlob && !other._pBlob)
        return true;    // Both empty
    if(!_pBlob || !other._pBlob)
        return false;   // One is empty but the other isn't

    return std::equal(_pBlob->data, _pBlob->data + _pBlob->size,
                      other._pBlob->data, other._pBlob->data + other._pBlob->size);
}

bool AppIdKey::operator<(const AppIdKey &other) const
{
    if(!_pBlob && !other._pBlob)
        return false;   // Both empty - equal
    // Empties are less than non-empties
    if(!_pBlob)
        return true; // This is empty
    if(!other._pBlob)
        return false; // Other is empty

    return std::lexicographical_compare(_pBlob->data, _pBlob->data + _pBlob->size,
                                        other._pBlob->data, other._pBlob->data + other._pBlob->size);
}

bool AppIdKey::operator==(const QByteArray &value) const
{
    if(!_pBlob)
        return value.isEmpty(); // Empty blob - equal if the value is also empty
    return std::equal(_pBlob->data, _pBlob->data + _pBlob->size,
                      value.begin(), value.end());
}

void AppIdKey::clear()
{
    if(_pBlob)
    {
        ::FwpmFreeMemory(reinterpret_cast<void**>(&_pBlob));
        _pBlob = nullptr;
    }
}

void AppIdKey::reset(WinLinkReader *pReader, const QString &appPath, std::wstring *pTarget)
{
    if(pTarget)
        *pTarget = {};

    clear();

    // If it's a shell link, get the ID for the link target.
    if(appPath.endsWith(QStringLiteral(".lnk"), Qt::CaseSensitivity::CaseInsensitive))
    {
        std::wstring linkTarget;
        // linkReader traces for any of these failures (other than
        // linkReader failing to load entirely, which was traced by the caller)
        if(pReader && pReader->loadLink(appPath))
            linkTarget = pReader->getLinkTarget(appPath);
        if(linkTarget.empty())
        {
            qWarning() << "Can't find link target for" << appPath;
        }
        else if(DWORD error = FwpmGetAppIdFromFileName(linkTarget.c_str(), &_pBlob))
        {
            qWarning() << "Can't get app ID for" << appPath << "->"
                << linkTarget << "-" << SystemError{HERE, error};
            _pBlob = nullptr;
        }
        else if(_pBlob)
        {
            qInfo() << "Got app ID for" << appPath << "->" << linkTarget;
            if(pTarget)
                *pTarget = std::move(linkTarget);
        }
    }
    // Otherwise, not a shell link, try to get an app ID from this file
    else if(DWORD error = FwpmGetAppIdFromFileName(qstringWBuf(appPath), &_pBlob))
    {
        qWarning() << "Can't get app ID for" << appPath << "-"
            << SystemError{HERE, error};
        _pBlob = nullptr;
        // Rely on the filter addition failing later
    }
    else if(_pBlob)
    {
        qInfo() << "Got app ID for" << appPath;
        if(pTarget)
            *pTarget = appPath.toStdWString();
    }
}

QByteArray AppIdKey::copyData() const
{
    if(!_pBlob)
        return {};
    return QByteArray{reinterpret_cast<const char*>(_pBlob->data),
                      static_cast<int>(_pBlob->size)};
}

QStringView AppIdKey::traceString() const
{
    static const auto &nullStr = QStringLiteral("<null>");

    if(!_pBlob)
        return nullStr;
    else
    {
        return QStringView{reinterpret_cast<const char16_t *>(_pBlob->data),
                           static_cast<qsizetype>(_pBlob->size / sizeof(char16_t))};
    }
}

QStringView AppIdKey::printableString() const
{
    auto rawStrView = traceString();

    auto truncateLen = rawStrView.size();
    while(truncateLen > 0 && rawStrView[truncateLen-1].isNull())
        --truncateLen;
    rawStrView.truncate(truncateLen);
    return rawStrView;
}

std::size_t AppIdKey::hash() const
{
    if(!_pBlob)
        return 0;
    // qHashBits provides a hash algorithm over an arbitrary data blob
    return static_cast<std::size_t>(qHashBits(_pBlob->data, _pBlob->size));
}

void AppIdKey::trace(QDebug &dbg) const
{
    dbg << traceString();
}

FirewallEngine::FirewallEngine(QObject* parent)
    : QObject{parent}, _handle(NULL)
{
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
        qCritical() << SystemError{HERE, error};
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
            qCritical() << SystemError{HERE, error};
            return false;
        }
        qInfo() << "Installed WFP provider";
        break;
    default:
        qCritical() << SystemError{HERE, error};
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
            qCritical() << SystemError{HERE, error};
            return false;
        }
        qInfo() << "Installed WFP sublayer";
        break;
    default:
        qCritical() << SystemError{HERE, error};
        return false;
    }
    return true;
}

bool FirewallEngine::uninstallProvider()
{
    bool result = true;
    switch (DWORD error = FwpmSubLayerDeleteByKey(_handle, &g_wfpSublayer.subLayerKey))
    {
    case ERROR_SUCCESS: qInfo() << "Removed WFP sublayer"; break;
    case FWP_E_SUBLAYER_NOT_FOUND: break;
    default: qCritical() << SystemError(HERE, error); result = false; break;
    }
    switch (DWORD error = FwpmProviderDeleteByKey(_handle, &g_wfpProvider.providerKey))
    {
    case ERROR_SUCCESS: qInfo() << "Removed WFP provider"; break;
    case FWP_E_PROVIDER_NOT_FOUND: break;
    default: qCritical() << SystemError(HERE, error); result = false; break;
    }
    return result;
}

WfpFilterObject FirewallEngine::add(const FirewallFilter& filter)
{
    UINT64 id = 0;
    if (DWORD error = FwpmFilterAdd(_handle, &filter, NULL, &id))
    {
        qCritical() << SystemError{HERE, error};
        return {zeroGuid};
    }
    return {filter.filterKey};
}

WfpCalloutObject FirewallEngine::add(const Callout& mCallout)
{
    UINT32 id = 0;
    if (DWORD error = FwpmCalloutAdd(_handle, &mCallout, NULL, &id))
    {
        qCritical() << SystemError{HERE, error};
        return {zeroGuid};
    }
    return {mCallout.calloutKey};
}

WfpProviderContextObject FirewallEngine::add(const ProviderContext& providerContext)
{
    UINT64 id = 0;
    if (DWORD error = FwpmProviderContextAdd(_handle, &providerContext, NULL, &id))
    {
        qCritical() << SystemError{HERE, error};
        return {zeroGuid};
    }
    return {providerContext.providerContextKey};
}

bool FirewallEngine::remove(const WfpFilterObject &filter)
{
    if(DWORD error = FwpmFilterDeleteByKey(_handle, &filter))
    {
        qCritical() << SystemError{HERE, error};
        return false;
    }
    return true;
}

bool FirewallEngine::remove(const WfpCalloutObject &callout)
{
    if(DWORD error = FwpmCalloutDeleteByKey(_handle, &callout))
    {
        qCritical() << SystemError{HERE, error};
        return false;
    }
    return true;
}

bool FirewallEngine::remove(const WfpProviderContextObject &providerContext)
{
    if(DWORD error = FwpmProviderContextDeleteByKey(_handle, &providerContext))
    {
        qCritical() << SystemError{HERE, error};
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
        qCritical() << SystemError{HERE, error};
        return false;
    }

    ObjectT** entries;
    UINT32 count;
    do
    {
        if (DWORD error = enumFunc(_handle, enumHandle, 100, &entries, &count))
        {
            qCritical() << SystemError(HERE, error);
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
        qWarning() << "WFP filter leaked:" << QUuid{filter.filterKey} << "in"
            << QUuid{layerKey};
        return true;
    });
    enumCallouts(layerKey, [&](const FWPM_CALLOUT &callout)
    {
        qWarning() << "WFP callout leaked:" << QUuid{callout.calloutKey} << "in"
            << QUuid{layerKey};
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
        qWarning() << "WFP provider context leaked:"
            << QUuid{providerContext.providerContextKey};
        return true;
    });

    qInfo() << "Finished enumerating WFP objects";
}

FirewallTransaction::FirewallTransaction(FirewallEngine* firewall)
    : _handle(firewall ? firewall->_handle : NULL)
{
    if (_handle)
    {
        if (DWORD error = FwpmTransactionBegin(_handle, 0))
        {
            qCritical() << SystemError{HERE, error};
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
            qCritical() << SystemError{HERE, error};
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
            qCritical() << SystemError{HERE, error};
        }
        _handle = NULL;
    }
}
