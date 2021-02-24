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

#include "daemon/src/latencytracker.h"
#include "common/src/locations.h"
#include <QtTest>
#include <cassert>
#include <unordered_set>

namespace
{
    enum
    {
        MockPingServerCount = 4
    };

    const QHostAddress localhost{QHostAddress::SpecialAddress::LocalHost};
}

class MockPingServers : public QObject
{
    Q_OBJECT

public:
    MockPingServers()
    {
        // Create some mock servers to listen for pings from LatencyTracker
        _mockServerList.reserve(MockPingServerCount);
        while(_mockPingLocations.size() < MockPingServerCount)
        {
            QSharedPointer<Location> pLocation{new Location{}};

            // Fill in the mock ServerLocation - most of these fields do not
            // matter, just add any VPN server with the loopback address so it
            // will be selected for ICMP latency measurements.
            auto serverId = QStringLiteral("mock-server-%1").arg(_mockServerList.size());
            pLocation->id(serverId);
            pLocation->name(serverId);
            pLocation->country(QStringLiteral("US"));
            pLocation->portForward(false);
            Server mockServer;
            mockServer.ip(QStringLiteral("127.0.0.%1").arg(_mockServerList.size()+1));
            mockServer.commonName("n/a");
            mockServer.wireguardPorts({1337});  // Indicates this has the WireGuard service
            pLocation->servers({std::move(mockServer)});

            _mockServerList[pLocation->id()] = pLocation;
            _mockPingLocations.push_back(pLocation);
        }
    }

public:
    // Get a ServerLocationList describing the mock servers.  (Used to pass the
    // mock servers to LatencyTracker.)
    const LocationsById &mockServerList() const {return _mockServerList;}

    // Get a vector of locations from the mock servers.  (Used to pass the mock
    // servers to LatencyBatch.)
    const std::vector<QSharedPointer<Location>> &mockPingLocations() const {return _mockPingLocations;}

    // Get a set of all mock location IDs - used by unit tests to keep track
    // of the measurements that have been emitted
    std::unordered_set<QString> mockLocationIds() const
    {
        std::unordered_set<QString> ids;
        for(const auto &pLoc : _mockPingLocations)
        {
            Q_ASSERT(pLoc); // Class invariant
            ids.emplace(pLoc->id());
        }
        return ids;
    }

private:
    LocationsById _mockServerList;
    std::vector<QSharedPointer<Location>> _mockPingLocations;
};

// LatencyBatch and LatencyTracker batch their measurements, but the batching is
// timing-dependent.  Using QSignalSpy on the batched measurement signals is
// cumbersome when multiple measurements are expected, and assuming they all
// would arrive in one batch is fragile.
//
// MeasurementSplitter splits up newMeasurements signals into individual
// newMeasurement signals to simplify this logic.
class MeasurementSplitter : public QObject
{
    Q_OBJECT

public:
    MeasurementSplitter(LatencyTracker &tracker);
    MeasurementSplitter(LatencyBatch &batch);

signals:
    void newMeasurement(const QString &locationId, std::chrono::milliseconds latency);

private slots:
    void onNewMeasurements(const LatencyTracker::Latencies &measurements);
};

MeasurementSplitter::MeasurementSplitter(LatencyTracker &tracker)
{
    connect(&tracker, &LatencyTracker::newMeasurements, this,
            &MeasurementSplitter::onNewMeasurements);
}

MeasurementSplitter::MeasurementSplitter(LatencyBatch &batch)
{
    connect(&batch, &LatencyBatch::newMeasurements, this,
            &MeasurementSplitter::onNewMeasurements);
}

void MeasurementSplitter::onNewMeasurements(const LatencyTracker::Latencies &measurements)
{
    for(const auto &measurement : measurements)
        emit newMeasurement(measurement.first, measurement.second);
}

class tst_latencytracker : public QObject
{
    Q_OBJECT

private slots:
    //Verify that LatencyTracker pings all the locations' addresses and then
    //emits latency measurements when the replies are sent
    void locationPings()
    {
        LatencyTracker tracker{};
        MeasurementSplitter splitter{tracker};
        tracker.start();

        auto pendingLocations = _mockServers.mockLocationIds();

        // Send the pings and expect latency measurements
        QSignalSpy latencySpy{&splitter, &MeasurementSplitter::newMeasurement};
        tracker.updateLocations(_mockServers.mockServerList());

        while(!pendingLocations.empty())
        {
            QVERIFY(latencySpy.wait());
            // The latency spy may trigger after more than one signal was
            // emitted
            while(!latencySpy.isEmpty())
            {
                const auto &signalArgs = latencySpy.takeFirst();
                const auto &locId = signalArgs[0].toString();
                // Can't emit the same location more than once
                QVERIFY(pendingLocations.erase(locId) == 1);
            }
        }
    }

