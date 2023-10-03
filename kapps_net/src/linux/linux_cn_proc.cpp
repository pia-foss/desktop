// Copyright (c) 2023 Private Internet Access, Inc.
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

#include "linux_cn_proc.h"
#include <kapps_core/core.h>
#include <kapps_core/logger.h>
#include <linux/netlink.h>
#include <linux/cn_proc.h>
#include <linux/connector.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <kapps_core/src/newexec.h>
#include <unistd.h>

namespace kapps { namespace net {

namespace
{
    // Explicitly specify struct alignment
    typedef struct __attribute__((aligned(NLMSG_ALIGNTO)))
    {
        nlmsghdr header;

        // Insert no padding as we want the members contiguous
        struct __attribute__((__packed__))
        {
            cn_msg body;
            proc_cn_mcast_op subscription_type;
        };
    } NetlinkRequest;

    typedef struct __attribute__((aligned(NLMSG_ALIGNTO)))
    {
        nlmsghdr header;

        struct __attribute__((__packed__))
        {
            cn_msg body;
            proc_event event;
        };
    } NetlinkResponse;
}

CnProc::CnProc()
    : _cnSock{}
{
    KAPPS_CORE_INFO() << "Connecting to Netlink";

    // Set SOCK_CLOEXEC to prevent socket being inherited by child processes (such as openvpn)
    _cnSock = core::PosixFd{::socket(PF_NETLINK, SOCK_DGRAM|SOCK_CLOEXEC, NETLINK_CONNECTOR)};
    if(!_cnSock)
    {
        KAPPS_CORE_WARNING() << "Failed to open Netlink connector socket -"
            << core::ErrnoTracer{};
        return;
    }

    _cnSockNotifier.activated = [this](){readFromSocket();};
    _cnSockNotifier.set(_cnSock.get(), core::PosixFdNotifier::WatchType::Read);

    sockaddr_nl address = {};
    // Port ID 0 allows the kernel to assign an ID that's not in use.  The
    // first socket per process is given the PID conventionally, but
    // subsequent sockets will be given random IDs.
    address.nl_pid = 0;
    address.nl_groups = CN_IDX_PROC;
    address.nl_family = AF_NETLINK;

    if(::bind(_cnSock.get(), reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr_nl)) < 0)
    {
        KAPPS_CORE_WARNING() << "Failed to bind Netlink connector socket -"
            << core::ErrnoTracer{};
        return;
    }

    if(!subscribeToProcEvents(true))
    {
        KAPPS_CORE_WARNING() << "Could not subscribe to proc events";
        return;
    }

    KAPPS_CORE_INFO() << "Successfully connected to Netlink";
}

CnProc::~CnProc()
{
    KAPPS_CORE_INFO() << "Disconnecting from Netlink";
    if(_cnSock)
    {
        // Unsubscribe from proc events
        subscribeToProcEvents(false);
    }
}

bool CnProc::subscribeToProcEvents(bool enabled)
{
    sockaddr_nl localAddr{};
    socklen_t addrLen{sizeof(localAddr)};
    if(::getsockname(_cnSock.get(), reinterpret_cast<sockaddr*>(&localAddr),
       &addrLen) < 0)
    {
        KAPPS_CORE_WARNING() << "Failed to get socket port -" << kapps::core::ErrnoTracer{};
        return false;
    }
    if(addrLen != sizeof(localAddr))
    {
        KAPPS_CORE_WARNING() << "Failed to get socket port - got address size"
            << addrLen << "- expected" << sizeof(localAddr);
        return false;
    }

    NetlinkRequest message = {};
    message.subscription_type = enabled ? PROC_CN_MCAST_LISTEN : PROC_CN_MCAST_IGNORE;

    message.header.nlmsg_len = sizeof(message);
    message.header.nlmsg_pid = localAddr.nl_pid;
    message.header.nlmsg_type = NLMSG_DONE;

    message.body.len = sizeof(proc_cn_mcast_op);
    message.body.id.val = CN_VAL_PROC;
    message.body.id.idx = CN_IDX_PROC;

    sockaddr_nl destination = {};
    destination.nl_pid = 0; // kernel
    destination.nl_family = AF_NETLINK;
    int sent = ::sendto(_cnSock.get(), &message,   sizeof(message), 0,
                        reinterpret_cast<sockaddr*>(&destination),
                        sizeof(destination));
    if(sent < 0)
    {
        KAPPS_CORE_WARNING() << "Failed to send Netlink request -" << kapps::core::ErrnoTracer{};
        return false;
    }
    if(sent != sizeof(message))
    {
        KAPPS_CORE_WARNING() << "Sent" << sent << "bytes, expected to send" << sizeof(message);
        return false;
    }

    return true;
}

void CnProc::readFromSocket()
{
    NetlinkResponse message = {};

    int received = ::recv(_cnSock.get(), &message, sizeof(message), 0);

    if(received < 0)
    {
        KAPPS_CORE_WARNING() << "Failed receiving from socket -" << kapps::core::ErrnoTracer{};
        return;
    }

    if(received != sizeof(message))
    {
        KAPPS_CORE_WARNING() << "Received" << received
            << "bytes for Netlink message, expected" << sizeof(message);
        return;
    }

    // shortcut
    const auto &eventData = message.event.event_data;

    switch(message.event.what)
    {
    case proc_event::PROC_EVENT_NONE:
        KAPPS_CORE_INFO() << "Listening to process events";
        connected();
        break;
    case proc_event::PROC_EVENT_EXEC:
        exec(eventData.exec.process_pid);
        break;
    case proc_event::PROC_EVENT_EXIT:
        exit(eventData.exit.process_pid);
        break;
    default:
        // We're not interested in any other events
        break;
    }
}

}}
