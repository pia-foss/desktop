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
#include "linux_nl80211.h"
#include "linux_libnl.h"
#include <array>
#include <algorithm>

namespace
{
    enum : int
    {
        // This is the version of the nl80211 protocol that we support.
        Nl80211Version = 1
    };

    class Nl80211Attrs
    {
    public:
        Nl80211Attrs() : _ifaceAttrs{}, _scanAttrs{}, _scanBssAttrs{}
        {
            // nl80211 has a _ton_ of attributes; default them to NLA_UNSPEC
            // other than the few we actually use
            for(auto &policy : _ifaceAttrs)
                policy.type = NLA_UNSPEC;
            _ifaceAttrs[NL80211_ATTR_IFINDEX].type = NLA_U32;

            for(auto &policy : _scanAttrs)
                policy.type = NLA_UNSPEC;
            _scanAttrs[NL80211_ATTR_IFINDEX].type = NLA_U32;
            _scanAttrs[NL80211_ATTR_BSS].type = NLA_NESTED;

            for(auto &policy : _scanBssAttrs)
                policy.type = NLA_UNSPEC;
            _scanBssAttrs[NL80211_BSS_CAPABILITY].type = NLA_U16;
            _scanBssAttrs[NL80211_BSS_STATUS].type = NLA_U32;
            // The IEs are needed to get the SSID from the scan results
            _scanBssAttrs[NL80211_BSS_INFORMATION_ELEMENTS].type = NLA_BINARY;
        }

    public:
        // Attribute policy for new interface messages
        constexpr auto &ifaceAttrs() {return _ifaceAttrs;}
        // Attribute policy for new scan messages
        constexpr auto &scanAttrs() {return _scanAttrs;}
        // In scan message, attribute policy for BSS nested attributes
        constexpr auto &scanBssAttrs() {return _scanBssAttrs;}

    private:
        std::array<nla_policy, NL80211_ATTR_MAX> _ifaceAttrs;
        std::array<nla_policy, NL80211_ATTR_MAX> _scanAttrs;
        std::array<nla_policy, NL80211_BSS_MAX> _scanBssAttrs;
    };

    Nl80211Attrs nl80211Attrs;
}

LinuxNl80211Cache::LinuxNl80211Cache(int nl80211Protocol, int configGroup,
                                     int mlmeGroup)
    : LinuxNlNtfSock{NETLINK_GENERIC}, _nl80211Protocol{nl80211Protocol},
      _configGroup{configGroup}, _mlmeGroup{mlmeGroup}, _interfaces{},
      _ready{false}, _pendingDump{PendingDumpRequest::None},
      _dumpProgress{DumpProgress::Inactive}, _receivingInterfaces{},
      _receivingScanInterface{}
{
    addMembership(_configGroup);
    addMembership(_mlmeGroup);
    qDebug() << "Dumping 802.11 config";
    _pendingDump = PendingDumpRequest::Needed;
    requestInterfaceDump();
}

void LinuxNl80211Cache::requestInterfaceDump()
{
    // This only occurs when a dump is needed (can't occur when one has already
    // been requested)
    Q_ASSERT(_pendingDump == PendingDumpRequest::Needed);
    // A dump can't already be in progress
    Q_ASSERT(_dumpProgress == DumpProgress::Inactive);

    NlUniquePtr<libnl::nl_msg> pDumpMsg{libnl::nlmsg_alloc()};
    libnl::genlmsg_put(pDumpMsg.get(), NL_AUTO_PORT, NL_AUTO_SEQ, _nl80211Protocol,
                       0, NLM_F_DUMP, NL80211_CMD_GET_INTERFACE, Nl80211Version);
    qInfo() << "Request interface dump";
    sendAuto(pDumpMsg.get());
    // We've sent the request for the dump, we can't start another one while
    // this is pending
    _pendingDump = PendingDumpRequest::Requested;
    _dumpProgress = DumpProgress::ReceivingInterfaces;
}

