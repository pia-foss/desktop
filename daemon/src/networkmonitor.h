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

#ifndef NETWORKMONITOR_H
#define NETWORKMONITOR_H

#include <common/src/common.h>
#include <kapps_core/src/ipaddress.h>
#include <QHostAddress>
#include <vector>

// NetworkConnection represents a connection to a given network.  These objects
// are returned by NetworkMonitor to identify the current network connections.
class NetworkConnection : public kapps::core::OStreamInsertable<NetworkConnection>
{
    Q_GADGET

private:
    static QString parseSsidWithCodec(const char *data, std::size_t len,
                                      const char *codec);

public:
    // Type of interface used for this connection - users can create rules that
    // apply to specific types of connections.
    enum class Medium
    {
        // "Wired" is generally Ethernet but in principle could apply to other
        // wired interface types if they're encountered.
        Wired,
        // WiFi specifically refers to Wi-Fi and not any wireless interface,
        // because this type also provides the wireless SSID and whether the
        // connection is encrypted.  Other future types might include mobile
        // data (a cellular modem present in the device) or tethering via
        // Bluetooth, etc.
        WiFi,
        // Any other medium is reported as Unknown.
        Unknown
    };
    Q_ENUM(Medium);

public:
    NetworkConnection() : NetworkConnection{{}, Medium::Unknown, false, false,
                                            {}, {}, {}, {}, 0, 0} {}
    // Create NetworkConnection.  Any of the fields could be empty (though it is
    // unusual for the interface to be empty).
    NetworkConnection(const QString &networkInterface,
                      Medium medium,
                      bool defaultIpv4, bool defaultIpv6,
                      const kapps::core::Ipv4Address &gatewayIpv4,
                      const kapps::core::Ipv6Address &gatewayIpv6,
                      std::vector<std::pair<kapps::core::Ipv4Address, unsigned>> addressesIpv4,
                      std::vector<std::pair<kapps::core::Ipv6Address, unsigned>> addressesIpv6,
                      unsigned mtu4, unsigned mtu6);

public:
    bool operator==(const NetworkConnection &other) const
    {
        // Address vectors can be compared directly since the constructor sorts
        // them
        return networkInterface() == other.networkInterface() &&
               medium() == other.medium() &&
               wifiAssociated() == other.wifiAssociated() &&
               wifiEncrypted() == other.wifiEncrypted() &&
               wifiSsid() == other.wifiSsid() &&
               defaultIpv4() == other.defaultIpv4() &&
               defaultIpv6() == other.defaultIpv6() &&
               gatewayIpv4() == other.gatewayIpv4() &&
               gatewayIpv6() == other.gatewayIpv6() &&
               addressesIpv4() == other.addressesIpv4() &&
               addressesIpv6() == other.addressesIpv6() &&
               mtu4() == other.mtu4() &&
               mtu6() == other.mtu6();
    }
    bool operator!=(const NetworkConnection &other) const {return !(*this == other);}
    // NetworkConnections can be sorted in order to compare sets of network
    // connections.
    bool operator<(const NetworkConnection &other) const;

private:
    // Try to parse Wifi SSID data (used by parseWifiSsid()), returns false
    // without modifying wifiSsid() if the data can't be represented as text.
    bool tryParseWifiSsid(const char *data, std::size_t len);

public:
    void trace(std::ostream &os) const;

    // The interface used to connect to this network.  On Windows this is a
    // device GUID; on Mac and Linux it is an interface name.  This isn't
    // necessarily unique among connections.
    const QString &networkInterface() const {return _networkInterface;}
    void networkInterface(QString networkInterface) {_networkInterface = networkInterface;}

    // The medium used by this interface
    Medium medium() const {return _medium;}
    void medium(Medium medium) {_medium = medium;}

    // If this is a Wi-Fi interface, whether it is associated with a network.
    //
    // IMPORTANT: Wi-Fi state is reported asynchronously on all platforms - a
    // network can become the default before its Wi-Fi association is known (and
    // it may still appear to be "associated" briefly after disconnecting.
    //
    // The automation rules engine avoids applying any rules when the current
    // network is a Wi-Fi network that does not appear to be associated, which
    // causes it to wait until the Wi-Fi association is known to take any
    // action.
    bool wifiAssociated() const {return _wifiAssociated;}
    void wifiAssociated(bool wifiAssociated) {_wifiAssociated = wifiAssociated;}

    // If this is a Wi-Fi interface that's currently associated, whether the
    // connection is encrypted.  Even weak encryption like WEP counts for this
    // for consistent behavior across all PIA platforms.
    bool wifiEncrypted() const {return _wifiEncrypted;}
    void wifiEncrypted(bool wifiEncrypted) {_wifiEncrypted = wifiEncrypted;}

