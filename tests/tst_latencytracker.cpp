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

#include "daemon/src/latencytracker.h"
#include <QtTest>
#include <cassert>

namespace
{
    enum
    {
        MockPingServerCount = 4
    };

    const QHostAddress localhost{QHostAddress::SpecialAddress::LocalHost};
}

//ReceivedPing describes a ping received by MockPingServers.  The
//receivedPing signal is emitted with a ReceivedPing object, which can be
//used to send the echo for the ping.
//
//ReceivedPing references a QUdpSocket owned by MockPingServers, so it must
//not outlive the MockPingServers.
class ReceivedPing
{
public:
    //QMetaType requires the type to have a default constructor.  However,
    //this constructor would violate ReceivedPing's invariants that
    //_pServerSocket and _pLocation are set, so it always throws.  It
    //shouldn't ever be called by these tests.
    ReceivedPing()
    {
        throw std::logic_error{"ReceivedPing can't be default-constructed"};
    }

    ReceivedPing(QUdpSocket *pServerSocket, QHostAddress remoteAddress,
                 quint16 remotePort,
                 QSharedPointer<ServerLocation> pLocation)
        : _pServerSocket{pServerSocket},
          _remoteAddress{std::move(remoteAddress)},
          _remotePort{remotePort},
          _pLocation{std::move(pLocation)}
    {
        Q_ASSERT(_pServerSocket);
        Q_ASSERT(_pLocation);
    }

public:
    //Get the server address that was pinged
    HostPortKey getServerAddress() const
    {
        return {_pServerSocket->localAddress(), _pServerSocket->localPort()};
    }

    //Echo this ping back to the sender
    void echo() const {_pServerSocket->writeDatagram({1, 0x61}, _remoteAddress, _remotePort);}

    //Get the location ID of the server that was pinged
    QString getLocationId() const {return _pLocation->id();}

private:
    //The UDP socket (owned by MockPingServers) that was pinged.  Always valid
    QUdpSocket *_pServerSocket;
    //The remote address where the ping originated
    QHostAddress _remoteAddress;
    //The remote port where the ping originated
    quint16 _remotePort;
    //The ServerLocation that describes this mock server.  Always valid
    QSharedPointer<ServerLocation> _pLocation;
};

class MockPingServers : public QObject
{
    Q_OBJECT

public:
    MockPingServers()
    {
        //Create some mock servers to listen for pings from LatencyTracker
        _mockPingServers.reserve(MockPingServerCount);
        _mockServerList.reserve(MockPingServerCount);
        while(_mockPingServers.size() < MockPingServerCount)
        {
            auto pServerSocket = new QUdpSocket{this};
            _mockPingServers.push_back(pServerSocket);
            pServerSocket->bind(localhost);

            QSharedPointer<ServerLocation> pLocation{new ServerLocation{}};

            //Fill in the mock ServerLocaiton - most of these fields do not
            //matter, but the ping address must be the actual address of the
            //socked opened above.
            auto serverId = QStringLiteral("mock-server-%1").arg(_mockServerList.size());
            pLocation->id(serverId);
            pLocation->name(serverId);
            pLocation->country(QStringLiteral("US"));
            pLocation->dns(serverId + QStringLiteral(".dummy.test"));
            pLocation->portForward(false);
            pLocation->openvpnUDP(QStringLiteral("127.0.0.1:500"));
            pLocation->openvpnTCP(QStringLiteral("127.0.0.1:8080"));
            auto pingAddr = QStringLiteral("%1:%2")
                .arg(pServerSocket->localAddress().toString())
                .arg(pServerSocket->localPort());
            pLocation->ping(pingAddr);

            _mockServerList.insert(pLocation->id(), pLocation);

            _mockPingLocations.push_back({serverId, pingAddr});

            connect(pServerSocket, &QUdpSocket::readyRead, this,
                    [=](){onDatagramReady(pServerSocket, pLocation);});
        }
    }

signals:
    void receivedPing(ReceivedPing ping);

private slots:
    void onDatagramReady(QUdpSocket *pServerSocket,
                         QSharedPointer<ServerLocation> pLocation)
    {
        QHostAddress remoteHost;
        quint16 remotePort;
        if(pServerSocket->readDatagram(nullptr, 0, &remoteHost, &remotePort) < 0)
            return; //Read failed, nothing to do

        emit receivedPing({pServerSocket, remoteHost, remotePort,
                           std::move(pLocation)});
    }

public:
    //Get a ServerLocationList describing the mock servers.  (Used to pass the
    //mock servers to LatencyTracker.)
    const ServerLocations &mockServerList() const {return _mockServerList;}

