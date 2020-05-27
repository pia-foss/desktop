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
#line SOURCE_FILE("mac_networks.mm")

#include "mac_networks.h"
#include "mac_dynstore.h"
#include "exec.h"
#include <QProcess>
#include <QRegularExpression>
#include <unordered_map>

static_assert(__has_feature(objc_arc), "MacNetworks requires Objective-C ARC");

// Mac implementation of PosixNetworks.
//
// General network state is read from the SCDynamicStore API - refer to the
// schemas defined here:
//   https://developer.apple.com/library/archive/documentation/Networking/Conceptual/SystemConfigFrameworks/SC_UnderstandSchema/SC_UnderstandSchema.html#//apple_ref/doc/uid/TP40001065-CH203-CHDIHDCG
class MacNetworks : public NetworkMonitor
{
public:
    MacNetworks();
    ~MacNetworks();

private:
    MacNetworks(const MacNetworks &) = delete;
    MacNetworks &operator=(const MacNetworks &) = delete;

private:
    // Check the global IPv4 or IPv4 state, find the primary service, and
    // return the complete key path for that service if it is found.
    QString getDefaultServiceKeyName(const MacString &globalKey,
                                     const QString &serviceType);

    QString getGlobalRouterIp(const QString &ipVersion);
public:
    // Read all active network connections and forward the information to
    // PosixNetworks.
    // Public so it can be called by the WiFi delegate only.
    void readConnections();

private:
    // Array containing the regex for the IPv4 and IPv6 keys we're interested in
    MacArray ipKeyPatterns;
    // Array containing all patterns we monitor - the IP key patterns, as well
    // as the global state keys indicating the default services
    MacArray monitorKeyPatterns;
    MacDynamicStore _dynStore;
};

std::unique_ptr<NetworkMonitor> createMacNetworks()
{
    return std::unique_ptr<NetworkMonitor>{new MacNetworks{}};
}

MacNetworks::MacNetworks()
{
    // Plain CFTypeRefs (not CFHandle) so we can pass a raw array to
    // CFArrayCreate().  Per doc, CFSTR strings are never destroyed; we don't
    // have to worry about retain/release.
    CFTypeRef ipKeyRegexes[4]{CFSTR("^State:/Network/Service/.*/IPv4$"),
                              CFSTR("^State:/Network/Service/.*/IPv6$"),
                              CFSTR("^State:/Network/Global/IPv4$"),
                              CFSTR("^State:/Network/Global/IPv6$")};
    // Just put the IP service patterns in this array (just the first 2)
    ipKeyPatterns = MacArray{::CFArrayCreate(nullptr, ipKeyRegexes, 2, &kCFTypeArrayCallBacks)};
    // Monitor all relevant key patterns
    monitorKeyPatterns = MacArray{::CFArrayCreate(nullptr, ipKeyRegexes, 4, &kCFTypeArrayCallBacks)};

    QObject::connect(&_dynStore, &MacDynamicStore::keysChanged, this,
                     &MacNetworks::readConnections);

    _dynStore.setNotificationKeys(nullptr, monitorKeyPatterns.get());

    readConnections();
}

MacNetworks::~MacNetworks()
{
}

QString MacNetworks::getGlobalRouterIp(const QString &ipVersion)
{
    auto globalKeyQStr = QStringLiteral("State:/Network/Global/%1").arg(ipVersion);
    MacString globalKey{globalKeyQStr.toCFString()};
    auto defIpPlist = _dynStore.copyValue(globalKey.get());
    MacDict defIpProps{defIpPlist.as<CFDictionaryRef>()};
    MacString defIp;
    if(defIpProps)
    {
        defIp = defIpProps.getValueObj(CFSTR("Router")).as<CFStringRef>();
        return defIp.toQString();
    }
    return {};
}

QString MacNetworks::getDefaultServiceKeyName(const MacString &globalKey,
                                              const QString &serviceType)
{
    auto defIpv4Plist = _dynStore.copyValue(globalKey.get());
    MacDict defIpv4Props{defIpv4Plist.as<CFDictionaryRef>()};
    MacString defIpv4;
    if(defIpv4Props)
        defIpv4 = defIpv4Props.getValueObj(CFSTR("PrimaryService")).as<CFStringRef>();
    if(defIpv4)
    {
        return QStringLiteral("State:/Network/Service/") + defIpv4.toQString()
            + QStringLiteral("/") + serviceType;
    }
    return {};
}