    // The SSID if this is an associated Wi-Fi connection and the SSID is
    // representable as text - empty string otherwise.
    //
    // Technically, SSIDs are arbitrary 0-32 byte values.  Only non-empty SSIDs
    // that are valid UTF-8 or Latin-1 can be represented as text.  We do not
    // distinguish UTF-8 and Latin-1; different encodings of the same text are
    // considered the same.
    const QString &wifiSsid() const {return _wifiSsid;}
    void wifiSsid(QString wifiSsid) {_wifiSsid = wifiSsid;}
    // Parse raw SSID data and store it in wifiSsid().
    //
    // SSIDs are converted to text as follows:
    // - If the SSID is empty, the result is an empty string (which can't be
    //   matched)
    // - Otherwise, if the SSID is valid printable UTF-8, it is parsed as UTF-8.
    //   ("Printable" means it cannot contain any control characters, including
    //   nulls.)
    // - Otherwise, if the SSID is valid printable Latin-1, it is parsed as
    //   Latin-1.
    // - Otherwise, the SSID can't be represented and can't be matched.
    //
    // Stores the SSID if the SSID was a valid and non-empty string.  Otherwise,
    // for an empty SSID or data that can't be represented as text, wifiSsid()
    // is cleared and the unrepresentable data are traced.
    void parseWifiSsid(const char *data, std::size_t len);

    // macOS only - the unique identifier for the primary IPv4 service
    const QString &macosPrimaryServiceKey() const {return _macosPrimaryServiceKey;}
    void macosPrimaryServiceKey(QString key) {_macosPrimaryServiceKey = key;}

    // Whether this connection is the default for IPv4 and/or IPv6.
    bool defaultIpv4() const {return _defaultIpv4;}
    void defaultIpv4(bool defaultIpv4) {_defaultIpv4 = defaultIpv4;}
    bool defaultIpv6() const {return _defaultIpv6;}
    void defaultIpv6(bool defaultIpv6) {_defaultIpv6 = defaultIpv6;}

    // The IPv4 gateway address of this network connection, if present.
    const kapps::core::Ipv4Address &gatewayIpv4() const {return _gatewayIpv4;}
    void gatewayIpv4(const kapps::core::Ipv4Address &gatewayIpv4) {_gatewayIpv4 = gatewayIpv4;}

    // The IPv6 gateway address of this network connection, if present.
    const kapps::core::Ipv6Address &gatewayIpv6() const {return _gatewayIpv6;}
    void gatewayIpv6(const kapps::core::Ipv6Address &gatewayIpv6) {_gatewayIpv6 = gatewayIpv6;}

    // The local IPv4 address(es) of this network connection - zero or more.
    // Provides both the local address and the network prefix length.
    // It's unusual for a connection to have more than one IPv4 address, but
    // platforms don't guarantee this; it's possible.
    const std::vector<std::pair<kapps::core::Ipv4Address, unsigned>> &addressesIpv4() const {return _addressesIpv4;}
    void addressesIpv4(std::vector<std::pair<kapps::core::Ipv4Address, unsigned>> addressesIpv4) {_addressesIpv4 = std::move(addressesIpv4);}

    // The globally-routable IPv6 address(es) of this network connection - zero
    // or more.  Like addressesIpv4(), includes network prefix lengths also.
    //
    // Link-local addresses are excluded, because we don't get these on all
    // platforms (the Mac implementation does not have them since they're not in
    // the dynamic store).  They're not currently needed.
    //
    // Unlike IPv4, it's common to have more than one IPv6 address (there is
    // usually at least a link-local address and a globally routable address,
    // and there may be additional global addresses that rotate).
    const std::vector<std::pair<kapps::core::Ipv6Address, unsigned>> &addressesIpv6() const {return _addressesIpv6;}
    void addressesIpv6(std::vector<std::pair<kapps::core::Ipv6Address, unsigned>> addressesIpv6) {_addressesIpv6 = std::move(addressesIpv6);}

    // MTU of this network connection.  Separate IPv4 and IPv6 MTUs are provided,
    // because Windows maintains separate IPv4 and IPv6 MTUs.  (On other OSes,
    // mtu4 and mtu6 are always the same for a given connection.)
    unsigned mtu4() const { return _mtu4; }
    void mtu4(unsigned mtu4) { _mtu4 = mtu4; }
    unsigned mtu6() const { return _mtu6; }
    void mtu6(unsigned mtu6) { _mtu6 = mtu6; }

private:
    QString _networkInterface;
    Medium _medium;
    bool _wifiAssociated, _wifiEncrypted;
    QString _wifiSsid;
    bool _defaultIpv4, _defaultIpv6;
    kapps::core::Ipv4Address _gatewayIpv4;
    kapps::core::Ipv6Address _gatewayIpv6;
    std::vector<std::pair<kapps::core::Ipv4Address, unsigned>> _addressesIpv4;
    std::vector<std::pair<kapps::core::Ipv6Address, unsigned>> _addressesIpv6;
    QString _macosPrimaryServiceKey;
    unsigned _mtu4, _mtu6;
};

// NetworkMonitor monitors the current network connections and identifies the
// networks that we're currently connected to, as a list of NetworkConnection
// objects.
//
// Whenever the connected networks change, it emits the networksChanged()
// signal.
//
// Currently, only the default IPv4 and/or IPv6 connections are required -
// connections other than the defaults do not need to be reported.  The Mac and
// Linux implementations provide all networks, but the Windows implementation
// only provides the defaults.
class NetworkMonitor : public QObject
{
    Q_OBJECT

public:
    NetworkMonitor(){}

protected:
    void updateNetworks(std::vector<NetworkConnection> newNetworks);

public:
    const std::vector<NetworkConnection> &getNetworks() const {return _lastNetworks;}

signals:
    // The network connections have changed
    void networksChanged(const std::vector<NetworkConnection> &newNetworks);

private:
    std::vector<NetworkConnection> _lastNetworks;
};

#endif
