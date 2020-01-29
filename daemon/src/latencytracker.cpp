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
#line SOURCE_FILE("latencytracker.cpp")

#include "latencytracker.h"
#include <algorithm>

namespace
{
    const std::chrono::minutes latencyRefreshInterval{1};
    const std::chrono::seconds latencyEchoTimeout{10};
    const std::chrono::milliseconds latencyBatchInterval{100};

    //The number of measurements that LatencyHistory stores
    const int measurementHistoryCount{5};

    RegisterMetaType<std::chrono::milliseconds> rxChronoMilliseconds;
    RegisterMetaType<LatencyTracker::Latencies> rxLatencies;
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
    QVector<PingLocation> measureLocations;
    measureLocations.reserve(_locations.size());
    for(auto itLocation = _locations.begin(); itLocation != _locations.end();
        ++itLocation)
    {
        measureLocations.push_back({itLocation.key(), itLocation->pingAddress});
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
            auto aggregateLatency = itLocation->latency.updateLatency(measurement.second);
            aggregatedMeasurements.push_back({measurement.first, aggregateLatency});
        }
    }

    if(!aggregatedMeasurements.empty())
        emit newMeasurements(aggregatedMeasurements);
}

void LatencyTracker::measureNewLocations()
{
    QVector<PingLocation> newLocations;

    for(auto itLocation = _locations.begin(); itLocation != _locations.end();
        ++itLocation)
    {
        //If this location hasn't been attempted yet, ping it now.
        if(!itLocation->pingAttempted)
        {
            itLocation->pingAttempted = true;
            newLocations.push_back({itLocation.key(), itLocation->pingAddress});
        }
    }

    if(!newLocations.empty())
    {
        beginMeasurement(newLocations);
    }
}

void LatencyTracker::beginMeasurement(const QVector<PingLocation> &locations)
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

void LatencyTracker::updateLocations(const ServerLocations &serverLocations)
{
    //Pull out the existing locations, then put back the ones that are still
    //present.
    QHash<QString, LocationData> oldLocations;
    oldLocations.swap(_locations);

    //Process the current locations
    _locations.reserve(serverLocations.size());
    for(const auto &pLocation : serverLocations)
    {
        //Create the location and store the current ping address.  No pings
        //have been attempted yet if we don't find this location in
        //oldLocations
        auto itNewLocation = _locations.insert(pLocation->id(),
                                               {pLocation->ping(), {}, false});

        //Did we have this location before?
        auto itOldLocation = oldLocations.find(pLocation->id());
        if(itOldLocation != oldLocations.end())
        {
            //It existed, so preserve its latency measurements
            itNewLocation->latency = std::move(itOldLocation->latency);
            //Preserve pingAttempted
            itNewLocation->pingAttempted = itOldLocation->pingAttempted;
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

LatencyBatch::LatencyBatch(const QVector<LatencyTracker::PingLocation> &locations,
                           QObject *pParent)
    : QObject{pParent}
{
    _batchTimer.setInterval(std::chrono::milliseconds(latencyBatchInterval).count());
    _batchTimer.setSingleShot(true);
    connect(&_batchTimer, &QTimer::timeout, this,
            &LatencyBatch::onBatchElapsed);

    //Receive echo responses in this slot
    connect(&_udpSocket, &QUdpSocket::readyRead, this,
            &LatencyBatch::onDatagramReady);

    //Bind a port so we can receive the echoes.  This binds on all interfaces.
    _udpSocket.bind();

    //Start the timer before sending the ping packets
    _timeSincePing.start();

    //Ping each address.
    for(const auto &location : locations)
    {
        QHostAddress host;
        quint16 port;
        if(parsePingAddress(location.pingAddress, host, port))
        {
            //This address is valid, so put it in the pending replies.
            _pendingReplies.insert({host, port}, location.id);

            //Send a one-byte datagram to this address
            _udpSocket.writeDatagram({1, 0x61}, host, port);
        }
    }


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

void LatencyBatch::onDatagramReady()
{
    //Get the roundtrip latency measurement now, before doing anything else.
    //(The packet has already arrived at this point, so any work we do later in
    //this function isn't part of the latency measurement.)
    std::chrono::milliseconds roundtripLatency{_timeSincePing.elapsed()};

    QHostAddress senderHost;
    quint16 senderPort;
    //Read the datagram that we were told about.  An echo isn't expected to
    //contain any data, so read up to 0 bytes.  We're just looking at the sender
    //host and port.
    auto dgramSize = _udpSocket.readDatagram(nullptr, 0, &senderHost,
                                             &senderPort);

    //If the read failed, there's nothing else to do.
    if(dgramSize < 0)
        return;
    //If the datagram was read successfully but contained more than 0 bytes,
    //this is fine, the data are ignored.

    //Look up this host in the pending replies.  Look for any possible
    //equivalent address - for example, an IPv4 address could now be represented
    //as an IPv4-mapped IPv6 address.
    const auto &equivalentSrcAddrs = getEquivalentAddresses(senderHost);
    auto itHostPendingReply = _pendingReplies.end();
    for(const auto &srcAddr : equivalentSrcAddrs)
    {
        itHostPendingReply = _pendingReplies.find({srcAddr, senderPort});
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
    _batchedMeasurements.push_back({itHostPendingReply.value(),
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

    for(const auto &locationId : _pendingReplies)
    {
        qInfo() << "Location" << locationId
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

bool parsePingAddress(const QString &address, QHostAddress &host,
                      quint16 &port)
{
    //Ping addresses are given as "<host>:<port>"
    const auto &addressParts = address.split(QStringLiteral(":"));
    //There should be exactly 2 parts
    if(addressParts.size() != 2)
        return false;   //Incorrectly formed address

    //Parse the host part
    if(!host.setAddress(addressParts[0]))
        return false;   //Invalid host address

    //Parse the port
    //This uses QString::toUShort(), which parses to ushort (unsigned short).
    //This should be the same as quint16 on all supported platforms, but this
    //isn't guaranteed.
    static_assert(sizeof(ushort) == sizeof(quint16),
                  "Update toUShort() call to something that's 16 bits");
    bool portValid = false;
    port = addressParts[1].toUShort(&portValid);
    //Note that 0 isn't a valid port
    if(!portValid || !port)
        return false;

    return true;
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
    //address to be an IPv4 address (it only hanldes IPv4-mapped IPv6 addresses,
    //this appears to be just because IPv4-compatbile addresses are deprecated).
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