void LinuxNl80211Cache::requestScanDump(std::uint32_t interface)
{
    // This can't occur when a dump has already been requested, but it can
    // occur in either 'None' or 'Needed' since a new dump might already have
    // been triggered
    Q_ASSERT(_pendingDump != PendingDumpRequest::Requested);
    // This follows either an interface dump or another scan dump (we never go
    // directly from Inactive to ReceivingScans)
    Q_ASSERT(_dumpProgress != DumpProgress::Inactive);

    NlUniquePtr<libnl::nl_msg> pDumpMsg{libnl::nlmsg_alloc()};
    libnl::genlmsg_put(pDumpMsg.get(), NL_AUTO_PORT, NL_AUTO_SEQ, _nl80211Protocol,
                       0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, Nl80211Version);
    libnl::nla_put_u32(pDumpMsg.get(), NL80211_ATTR_IFINDEX, interface);
    qInfo() << "Request scan dump for interface" << interface;
    sendAuto(pDumpMsg.get());
    // Advance to ReceivingScans if we were in ReceivingInterfaces
    _dumpProgress = DumpProgress::ReceivingScans;
}

void LinuxNl80211Cache::parseInterfaceMsg(libnl::nlattr **attrs)
{
    std::uint32_t ifindex{};
    if(attrs[NL80211_ATTR_IFINDEX])
    {
        ifindex = libnl::nla_get_u32(attrs[NL80211_ATTR_IFINDEX]);
        qInfo() << "nl80211 reports new interface" << ifindex;
    }

    // Ignore the message if there was no ifindex somehow, or if the index was
    // somehow 0.  0 isn't a valid interface index, and we rely on this.
    if(!ifindex)
    {
        qInfo() << "Received new interface message with no interface, ignore";
        return;
    }

    // If we're currently dumping interfaces, add this to the queue to dump the
    // interface's scan
    if(_dumpProgress == DumpProgress::ReceivingInterfaces)
    {
        qInfo() << "Queue ifindex" << ifindex << "for scan dump";
        _receivingInterfaces[ifindex] = {};
    }
    // Otherwise, this is really a new interface, such as a hotplugged USB
    // Wi-Fi adapter.  In principle, it shouldn't currently be associated with
    // anything, and we should get notifications when it does associate, but
    // trigger a full dump anyway just in case the messages aren't ordered in
    // the way we expect.
    else if(_pendingDump == PendingDumpRequest::None)
    {
        qInfo() << "Trigger interface dump due to new interface" << ifindex;
        _pendingDump = PendingDumpRequest::Needed;
    }
    else
    {
        // A dump is already requested.
        qInfo() << "Not triggering interface dump for new interface" << ifindex
            << "- one is already requested:" << traceEnum(_pendingDump);
    }
}

