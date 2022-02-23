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

#include "common.h"
#line SOURCE_FILE("latencytracker.cpp")

#include "latencytracker.h"
#include <algorithm>
#include <QRandomGenerator>

#if defined(Q_OS_WIN)
#include "win/win_ping.h"
#else
#include "posix/posix_ping.h"
#endif

namespace
{
    const std::chrono::minutes latencyRefreshInterval{1};
    const std::chrono::seconds latencyEchoTimeout{10};
    const std::chrono::milliseconds latencyBatchInterval{100};

    //The number of measurements that LatencyHistory stores
    const int measurementHistoryCount{5};

    RegisterMetaType<std::chrono::milliseconds> rxChronoMilliseconds;
    RegisterMetaType<LatencyTracker::Latencies> rxLatencies;

    // Select a ping address for a location when using ICMP pings.  A server is
    // selected randomly.  If no server can be selected, this returns a
    // PingLocation with an empty echoIp/echoPort.
    quint32 selectIcmpPingAddress(const QSharedPointer<Location> &pLocation)
    {
        const Server *pLatencyServer{};
        if(pLocation)
            pLatencyServer = pLocation->randomIcmpLatencyServer();

        quint32 address{0};
        bool addressOk{false};
        if(pLatencyServer)
            address = QHostAddress{pLatencyServer->ip()}.toIPv4Address(&addressOk);
        if(addressOk)
            return address;
        return 0;
    }

#if defined(Q_OS_WIN)
    // Implementation of BatchPinger using ICMP echoes via WinIcmpEcho
    class WinIcmpBatchPinger : public BatchPinger
    {
        Q_OBJECT
        CLASS_LOGGING_CATEGORY("latency");

    public:
        // Create WinIcmpBatchPinger with the locations to be pinged.
        // WinIcmpBatchPinger will insert all locations that were successfully
        // pinged into pendingReplies (values are the location IDs).  Ports are
        // always 0 for WinIcmpBatchPinger since it uses ICMP.
        WinIcmpBatchPinger(const std::vector<QSharedPointer<Location>> &locations,
                           PendingRepliesMap &pendingReplies,
                           std::chrono::milliseconds timeout);
    };

    WinIcmpBatchPinger::WinIcmpBatchPinger(const std::vector<QSharedPointer<Location>> &locations,
                                           PendingRepliesMap &pendingReplies,
                                           std::chrono::milliseconds timeout)
    {
        //Ping each location.
        for(const auto &pLocation : locations)
        {
            quint32 echoAddr = selectIcmpPingAddress(pLocation);
            QPointer<WinIcmpEcho> pEcho;
            if(echoAddr && (pEcho = WinIcmpEcho::send(echoAddr, timeout)))
            {
                // Pinged this location, put it in the pending replies.
                pendingReplies[HostPortKey{QHostAddress{echoAddr}, 0}] = pLocation->id();

                connect(pEcho.data(), &WinIcmpEcho::receivedReply, this,
                        [this](quint32 address)
                        {
                            emit receivedResponse(QHostAddress{address}, 0);
                        });
            }
        }
    }

#else
    // Implementation of BatchPinger using ICMP echoes via raw sockets using PosixPing
    class PosixIcmpBatchPinger : public BatchPinger
    {
        Q_OBJECT
        CLASS_LOGGING_CATEGORY("latency");

    public:
        // Create PosixIcmpBatchPinger with the locations to be pinged.
        // PosixIcmpBatchPinger will insert all locations that were successfully
        // pinged into pendingReplies (values are the location IDs).  Ports are
        // always 0 for PosixIcmpBatchPinger since it uses ICMP.
        PosixIcmpBatchPinger(const std::vector<QSharedPointer<Location>> &locations,
                             PendingRepliesMap &pendingReplies);

    private:
        PosixPing _ping;
    };

