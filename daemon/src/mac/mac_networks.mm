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

#include <common/src/common.h>
#line SOURCE_FILE("mac_networks.mm")

#include "mac_networks.h"
#include "mac_dynstore.h"
#include <common/src/exec.h>
#include <kapps_core/src/posix/posix_objects.h>
#include <QProcess>
#include <QRegularExpression>
#include <unordered_map>
#include <QSocketNotifier>
#import <CoreWLAN/CoreWLAN.h>

static_assert(__has_feature(objc_arc), "MacNetworks requires Objective-C ARC");

class MacNetworks;

// Delegate attached to CWWiFiClient to forward changes to MacNetworks.
//
// The delegate methods are invoked on a worker queue thread.  In order to
// dispatch back to the main thread, we use a socketpair.  This just indicates
// to the main thread to re-read the Wi-Fi networks, so MacNetworkWiFiDelegate
// just writes single bytes that are read and ignored by MacNetworks.
//
// The socketpair is used instead of dispatching a queued method invocation
// because we can't control precisely when the delegate is destroyed, we'd need
// additional synchronization for destruction.  (Typically it's destroyed when
// MacNetworks shuts down, but if a delegate was being invoked at that time,
// that thread might have obtained a strong reference that will keep it alive
// while it's being invoked.)  With a socketpair, we can just close one end of
// the socketpair and let the delegate continue to attempt to write to its end
// until it shuts down.
@interface MacNetworkWiFiDelegate : NSObject<CWEventDelegate>
{
@private
    kapps::core::PosixFd signalSocket;
}
// Create the signaling socket pair and return MacNetwork's end of the socket
 - (kapps::core::PosixFd) createSockets;
 - (void) signalMacNetworks;
 - (void) bssidDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName;
 - (void) clientConnectionInterrupted;
 - (void) clientConnectionInvalidated;
 - (void) countryCodeDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName;
 - (void) linkDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName;
 - (void) linkQualityDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName
    rssi:(NSInteger)rssi transmitRate:(double)transmitRate;
 - (void) modeDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName;
 - (void) powerStateDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName;
 - (void) scanCacheUpdatedForWiFiInterfaceWithName:(NSString*)interfaceName;
 - (void) ssidDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName;
@end

// Mac implementation of PosixNetworks.
//
// General network state is read from the SCDynamicStore API - refer to the
// schemas defined here:
//   https://developer.apple.com/library/archive/documentation/Networking/Conceptual/SystemConfigFrameworks/SC_UnderstandSchema/SC_UnderstandSchema.html#//apple_ref/doc/uid/TP40001065-CH203-CHDIHDCG
//
// Wireless SSIDs are read using the Core WLAN API.
class MacNetworks : public NetworkMonitor
{
public:
    // This value type is used to hold a CWInterface reference from
    // readWifiSsids().  The CWInterface object is an NSObject and is reference
    // counted by ARC, but there are reports of nuanced cases when ARC fails to
    // apply to Objective-C references held by C++ template/polymorph/etc.
    // Putting this in an object with an explicit destructor should be friendly
    // to ARC.
    class WifiConfig
    {
    public:
        CWInterface *pInterface;
    };
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
    // Check all WiFi interfaces and build a map of interface names to
    // CWInterface objects, which we'll use to find SSIDs and encryption modes.
    std::unordered_map<QString, WifiConfig> readWifiConfig();
    // Read interface media types, MTUs, etc. from System Configuration
    struct ScNetworkItfInfo
    {
        NetworkConnection::Medium medium;
        unsigned mtu;
    };
    std::unordered_map<QString, ScNetworkItfInfo> readScNetworkInterfaces();

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
    // Core WLAN client used to read network SSIDs and detect changes
    CWWiFiClient *_pWifiClient;
    // CWWiFiClient.delegate is a weak property, we have to keep the delegate
    // alive here
    MacNetworkWiFiDelegate *_pWifiDelegate;
    // Socket used to receive signals from the WiFi delegate
    kapps::core::PosixFd _delegateReceiveSocket;
    // Notifier for that socket
    nullable_t<QSocketNotifier> _pDelegateNotifier;
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

    _pWifiClient = [[CWWiFiClient alloc] init];
    _pWifiDelegate = [MacNetworkWiFiDelegate alloc];
    _delegateReceiveSocket = [_pWifiDelegate createSockets];
    _pWifiClient.delegate = _pWifiDelegate;

    [_pWifiClient startMonitoringEventWithType:CWEventTypeLinkDidChange error:nil];
    [_pWifiClient startMonitoringEventWithType:CWEventTypeModeDidChange error:nil];
    [_pWifiClient startMonitoringEventWithType:CWEventTypeSSIDDidChange error:nil];

