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
#line HEADER_FILE("wireguardgobackend.h")

#ifndef WIREGUARDGOBACKEND_H
#define WIREGUARDGOBACKEND_H

#include "wireguardbackend.h"
#include "wireguarduapi.h"
#include "processrunner.h"
#include "recursivedirwatcher.h"
#include "async.h"
#include "vpn.h"
#include <QLocalSocket>

// ProcessRunner that applies the interface name file environment variable on
// Mac.
class WireguardGoRunner : public ProcessRunner
{
public:
    using ProcessRunner::ProcessRunner;
public:
    virtual void setupProcess(UidGidProcess &process) override;
};

// WireguardGoBackend uses the wireguard-go userspace implementation of
// Wireguard.  This implementation works on Mac and Linux.
//
// Configuration and stat updates use the Wireguard userspace IPC protocol.
class WireguardGoBackend : public WireguardBackend
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("wireguardgobackend")

private:
    static const QString wgSocketPath;

public:
    // Do cleanup of this method in case anything was left over from a crashed
    // daemon (kills existing pia-wireguard-go processes).
    static void cleanup();

public:
    WireguardGoBackend();
    virtual ~WireguardGoBackend() override;

private:
    void wgGoStarted(qint64 pid);
    void wgGoFailed();

public:
    virtual auto createInterface(wg_device &wgDev,
                                 const QPair<QHostAddress, int> &peerIpNet)
        -> Async<std::shared_ptr<NetworkAdapter>> override;

    virtual Async<WgDevPtr> getStatus() override;

    virtual Async<void> shutdown() override;
private:
    // Handling the various error and finish signals from QProcess is
    // nontrivial, so use a ProcessRunner to run wireguard-go.
    //
    // However, we never let it restart wireguard-go if it fails.  In theory, it
    // is possible to restart wireguard-go if it crashes without restarting the
    // entire connection (we should not need to authenticate again, etc.), but
    // we would then have to reconfigure it, reconnect, adjust bytecounts, defer
    // stats, etc.  wireguard-go is also pretty robust and unlikely to crash or
    // terminate unexpectedly.
    //
    // So, if it fails, we fail the whole connection and let VPNConnection start
    // over.
    //
    // At shutdown, on Mac, we need to remove the interface name file after
    // wireguard-go shuts down.  The WireguardGoRunner is in a nullable_t so we
    // can tear it down to ensure the process has exited.
    nullable_t<WireguardGoRunner> _wgGoRunner;
    // When we're attempting to connect, this is the asynchronous connect
    // operation.
    Async<void> _pConnectAttempt;
    // The interface name, once it is known.  This is always the same on Linux,
    // but on Mac it is a utun device chosen by the kernel.
    QString _interfaceName;
    // Wireguard socket path; built from that interface name.
    QString _wgSocketPath;
    // PID of the wireguard-go process
    qint64 _wgGoPid;
    // When shutdown() is called, we try to shut down wireguard-go.  If it shuts
    // down, this task (the task returned by shutdown()) is resolved.
    Async<void> _pShutdownTask;
};

#endif
