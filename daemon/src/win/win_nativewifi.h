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

#ifndef WIN_NATIVEWIFI_H
#define WIN_NATIVEWIFI_H

#include <common/src/common.h>
#include "win.h"
#include <common/src/win/win_util.h>
#include <Wlanapi.h>

// Close a NativeWifi handle
struct WinCloseNativeWifi
{
    void operator()(HANDLE handle){::WlanCloseHandle(handle, nullptr);}
};

using WifiHandle = WinGenericHandle<HANDLE, WinCloseNativeWifi>;

// Interface LUID on Windows.  Can be traced, compared, used as a map key, etc.
class WinLuid : public kapps::core::OStreamInsertable<WinLuid>
{
public:
    // Creates a zero LUID, which isn't a valid interface identifier.  (0 is not
    // a valid interface type; see Ipifcons.h - MIN_IF_TYPE).
    WinLuid() : _luid{} {}
    WinLuid(const NET_LUID &luid) : _luid{luid} {}

public:
    bool operator==(const WinLuid &other) const {return value() == other.value();}
    bool operator!=(const WinLuid &other) const {return !(*this == other);}
    explicit operator bool() const {return value();}
    bool operator!() const {return !value();}

public:
    void trace(std::ostream &os) const;
    std::uint64_t value() const {return _luid.Value;}
    // Get a pointer where a NET_LUID can be written; used to receive a LUID
    // from a Win32 API.  The LUID is zeroed before receiving in case no value
    // is written.
    NET_LUID *receive() {*this = {}; return &_luid;}

private:
    NET_LUID _luid;
};

namespace std
{
    // Hash WinLuid by hashing the 64-bit LUID value
    template<>
    struct hash<WinLuid> : public hash<std::uint64_t>
    {
        std::size_t operator()(const WinLuid &luid) const
        {
            return hash<std::uint64_t>::operator()(luid.value());
        }
    };
};

// WinNativeWifi uses the Native Wifi API to enumerate 802.11 interfaces on the
// system and determine their current states.
class WinNativeWifi : public QObject
{
    Q_OBJECT

public:
    enum : std::size_t
    {
        SsidMaxLen = 32,
    };
    // Data provided for each Wi-Fi interface - the connected SSID (if any),
    // and whether the network is encrypted (when connected).
    struct WifiStatus
    {
        // Whether this network is associated with a BSS.  Note that the
        // SSID might not be reported even if we are associated if we're
        // unable to find the SSID for some reason.
        bool associated;
        // Length of the SSID (when associated).  If this is 0 while
        // associated, we couldn't figure out the SSID.
        std::size_t ssidLength;
        // SSID data
        unsigned char ssid[SsidMaxLen];
        // Whether the network is encrypted (when associated).
        bool encrypted;
    };

    using InterfaceMap = std::unordered_map<WinLuid, WifiStatus>;
private:
    // Static notification callback passed to Win32
    static void WINAPI wlanNotificationCallback(PWLAN_NOTIFICATION_DATA pData, PVOID pThis);

    static WifiHandle createWlanHandle();

public:
    // Create WinNativeWifi - connects to the Native Wifi client and loads the
    // initial state (synchronously - initial state is available from
    // interfaces()).  Throws if the APIs can't be initialized.
    WinNativeWifi();

private:
    WinLuid getInterfaceLuid(const GUID &interfaceGuid) const;
    void addInitialInterface(const WLAN_INTERFACE_INFO &info);
    void handleWlanNotification(const WLAN_NOTIFICATION_DATA &data);
    void handleAcmNotification(const WinLuid &luid,
                               const WLAN_NOTIFICATION_DATA &data);
    void handleMsmNotification(const WinLuid &luid,
                               const WLAN_NOTIFICATION_DATA &data);

public:
    // Get the current interface states
    const InterfaceMap &interfaces() const {return _interfaces;}

signals:
    // The interface states have changed; call interfaces() for the new state
    void interfacesChanged();

public:
    // Handle to the Native Wifi client
    WifiHandle _client;
    // Current WiFi interface states.  Keys are interface LUIDs.  All known
    // WiFi interfaces are present, if they're not associated (or we can't
    // detect that they're associated), that's indicated in the WifiStatus.
    InterfaceMap _interfaces;
};

#endif