    // Set the receive socket file descriptor as non-blocking so we can read all
    // outstanding signals at once.
    _delegateReceiveSocket.applyNonblock();
    _pDelegateNotifier.emplace(_delegateReceiveSocket.get(),
                               QSocketNotifier::Type::Read, nullptr);
    connect(&_pDelegateNotifier.get(), &QSocketNotifier::activated, this,
            [this]()
            {
                qInfo() << "Re-read connections due to Wi-Fi event";
                // Discard all signals (deduplicate multiple changes)
                unsigned char dummy;
                while(::read(_delegateReceiveSocket.get(), &dummy, sizeof(dummy)) > 0);
                readConnections();
            });

    readConnections();
}

MacNetworks::~MacNetworks()
{
    // Make sure the delegate is removed here in case the CWWiFiClient hangs
    // around for any reason
    _pWifiClient.delegate = nullptr;
    _pWifiDelegate = nullptr;
}

auto MacNetworks::readWifiConfig() -> std::unordered_map<QString, WifiConfig>
{
    NSArray<CWInterface*> *pInterfaces = [_pWifiClient interfaces];

    if(!pInterfaces)
        return {};

    std::unordered_map<QString, WifiConfig> wifiConfigs;

    for(unsigned i=0; i<pInterfaces.count; ++i)
    {
        CWInterface *pInterface = pInterfaces[i];
        if(!pInterface)
            continue;

        QString interfaceId = QString::fromNSString(pInterface.interfaceName);

        if(!interfaceId.isEmpty())
            wifiConfigs.emplace(interfaceId, WifiConfig{pInterface});
    }

    return wifiConfigs;
}

