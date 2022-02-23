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
#include "win_nativewifi.h"
#include <algorithm>

#pragma comment(lib, "wlanapi.lib")

void WinLuid::trace(QDebug &dbg) const
{
    QDebugStateSaver save{dbg};
    dbg.noquote().nospace();
    // Reserved should be zero, if it's not just dump the whole 64-bit value, we
    // can't make sense of this LUID
    if(_luid.Info.Reserved)
    {
        dbg << "<invalid " << value() << ">";
        return;
    }

    // Trace common types as names
    switch(_luid.Info.IfType)
    {
        case IF_TYPE_OTHER:
            dbg << "other";
            break;
        case IF_TYPE_ETHERNET_CSMACD:
            dbg << "ethernet";
            break;
        case IF_TYPE_SOFTWARE_LOOPBACK:
            dbg << "loopback";
            break;
        case IF_TYPE_IEEE80211:
            dbg << "802.11";
            break;
        case IF_TYPE_TUNNEL:
            dbg << "tunnel";
            break;
        default:
            dbg << "type " << _luid.Info.IfType;
            break;
    }

    dbg << "-" << _luid.Info.NetLuidIndex;
}

void WINAPI WinNativeWifi::wlanNotificationCallback(PWLAN_NOTIFICATION_DATA pData,
                                                    PVOID pThis)
{
    if(!pData || !pThis)
    {
        qWarning() << "Invalid Native Wifi notification, ignoring (pData"
            << !!pData << "; pThis" << !!pThis << ")";
        return;
    }

    reinterpret_cast<WinNativeWifi*>(pThis)->handleWlanNotification(*pData);
}

WifiHandle WinNativeWifi::createWlanHandle()
{
    DWORD negotiatedVersion{0};
    WifiHandle client;
    // Open the Wi-Fi client.  The documentation for the version parameters
    // seems to be conflicting - the parameter documentation and most samples
    // indicate that the supported version can be 1 or 2, but there's a note
    // saying that it's supposed to be a composite major/minor version number.
    //
    // Either the major/minor note is just wrong, or the "2" here really means
    // (2, 0).  Either way it should be fine, but if we end up getting a nonzero
    // minor in the negotiated version it'll trace strangely.
    DWORD err = ::WlanOpenHandle(2, nullptr, &negotiatedVersion, client.receive());
    if(err != ERROR_SUCCESS || !client)
    {
        qWarning() << "Unable to open Native Wifi client -"
            << WinErrTracer{err};

        // If we weren't able to open the Wi-Fi client, throw.  This happens if
        // the WLAN AutoConfig service ('WlanSvc') isn't running, which can be
        // the case if there are no Wi-Fi adapters on the system.  (Typically
        // the service is enabled when installing the first Wi-Fi driver.)  The
        // owner of WinNativeWifi will attempt to connect again if the service
        // starts.
        throw SystemError{HERE, err};
    }

    return client;
}

WinNativeWifi::WinNativeWifi()
    : _client{createWlanHandle()}
{
    Q_ASSERT(_client);  // Postcondition of createWlanHandle(), ensures class invariant

    // Activate notifications for ACM and MSM.
    // As discussed in handleMsmNotification(), we only trace those.  The ACM
    // notifications appear to be complete, but the documentation isn't
    // perfectly clear whether we always receive both ACM and MSM notifications,
    // or if there is some obscure situation that triggers only one.
    DWORD err = ::WlanRegisterNotification(_client.get(),
        WLAN_NOTIFICATION_SOURCE_ACM|WLAN_NOTIFICATION_SOURCE_MSM,
        true, &WinNativeWifi::wlanNotificationCallback, this, nullptr,
        nullptr);
    if(err != ERROR_SUCCESS)
    {
        qWarning() << "Unable to enable Wifi notifications -"
            << WinErrTracer{err};
        throw SystemError{HERE, err};
    }

    // Enumerate the current interfaces to load the initial state
    WLAN_INTERFACE_INFO_LIST *pItfListRaw{};
    err = ::WlanEnumInterfaces(_client.get(), nullptr, &pItfListRaw);
    // Own the interface list memory allocated by WlanEnumInterfaces(); free it
    // with WlanFreeMemory().
    std::unique_ptr<WLAN_INTERFACE_INFO_LIST, decltype(&::WlanFreeMemory)>
        pItfList{pItfListRaw, &::WlanFreeMemory};

    if(err != ERROR_SUCCESS || !pItfList)
    {
        // We can't load the initial state, the state reported would not be
        // accurate.
        qWarning() << "Unable to load Native Wifi initial state -"
            << WinErrTracer{err};
        throw SystemError{HERE, err};
    }

    for(int i=0; i<pItfList->dwNumberOfItems; ++i)
        addInitialInterface(pItfList->InterfaceInfo[i]);
}