    PosixIcmpBatchPinger::PosixIcmpBatchPinger(const std::vector<QSharedPointer<Location>> &locations,
                                               PendingRepliesMap &pendingReplies)
    {
        //Ping each location.
        for(const auto &pLocation : locations)
        {
            quint32 echoAddr = selectIcmpPingAddress(pLocation);
            if(echoAddr && _ping.sendEchoRequest(echoAddr))
            {
                // Pinged this location, put it in the pending replies.
                pendingReplies[HostPortKey{QHostAddress{echoAddr}, 0}] = pLocation->id();
            }
        }

        connect(&_ping, &PosixPing::receivedReply, this,
                [this](quint32 addr)
                {
                    receivedResponse(QHostAddress{addr}, 0);
                });
    }

#endif
}

std::chrono::milliseconds LatencyHistory::updateLatency(std::chrono::milliseconds newMeasurement)
{
    //If we already have the maximum number of entries, discard the oldest one.
    if(_lastMeasurements.size() >= measurementHistoryCount)
    {
        _lastMeasurements.pop_front();
    }

    //Store the new one.
    _lastMeasurements.push_back(newMeasurement);

    //Compute the average latency over these measurements.
    //An average is probably the best way to aggregate these (as opposed to min/
    //max/etc.), because it'll reduce the effect of anomalous measurements at
    //either end of the spectrum.
    std::chrono::milliseconds sum = std::accumulate(_lastMeasurements.begin(),
                                                    _lastMeasurements.end(),
                                                    std::chrono::milliseconds{0});

    return sum / _lastMeasurements.size();
}

LatencyTracker::LatencyTracker()
{
    _measureTrigger.setInterval(std::chrono::milliseconds(latencyRefreshInterval).count());
    connect(&_measureTrigger, &QTimer::timeout, this,
            &LatencyTracker::onMeasureTrigger);
}

void LatencyTracker::onMeasureTrigger()
{
    //Measure all known addresses (if there are any)
    //This vector of PingLocations could in principle be built with _locations
    //and stored, but it shouldn't be a significant cost to just build it here.
    std::vector<QSharedPointer<Location>> measureLocations;
    measureLocations.reserve(_locations.size());
    for(auto itLocation = _locations.begin(); itLocation != _locations.end();
        ++itLocation)
    {
        measureLocations.push_back(itLocation->second.pLocation);
    }
    beginMeasurement(measureLocations);
}

void LatencyTracker::onNewMeasurements(const Latencies &measurements)
{
    Latencies aggregatedMeasurements;
    aggregatedMeasurements.reserve(measurements.size());
    for(const auto &measurement : measurements)
    {
        // Find this location
        auto itLocation = _locations.find(measurement.first);
        // If it was found, store it and get the new aggregated value.  If it's
        // no longer present, there's nothing to do.
        if(itLocation != _locations.end())
        {
            // Store the new latency measurement, and get the current aggregate
            // value.
            auto aggregateLatency = itLocation->second.latency.updateLatency(measurement.second);
            aggregatedMeasurements.push_back({measurement.first, aggregateLatency});
        }
    }

    if(!aggregatedMeasurements.empty())
        emit newMeasurements(aggregatedMeasurements);
}

void LatencyTracker::measureNewLocations()
{
    std::vector<QSharedPointer<Location>> newLocations;

    for(auto &locationEntry : _locations)
    {
        //If this location hasn't been attempted yet, ping it now.
        if(!locationEntry.second.pingAttempted)
        {
            locationEntry.second.pingAttempted = true;
            newLocations.push_back(locationEntry.second.pLocation);
        }
    }

    if(!newLocations.empty())
    {
        beginMeasurement(newLocations);
    }
}

void LatencyTracker::beginMeasurement(const std::vector<QSharedPointer<Location>> &locations)
{
    //If there's at least one address to measure, start a measurement.
    if(!locations.empty())
    {
        // Create the LatencyBatch on the worker thread so there's no
        // interference between activity on the main thread and the events that
        // have to be measured to calculate latency.
        //
        // This is a blocking call over to the worker thread, but we don't do
        // any long-running operations on the worker thread, so this is fine.
        // Latency batches also time out before another batch would be
        // scheduled, so there shouldn't be any activity at all on the worker
        // thread when this is called.
        _measurementThread.invokeOnThread([&]()
        {
            //Create a LatencyBatch; parent it to this object so it is cleaned up if
            //LatencyTracker is destroyed
            LatencyBatch *pNewBatch = new LatencyBatch{locations,
                                                       &_measurementThread.objectOwner()};
            //Forward newMeasurements signals from this new batch
            connect(pNewBatch, &LatencyBatch::newMeasurements, this,
                    &LatencyTracker::onNewMeasurements);
        });
    }
}

