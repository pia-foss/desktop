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

#include <common/src/common.h>
#line SOURCE_FILE("networkmonitor.cpp")

#include "networkmonitor.h"
#include <QTextCodec>

QString NetworkConnection::parseSsidWithCodec(const char *data, std::size_t len,
                                              const char *codec)
{
    QTextCodec *pCodec = QTextCodec::codecForName(codec);
    Q_ASSERT(pCodec);   // UTF-8 and Latin-1 are provided by Qt
    QTextCodec::ConverterState convState{};
    convState.flags |= QTextCodec::ConversionFlag::IgnoreHeader;
    QString result = pCodec->toUnicode(data, static_cast<int>(len), &convState);
    // If any data were not consumed (UTF-8 data ended in a partial character
    // sequence, for example), or if invalid characters occurred, the result is
    // not valid
    if(convState.remainingChars || convState.invalidChars)
        return {};
    return result;
}

NetworkConnection::NetworkConnection(const QString &networkInterface,
                                     Medium medium,
                                     bool defaultIpv4, bool defaultIpv6,
                                     const kapps::core::Ipv4Address &gatewayIpv4,
                                     const kapps::core::Ipv6Address &gatewayIpv6,
                                     std::vector<std::pair<kapps::core::Ipv4Address, unsigned>> addressesIpv4,
                                     std::vector<std::pair<kapps::core::Ipv6Address, unsigned>> addressesIpv6,
                                     unsigned mtu4, unsigned mtu6)
    : _networkInterface{networkInterface}, _medium{medium},
      _wifiAssociated{false}, _wifiEncrypted{false}, _wifiSsid{},
      _defaultIpv4{defaultIpv4}, _defaultIpv6{defaultIpv6},
      _gatewayIpv4{gatewayIpv4}, _gatewayIpv6{gatewayIpv6},
      _addressesIpv4{std::move(addressesIpv4)},
      _addressesIpv6{std::move(addressesIpv6)}, _mtu4{mtu4}, _mtu6{mtu6}
{
    // Sort the local IP addresses so we can check for equality by just
    // comparing the vectors
    std::sort(_addressesIpv4.begin(), _addressesIpv4.end());
    // Ignore link-local addresses since we don't get them for all platforms
    _addressesIpv6.erase(std::remove_if(_addressesIpv6.begin(), _addressesIpv6.end(),
                                        [](const auto &addr){return addr.first.isLinkLocal();}),
                         _addressesIpv6.end());
    std::sort(_addressesIpv6.begin(), _addressesIpv6.end());
}

bool NetworkConnection::operator<(const NetworkConnection &other) const
{
    auto cmp = networkInterface().compare(other.networkInterface());
    if(cmp != 0)
        return cmp < 0;
    if(medium() != other.medium())
        return static_cast<int>(medium()) < static_cast<int>(other.medium());
    if(wifiAssociated() != other.wifiAssociated())
        return !wifiAssociated();   // false precedes true
    if(wifiEncrypted() != other.wifiEncrypted())
        return !wifiEncrypted();    // false precedes true
    cmp = wifiSsid().compare(other.wifiSsid());
    if(cmp != 0)
        return cmp < 0;
    if(gatewayIpv4() != other.gatewayIpv4())
        return gatewayIpv4() < other.gatewayIpv4();
    if(gatewayIpv6() != other.gatewayIpv6())
        return gatewayIpv6() < other.gatewayIpv6();
    if(addressesIpv4() != other.addressesIpv4())
    {
        return std::lexicographical_compare(addressesIpv4().begin(), addressesIpv4().end(),
                                            other.addressesIpv4().begin(), other.addressesIpv4().end());
    }
    if(addressesIpv6() != other.addressesIpv6())
    {
        return std::lexicographical_compare(addressesIpv6().begin(), addressesIpv6().end(),
                                            other.addressesIpv6().begin(), other.addressesIpv6().end());
    }
    return false;
}

void NetworkConnection::trace(std::ostream &os) const
{
    os << "{" << networkInterface() << ": " << traceEnum(medium());

    if(medium() == Medium::WiFi)
    {
        if(wifiEncrypted())
            os << " enc.";
        if(!wifiSsid().isEmpty())
            os << " ssid:" << wifiSsid();
    }

    if(defaultIpv4())
        os << " def4";
    if(defaultIpv6())
        os << " def6";

    // Gateways and addresses aren't traced in the short summary
    os << "}";
}

bool NetworkConnection::tryParseWifiSsid(const char *data, std::size_t len)
{
    // Empty - nullptr or length of 0
    if(!data || !len)
        return false;

    // Invalid - len out of range for SSID (ensures we can convert to int below)
    if(len > 32)
        return false;

    // There can't be any zero bytes - in both UTF-8 and Latin-1, this results
    // in null characters in the result (which can't be expressed otherwise,
    // overlong UTF-8 encodings of 0 would be rejected by the codec).
    //
    // We don't check for other control characters though, which would be
    // unusual but could occur in the field (for example, ZWJ in emoji
    // sequences, and there are almost certainly people putting emoji in Wi-Fi
    // SSIDs).  Any other control characters should be OK, they just might not
    // display in the UI.
    if(std::any_of(data, data + len, [](const char c){return !c;}))
        return false;

    // Try to decode as UTF-8
    QString result = parseSsidWithCodec(data, len, "UTF-8");
    if(!result.isEmpty())
    {
        wifiSsid(std::move(result));
        return true;
    }

    // Try to decode as Latin-1
    result = parseSsidWithCodec(data, len, "ISO-8859-1");
    if(!result.isEmpty())
    {
        wifiSsid(std::move(result));
        return true;
    }

    return false;
}

void NetworkConnection::parseWifiSsid(const char *data, std::size_t len)
{
    if(!tryParseWifiSsid(data, len))
    {
        // Dump the SSID data, "percent encoding" is a pretty compact way to
        // represent arbitrary binary data, and actual ASCII text is visible
        // directly.
        qWarning() << "Wi-Fi SSID can't be represented as text (" << len
            << "bytes):"
            << QString::fromLatin1(QByteArray::fromRawData(data, len).toPercentEncoding());
        wifiSsid({});
    }
}

void NetworkMonitor::updateNetworks(std::vector<NetworkConnection> newNetworks)
{
    if(newNetworks != _lastNetworks)
    {
        _lastNetworks = std::move(newNetworks);
        emit networksChanged(_lastNetworks);
    }
}