WinLuid WinNativeWifi::getInterfaceLuid(const GUID &interfaceGuid) const
{
    // Native Wifi gives us the interface GUID; we report interfaces using LUIDs
    // to align with the routing APIs used by WinNetworks.
    WinLuid interfaceLuid{};
    NETIO_STATUS err = ::ConvertInterfaceGuidToLuid(&interfaceGuid,
                                                    interfaceLuid.receive());
    if(err != NO_ERROR)
    {
        qWarning() << "Unable to find interface LUID interface"
            << QUuid{interfaceGuid}.toString() << "-" << WinErrTracer{err};
    }

    return interfaceLuid;
}

void WinNativeWifi::addInitialInterface(const WLAN_INTERFACE_INFO &info)
{
    WinLuid interfaceLuid{getInterfaceLuid(info.InterfaceGuid)};
    if(!interfaceLuid)
    {
        // Fail - this fails the entire WinNativeWifi construction.  This
        // could cause us to misinterpret an 802.11 interface as a wired
        // interface.
        qWarning() << "Unable to find interface LUID for interface in initial status dump";
        throw Error{HERE, Error::Code::Unknown};
    }

    // If the state says the interface is connected, we need to query the status
    // to get the SSID and security mode.
    // Otherwise, it's not associated, just add it with no association.
    if(info.isState != wlan_interface_state_connected)
    {
        _interfaces[interfaceLuid] = WifiStatus{};
        return; // We're done
    }

    DWORD connAttrSize{};
    WLAN_CONNECTION_ATTRIBUTES *pConnAttrRaw{};
    DWORD err = ::WlanQueryInterface(_client.get(), &info.InterfaceGuid,
                                     wlan_intf_opcode_current_connection,
                                     nullptr, &connAttrSize,
                                     reinterpret_cast<void**>(&pConnAttrRaw),
                                     nullptr);
    // Own the structure returned; free with WlanFreeMemory()
    std::unique_ptr<WLAN_CONNECTION_ATTRIBUTES, decltype(&::WlanFreeMemory)>
        pConnAttr{pConnAttrRaw, &::WlanFreeMemory};
    if(err != ERROR_SUCCESS || !pConnAttr)
    {
        // This error is probably unlikely but also fatal; the dump said this
        // interface is connected but we cannot get any info.  This could cause
        // us to misinterpret the type of interface, which could cause an
        // incorrect rule application since this interface is connected.
        qWarning() << "Unable to get interface status for connected interface"
            << interfaceLuid << "in initial status dump -" << WinErrTracer{err};
        throw SystemError{HERE, err};
    }

    // Populate the status - although the initial dump said this interface was
    // connected, check the status's connection state instead in case they differ.
    if(pConnAttr->isState != wlan_interface_state_connected)
    {
        qInfo() << "Interface" << interfaceLuid
            << "was connected in initial dump, but not connected in interface query";
        _interfaces[interfaceLuid] = WifiStatus{};
        return;
    }

    WifiStatus newItfStatus{};
    newItfStatus.associated = true;
    newItfStatus.ssidLength = std::min<std::size_t>(SsidMaxLen,
        pConnAttr->wlanAssociationAttributes.dot11Ssid.uSSIDLength);
    std::copy(pConnAttr->wlanAssociationAttributes.dot11Ssid.ucSSID,
              pConnAttr->wlanAssociationAttributes.dot11Ssid.ucSSID + newItfStatus.ssidLength,
              newItfStatus.ssid);
    newItfStatus.encrypted = pConnAttr->wlanSecurityAttributes.bSecurityEnabled;
    _interfaces[interfaceLuid] = newItfStatus;
}

