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

#include <kapps_net/src/mac/flow_tracker.h>
#include <kapps_core/src/ipaddress.h>
#include <QtTest>
#include <QHostAddress>
#include <unordered_set>

using FlowTracker = kapps::net::FlowTracker;
using PacketFlow4 = kapps::net::PacketFlow4;
using PacketFlow6 = kapps::net::PacketFlow6;

class tst_flow_tracker : public QObject
{
    Q_OBJECT

private slots:
    void testTrack()
    {
        const std::uint32_t sourceAddress1{QHostAddress{"192.168.1.2"}.toIPv4Address()};
        const std::uint32_t destAddress1{QHostAddress{"1.1.1.1"}.toIPv4Address()};

        {
            // Repeated packets > MaxRepeatedFlows triggers RepeatedFlow
            FlowTracker ft{2, 2}; // MaxWindowSize, MaxRepeatedFlows
            PacketFlow4 flow{sourceAddress1, 100,  destAddress1, 200, IPPROTO_TCP};

            QCOMPARE(ft.track(flow), FlowTracker::NormalFlow);
            QCOMPARE(ft.track(flow), FlowTracker::NormalFlow);
            // flow appears 3 times, so return RepeatedFlow
            QCOMPARE(ft.track(flow), FlowTracker::RepeatedFlow);
        }

        {
            FlowTracker ft{2, 2}; // MaxWindowSize, MaxRepeatedFlows
            PacketFlow4 flow1{sourceAddress1, 100,  destAddress1, 200, IPPROTO_TCP};
            PacketFlow4 flow2{sourceAddress1, 101,  destAddress1, 200, IPPROTO_TCP};
            PacketFlow4 flow3{sourceAddress1, 102,  destAddress1, 200, IPPROTO_TCP};

            QCOMPARE(ft.track(flow1), FlowTracker::NormalFlow);
            QCOMPARE(ft.track(flow2), FlowTracker::NormalFlow);

            // At this point flow1 drops off and is replaced by flow3
            // as we have a windowsize of 2 (meaning we only keep track of 2 flows at most)
            QCOMPARE(ft.track(flow3), FlowTracker::NormalFlow);
            QCOMPARE(ft.track(flow1), FlowTracker::NormalFlow);

            // Even though flow1 has appeared 3 times, it does not generate
            // a repeated flow as it was displaced by flow3
            // so at this point flow1 only has a count of 2
            QCOMPARE(ft.track(flow1), FlowTracker::NormalFlow);
        }
    }

    void testHash4()
    {
        // Make sure all parts of the flow contribute to the IP address
        // correctly.
        PacketFlow4 flow1{0xC0A80001, 57777, 0xAC100001, 443, IPPROTO_TCP};
        PacketFlow4 flow2{0xC0A80002, 57777, 0xAC100001, 443, IPPROTO_TCP};
        PacketFlow4 flow3{0xC0A80001, 57778, 0xAC100001, 443, IPPROTO_TCP};
        PacketFlow4 flow4{0xC0A80001, 57777, 0xAC100002, 443, IPPROTO_TCP};
        PacketFlow4 flow5{0xC0A80001, 57777, 0xAC100001, 444, IPPROTO_TCP};
        PacketFlow4 flow6{0xC0A80001, 57777, 0xAC100001, 443, IPPROTO_UDP};

        // Make sure they're all unique by putting all hashes in a
        // std::unordered_set, then verify that it has 6 distinct entries.
        // (Note that we put the _hashes_ in the set to verify that the
        // hashes are unique, putting the PacketFlow4s in the set wouldn't do
        // this.)
        std::unordered_set<std::size_t> hashes;
        QVERIFY(hashes.insert(std::hash<PacketFlow4>{}(flow1)).second);
        QVERIFY(hashes.insert(std::hash<PacketFlow4>{}(flow2)).second);
        QVERIFY(hashes.insert(std::hash<PacketFlow4>{}(flow3)).second);
        QVERIFY(hashes.insert(std::hash<PacketFlow4>{}(flow4)).second);
        QVERIFY(hashes.insert(std::hash<PacketFlow4>{}(flow5)).second);
        QVERIFY(hashes.insert(std::hash<PacketFlow4>{}(flow6)).second);

        QCOMPARE(hashes.size(), 6u);
    }

    void testHash6()
    {
        // Make sure all parts of the flow contribute to the IP address
        // correctly.
        in6_addr s1{}, s2{}, d1{}, d2{};
        s1.s6_addr[0] = 0xFE;
        s1.s6_addr[1] = 0x80;
        s1.s6_addr[15] = 0x01;  // fe80::1
        s2.s6_addr[0] = 0xFE;
        s2.s6_addr[1] = 0x80;
        s2.s6_addr[15] = 0x02;  // fe80::2
        d1.s6_addr[0] = 0xFC;
        d1.s6_addr[15] = 0x01;  // fc00::1
        d1.s6_addr[0] = 0xFC;
        d1.s6_addr[15] = 0x02;  // fc00::2

        PacketFlow6 flow1{s1, 57777, d1, 443, IPPROTO_TCP};
        PacketFlow6 flow2{s2, 57777, d1, 443, IPPROTO_TCP};
        PacketFlow6 flow3{s1, 57778, d1, 443, IPPROTO_TCP};
        PacketFlow6 flow4{s1, 57777, d2, 443, IPPROTO_TCP};
        PacketFlow6 flow5{s1, 57777, d1, 444, IPPROTO_TCP};
        PacketFlow6 flow6{s1, 57777, d1, 443, IPPROTO_UDP};

        // Make sure they're all unique by putting all hashes in a
        // std::unordered_set, then verify that it has 6 distinct entries.
        // (Note that we put the _hashes_ in the set to verify that the
        // hashes are unique, putting the PacketFlow4s in the set wouldn't do
        // this.)
        std::unordered_set<std::size_t> hashes;
        QVERIFY(hashes.insert(std::hash<PacketFlow6>{}(flow1)).second);
        QVERIFY(hashes.insert(std::hash<PacketFlow6>{}(flow2)).second);
        QVERIFY(hashes.insert(std::hash<PacketFlow6>{}(flow3)).second);
        QVERIFY(hashes.insert(std::hash<PacketFlow6>{}(flow4)).second);
        QVERIFY(hashes.insert(std::hash<PacketFlow6>{}(flow5)).second);
        QVERIFY(hashes.insert(std::hash<PacketFlow6>{}(flow6)).second);

        QCOMPARE(hashes.size(), 6u);
    }
};

QTEST_APPLESS_MAIN(tst_flow_tracker)
#include TEST_MOC
