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

#ifndef LINUX_NL80211_H
#define LINUX_NL80211_H

#include <common/src/common.h>
#include "linux_nlcache.h"
#include "linux_libnl.h"

// LinuxNl80211Cache caches nl80211 config objects.  It uses a LinuxNlNtfSock to
// receive updates - it can't use LinuxNlCacheSock because libnl doesn't provide
// a cacheable object type for nl80211 config.
//
// Only one LinuxNl80211Cache can exist at a time.  (This is currently because
// it handles registering the generic family with libnl; if this was handled
// separately by LinuxNl then more than one could be used.)
class LinuxNl80211Cache : public LinuxNlNtfSock
{
    Q_GADGET

public:
    // Status of any dump request needed to re-dump the nl80211 state.
    enum class PendingDumpRequest
    {
        // No dump is needed currently.  (Advances to Needed when an event
        // occurs that requires us to re-dump the state.)
        None,
        // A dump is needed but no request has been sent yet.  (Advances to
        // Requested when we send the request.)
        //
        // (This state can occur when LinuxNlNtfSock::inDump() is true, which
        // happens if more events occur during the dump that require a new dump.
        // This is common when connecting/disconnecting.)
        Needed,
        // A dump request has been sent, but we haven't received any response
        // yet.  (Advances to None when we _start_ receiving the response, since
        // events triggering a dump can occur in the middle of a dump.)
        //
        // In this state, DumpProgress is always ReceivingInterfaces, because we
        // have just triggered a new dump.
        Requested,
    };
    Q_ENUM(PendingDumpRequest);

    // Progress of an active dump request.  Tracked separately from the
    // pending request since a re-dump can be triggered while a dump is
    // ongoing.
    //
    // The nl80211 interface state does not indicate whether the connection is
    // encrypted, so we have to additionally dump the current scan for each
    // interface (this is the only place encryption is indicated, but it does
    // helpfully also indicate which BSS is actually connected right now).
    //
    // Thus any dump actually consists of several steps:
    // 1. Dump the known nl80211 interfaces (so we know which interfaces are
    //    actually 802.11 interfaces)
    // 2. For each 802.11 interface, dump the current scan.  If there's a
    //    currently-associated BSS, get the SSID, whether it's encrypted, etc.
    //
    // Dumping scans seems to be the only way to get the complete scan data
    // (including whether the network is encrypted); although there are scan
    // notifications on the "scan" group, these do not seem to report the
    // complete scan data.
    enum class DumpProgress
    {
        // No dump is occurring.
        Inactive,
        // We've requested an interface dump and are receiving these.  When this
        // completes, we request the first scan and advance to ReceivingScans.
        ReceivingInterfaces,
        // We're receiving the scans.  The current scan is indicated by
        // _receivingScanInterface; when it completes we advance and request the
        // next one.  When there are none, emit new results and advance to
        // Inactive.
        ReceivingScans,
    };
    Q_ENUM(DumpProgress);

    enum : std::size_t
    {
        SsidMaxLen = 32,
    };

    // Data provided for each Wi-Fi interface - the connected SSID (if any),
    // and whether the network is encrypted (when connected).
    struct WifiStatus
    {
        // Whether this network is associated with a BSS.  Note that the
        // SSID might not be reported even if we are associated if we're
        // unable to find the SSID for some reason.
        bool associated;
        // Length of the SSID (when associated).  If this is 0 while
        // associated, we couldn't figure out the SSID.
        std::size_t ssidLength;
        // SSID data
        unsigned char ssid[SsidMaxLen];
        // Whether the network is encrypted (when associated).
        bool encrypted;
    };

public:
    LinuxNl80211Cache(int nl80211Protocol, int configGroup, int mlmeGroup);

private:
    void requestInterfaceDump();
    void requestScanDump(std::uint32_t interface);

    // Parse and apply a netlink message.
    void parseInterfaceMsg(libnl::nlattr **attrs);
    void parseScanMsg(const std::array<libnl::nlattr*, NL80211_ATTR_MAX> &attrs,
                      const std::array<libnl::nlattr*, NL80211_BSS_MAX> &bssAttrs);

    virtual int handleMsg(libnl::nl_msg *pMsg) override;
    virtual int handleFinish(libnl::nl_msg *pMsg) override;

    // Dumps triggered by change events are deferred, we usually get multiple
    // messages in a row.  They set the _dumpRequired flag, which is handled
    // here.
    virtual void handleMsgBatchEnd() override;

public:
    // Get the protocol and group IDs that were used to create this cache
    int nl80211Protocol() const {return _nl80211Protocol;}
    int configGroup() const {return _configGroup;}
    int mlmeGroup() const {return _mlmeGroup;}

    // Whether the initial dumps have completed - the state information is not
    // accurate before this occurs.  (Never goes back to 'false' once set.)
    bool ready() const {return _ready;}

    // Get the current 802.11 state.  Keys in the map are interface IDs.
    const std::map<std::uint32_t, WifiStatus> &interfaces() const {return _interfaces;}

private:
    int _nl80211Protocol;
    int _configGroup, _mlmeGroup;

    // Whether the initial dump has completed - indicates that we're ready to
    // report information.  (This never goes back to 'false' once it's set.)
    bool _ready;

    // The current known interface statuses.  Keys are interface indices.
    std::map<std::uint32_t, WifiStatus> _interfaces;

    // Status of any pending dump request.  We can't send any dump requests
    // while one is ongoing.
    PendingDumpRequest _pendingDump;
    // Status of any ongoing dump request.  This indicates what part of
    // _receivingInterfaces we're currently populating.
    DumpProgress _dumpProgress;

    // When dumps are occurring, this is the new interface state we're filling
    // out.  This pivots over to _interfaces when the dump completes.
    std::map<std::uint32_t, WifiStatus> _receivingInterfaces;
    // When receiving scans for the current dump, this is the interface index
    // that is currently being received.  When this one completes, we'll find
    // the next interface in _receivingInterfaces and then scan it.
    std::uint32_t _receivingScanInterface;
};

#endif