void WinNativeWifi::handleWlanNotification(const WLAN_NOTIFICATION_DATA &data)
{
    WinLuid interfaceLuid{getInterfaceLuid(data.InterfaceGuid)};
    if(!interfaceLuid)
    {
        qWarning() << "Unable to find interface LUID for Native Wifi notification (source"
            << data.NotificationSource << "code:" << data.NotificationCode
            << ")";
        return; // Ignore the notification
    }

    switch(data.NotificationSource)
    {
        case WLAN_NOTIFICATION_SOURCE_ACM:
            // Auto-configuration module
            handleAcmNotification(interfaceLuid, data);
            break;
        case WLAN_NOTIFICATION_SOURCE_MSM:
            // Media-specific module
            handleMsmNotification(interfaceLuid, data);
            break;
        default:
            qWarning() << interfaceLuid << "unknown notification source:"
                << data.NotificationSource << "code:" << data.NotificationCode;
            break;  // Unexpected, only asked for ACM and MSM
    }
}

void WinNativeWifi::handleAcmNotification(const WinLuid &luid,
                                          const WLAN_NOTIFICATION_DATA &data)
{
    // Checked by caller
    Q_ASSERT(data.NotificationSource == WLAN_NOTIFICATION_SOURCE_ACM);

    switch(data.NotificationCode)
    {
        // Trace other codes so in the event that we fail to identify the
        // network state correctly, we can see what events we were getting.
        default:
            qInfo() << "Interface" << luid << "ignored ACM code" << data.NotificationCode;
            break;
        // Silently ignore some codes that we definitely don't care about (and
        // that happen a lot).
        case wlan_notification_acm_background_scan_enabled:
        case wlan_notification_acm_background_scan_disabled:
        case wlan_notification_acm_scan_complete:
        case wlan_notification_acm_profile_change:
        case wlan_notification_acm_profiles_exhausted:
        case wlan_notification_acm_network_not_available:
        case wlan_notification_acm_network_available:
        case wlan_notification_acm_scan_list_refresh:
            break;
        case wlan_notification_acm_disconnecting:
        {
            // An interface is disconnecting, it is no longer associated.
            auto itInterface = _interfaces.find(luid);
            if(itInterface == _interfaces.end())
            {
                qWarning() << "Interface" << luid
                    << "disconnecting, but was not known";
            }
            else if(!itInterface->second.associated)
            {
                qInfo() << "Interface" << luid
                    << "disconnecting, but was not associated";
            }
            else
            {
                qInfo() << "Interface" << luid << "disconnecting";
                // Reset the WifiStatus; it's no longer associated and any stored
                // SSID is no longer meaningful.
                itInterface->second = WifiStatus{};
                emit interfacesChanged();
            }
            break;
        }
        case wlan_notification_acm_connection_start:
        case wlan_notification_acm_disconnected:
        {
            const char *pEventName =
                (data.NotificationCode == wlan_notification_acm_connection_start) ?
                    "connection start" : "disconnected";
            // These shouldn't actually cause any state change - they indicate
            // that the interface is not associated, but it shouldn't have been
            // in an associated state before.
            auto itInterface = _interfaces.find(luid);
            if(itInterface == _interfaces.end())
            {
                qWarning() << "Interface" << luid
                    << pEventName << ", but was not known";
            }
            else if(!itInterface->second.associated)
            {
                qInfo() << "Interface" << luid << pEventName;
            }
            else
            {
                qWarning() << "Interface" << luid << pEventName
                    << ", but was associated unexpectedly";
                // Reset the WifiStatus since the interface is apparently not
                // associated
                itInterface->second = WifiStatus{};
                emit interfacesChanged();
            }
            break;
        }
        case wlan_notification_acm_connection_complete:
        {
            auto itInterface = _interfaces.find(luid);
            if(itInterface == _interfaces.end())
            {
                qWarning() << "Interface" << luid
                    << "connected, but was not known";
            }
            else if(data.dwDataSize < sizeof(WLAN_CONNECTION_NOTIFICATION_DATA))
            {
                qWarning() << "Interface" << luid
                    << "connected, but notification was truncated - expected"
                    << sizeof(WLAN_CONNECTION_NOTIFICATION_DATA) << "bytes, got"
                    << data.dwDataSize;
            }
            else if(!data.pData)
            {
                qWarning() << "Interface" << luid
                    << "connected, but notification data were missing";
            }
            else
            {
                // Get the connection notification data; checked this pointer
                // and the payload size above
                const WLAN_CONNECTION_NOTIFICATION_DATA *pConnData =
                    reinterpret_cast<const WLAN_CONNECTION_NOTIFICATION_DATA*>(data.pData);

                // Trace a warning if the interface was already associated, this
                // is unexpected
                if(itInterface->second.associated)
                {
                    qWarning() << "Interface" << luid
                        << "connected, but was already associated";
                    // Update the state below anyway
                }
                else
                {
                    qInfo() << "Interface" << luid << "connected";
                }

                itInterface->second.associated = true;
                itInterface->second.encrypted = pConnData->bSecurityEnabled;
                itInterface->second.ssidLength = std::min<std::size_t>(pConnData->dot11Ssid.uSSIDLength, SsidMaxLen);
                std::copy(pConnData->dot11Ssid.ucSSID,
                          pConnData->dot11Ssid.ucSSID + itInterface->second.ssidLength,
                          itInterface->second.ssid);
                emit interfacesChanged();
            }
            break;
        }
        case wlan_notification_acm_interface_arrival:
            // Add this interface if it's not yet known.  It might already be
            // here if the notification raced with the initial interface dump,
            // in that case keep whatever state we dumped.
            if(_interfaces.insert({luid, WifiStatus{}}).second)
            {
                qInfo() << "Interface" << luid << "interface connected";
                emit interfacesChanged();
            }
            else
            {
                qInfo() << "Interface" << luid << "interface connnected - was already known, ignored";
            }
            break;
        case wlan_notification_acm_interface_removal:
            // Remove this interface.  It might already be gone if the
            // notification raced with the initial interface dump, in that case
            // there's nothing to do.
            if(_interfaces.erase(luid) > 0)
            {
                qInfo() << "Interface" << luid << "interface removed";
                emit interfacesChanged();
            }
            else
            {
                qInfo() << "Interface" << luid << "interface removed - was not known, ignored";
            }
            break;
    }
}