    //Get a vector of PingLocations describing the mock servers.  (Used to pass
    //the mock servers to LatencyBatch.)
    const QVector<LatencyTracker::PingLocation> &mockPingLocations() const {return _mockPingLocations;}

    //Get a set of the the server socket addresses - used by unit tests to
    //verify that each server was pinged, etc.
    QSet<HostPortKey> getServerAddresses() const
    {
        QSet<HostPortKey> serverAddresses;
        serverAddresses.reserve(_mockPingServers.size());
        for(const auto &pServerSocket : _mockPingServers)
        {
            serverAddresses.insert({pServerSocket->localAddress(),
                                    pServerSocket->localPort()});
        }

        return serverAddresses;
    }

private:
    QVector<QUdpSocket*> _mockPingServers;
    ServerLocations _mockServerList;
    QVector<LatencyTracker::PingLocation> _mockPingLocations;
};

class AutoEcho : public QObject
{
    Q_OBJECT

public:
    AutoEcho(MockPingServers &mockServers)
    {
        connect(&mockServers, &MockPingServers::receivedPing, this,
                [](const auto &receivedPing){receivedPing.echo();});
    }
};

//Required to extract ReceivedPings and Latencies from QVariants captured by
//QSignalSpy
Q_DECLARE_METATYPE(ReceivedPing);
namespace
{
    RegisterMetaType<ReceivedPing> rxPingMetaType;
}

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
        LatencyTracker tracker;
        MeasurementSplitter splitter{tracker};
        tracker.start();

        //Watch for pings to be sent to all of the mock servers
        QSignalSpy pingSpy{&_mockServers, &MockPingServers::receivedPing};

        tracker.updateLocations(_mockServers.mockServerList());

        auto pendingServers = _mockServers.getServerAddresses();
        //Wait for a packet to be emitted for each server.  Note that
        //pingSpy.wait() can return after more than one signal has occurred, so
        //we can't simply call wait() 4 times, we have to actually wait until
        //4 signals have occurred.
        while(pingSpy.size() < MockPingServerCount)
        {
            QVERIFY(pingSpy.wait());
            qDebug() << "Received" << pingSpy.size() << "pings so far";
        }

        for(const auto &pingSignal : pingSpy)
        {
            //Remove this from the pending pings.  Verify that something was
            //removed (we shouldn't receive any duplicates, and we shouldn't
            //receive any unsolicited pings)
            const auto &receivedPing = pingSignal[0].value<ReceivedPing>();
            QVERIFY(pendingServers.remove(receivedPing.getServerAddress()));
            qDebug() << "Received expected ping to" << receivedPing.getLocationId();
        }

        //Now send the replies and expect latency measurements
        QSignalSpy latencySpy{&splitter, &MeasurementSplitter::newMeasurement};

        for(const auto &ping : pingSpy)
        {
            const auto &receivedPing = ping[0].value<ReceivedPing>();
            receivedPing.echo();
            QVERIFY(latencySpy.wait());
            const auto &latencyArgs = latencySpy.takeFirst();
            QCOMPARE(latencyArgs[0].toString(), receivedPing.getLocationId());
        }
    }

    //Verify that location updates with new locations ping the new locations
    //immediately
    void newLocations()
    {
        LatencyTracker tracker;
        MeasurementSplitter splitter{tracker};
        tracker.start();

        //Pass in the first three locations initially, echo all of these pings
        {
            AutoEcho autoEcho{_mockServers};
            auto initialLocations = _mockServers.mockServerList();
            //Remove a server so we can add it later
            initialLocations.remove(QStringLiteral("mock-server-0"));

            //Wait for all of the measurements to be emitted
            QSignalSpy initialLatencySpy{&splitter, &MeasurementSplitter::newMeasurement};
            tracker.updateLocations(initialLocations);
            while(initialLatencySpy.size() < MockPingServerCount-1)
                QVERIFY(initialLatencySpy.wait());
        }

        //Now update with a different set of locations.  Add in the location we
        //removed above (which should trigger an immediate measurement), and
        //delete a different location (which should not trigger anything).
        auto updatedLocations = _mockServers.mockServerList();
        updatedLocations.remove(QStringLiteral("mock-server-1"));

        QSignalSpy pingSpy{&_mockServers, &MockPingServers::receivedPing};
        tracker.updateLocations(updatedLocations);
        QVERIFY(pingSpy.wait());
        const auto &receivedPing = pingSpy.front()[0].value<ReceivedPing>();
        QCOMPARE(receivedPing.getLocationId(), QStringLiteral("mock-server-0"));

        //Echo and ensure that a measurement is emitted
        QSignalSpy latencySpy{&splitter, &MeasurementSplitter::newMeasurement};
        receivedPing.echo();
        QVERIFY(latencySpy.wait());
        QCOMPARE(latencySpy.front()[0].toString(), receivedPing.getLocationId());
    }

    //Verify that new locations created before measurements are started will be
    //measured after measurements are enabled
    void delayedNewLocations()
    {
        LatencyTracker tracker;
        MeasurementSplitter splitter{tracker};

        QSignalSpy pingSpy{&_mockServers, &MockPingServers::receivedPing};
        QSignalSpy measurementSpy{&splitter, &MeasurementSplitter::newMeasurement};

        tracker.updateLocations(_mockServers.mockServerList());

        //This wait *should* time out - we don't expect pings to happen yet.
        //We need to enter a message loop here to be sure, otherwise the events
        //would be queued up anyway, and we couldn't tell whether they occurred
        //before or after the call to start().
        QVERIFY(!pingSpy.wait(2000));

        AutoEcho autoEcho{_mockServers};
        //Now, start measurements.  This should emit the pings and measurements
        //now
        tracker.start();
        while(measurementSpy.size() < MockPingServerCount)
        {
            QVERIFY(measurementSpy.wait());
        }
        QCOMPARE(pingSpy.size(), MockPingServerCount);
    }

    //If a location is added and deleted again while stopped, no ping should be
    //emitted for it when we start measurements.
    void deletionWhileStopped()
    {
        LatencyTracker tracker;
        MeasurementSplitter splitter{tracker};
        tracker.start();

        AutoEcho autoEcho{_mockServers};

        QSignalSpy measurementSpy{&splitter, &MeasurementSplitter::newMeasurement};

        auto initialLocations = _mockServers.mockServerList();
        //Delete a location
        initialLocations.remove(QStringLiteral("mock-server-0"));
        tracker.updateLocations(initialLocations);

        //Expect 3 measurements
        while(measurementSpy.size() < MockPingServerCount-1)
            QVERIFY(measurementSpy.wait());
        measurementSpy.clear();

        //Stop measurements
        tracker.stop();
        //Add and delete the other location
        tracker.updateLocations(_mockServers.mockServerList());
        tracker.updateLocations(initialLocations);

        //Start notifications again.  We don't expect to get any pings or
        //measurements, so this wait *should* time out
        QSignalSpy pingSpy{&_mockServers, &MockPingServers::receivedPing};
        QVERIFY(!measurementSpy.wait(2000));
        QCOMPARE(pingSpy.size(), 0);
    }

    //Verify that a LatencyBatch is cleaned up properly under normal
    //circumstances (all addresses are valid and respond)
    void normalCleanup()
    {
        AutoEcho autoEcho{_mockServers};

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

    //Verify that a LatencyBatch destroys itself correctly when none of the
    //addresses given are valid.  (It should be destroyed immediately, not after
    //the measurement timeout.)
    void invalidCleanup()
    {
        //Bogus unparseable ping addresses
        QVector<LatencyTracker::PingLocation> bogusAddresses
        {
            {"bogus-id-1", "bogus-address-1"},
            {"bogus-id-2", "bogus-address-2"}
        };

        auto pBatch{new LatencyBatch{bogusAddresses, this}};
        QSignalSpy destroySpy{pBatch, &QObject::destroyed};

        //If this wait times out, the LatencyBatch probably waited for the
        //measurement timeout to elapse instead of destroying itself
        //immediately.
        destroySpy.wait(1000);
    }

    //Verify that a LatencyBatch destroys itself after the timeout interval if
    //any of the servers never respond
    void unresponsiveCleanup()
    {
        //Note that we're not creating an AutoEcho and not calling echo(); the
        //servers will not respond at all.
        auto pBatch{new LatencyBatch{_mockServers.mockPingLocations(), this}};
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

    //Verify host/port parsing
    void hostPortParsing()
    {
        QHostAddress host;
        quint16 port;

        QVERIFY(parsePingAddress("172.16.254.254:8888", host, port));
        QCOMPARE(host, QHostAddress{0xAC10FEFE});
        QCOMPARE(port, 8888);

        //Invalid - no port
        QCOMPARE(parsePingAddress("172.16.254.254", host, port), false);
        //Invalid - port is 0
        QCOMPARE(parsePingAddress("172.16.254.254:0", host, port), false);
        //Invalid - bogus address
        QCOMPARE(parsePingAddress("bogus:8888", host, port), false);
    }

private:
    MockPingServers _mockServers;
};

QTEST_GUILESS_MAIN(tst_latencytracker)
#include TEST_MOC