void LinuxNl80211Cache::parseScanMsg(
    const std::array<libnl::nlattr*, NL80211_ATTR_MAX> &attrs,
    const std::array<libnl::nlattr*, NL80211_BSS_MAX> &bssAttrs)
{
    if(!attrs[NL80211_ATTR_IFINDEX])
    {
        qWarning() << "Received scan message with no interface index";
        return;
    }

    std::uint32_t ifindex = libnl::nla_get_u32(attrs[NL80211_ATTR_IFINDEX]);
    if(ifindex != _receivingScanInterface)
    {
        qWarning() << "Received scan message for interface" << ifindex
            << "when expecting interface" << _receivingScanInterface;
        return;
    }

    // Check if this is the connected BSS, we don't care about any others
    if(!bssAttrs[NL80211_BSS_STATUS])
    {
        // This is not the connected BSS, this is normal; ignore it.
        // Don't trace, this can happen a lot
        return;
    }
    // Any other status is "connected" as far as we're concerned (note that
    // there is no "status" value for an unused BSS, the attribute is omitted
    // instead.)

    // This interface is associated - report this even if we're unable to get
    // the other data
    WifiStatus newStatus{};
    newStatus.associated = true;

    if(!bssAttrs[NL80211_BSS_CAPABILITY] || !bssAttrs[NL80211_BSS_INFORMATION_ELEMENTS])
    {
        qWarning() << "Connected BSS on interface" << ifindex
            << "does not have capability or IEs attributes, can't report current state";
        return;
    }

    if(!bssAttrs[NL80211_BSS_CAPABILITY])
    {
        qWarning() << "Connected BSS on interface" << ifindex
            << "does not have capabilities, assuming not encrypted";
    }
    else
    {
        std::uint16_t capabilities = libnl::nla_get_u16(bssAttrs[NL80211_BSS_CAPABILITY]);
        enum : std::uint16_t
        {
            // 802.11 "Privacy" capability flag, indicates that the network supports
            // encrypted traffic.  We don't care about any other capabilities.
            WlanCapabilityPrivacy = 0x0010,
        };
        // The capabilities indicate whether any form of encryption is supported.
        // We also have the RSN info in the IEs and could differentiate
        // WEP/WPA/ciphers/etc., but we don't care about any of that currently.
        newStatus.encrypted = !!(capabilities & WlanCapabilityPrivacy);
    }

    if(!bssAttrs[NL80211_BSS_INFORMATION_ELEMENTS])
    {
        qWarning() << "Connected BSS on interface" << ifindex
            << "does not have information elements, can't determine SSID";
    }
    else
    {
        // Pull the SSID out of the 802.11 information elements.  This blob comes
        // directly from a 802.11 management frame, and it consists of
        // id-length-data fields.
        //
        // https://www.oreilly.com/library/view/80211-wireless-networks/0596100523/ch04.html#wireless802dot112-CHP-4-FIG-31
        //
        // The only field we care about is the SSID, so we can just run through this
        // blob until we find ID 0.
        //
        // nl80211 reports two IE fields - ..._INFORMATION_ELEMENTS and
        // ..._BEACON_IES.  If present, ..._BEACON_IES is always from a beacon.
        // ..._INFORMATION_ELEMENTS can instead be from a probe response if it
        // differs from the beacon.  The difference is relevant for hidden networks,
        // these wouldn't include the SSID in a beacon but would include it in a
        // probe response.
        enum
        {
            WlanTagSsid = 0,
        };
        libnl::nlattr *pAttr = bssAttrs[NL80211_BSS_INFORMATION_ELEMENTS];
        const unsigned char *pData = reinterpret_cast<const unsigned char*>(libnl::nla_data(pAttr));
        const unsigned char *pDataEnd = pData + libnl::nla_len(pAttr);
        while(pData <= pDataEnd - 2) // Must be 2 bytes for ID and length
        {
            unsigned char tagId = pData[0];
            unsigned char tagLen = pData[1];
            const unsigned char *pTagData = pData + 2;
            const unsigned char *pTagEnd = pTagData + tagLen;
            if(pTagEnd > pDataEnd)
            {
                qWarning() << "Connected BSS on interface" << ifindex
                    << "has corrupt information element data, can't determine SSID";
                break;  // Tag length is invalid, overruns end of IE data
            }
            if(tagId == WlanTagSsid)
            {
                // Copy out the SSID to the new status
                newStatus.ssidLength = tagLen;
                std::copy(pTagData, pTagEnd, newStatus.ssid);
                break;  // Found the SSID, don't care about anything else
            }
            // Advance to the next tag (if there is one)
            pData = pTagEnd;
        }
        if(newStatus.ssidLength == 0)
        {
            qWarning() << "Connected BSS on interface" << ifindex
                << "did not have SSID in information elements";
        }
    }

    qInfo() << "Found associated BSS for interface" << _receivingScanInterface
        << "- enc:" << newStatus.encrypted << "- ssid:"
        << QString::fromLatin1(QByteArray::fromRawData(reinterpret_cast<const char *>(newStatus.ssid), newStatus.ssidLength).toPercentEncoding());
    _receivingInterfaces[_receivingScanInterface] = newStatus;
}