void WinNativeWifi::handleMsmNotification(const WinLuid &luid,
                                          const WLAN_NOTIFICATION_DATA &data)
{
    // Checked by caller
    Q_ASSERT(data.NotificationSource == WLAN_NOTIFICATION_SOURCE_MSM);

    // The only thing we do with MSM notifications currently is trace them.
    // It's not perfectly clear from the documentation if there are situations
    // in which we might receive MSM association/connection notifications, but
    // not the corresponding ACM notifications.
    //
    // We seem to always get the ACM notifications, and oddly "interface
    // removed" only exists in ACM, so the ACM notifications appear to be
    // complete.  These are traced though in case there's some situation found
    // where we do not receive ACM notifications but do receive MSM
    // notifications.
    switch(data.NotificationCode)
    {
        // Ignore some common notifications that we definitely don't care about
        case wlan_notification_msm_signal_quality_change:
        case wlan_notification_msm_radio_state_change:
        case 59:    // Not documented - unclear what this is, but it happens a
                    // _lot_ - probably some other sort of status update while
                    // connected.
            break;
        default:
            qInfo() << "Interface" << luid << "MSM code" << data.NotificationCode;
            break;
        case  wlan_notification_msm_associating:
            qInfo() << "Interface" << luid << "MSM associating";
            break;
        case  wlan_notification_msm_associated:
            qInfo() << "Interface" << luid << "MSM associated";
            break;
        case  wlan_notification_msm_authenticating:
            qInfo() << "Interface" << luid << "MSM authenticating";
            break;
        case  wlan_notification_msm_connected:
            qInfo() << "Interface" << luid << "MSM connected";
            break;
        case  wlan_notification_msm_disassociating:
            qInfo() << "Interface" << luid << "MSM disassociating";
            break;
        case  wlan_notification_msm_disconnected:
            qInfo() << "Interface" << luid << "MSM disconnected";
            break;
        case  wlan_notification_msm_adapter_removal:
            qInfo() << "Interface" << luid << "MSM adapter removed";
            break;
    }
}
