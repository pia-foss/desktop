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
#line HEADER_FILE("latencytracker.h")

#ifndef LATENCYTRACKER_H
#define LATENCYTRACKER_H

#include "thread.h"
#include "settings.h"
#include "vpn.h"
#include <QObject>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTimer>
#include <QUdpSocket>
#include <chrono>

//Key for a map/set containing both a host address and a port number.  (Used in
//both LatencyTracker and unit tests.)
using HostPortKey = std::pair<QHostAddress, quint16>;

struct HashPair
{
    template<class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2> &value) const
    {
        return std::hash<T1>{}(value.first) ^ std::hash<T2>{}(value.second);
    }
};

// Map of pending replies - keys are server addresses+ports that were pinged,
// values are the associated location IDs.
using PendingRepliesMap = std::unordered_map<HostPortKey, QString, HashPair>;

//LatencyHistory queues up latency measurements for a particular remote host.
//As measurements are taken, they're queued up in LatencyHistory, which
//computes a latency value based on those measurements.
class LatencyHistory
{
    CLASS_LOGGING_CATEGORY("latency");

public:
    //Add a new measurement and calculate the current latency based on all
    //recent measurements.
    std::chrono::milliseconds updateLatency(std::chrono::milliseconds newMeasurement);

private:
    //The last few measurements are stored here.  Qt doesn't have a deque,
    //which would probably be ideal, but a QList should be OK.  It'll still have
    //to copy the entire array when we run out of space in the back of its
    //buffer, but it'll probably do better than a vector since it won't have to
    //do it every time we delete something from the front.
    QList<std::chrono::milliseconds> _lastMeasurements;
};

//LatencyTracker takes measurements of the latency to each location's "ping"
//address.
//
//When Daemon receives an updated location list, it provides the new locations
//to LatencyTracker::updateLocations().  LatencyTracker measures the latency to
//each location periodically and emits the "newMeasurements" signal when new
//measurements are taken.
//
//LatencyTracker identifies locations by their ID, not by their ping address.
//This means that if a location's ping address changes (which usually happens
//when we refresh the server list), the measurements from the old address carry
//over to the new address.
class LatencyTracker : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("latency");

private:
    struct LocationData
    {
        QSharedPointer<Location> pLocation;
        LatencyHistory latency;
        //Locations can sit in _locations without having been attempted if
        //measurements are not enabled.
        bool pingAttempted;
    };

public:
    // Group of latency measurements - location IDs and latency values.
    using Latencies = std::vector<QPair<QString, std::chrono::milliseconds>>;

public:
    // LatencyTracker begins with measurements stopped - call start() to enable
    // them.
    // Specify the infrastructure type to determine what type of measurements
    // are used; this is used for tracing and emitted with newMeasurements().
    explicit LatencyTracker(ConnectionConfig::Infrastructure infrastructure);

signals:
    // This signal is emitted whenever new measurements have been taken.
    //
    // (Note that moc requires redundant qualifications of nested types)
    void newMeasurements(ConnectionConfig::Infrastructure infrastructure,
                         const LatencyTracker::Latencies &measurements);

private slots:
    //Trigger a new latency measurement
    void onMeasureTrigger();
    //Measurements were taken by a LatencyBatch
    void onNewMeasurements(const Latencies &measurements);

private:
    //Begin a measurement for all locations in _locations that haven't been
    //attempted yet
    void measureNewLocations();

    //Begin a new measurement for a group of locations
    void beginMeasurement(const std::vector<QSharedPointer<Location>> &locations);

public:
    //Daemon passes the current set of locations to this method.
    //
    //If measurements are enabled, any new locations observed in this list will
    //be measured immediately.  If they are not enabled, new locations will be
    //measured whenever measurements are re-enabled.
    void updateLocations(const LocationsById &serverLocations);

    //Enable latency measurements.
    //
    //If they were already enabled, this has no effect.  If they weren't
    //enabled, and new locations have been added since they were last enabled,
    //a measurement is started immediately for those locations.
    void start();

    //Stop latency measurements.  If they were already stopped, this has no
    //effect.  If a measurement is taking place right now, it will still wait
    //for responses, but no new measurements will be started.  (The measurement
    //signals can still be emitted after stop() is called if a measurement was
    //in progress.)
    void stop();