int LinuxNl80211Cache::handleMsg(libnl::nl_msg *pMsg)
{
    Q_ASSERT(pMsg); // Guarantee by libnl

    libnl::nlmsghdr *pHeader = libnl::nlmsg_hdr(pMsg);
    libnl::genlmsghdr *pGenHeader = libnl::genlmsg_hdr(pHeader);

    // If this is the first part of a dump, clear the 'Requested' state - if
    // more events occur, we'll request a new dump after this one completes.
    //
    // Technically, it might be more accurate to look specifically for a
    // multipart message containing a ..._NEW_INTERFACE command, but if there
    // are zero interfaces then we won't get this, we just have to assume that
    // any multipart+done sequence is our dump response.  This is fine as long
    // as we never have more than one dump request (of any type) outstanding at
    // a time.
    if(pHeader->nlmsg_flags & NLM_F_MULTI && _pendingDump == PendingDumpRequest::Requested)
    {
        qInfo() << "Received first message of interface dump response";
        // The only valid DumpProgress state just after requesting a dump is
        // ReceivingInterfaces
        Q_ASSERT(_dumpProgress == DumpProgress::ReceivingInterfaces);
        // Return to PendingDumpRequest::None, now that the dump has started we
        // could trigger another (which would be deferred until this dump
        // completes).
        _pendingDump = PendingDumpRequest::None;
        // The new interface state should already be empty (emptied by
        // publishing the previous results)
        Q_ASSERT(_receivingInterfaces.empty());
    }

    // Sanity check - should be an nl80211 message and version 1
    LibnlError::verify(pHeader->nlmsg_type == _nl80211Protocol, HERE, "Unexpected netlink message type");
    LibnlError::verify(pGenHeader->version == Nl80211Version, HERE, "Unsupported nl80211 version");

    // Check for supported commands
    switch(pGenHeader->cmd)
    {
        case NL80211_CMD_NEW_INTERFACE:
        {
            std::array<libnl::nlattr*, nl80211Attrs.ifaceAttrs().size()> attrs{};

            auto parseErr = libnl::nlmsg_parse(pHeader, GENL_HDRLEN, attrs.data(),
                                               attrs.size()-1,
                                               nl80211Attrs.ifaceAttrs().data());
            LibnlError::checkRet(parseErr, HERE, "Could not parse nl80211 message");

            parseInterfaceMsg(attrs.data());
            break;
        }
        // nl80211 does not send new-interface updates for changes to interface
        // config.  Since we only really care about SSID, listen for these
        // events and trigger a new dump when any of them occur.
        //
        // NetworkManager seems to poll every second or so when it expects
        // changes; these dumps should at least be less overhead than that.
        case NL80211_CMD_AUTHENTICATE:
        case NL80211_CMD_ASSOCIATE:
        case NL80211_CMD_DEAUTHENTICATE:
        case NL80211_CMD_DISASSOCIATE:
        case NL80211_CMD_CONNECT:
        case NL80211_CMD_DISCONNECT:
            // We need a new dump
            if(_pendingDump == PendingDumpRequest::None)
            {
                qInfo() << "Request interface dump due to event" << pGenHeader->cmd;
                _pendingDump = PendingDumpRequest::Needed;
            }
            else
            {
                qInfo() << "No dump needed for event" << pGenHeader->cmd
                    << "- already in state" << traceEnum(_pendingDump);
            }
            break;
        case NL80211_CMD_NEW_SCAN_RESULTS:
        {
            // Parse the scan attributes at the top level
            std::array<libnl::nlattr*, nl80211Attrs.scanAttrs().size()> attrs{};
            auto parseErr = libnl::nlmsg_parse(pHeader, GENL_HDRLEN, attrs.data(),
                                               attrs.size()-1,
                                               nl80211Attrs.scanAttrs().data());
            LibnlError::checkRet(parseErr, HERE, "Could not parse nl80211 scan message");

            // The parse the nested BSS attributes
            if(!attrs[NL80211_ATTR_BSS])
            {
                qInfo() << "Can't parse new scan result - no BSS attribute";
                break;
            }
            std::array<libnl::nlattr*, nl80211Attrs.scanBssAttrs().size()> bssAttrs{};
            parseErr = libnl::nla_parse_nested(bssAttrs.data(), bssAttrs.size()-1,
                                               attrs[NL80211_ATTR_BSS],
                                               nl80211Attrs.scanBssAttrs().data());
            LibnlError::checkRet(parseErr, HERE, "Could not parse nl80211 scan BSS");

            parseScanMsg(attrs, bssAttrs);
            break;
        }
        default:
            break;
    }

    return  0;
}

