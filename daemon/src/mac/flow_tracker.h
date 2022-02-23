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

#include "mac/packet.h"
#include <algorithm>
#include <unordered_map>
#include <deque>
#include <string.h>

template <typename AddressType>
class PacketFlow
{
public:
    template <typename PacketType>
    PacketFlow(const PacketType &packet)
    : _sourceAddress{packet.sourceAddress()}
    , _sourcePort{packet.sourcePort()}
    , _destAddress{packet.destAddress()}
    , _destPort{packet.destPort()}
    , _protocol{packet.protocol()}
    {}

    PacketFlow(
        const AddressType& sourceAddress,
        std::uint16_t sourcePort,
        const AddressType& destAddress,
        std::uint16_t destPort,
        std::uint8_t protocol)
        : _sourceAddress{sourceAddress}
        , _sourcePort{sourcePort}
        , _destAddress{destAddress}
        , _destPort{destPort}
        , _protocol{protocol}
    {}

    bool operator==(const PacketFlow &rhs) const
    {
         return inAddrEqual(_sourceAddress, rhs._sourceAddress) &&
            _sourcePort == rhs._sourcePort &&
            inAddrEqual(_destAddress, rhs._destAddress) &&
            _destPort == rhs._destPort;
    }

    bool operator!=(const PacketFlow &rhs) const {return !(*this == rhs);}

    const AddressType &sourceAddress() const {return _sourceAddress;}
    std::uint16_t sourcePort() const {return _sourcePort;}
    const AddressType &destAddress() const {return _destAddress;}
    std::uint16_t destPort() const {return _destPort;}
    std::uint8_t protocol() const {return _protocol;}

private:
    // Ipv4
    bool inAddrEqual(std::uint32_t lhs, std::uint32_t rhs) const
    {
        return lhs == rhs;
    }

    // Ipv6
    bool inAddrEqual(const in6_addr &lhs, const in6_addr &rhs) const
    {
        return lhs.__u6_addr.__u6_addr32[0] == rhs.__u6_addr.__u6_addr32[0] &&
            lhs.__u6_addr.__u6_addr32[1] == rhs.__u6_addr.__u6_addr32[1] &&
            lhs.__u6_addr.__u6_addr32[2] == rhs.__u6_addr.__u6_addr32[2] &&
            lhs.__u6_addr.__u6_addr32[3] == rhs.__u6_addr.__u6_addr32[3];
    }

private:
    AddressType _sourceAddress;
    std::uint16_t _sourcePort;
    AddressType _destAddress;
    std::uint16_t _destPort;
    std::uint8_t _protocol;
};

using PacketFlow4 = PacketFlow<std::uint32_t>;
using PacketFlow6 = PacketFlow<in6_addr>;



inline std::size_t hashFields() {return 0;}

template<class T, class... Ts>
std::size_t hashFields(const T &first, const Ts &...rest)
{
    // The shuffle constant is derived from the golden ratio - first 64 or 32
    // bits of the fractional part.
    static const std::size_t shuffleConstant{ (sizeof(std::size_t) == sizeof(std::uint64_t)) ?
        static_cast<std::size_t>(0x9e3779b97f4a7c15) :    // 64-bit
        0x9e3779b9};            // 32-bit
    std::size_t result{hashFields(rest...)};
    result ^= std::hash<T>{}(first) + shuffleConstant + (result<<6) + (result >>2);
    return result;
}

namespace std
{
    template <>
    struct hash<PacketFlow4>
    {
        std::size_t operator()(const PacketFlow4 &packetFlow) const
        {
            return hashFields(packetFlow.sourceAddress(),
                packetFlow.sourcePort(), packetFlow.destAddress(),
                packetFlow.destPort(), packetFlow.protocol());
        }
    };
}

namespace std
{
    template <>
    struct hash<in6_addr>
    {
        std::size_t operator()(const in6_addr &addr) const
        {
            return hashFields(addr.__u6_addr.__u6_addr32[0],
                addr.__u6_addr.__u6_addr32[1],
                addr.__u6_addr.__u6_addr32[2],
                addr.__u6_addr.__u6_addr32[3]);
        }
    };
}