private:
    ConnectionConfig::Infrastructure _infrastructure;
    // Measurement batches are executed on this thread.
    RunningWorkerThread _measurementThread;
    //This QTimer triggers when we need to refresh the measurements for all
    //servers.  This timer is running if and only if measurements have been
    //started.
    QTimer _measureTrigger;
    //All locations received from the last call to updateLocations() are
    //held here.  The rest of the location list isn't stored; we only keep track
    //of the distinct addresses that are pinged.
    //
    //Keys in this map are location IDs.
    //
    //Values are LocationData objects, which contain the location's ping address
    //and its LatencyHistory.
    std::unordered_map<QString, LocationData> _locations;
};

Q_DECLARE_METATYPE(std::chrono::milliseconds);
Q_DECLARE_METATYPE(LatencyTracker::Latencies);

// BatchPinger is an interface to measure latency to a batch of servers using
// different methods.  There is a UDP echo implementation of this interface for
// the legacy infrastructure, and there are platform-specific ICMP echo
// implementations for the modern infrastructure.
//
// The servers are batched in order to allow one socket to be used to measure
// all servers when possible.
class BatchPinger : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("latency");

public:
    virtual ~BatchPinger() = default;

signals:
    // A server has responded.
    // It's possible that the BatchPinger could receive spurious replies from
    // hosts that weren't pinged in this batch; the receiver of this signal
    // should ignore these.
    void receivedResponse(const QHostAddress &address, quint16 port);
};

// LatencyBatch represents one batch of latency measurements.
// LatencyTracker creates a batch each time it needs to measure latency to one or
// more servers.
//
// LatencyBatch sends UDP packets to each configured address, then waits for
// echos until the timeout time elapses.  When a reply is received, it
// calculates the measured latency.  Groups of measurements are emitted in the
// newMeasurements signal, which LatencyTracker forwards on.
//
// Once all measurements are received, or if the timeout time elapses,
// LatencyBatch destroys itself.
class LatencyBatch : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("latency");

public:
    using Latencies = LatencyTracker::Latencies;

public:
    //Create LatencyBatch with the locations that will be checked.
    LatencyBatch(ConnectionConfig::Infrastructure infrastructure,
                 const std::vector<QSharedPointer<Location>> &locations,
                 QObject *pParent);

signals:
    // This signal is emitted when new measurements have been calculated.
    // Each location specified in the constructor will be emitted up to one
    // time.  Locations that do not respond within the timeout will not be
    // emitted at all.
    //
    // (As above, moc requires redundant qualifications of nested types; it
    // also can't figure out a type alias)
    void newMeasurements(const LatencyTracker::Latencies &measurements);

private:
    void emitBatchedMeasurements();

private:
    void onReceivedResponse(const QHostAddress &address, quint16 port);
    void onTimeoutElapsed();
    // The batch timer has elapsed, process the batched measurements
    void onBatchElapsed();

private:
    ConnectionConfig::Infrastructure _infrastructure;
    //This timer measures the elapsed time since the pings were sent, which is
    //used to calculate latency when the echoes are received.
    QElapsedTimer _timeSincePing;
    //This map holds the addresses that we haven't heard echoes from yet.
    //Values are the location IDs that we received in the constructor.
    PendingRepliesMap _pendingReplies;
    // Ping implementation
    std::unique_ptr<BatchPinger> _pPinger;
    // This QTimer is used to batch up new measurements.
    // We batch them and report them in groups to reduce the amount of changes
    // broadcast to clients and the number of events that have to be processed
    // on the main thread.
    QTimer _batchTimer;
    // These are the latency measurements we've received in this batch.
    Latencies _batchedMeasurements;
};

//Get other QHostAddress values that are semantically equivalent to this one.
//
//This function considers IPv4 addresses, IPv4-mapped IPv6 addresses, and
//IPv4-compatible IPv6 addresses equivalent.  If the given address is one of
//those, it all of those variations of the address (including the original one).
//
//This is needed because the replies received by LatencyBatch might return a
//different form of the address they were sent to.  (GET /vpninfo/servers
//seems to return IPv4 addresses, but at least on OS X the reply packets always
//have IPv4-compatible IPv6 addresses.  Other combinations are probably possible
//too.)
//
//If the given address does not have any other equivalent addresses (it's an
//IPv6 address), this returns a vector containing just the original address.
QVector<QHostAddress> getEquivalentAddresses(const QHostAddress &srcAddr);

#endif // LATENCYTRACKER_H
