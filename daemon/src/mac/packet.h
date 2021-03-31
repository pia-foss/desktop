// Copyright (c) 2021 Private Internet Access, Inc.
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
#line HEADER_FILE("mac/packet.h")

#ifndef PACKET_H
#define PACKET_H

#include <vector>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

// Both TCP and UDP are supported - this is the source/dest port part that's
// common to both headers.
struct TransportPortHeader
{
    std::uint16_t sport;
    std::uint16_t dport;
};

class Packet
{
public:
    enum PacketType
    {
        Tcp,
        Udp,
        Other
    };

public:
    static nullable_t<Packet> createFromData(std::vector<unsigned char> data,
                                             unsigned skipBytes);

public:
    Packet(std::vector<unsigned char> data, ip *pIpHdr, TransportPortHeader *pTransportHdr)
        : _data{std::move(data)}, _ipHdr{pIpHdr}, _transportHdr{pTransportHdr}
    {
        // Prepare data for re-injection
        // ip_len and ip_off must be in host order (macOS quirk)
        _ipHdr->ip_len = ntohs(_ipHdr->ip_len);
        _ipHdr->ip_off = ntohs(_ipHdr->ip_off);
        _ipHdr->ip_sum = 0;
        _ipHdr->ip_sum = csum(reinterpret_cast<const std::uint16_t *>(_ipHdr), _ipHdr->ip_len);
    }

    // ip->ip_len
    quint16 len() const { return _ipHdr->ip_len; }

    // ip->ip_p
    PacketType packetType() const;

    uchar protocol() const { return _ipHdr->ip_p; }

    // tcphdr->th_sport
    quint16 sourcePort() const {return _transportHdr ? ntohs(_transportHdr->sport) : 0; }
    // tcphdr->th_dport
    quint16 destPort() const {return _transportHdr ? ntohs(_transportHdr->dport) : 0; }

    quint32 sourceAddress() const { return ntohl(_ipHdr->ip_src.s_addr); }
    quint32 destAddress() const { return ntohl(_ipHdr->ip_dst.s_addr); }

    QString toString() const;

    // Get the raw data for re-injection
    ip * toRaw() const { return _ipHdr; }

private:
    std::uint16_t csum(const std::uint16_t *buf, int words);

private:
    // Actual packet data buffer (_ipHdr and _transportHdr point to this)
    std::vector<unsigned char> _data;
    ip * _ipHdr;
    TransportPortHeader * _transportHdr;
};

class Packet6
{
public:
    enum PacketType
    {
        Tcp,
        Udp,
        Other
    };

public:
    static nullable_t<Packet6> createFromData(std::vector<unsigned char> data,
                                              unsigned skipBytes);
public:
    Packet6(std::vector<unsigned char> data, std::uint8_t transportProtocol,
           ip6_hdr *pIpHdr, TransportPortHeader *pTransportHdr)
        : _data{std::move(data)}, _transportProtocol{transportProtocol},
          _ipHdr{pIpHdr}, _transportHdr{pTransportHdr}
    {
    }

    // _ipHdrr->ip6_nxt (next header)
    PacketType packetType() const;

    uchar protocol() const { return _transportProtocol; }

    // tcphdr->th_sport
    quint16 sourcePort() const {return _transportHdr ? ntohs(_transportHdr->sport) : 0; }
    // tcphdr->th_dport
    quint16 destPort() const {return _transportHdr ? ntohs(_transportHdr->dport) : 0; }

    const in6_addr& sourceAddress() const {return _ipHdr->ip6_src;}
    const in6_addr& destAddress() const {return _ipHdr->ip6_dst;}

    QString toString() const;

    // Get the raw data for re-injection
    ip6_hdr * toRaw() const { return _ipHdr; }

private:
    // Actual packet data buffer (_ipHdr and _transportHdr point to this)
    std::vector<unsigned char> _data;
    std::uint8_t _transportProtocol;
    ip6_hdr * _ipHdr;
    TransportPortHeader * _transportHdr;
};

#endif