void MacNetworks::readConnections()
{
    // Get the default IPv4 and IPv6 services
    QString defIpv4SvcKey = getDefaultServiceKeyName(MacString{CFSTR("State:/Network/Global/IPv4")},
                                                     QStringLiteral("IPv4"));
    QString defIpv6SvcKey = getDefaultServiceKeyName(MacString{CFSTR("State:/Network/Global/IPv6")},
                                                     QStringLiteral("IPv6"));

    auto ipKeysDict = _dynStore.copyMultiple(nullptr, ipKeyPatterns.get());
    auto ipKeysValues = ipKeysDict.getObjKeysValues();

    // Pair up the IPv4 and IPv6 configurations for each interface by putting
    // them in a map by interface.
    struct InterfaceConfigs
    {
        MacDict _ipv4, _ipv6;
        bool _defIpv4, _defIpv6;
    };
    std::unordered_map<QString, InterfaceConfigs> interfaceIpConfigs;

    auto ipKeysView = ipKeysValues.first.view<CFStringRef>();
    auto ipValuesView = ipKeysValues.second.view<CFDictionaryRef>();

    auto itIpKey = ipKeysView.begin();
    auto itIpValue = ipValuesView.begin();
    while(itIpKey != ipKeysView.end() && itIpValue != ipValuesView.end())
    {
        MacString ipKey{*itIpKey};
        MacDict ipValue{*itIpValue};

        // Get the interface name
        MacString interface = ipValue.getValueObj(CFSTR("InterfaceName")).as<CFStringRef>();
        QString interfaceQStr = interface.toQString();

        if(!interfaceQStr.isEmpty())
        {
            auto &interfaceConfig = interfaceIpConfigs[interfaceQStr];
            // Is this IPv4?
            if(ipKey.toQString().endsWith(QStringLiteral("/IPv4")))
                interfaceConfig._ipv4 = std::move(ipValue);
            else
                interfaceConfig._ipv6 = std::move(ipValue);

            // Is this the default IPv4 or IPv6 interface?
            auto keyQstr = ipKey.toQString();
            if(keyQstr == defIpv4SvcKey)
                interfaceConfig._defIpv4 = true;
            if(keyQstr == defIpv6SvcKey)
                interfaceConfig._defIpv6 = true;
        }

        ++itIpKey;
        ++itIpValue;
    }

    std::vector<NetworkConnection> connectionInfo;
    connectionInfo.reserve(interfaceIpConfigs.size());

    for(auto &ipConfigEntry : interfaceIpConfigs)
    {
        auto &ipConfig = ipConfigEntry.second;
        MacArray ipv4Addrs = ipConfig._ipv4.getValueObj(CFSTR("Addresses")).as<CFArrayRef>();
        MacString ipv4ServiceRouterIp = ipConfig._ipv4.getValueObj(CFSTR("Router")).as<CFStringRef>();

        MacArray ipv6Addrs = ipConfig._ipv6.getValueObj(CFSTR("Addresses")).as<CFArrayRef>();
        MacString ipv6ServiceRouterIp = ipConfig._ipv6.getValueObj(CFSTR("Router")).as<CFStringRef>();

        std::vector<Ipv4Address> addressesIpv4;
        addressesIpv4.reserve(ipv4Addrs.getCount());
        for(CFIndex i=0; i<ipv4Addrs.getCount(); ++i)
        {
            MacString addrStr{ipv4Addrs.getObjAtIndex(i).as<CFStringRef>()};
            Ipv4Address addr{addrStr.toQString()};
            if(addr != Ipv4Address{})
                addressesIpv4.push_back(addr);
        }
        std::vector<Ipv6Address> addressesIpv6;
        addressesIpv6.reserve(ipv6Addrs.getCount());
        for(CFIndex i=0; i<ipv6Addrs.getCount(); ++i)
        {
            MacString addrStr{ipv6Addrs.getObjAtIndex(i).as<CFStringRef>()};
            Ipv6Address addr{addrStr.toQString()};
            if(addr != Ipv6Address{})
                addressesIpv6.push_back(addr);
        }

        // On some versions of macOS Catalina the "Router" field is not present for network services.
        // This is very likely a bug in Catalina, nonetheless we work around the absense of this field
        // by falling back to the "Global" IPv4 state which appears to always have the Router field set.
        // If this bug is fixed in the future we can remove all the code below and just use the ipv{4,6}ServiceRouterIp
        // directly when setting the ipv{4,6}RouterAddr.
        // Note: we only care about setting the router for the Default network.
        auto getRouterIp = [&](const QString &serviceRouterIp, const QString &globalRouterIp, bool isDefault) {
            if(serviceRouterIp.isEmpty() && isDefault)
            {
                qWarning() << "Router IP not found in service data, falling back to Global Router IP:" << globalRouterIp;
                return globalRouterIp;
            }
            else
            {
                return serviceRouterIp;
            }
        };

        QString ipv4RouterIp = getRouterIp(ipv4ServiceRouterIp.toQString(), getGlobalRouterIp("IPv4"), ipConfig._defIpv4);
        QString ipv6RouterIp = getRouterIp(ipv6ServiceRouterIp.toQString(), getGlobalRouterIp("IPv6"), ipConfig._defIpv6);

        Ipv4Address ipv4RouterAddr{ipv4RouterIp};
        Ipv6Address ipv6RouterAddr{ipv6RouterIp};

        Q_ASSERT(!ipConfigEntry.first.isEmpty());    // Didn't put empty interfaces in this map

        connectionInfo.emplace_back(ipConfigEntry.first,
                                    ipConfig._defIpv4, ipConfig._defIpv6,
                                    ipv4RouterAddr, ipv6RouterAddr,
                                    std::move(addressesIpv4),
                                    std::move(addressesIpv6));
    }

    updateNetworks(std::move(connectionInfo));
}