    // Verify that location updates with new locations ping the new locations
    // immediately
    void newLocations()
    {
        LatencyTracker tracker{};
        MeasurementSplitter splitter{tracker};
        tracker.start();

        //Pass in the first three locations initially, echo all of these pings
        {
            auto initialLocations = _mockServers.mockServerList();
            // Remove a server so we can add it later
            initialLocations.erase(QStringLiteral("mock-server-0"));

            // Wait for all of the measurements to be emitted
            QSignalSpy initialLatencySpy{&splitter, &MeasurementSplitter::newMeasurement};
            tracker.updateLocations(initialLocations);
            while(initialLatencySpy.size() < MockPingServerCount-1)
                QVERIFY(initialLatencySpy.wait());
        }

        // Now update with a different set of locations.  Add in the location we
        // removed above (which should trigger an immediate measurement), and
        // delete a different location (which should not trigger anything).
        auto updatedLocations = _mockServers.mockServerList();
        updatedLocations.erase(QStringLiteral("mock-server-1"));
        tracker.updateLocations(updatedLocations);

        // We get exactly one measurement
        QSignalSpy latencySpy{&splitter, &MeasurementSplitter::newMeasurement};
        QVERIFY(latencySpy.wait());
        // Wait to make sure we don't get any more measurements that weren't
        // grouped by the spy.  These are localhost pings, so we shouldn't have
        // to wait more than a few ms.
        QVERIFY(!latencySpy.wait(100));

        // We should have gotten exactly one measurement
        QVERIFY(latencySpy.size() == 1);
        // That measurement should be for the one server we added
        const auto &args = latencySpy.takeFirst();
        const auto &id = args[0].toString();
        QCOMPARE(id, QStringLiteral("mock-server-0"));
    }

    //Verify that new locations created before measurements are started will be
    //measured after measurements are enabled
    void delayedNewLocations()
    {
        LatencyTracker tracker{};
        MeasurementSplitter splitter{tracker};

        QSignalSpy measurementSpy{&splitter, &MeasurementSplitter::newMeasurement};

        tracker.updateLocations(_mockServers.mockServerList());

        // This wait *should* time out - we don't expect pings to happen yet.
        // We need to enter a message loop here to be sure, otherwise the events
        // would be queued up anyway, and we couldn't tell whether they occurred
        // before or after the call to start().
        QVERIFY(!measurementSpy.wait(2000));

        // Now, start measurements.  This should emit the pings and measurements
        // now
        tracker.start();
        while(measurementSpy.size() < MockPingServerCount)
        {
            QVERIFY(measurementSpy.wait());
        }
    }

    //If a location is added and deleted again while stopped, no ping should be
    //emitted for it when we start measurements.
    void deletionWhileStopped()
    {
        LatencyTracker tracker{};
        MeasurementSplitter splitter{tracker};
        tracker.start();

        QSignalSpy measurementSpy{&splitter, &MeasurementSplitter::newMeasurement};

        auto initialLocations = _mockServers.mockServerList();
        // Delete a location
        initialLocations.erase(QStringLiteral("mock-server-0"));
        tracker.updateLocations(initialLocations);

        // Expect 3 measurements
        while(measurementSpy.size() < MockPingServerCount-1)
            QVERIFY(measurementSpy.wait());
        measurementSpy.clear();

        // Stop measurements
        tracker.stop();
        // Add and delete the other location
        tracker.updateLocations(_mockServers.mockServerList());
        tracker.updateLocations(initialLocations);

        // Start notifications again.  We don't expect to get any pings or
        // measurements, so this wait *should* time out
        QVERIFY(!measurementSpy.wait(2000));
    }

    //Verify that a LatencyBatch is cleaned up properly under normal
    //circumstances (all addresses are valid and respond)
    void normalCleanup()
    {
        //Create a LatencyBatch with these ping locations, then verify that it
        //destroys itself.
        auto pBatch{new LatencyBatch{_mockServers.mockPingLocations(), this}};
        MeasurementSplitter splitter{*pBatch};
        QSignalSpy measurementSpy{&splitter, &MeasurementSplitter::newMeasurement};
        QSignalSpy destroySpy{pBatch, &QObject::destroyed};

        //Wait until the object is destroyed
        destroySpy.wait();

        //All measurements should be emitted before it destroys itself
        QCOMPARE(measurementSpy.size(), MockPingServerCount);
    }

