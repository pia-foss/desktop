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
#line HEADER_FILE("linux/linux_cn_proc.h")

#ifndef LINUX_CN_PROC_H
#define LINUX_CN_PROC_H

#include <QSocketNotifier>
#include "linux_objects.h"

// CnProc connects a NETLINK_CONNECTOR socket and subscribes to Proc events
// (exec, exit, etc.).  This is used by split tunnel to monitor process
// execution.
//
// Note that both NETLINK_CONNECTOR and cn_proc are optional features of the
// Linux kernel, most x86_64 kernels seem to include cn_proc, but many ARM
// kernels seem to omit it.  (These kernels often include NETLINK_CONNECTOR as a
// module, so the socket will connect but we won't actually receive any events.)
class CnProc : public QObject
{
    Q_OBJECT

public:
    CnProc();
    ~CnProc();

private:
    void readFromSocket();
    bool subscribeToProcEvents(bool enable);

signals:
    // Indicates that the Netlink socket has been connected, _and_ we have
    // received the initial no-op event indicating that cn_proc events are
    // actually supported.
    //
    // There is no specific negative indication if these events are not enabled
    // in the running kernel; it may accept the subscription request but just
    // not generate any events.
    void connected();

    // A process exec() has occurred
    void exec(pid_t pid);

    // A process exit has occurred
    void exit(pid_t pid);

private:
    LinuxFd _cnSock;
    nullable_t<QSocketNotifier> _pReadNotifier;
};

#endif