auto MacNetworks::readScNetworkInterfaces()
    -> std::unordered_map<QString, ScNetworkItfInfo>
{
    MacArray interfaces{::SCNetworkInterfaceCopyAll()};

    std::unordered_map<QString, ScNetworkItfInfo> itfs;

    for(const auto &interface : interfaces.view<SCNetworkInterfaceRef>())
    {
        if(!interface)
            continue;

        // macOS gives us the medium as a text name - "Ethernet", "IEEE80211",
        // etc.  We've observed that "Ethernet" can occur for many non-Ethernet
        // devices as well - "Thunderbolt Bridge", "Bluetooth PAN", etc., but we
        // still treat this as "Wired" as a reasonable default for those
        // interfaces.
        //
        // The media types can be found in SCNetworkInterface.c:
        // - https://opensource.apple.com/source/configd/configd-453.18/SystemConfiguration.fproj/SCNetworkInterface.c.auto.html
        //
        // ("Wired" makes sense for Thunderbolt Bridge, which might even get
        // the default route when using internet connection sharing, etc.  It
        // doesn't make sense for Bluetooth PAN, but it's unlikely that this
        // would ever have the default route and is still at least a consistent
        // behavior.)
        auto mediumName = QString::fromCFString(::SCNetworkInterfaceGetInterfaceType(interface.get()));
        ScNetworkItfInfo itf{NetworkConnection::Medium::Unknown, 0};
        if(mediumName == QStringLiteral("Ethernet"))
            itf.medium = NetworkConnection::Medium::Wired;
        else if(mediumName == QStringLiteral("IEEE80211"))
            itf.medium = NetworkConnection::Medium::WiFi;
        int mtuCur{}, mtuMin{}, mtuMax{};
        if(::SCNetworkInterfaceCopyMTU(interface.get(), &mtuCur, &mtuMin, &mtuMax) && mtuCur > 0)
            itf.mtu = static_cast<unsigned>(mtuCur);
        itfs.emplace(QString::fromCFString(::SCNetworkInterfaceGetBSDName(interface.get())),
                      std::move(itf));
    }

    return itfs;
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
    auto scNetItfInfo = readScNetworkInterfaces();
    auto wifiConfigs = readWifiConfig();

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
        MacArray ipv4SubnetMasks = ipConfig._ipv4.getValueObj(CFSTR("SubnetMasks")).as<CFArrayRef>();
        MacString ipv4ServiceRouterIp = ipConfig._ipv4.getValueObj(CFSTR("Router")).as<CFStringRef>();

        MacArray ipv6Addrs = ipConfig._ipv6.getValueObj(CFSTR("Addresses")).as<CFArrayRef>();
        MacArray ipv6PrefixLengths = ipConfig._ipv6.getValueObj(CFSTR("PrefixLength")).as<CFArrayRef>();
        MacString ipv6ServiceRouterIp = ipConfig._ipv6.getValueObj(CFSTR("Router")).as<CFStringRef>();

        std::vector<std::pair<kapps::core::Ipv4Address, unsigned>> addressesIpv4;
        addressesIpv4.reserve(ipv4Addrs.getCount());
        for(CFIndex i=0; i<ipv4Addrs.getCount(); ++i)
        {
            MacString addrStr{ipv4Addrs.getObjAtIndex(i).as<CFStringRef>()};
            kapps::core::Ipv4Address addr{addrStr.toStdString()};

            // The subnet masks are supposed to correspond to the addresses.
            kapps::core::Ipv4Address mask{};
            if(i<ipv4SubnetMasks.getCount())
            {
                MacString maskStr{ipv4SubnetMasks.getObjAtIndex(i).as<CFStringRef>()};
                mask = kapps::core::Ipv4Address{maskStr.toStdString()};
            }
            // If we fail to get a subnet mask for some reason, use /32, so the
            // local address is still known.
            if(mask == kapps::core::Ipv4Address{})
                mask = kapps::core::Ipv4Address{0xFFFFFFFF};

            if(addr != kapps::core::Ipv4Address{})
                addressesIpv4.push_back({addr, mask.getSubnetMaskPrefix()});
        }
        std::vector<std::pair<kapps::core::Ipv6Address, unsigned>> addressesIpv6;
        addressesIpv6.reserve(ipv6Addrs.getCount());
        for(CFIndex i=0; i<ipv6Addrs.getCount(); ++i)
        {
            MacString addrStr{ipv6Addrs.getObjAtIndex(i).as<CFStringRef>()};
            kapps::core::Ipv6Address addr{addrStr.toStdString()};

            // Prefix lengths correspond to the addresses array.  If we can't
            // find it for some reason, use /128.
            unsigned prefix = 128;
            if(i<ipv6PrefixLengths.getCount())
            {
                auto prefixCfNum{ipv6PrefixLengths.getObjAtIndex(i).as<CFNumberRef>()};
                int prefixSigned = 128;
                if(prefixCfNum &&
                   CFNumberGetValue(prefixCfNum.get(), kCFNumberIntType, &prefixSigned))
                {
                    prefix = static_cast<unsigned>(prefixSigned);
                }
            }

            if(addr != kapps::core::Ipv6Address{})
                addressesIpv6.push_back({addr, prefix});
        }

        // On some versions of macOS Catalina the "Router" field is not present in the service data
        // This is very likely a bug in Catalina, nonetheless we work around the absense of this field
        // by falling back to the Global IP state which appears to always have the Router field set.
        // If this bug is fixed in the future we can remove all the code below and just use the ipv{4,6}ServiceRouterIp
        // directly when setting the ipv{4,6}RouterAddr.
        // Note: we only care about setting the router for the Default network.
        auto getRouterIp = [&](const QString &serviceRouterIp, const QString &ipVersion, bool isDefault) {
            if(serviceRouterIp.isEmpty() && isDefault)
            {
                QString globalRouterIp = getGlobalRouterIp(ipVersion);
                qWarning() << QStringLiteral("Router %1 address not found in service data, falling back to Global Router address: %2").arg(ipVersion, globalRouterIp);
                return globalRouterIp;
            }
            else
            {
                return serviceRouterIp;
            }
        };

        QString ipv4RouterIp = getRouterIp(ipv4ServiceRouterIp.toQString(), "IPv4", ipConfig._defIpv4);
        QString ipv6RouterIp = getRouterIp(ipv6ServiceRouterIp.toQString(), "IPv6", ipConfig._defIpv6);

        kapps::core::Ipv4Address ipv4RouterAddr{ipv4RouterIp.toStdString()};
        kapps::core::Ipv6Address ipv6RouterAddr{ipv6RouterIp.toStdString()};

        Q_ASSERT(!ipConfigEntry.first.isEmpty());    // Didn't put empty interfaces in this map

        connectionInfo.emplace_back(ipConfigEntry.first,
                                    NetworkConnection::Medium::Unknown,
                                    ipConfig._defIpv4,
                                    ipConfig._defIpv6, ipv4RouterAddr,
                                    ipv6RouterAddr,
                                    std::move(addressesIpv4),
                                    std::move(addressesIpv6), 0, 0);
        auto &connection = connectionInfo.back();

        auto itWifiConfig = wifiConfigs.find(ipConfigEntry.first);

        if(itWifiConfig != wifiConfigs.end() && itWifiConfig->second.pInterface)
        {
            CWInterface *pItf = itWifiConfig->second.pInterface;

            // Store the interfaceName CFStringRef in a MacString so it's properly managed and released
            MacString interfaceName{connection.networkInterface().toCFString()};

            // This is the prior (possibly deprecated or just bugged approach) to retrieve SSID info about the current Wifi interface
            // We keep it here in case it's just a bug so we can restore it later if possible
            // QByteArray ssidName{_dynStore.ssidFromInterface(interfaceName.get()).toUtf8QByteArray()};

            // Given the interfaceName, retrieve the associated SSID as a UTF-8 QByteArray
            // We're doing this using an unsatisfactory approach (running an external network tool) since Apple have
            // deprecated all the APIs we've used before and have now even deprecated the DynStore we were using previously
            // In order to do this the 'Apple way' we need to assign the com.apple.security.personal-information.location entitlement
            // to the PIA daemon - but this is non trivial and it requires a total app bundle restructuring. So we use these band-aid approaches
            // until our hand is forced.
            QByteArray fullOutput = Exec::bashWithOutput(QStringLiteral("networksetup -getairportnetwork %1").arg(connection.networkInterface())).toUtf8();
            QByteArray ssidName;
            // Find the last occurrence of ": " and extract everything after it. We can't split on spaces (to get the last 'word')
            // as SSIDs can have spaces in them.
            int index = fullOutput.lastIndexOf(": ");
            if(index != -1)
            {
                ssidName = fullOutput.mid(index + 2); // +2 to skip over the ": " itself
                qInfo() << "Found ssidName using networksetup" << QString{ssidName};
            }
            else
            {
                qInfo() << "Unable to find the ssidName using networksetup";
            }

            if(!ssidName.isEmpty())
            {
                // parseWifiSsid() expects a C string and a length - so we use constData to get a const char*
                // parseWifiSsid validates the string - checking for length, encoding and embedded null chars
                connection.parseWifiSsid(ssidName.constData(), ssidName.size());
                connection.wifiAssociated(true);

                // Note the special pItf.security syntax even though pItf is a CWInterface*
                // This is special objective-c syntax, equivalent to using: [pItf security]
                // It is not a member invocation, hence why pItf->security doesn't work. It's obj-c++ magic.
                switch(pItf.security)
                {
                    default:
                    case kCWSecurityNone:
                    case kCWSecurityUnknown:
                    // It's not clear what kCWSecurityEnterprise is - it does
                    // not seem to be documented at all.
                    case kCWSecurityEnterprise:
                        // Default is "not encrypted" for any future modes we're not
                        // aware of
                        break;
                    case kCWSecurityDynamicWEP:
                    case kCWSecurityWEP:
                    case kCWSecurityWPA2Enterprise:
                    case kCWSecurityWPA2Personal:
                    case kCWSecurityWPAEnterprise:
                    case kCWSecurityWPAEnterpriseMixed:
                    case kCWSecurityWPAPersonal:
                    case kCWSecurityWPAPersonalMixed:
                    // WPA3 constants aren't available in the 10.14 SDK that we
                    // currently build with, but are supported by PIA
                    case 12: //kCWSecurityWPA3Enterprise
                    case 11: //kCWSecurityWPA3Personal
                    case 13: //kCWSecurityWPA3Transition
                        connection.wifiEncrypted(true);
                        break;
                }
            }
        }

        auto itInfo = scNetItfInfo.find(ipConfigEntry.first);
        if(itInfo != scNetItfInfo.end())
        {
            connection.medium(itInfo->second.medium);
            // macOS does not have separate IPv4/IPv6 MTUs
            connection.mtu4(itInfo->second.mtu);
            connection.mtu6(itInfo->second.mtu);
        }

        // Expose the primary service key
        connection.macosPrimaryServiceKey(defIpv4SvcKey);
    }

    updateNetworks(std::move(connectionInfo));
}