    // Verify that a LatencyBatch destroys itself correctly when none of the
    // addresses given are valid.  (It should be destroyed immediately, not after
    // the measurement timeout.)
    void invalidCleanup()
    {
        //Bogus locations - no latency servers, unparseable addresses
        QSharedPointer<Location> pBogusAddr{new Location{}};
        pBogusAddr->id(QStringLiteral("mock-bogus-addr"));
        pBogusAddr->name(QStringLiteral("mock-bogus-addr"));
        pBogusAddr->country(QStringLiteral("US"));
        pBogusAddr->portForward(false);
        Server mockServer;
        mockServer.ip(QStringLiteral("bogus_address"));
        mockServer.commonName("n/a");
        mockServer.wireguardPorts({1337});
        pBogusAddr->servers({std::move(mockServer)});
        QSharedPointer<Location> pNoServers{new Location{}};
        pBogusAddr->id(QStringLiteral("mock-no-servers"));
        pBogusAddr->name(QStringLiteral("mock-no-servers"));
        pBogusAddr->country(QStringLiteral("US"));
        pBogusAddr->portForward(false);

        auto pBatch{new LatencyBatch{{pBogusAddr, pNoServers}, this}};
        QSignalSpy destroySpy{pBatch, &QObject::destroyed};

        // If this wait times out, the LatencyBatch probably waited for the
        // measurement timeout to elapse instead of destroying itself
        // immediately.
        destroySpy.wait(1000);
    }

    // Verify that a LatencyBatch destroys itself after the timeout interval if
    // any of the servers never respond
    void unresponsiveCleanup()
    {
        // Prevent responses from being received by using bogus servers with
        // valid but unreachable IP addresses.
        std::vector<QSharedPointer<Location>> mockPingLocations;
        for(int i=0; i<MockPingServerCount; ++i)
        {
            QSharedPointer<Location> pBogusAddr{new Location{}};
            pBogusAddr->id(QStringLiteral("mock-bogus-addr-%1").arg(i));
            pBogusAddr->name(QStringLiteral("mock-bogus-addr-%1").arg(i));
            pBogusAddr->country(QStringLiteral("US"));
            pBogusAddr->portForward(false);
            Server mockServer;
            // IP in documentation range
            mockServer.ip(QStringLiteral("192.0.2.%1").arg(i));
            mockServer.commonName("n/a");
            mockServer.wireguardPorts({1337});
            pBogusAddr->servers({std::move(mockServer)});
            mockPingLocations.push_back(std::move(pBogusAddr));
        }
        auto pBatch{new LatencyBatch{mockPingLocations, this}};
        QSignalSpy measurementSpy{pBatch, &LatencyBatch::newMeasurements};
        QSignalSpy destroySpy{pBatch, &QObject::destroyed};

        //Wait until the object is destroyed
        destroySpy.wait(30000);

        //No measurements should be emitted
        QCOMPARE(measurementSpy.size(), 0);
    }

    //Verify that equivalent IP addresses are found correctly
    void equivalentIpAddresses()
    {
        //Arbitrary IPv4 address - 172.16.254.254
        QHostAddress ipv4{0xAC10FEFE};
        quint8 compatBytes[]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                             0, 0, 0xAC, 0x10, 0xFE, 0xFE};
        quint8 mappedBytes[]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                             0xFF, 0xFF, 0xAC, 0x10, 0xFE, 0xFE};
        QHostAddress ipv4compat{compatBytes}, ipv4mapped{mappedBytes};

        QVector<QHostAddress> expectedAddrs{ipv4, ipv4compat, ipv4mapped};

        auto v4actual = getEquivalentAddresses(ipv4mapped);

        //Note that these will fail if the addresses are returned in a different
        //order, which is actually fine - the test would just need to be updated
        QCOMPARE(getEquivalentAddresses(ipv4), expectedAddrs);
        QCOMPARE(getEquivalentAddresses(ipv4compat), expectedAddrs);
        QCOMPARE(getEquivalentAddresses(ipv4mapped), expectedAddrs);
    }

    //Verify that unique IP addresses are handled correctly
    void uniqueIpAddresses()
    {
        QHostAddress ipv6{"2001:0db8:0000:0042:0000:8a2e:0370:7334"};

        QCOMPARE(getEquivalentAddresses(ipv6), QVector<QHostAddress>{{ipv6}});
    }

private:
    MockPingServers _mockServers;
};

QTEST_GUILESS_MAIN(tst_latencytracker)
#include TEST_MOC