namespace std
{
    template <>
    struct hash<PacketFlow6>
    {
        std::size_t operator()(const PacketFlow6 &packetFlow) const
        {
            return hashFields(packetFlow.sourceAddress(),
                packetFlow.sourcePort(), packetFlow.destAddress(),
                packetFlow.destPort(), packetFlow.protocol());
        }
    };
}

template <typename KeyType_T, typename ValueType_T>
class ConstrainedHash
{
public:
    using KeyType = KeyType_T;
    using ValueType = ValueType_T;

    ConstrainedHash(size_t maxSize)
    : _maxSize{maxSize}
    {
    }

public:
    void insert(const std::pair<KeyType_T, ValueType_T> &pair)
    {
        if(_orderedKeys.size() >= _maxSize)
        {
            _packetMap.erase(_orderedKeys.front());
            _orderedKeys.pop_front();
        }

        _orderedKeys.push_back(pair.first);
        _packetMap.insert(pair);
    }

    bool contains(const KeyType_T &key) const
    {
        return _packetMap.count(key) != 0;
    }

    ValueType_T &at(const KeyType_T &key)
    {
        return _packetMap.at(key);
    }

    size_t size() const { return _orderedKeys.size(); }

private:
    size_t _maxSize;
    std::deque<KeyType_T> _orderedKeys;
    std::unordered_map<KeyType_T, ValueType_T> _packetMap;
};

class FlowTracker
{
public:
    enum
    {
        // The number of flows to keep track of.
        // If we get a new flow beyond this window size, then the oldest flow is dropped
        WindowSize = 50,

        // The number of repeated flows we allow within WindowSize
        // before dropping the packet
        MaxRepeatedFlows = 10
    };

    enum TrackingResult
    {
        WillNotTrack,
        NormalFlow,
        RepeatedFlow
    };

    struct Info
    {
        size_t count{0};
    };

    FlowTracker(size_t windowSize = WindowSize, size_t maxRepeatedFlows = MaxRepeatedFlows)
    : _maxWindowSize{windowSize}
    , _maxRepeatedFlows{maxRepeatedFlows}
    , _flowMap4{_maxWindowSize}
    , _flowMap6{_maxWindowSize}
    {}

private:
    template <typename PacketType, typename FlowMap>
    TrackingResult trackImpl(const PacketType &flow, FlowMap &flowMap)
    {
        if(!(flow.protocol() == IPPROTO_TCP || flow.protocol() == IPPROTO_UDP))
        {
            // Not a tcp/udp packet, so no 'flow' to track
            return WillNotTrack;
        }

        if(!flowMap.contains(flow))
        {
            // Track the new flow
            flowMap.insert({flow, Info{}});
        }

        Info &info = flowMap.at(flow);

        // We've just seen this flow (again), so increment the count
        info.count++;

        if(info.count > _maxRepeatedFlows)
        {
            // This flow has appeared too many times,
            // classify it as 'repeated' (we probably want to drop associated packets)
            return RepeatedFlow;
        }
        else
        {
            // This flow has not yet reached the repeated threshold. We should probably
            // allow packets associated with this flow to continue.
            return NormalFlow;
        }
    }

public:
    TrackingResult track(const Packet &packet)
    {
        PacketFlow4 flow{packet};
        return track(flow);
    }

    TrackingResult track(const Packet6 &packet)
    {
        PacketFlow6 flow{packet};
        return track(flow);
    }

    TrackingResult track(const PacketFlow4 &flow) { return trackImpl(flow, _flowMap4); }
    TrackingResult track(const PacketFlow6 &flow) { return trackImpl(flow, _flowMap6); }

private:
    size_t _maxWindowSize;
    size_t _maxRepeatedFlows;
    ConstrainedHash<PacketFlow4, Info> _flowMap4;
    ConstrainedHash<PacketFlow6, Info> _flowMap6;
};