#pragma clang diagnostic ignored "-Wunused-parameter"

// We don't care about a lot of these notifications; the ones we do care about
// call MacNetworks::readConnections().
@implementation MacNetworkWiFiDelegate {}
 - (kapps::core::PosixFd) createSockets
{
    auto [sigSock, rcvSock] = kapps::core::createSocketPair();
    signalSocket = std::move(sigSock);
    qInfo() << "will signal with fd" << signalSocket.get();
    return std::move(rcvSock);
}
 - (void) signalMacNetworks
{
    // Write to the socket to signal MacNetworks, the actual data does not
    // matter.  Errors writing are ignored.
    unsigned char zero = 0;
    if(signalSocket)
        ::write(signalSocket.get(), &zero, sizeof(zero));
}
 - (void) bssidDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName {}
 - (void) clientConnectionInterrupted {}
 - (void) clientConnectionInvalidated {}
 - (void) countryCodeDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName {}
 - (void) linkDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName
{
    [self signalMacNetworks];
}
 - (void) linkQualityDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName
    rssi:(NSInteger)rssi transmitRate:(double)transmitRate {}
 - (void) modeDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName
{
    [self signalMacNetworks];
}
 - (void) powerStateDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName {}
 - (void) scanCacheUpdatedForWiFiInterfaceWithName:(NSString*)interfaceName {}
 - (void) ssidDidChangeForWiFiInterfaceWithName:(NSString*)interfaceName
{
    [self signalMacNetworks];
}
@end
