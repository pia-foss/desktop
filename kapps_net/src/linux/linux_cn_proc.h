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

#pragma once
#include <kapps_net/net.h>
#include <kapps_core/src/logger.h>
#include <kapps_core/src/coresignal.h>
#include <kapps_core/src/posix/posix_objects.h>
#include <kapps_core/src/posix/posixfdnotifier.h>
#include <functional>
#include <kapps_core/src/util.h>

namespace kapps { namespace net {

// CnProc connects a NETLINK_CONNECTOR socket and subscribes to Proc events
// (exec, exit, etc.).  This is used by split tunnel to monitor process
// execution.
//
// Note that both NETLINK_CONNECTOR and cn_proc are optional features of the
// Linux kernel, most x86_64 kernels seem to include cn_proc, but many ARM
// kernels seem to omit it.  (These kernels often include NETLINK_CONNECTOR as a
// module, so the socket will connect but we won't actually receive any events.)
class KAPPS_NET_EXPORT CnProc
{
public:
    CnProc();
    ~CnProc();

private:
    bool subscribeToProcEvents(bool enable);
    void readFromSocket();

public:
    // Indicates that the Netlink socket has been connected, _and_ we have
    // received the initial no-op event indicating that cn_proc events are
    // actually supported.
    //
    // There is no specific negative indication if these events are not enabled
    // in the running kernel; it may accept the subscription request but just
    // not generate any events.
    core::Signal<> connected;

    // A process exec() has occurred
    core::Signal<pid_t> exec;

    // A process exit has occurred
    core::Signal<pid_t> exit;

private:
    core::PosixFd _cnSock;
    core::PosixFdNotifier _cnSockNotifier;
};

}}