int LinuxNl80211Cache::handleFinish(libnl::nl_msg *)
{
    // This occurs if we request to dump interfaces when there are zero
    // interfaces present on the system.  (Note that this has to be after the
    // nl80211 protocol is registered, which usually means there _was_ an
    // interface at some point in the past.)  We won't get any ..._NEW_INTERFACE
    // commands in this case, we just get a "done" message and we have to
    // assume that it's our dump response.
    if(_pendingDump == PendingDumpRequest::Requested)
    {
        qInfo() << "Dump finished with no 802.11 interfaces";
        _pendingDump = PendingDumpRequest::None;
        // Stay in ReceivingInterfaces, let the normal logic take care of
        // advancing through all the remaining states with no interfaces
    }

    switch(_dumpProgress)
    {
        case DumpProgress::Inactive:
            // Shouldn't happen, we must have requested a dump
            qWarning() << "Received unexpected finish message while in state"
                << traceEnum(_pendingDump) << "/" << traceEnum(_dumpProgress);
            break;
        case DumpProgress::ReceivingInterfaces:
            // Start with the first interface.  0 isn't a valid interface
            // index, just let _receivingScanInterface = 0 and the normal logic
            // will advance to the first interface, then request it.
            qInfo() << "Interface dump completed with"
                << _receivingInterfaces.size()
                << "interfaces, check each interface";
            _receivingScanInterface = 0;
            [[fallthrough]];
        case DumpProgress::ReceivingScans:
        {
            // Find the next interface index (note that this is an ordered map)
            auto itNextItf = _receivingInterfaces.upper_bound(_receivingScanInterface);
            if(itNextItf != _receivingInterfaces.end())
            {
                _receivingScanInterface = itNextItf->first;
                requestScanDump(_receivingScanInterface);
            }
            else
            {
                // No more interfaces, publish new results
                _interfaces.swap(_receivingInterfaces);
                _receivingInterfaces.clear();
                qInfo() << "Scan dump completed with" << _interfaces.size()
                    << "interfaces";
                // We're done, go to Inactive.  It's possible that another dump
                // has already triggered, but we don't need to check
                // _pendingDump here, it'll be checked in handleMsgBatchEnd().
                _dumpProgress = DumpProgress::Inactive;
                // If this was the first dump, we're now ready to report data
                if(!_ready)
                {
                    qInfo() << "Initial nl80211 dump is ready";
                    _ready = true;
                }
            }
            break;
        }
    }

    return NL_OK;
}

void LinuxNl80211Cache::handleMsgBatchEnd()
{
    // We can't dump again when a request has already been sent, or in the
    // middle of a dump response - nl80211 fails the second dump with EBUSY.
    //
    // This does occur in practice.  For example, when disconnecting from a
    // network, it's common to observe the NL80211_CMD_DEAUTHENTICATE event,
    // then observe NL80211_CMD_DISCONNECT while waiting for the dump result.
    if(_pendingDump == PendingDumpRequest::Needed)
    {
        // We can't dump when in the middle of a dump response - nl80211 fails
        // the second dump with EBUSY.
        if(_dumpProgress != DumpProgress::Inactive || inDump())
        {
            qInfo() << "Dump has been requested but an ongoing dump hasn't completed, wait to send deferred dump request";
            // We'll have to get another message that ends the dump, that will
            // in turn call handleMsgBatchEnd() again so we can re-check.
        }
        else
        {
            requestInterfaceDump();
        }
    }
}