void LatencyTracker::updateLocations(const LocationsById &serverLocations)
{
    // Pull out the existing locations, then put back the ones that are still
    // present.
    std::unordered_map<QString, LocationData> oldLocations;
    oldLocations.swap(_locations);

    //Process the current locations
    _locations.reserve(serverLocations.size());
    for(const auto &location : serverLocations)
    {
        // Create the location.  No pings have been attempted yet if we don't
        // find this location in oldLocations
        auto &newLocation = _locations[location.first];
        newLocation = {location.second, {}, false};

        // Did we have this location before?
        auto itOldLocation = oldLocations.find(location.first);
        if(itOldLocation != oldLocations.end())
        {
            //It existed, so preserve its latency measurements
            newLocation.latency = std::move(itOldLocation->second.latency);
            //Preserve pingAttempted
            newLocation.pingAttempted = itOldLocation->second.pingAttempted;
        }
    }

    //If measurements are enabled, trigger a new measurement for the new
    //locations.  Otherwise, leave them in _locations to be attempted later.
    if(_measureTrigger.isActive())
        measureNewLocations();
}

void LatencyTracker::start()
{
    if(!_measureTrigger.isActive())
    {
        _measureTrigger.start();
        //Trigger measurements for anything that hasn't been measured yet
        measureNewLocations();
    }
}

void LatencyTracker::stop()
{
    _measureTrigger.stop();
}

LatencyBatch::LatencyBatch(const std::vector<QSharedPointer<Location>> &locations,
                           QObject *pParent)
    : QObject{pParent}
{
    _batchTimer.setInterval(std::chrono::milliseconds(latencyBatchInterval).count());
    _batchTimer.setSingleShot(true);
    connect(&_batchTimer, &QTimer::timeout, this,
            &LatencyBatch::onBatchElapsed);

    //Start the timer before sending the ping packets
    _timeSincePing.start();

#if defined(Q_OS_WIN)
    _pPinger.reset(new WinIcmpBatchPinger{locations, _pendingReplies, latencyEchoTimeout});
#else
    _pPinger.reset(new PosixIcmpBatchPinger{locations, _pendingReplies});
#endif

    connect(_pPinger.get(), &BatchPinger::receivedResponse, this,
            &LatencyBatch::onReceivedResponse);

    if(_pendingReplies.size() >= 1)
    {
        //We sent at least one ping, so start the timeout timer.
        QTimer::singleShot(std::chrono::milliseconds(latencyEchoTimeout).count(), this,
                           &LatencyBatch::onTimeoutElapsed);
    }
    else
    {
        //Nothing was sent, so there's nothing to do.
        //Creating the LatencyBatch still succeeds, but destroy it immediately
        //since there is nothing to do.
        deleteLater();
    }
}

void LatencyBatch::emitBatchedMeasurements()
{
    if(!_batchedMeasurements.empty())
    {
        Latencies elapsedBatch;
        _batchedMeasurements.swap(elapsedBatch);
        emit newMeasurements(elapsedBatch);
    }
}

void LatencyBatch::onReceivedResponse(const QHostAddress &address, quint16 port)
{
    // Get the roundtrip latency measurement.
    std::chrono::milliseconds roundtripLatency{_timeSincePing.elapsed()};

    //Look up this host in the pending replies.  Look for any possible
    //equivalent address - for example, an IPv4 address could now be represented
    //as an IPv4-mapped IPv6 address.
    const auto &equivalentSrcAddrs = getEquivalentAddresses(address);
    auto itHostPendingReply = _pendingReplies.end();
    for(const auto &srcAddr : equivalentSrcAddrs)
    {
        itHostPendingReply = _pendingReplies.find({srcAddr, port});
        //If we found a match with this address, we're done searching
        if(itHostPendingReply != _pendingReplies.end())
            break;
    }

    //If this host isn't in the pending replies, there's nothing to do.  (This
    //could be an unsolicited packet from some irrelevant host, or it could be
    //an extra packet from a host whose reply was already received.)
    if(itHostPendingReply == _pendingReplies.end())
        return;

    // Store a measurement for this host
    _batchedMeasurements.push_back({itHostPendingReply->second,
                                    roundtripLatency});

    //This host has been measured, so remove it from _pendingReplies
    _pendingReplies.erase(itHostPendingReply);

    //If there are no pending echoes left, this LatencyBatch is done
    if(_pendingReplies.empty())
    {
        // Emit all measurements that are still queued (there might be others
        // besides the one that was just taken).
        // It's possible the batch timer could be elapsing now, so we still need
        // to clear out _batchedMeasurements to ensure that the measurements
        // aren't emitted twice.
        emitBatchedMeasurements();

        //Destroy this LatencyBatch
        deleteLater();
    }
    else
    {
        // There are still measurements being taken.  Start the batch timer if
        // it isn't already running.
        if(!_batchTimer.isActive())
            _batchTimer.start();
    }
}

void LatencyBatch::onTimeoutElapsed()
{
    if(_pendingReplies.size() > 0)
    {
        qDebug() << "Did not receive echoes from" << _pendingReplies.size()
                 << "addresses";
    }

    for(const auto &replyEntry : _pendingReplies)
    {
        qInfo() << "Location" << replyEntry.second
                << "did not respond to latency ping";
    }

    // Nothing left to do.  Emit any remaining measurements, then destroy this
    // LatencyBatch
    emitBatchedMeasurements();
    deleteLater();
}

void LatencyBatch::onBatchElapsed()
{
    emitBatchedMeasurements();
}

QVector<QHostAddress> getIPv4EquivalentAddresses(quint32 ipv4addr)
{
    //Pick out the individual bytes from the IPv4 address to put them into
    //the IPv6 addresses as a quint8 array.
    quint8 v4b0 = static_cast<quint8>((ipv4addr >> 24) & 0xFF);
    quint8 v4b1 = static_cast<quint8>((ipv4addr >> 16) & 0xFF);
    quint8 v4b2 = static_cast<quint8>(ipv4addr >> 8) & 0xFF;
    quint8 v4b3 = static_cast<quint8>(ipv4addr & 0xFF);
    quint8 ipv4compat[]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, v4b0, v4b1, v4b2, v4b3};
    quint8 ipv4mapped[]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0xFF, 0xFF, v4b0, v4b1, v4b2, v4b3};
    return {QHostAddress{ipv4addr}, QHostAddress{ipv4compat},
            QHostAddress{ipv4mapped}};
}

QVector<QHostAddress> getEquivalentAddresses(const QHostAddress &srcAddr)
{
    //Get the IPv4 address if this is an ordinary IPv4 address or any IPv4
    //mapped/compatible IPv6 address.
    bool isIPv4 = false;
    quint32 ipv4addr = srcAddr.toIPv4Address(&isIPv4);

    //If it was an IPv4 address or an IPv4-mapped IPv6 address, return all
    //variations of that address.
    if(isIPv4)
        return getIPv4EquivalentAddresses(ipv4addr);

    //QHostAddress::toIPv4Address() doesn't consider an IPv4-compatible IPv6
    //address to be an IPv4 address (it only handles IPv4-mapped IPv6 addresses,
    //this appears to be just because IPv4-compatible addresses are deprecated).
    if(srcAddr.protocol() == QAbstractSocket::NetworkLayerProtocol::IPv6Protocol)
    {
        Q_IPV6ADDR ipv6 = srcAddr.toIPv6Address();
        //If the first 96 bits are all 0, this is an IPv4-compatbile IPv6
        //address
        if(std::all_of(ipv6.c, ipv6.c + 12, [](quint8 byte){return byte == 0;}))
        {
            ipv4addr = ipv6[12];
            ipv4addr <<= 8;
            ipv4addr |= ipv6[13];
            ipv4addr <<= 8;
            ipv4addr |= ipv6[14];
            ipv4addr <<= 8;
            ipv4addr |= ipv6[15];
            return getIPv4EquivalentAddresses(ipv4addr);
        }
    }

    //Otherwise, it's not an IPv4 address in any form, just return the original
    //address.
    return {srcAddr};
}

#include "latencytracker.moc"
