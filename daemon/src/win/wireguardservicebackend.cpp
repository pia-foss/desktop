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
#line SOURCE_FILE("wireguardservicebackend.cpp")

#include "wireguardservicebackend.h"
#include "configwriter.h"
#include "path.h"
#include "exec.h"
#include "win_servicestate.h"
#include "../../extras/installer/win/service_inl.h"
#include "win/win_util.h"
#include "win/win_daemon.h" // WinNetworkAdapter
#include <QElapsedTimer>

#include <WS2tcpip.h>   // inet_pton()
#pragma comment(lib, "ws2_32.lib")

// Firewall notes:
//
// On Windows, the Wireguard embeddable-dll-service normally creates its own
// firewall rules.  We can't rely on them for PIA, because the PIA killswitch
// may remain engaged when the Wireguard service isn't running (during
// connection loss, etc.), and Wireguard's rules are incompatible with PIA
// features such as killswitch settings, Use Existing DNS, Allow LAN, split
// tunnel, etc.
//
// We avoid most of Wireguard's firewall rules:
// - We apply the allowed IPs as 0/1 and 128/1 instead of 0/0, which avoids the
//   killswitch rules (they're incompatible with PIA features such as KS,
//   Allow LAN)
// - We don't pass DNS servers to Wireguard, so it doesn't create DNS server
//   rules.  (PIA applies DNS itself, it just calls netsh, which is all
//   Wireguard does anyway.)
//
// Wireguard still creates a few rules but they're in its own WFP sublayer
// (with sublayer weight 0), and they are all permit rules, so they have no
// effect.  (It creates "permit all on TUN" and "permit Wireguard service" rules
// for both IPv4 and IPv6.)
//
// (In WFP, generally traffic must be allowed or ignored in each sublayer to be
// allowed - a block in any sublayer overrides a permit in any other sublayer.
// Since the Wireguard sublayer contains only "permit" rules, it has no effect.
// This ignores "hard permit" and "veto" actions, but those generally only apply
// when callout drivers are in use.)

namespace
{
    // Timeout for service shutdown during initial cleanup.
    // The only effect of this is just to abandon tasks and trace the timeout,
    // since we already told SCM to shut down the service.
    const std::chrono::seconds cleanupTimeout{40};

    // When starting or stopping Wireguard, this WinServiceState is used to
    // monitor the state of the service.
    //
    // It's initially created the first time we try to start/stop the service,
    // which occurs when Wireguard cleanup occurs at daemon startup.
    //
    // If we need to start/stop the service at any time, but the WinServiceState
    // has reached the Deleted state, it's re-created to try to reopen the
    // service.
    nullable_t<WinServiceState> &wgServiceState()
    {
        // This is a function local static to ensure it's destroyed before the
        // statics used by the logger; WinServiceState traces during
        // destruction.
        static nullable_t<WinServiceState> pWgServiceState;
        return pWgServiceState;
    }

    void openWgServiceState()
    {
        auto &pWgServiceState = wgServiceState();
        if(!pWgServiceState || pWgServiceState->lastState() == WinServiceState::State::Deleted)
        {
            qInfo() << "WireGuard service wasn't open, open it now";
            pWgServiceState.clear();
            pWgServiceState.emplace(std::wstring{g_wireguardServiceParams.pName});
        }
    }

    // Write an IPv4 address
    ConfigWriter &operator<<(ConfigWriter &w, const in_addr &addr4)
    {
        char address[INET_ADDRSTRLEN]{};
        if(!::inet_ntop(AF_INET, &addr4, address, sizeof(address)))
        {
            // Not possible - only fails due to invalid family or buffer
            // length
            Q_ASSERT(false);
        }
        w << address;
        return w;
    }

    // Write a WG peer endpoint
    ConfigWriter &operator<<(ConfigWriter &w, const decltype(wg_peer::endpoint) &endpoint)
    {
        switch(endpoint.addr.sa_family)
        {
            case AF_INET:
                w << endpoint.addr4.sin_addr << ':' << ntohs(endpoint.addr4.sin_port);
                break;
            case AF_INET6:
                // Not implemented - not used by PIA
                // Fall through
            default:
                qWarning() << "Endpoint family not implemented:" << endpoint.addr.sa_family;
                w.invalidate();
                break;
        }
        return w;
    }

    // Write a WG allowed IP
    ConfigWriter &operator<<(ConfigWriter &w, const wg_allowedip &ip)
    {
        switch(ip.family)
        {
            case AF_INET:
                w << ip.ip4 << '/' << ip.cidr;
                break;
            case AF_INET6:
                // Not implemented - not used by PIA
                // Fall through
            default:
                qWarning() << "Allowed IP family not implemented:" << ip.family;
                w.invalidate();
                break;
        }
        return w;
    }

    // Task to connect to a local socket asynchronously...to the extent possible
    // on Windows.
    //
    // Connecting to a named pipe on Windows can fail in a 'busy' state if the
    // server has accepted a connection but has not called ConnectNamedPipe()
    // again to wait for another connection.  Servers are supposed to minimize
    // this time by calling ConnectNamedPipe() again ASAP, but some busy time is
    // unavoidable.
    //
    // MS recommends WaitNamedPipe() to detect when the pipe is no longer busy,
    // but this is essentially just a blocking busy wait.  QLocalSocket does
    // this too.
    //
    // Instead, WinLocalSocketTask delays for 100ms and tries again if the pipe
    // is busy.  The worst effect of this is ~100ms extra delay if the pipe
    // becomes ready just as we begin a delay, which is not bad.  The pipe
    // should not be busy much; PIA should be the main client of the pipe, and
    // we don't send requests too frequently.
    //
    // WinLocalSocketTask continues to retry as long as the pipe is busy; an
    // overall timeout can be applied with Async::timeout().  (It does not retry
    // for other errors.)
    class WinLocalSocketTask : public Task<std::shared_ptr<QLocalSocket>>
    {
        Q_OBJECT
        CLASS_LOGGING_CATEGORY("winlocalsockettask")

    public:
        // Start an asynchronous connection attempt to the pipe specified.
        explicit WinLocalSocketTask(QString path)
            : _path{std::move(path)}, _attempts{0}
        {
            _totalTime.start();
            connect();
        }

    private:
        // Try to connect now, schedule a retry if the pipe is busy;
        // resolve/reject otherwise.
        void connect();

    private:
        QString _path;
        QElapsedTimer _totalTime;
        unsigned _attempts;
    };

    void WinLocalSocketTask::connect()
    {
        // Class invariant; only called when pending (timer is not scheduled if
        // the task resolves)
        Q_ASSERT(isPending());

        auto keepAlive = sharedFromThis();

        ++_attempts;

        // QLocalSocket expects an overlapped socket handle
        HANDLE socket = ::CreateFile(qstringWBuf(_path),
                                     GENERIC_READ|GENERIC_WRITE,
                                     0,
                                     nullptr,
                                     OPEN_EXISTING,
                                     FILE_FLAG_OVERLAPPED,
                                     nullptr);
        if(socket && socket != INVALID_HANDLE_VALUE)
        {
            // Put the socket handle in a QLocalSocket
            auto pLocalSocket = std::make_shared<QLocalSocket>();
            // setSocketDescriptor() returns bool, but it always returns true on
            // Windows.
            bool openSuccess = pLocalSocket->setSocketDescriptor(reinterpret_cast<qintptr>(socket));
            Q_ASSERT(openSuccess);
            resolve(std::move(pLocalSocket));
            return;
        }

        WinErrTracer connectError{::GetLastError()};
        if(connectError.code() != ERROR_PIPE_BUSY)
        {
            qWarning() << "Failed to connect to pipe" << _path << "after"
                << _attempts << "attempts -" << traceMsec(_totalTime.elapsed())
                << connectError;
            reject({HERE, Error::Code::WireguardNotResponding});
            return;
        }

        // The pipe is busy - try again in 100ms.  This isn't expected to happen
        // much, so trace at warning.
        qWarning() << "Pipe" << _path << "busy after" << _attempts
            << "attempts -" << traceMsec(_totalTime.elapsed())
            << "wait and try again";
        QTimer::singleShot(100, this, &WinLocalSocketTask::connect);
    }

    // Task to locate the WinTUN interface created by WireGuard
    class WinTunInterfaceTask : public Task<std::shared_ptr<NetworkAdapter>>
    {
        Q_OBJECT

    public:
        WinTunInterfaceTask()
        {
            connect(&WinInterfaceMonitor::instance(), &WinInterfaceMonitor::changed,
                    this, &WinTunInterfaceTask::checkInterface);
            checkInterface();
        }

    private:
        // Check for the interface, resolve if it exists
        void checkInterface()
        {
            if(!isPending())
                return; // Already finished, nothing to do

            auto keepAlive = sharedFromThis();

            int findResult = Exec::cmd(Path::WireguardServiceExecutable,
                                       {QStringLiteral("/findinterface"),
                                        WireguardBackend::interfaceName,
                                        Path::WireguardInterfaceFile});
            // Not finding the device is normal, it likely just hasn't been
            // created yet.
            if(findResult == 2)
                return; // Output traced from /findinterface

            // Any other error is unexpected
            if(findResult != 0)
            {
                qWarning() << "Attempt to find interface returned error" << findResult;
                return;
            }

            // Read the LUID from the interface file
            QFile itfFile{Path::WireguardInterfaceFile};
            if(!itfFile.open(QIODevice::ReadOnly))
            {
                qWarning() << "Interface file could not be opened";
                return;
            }

            enum { NameLen = 20 };  // Max length for a 64-bit integer as a string
            char itfLuidBuf[NameLen]{};
            qint64 read = itfFile.read(itfLuidBuf, NameLen);
            if(read <= 0)
            {
                qWarning() << "Failed to read interface LUID from file -"
                    << read << "-" << itfFile.errorString();
                return;
            }

            auto luidStr = QString::fromUtf8(itfLuidBuf, read);
            // Parse the unsigned 64-bit value - 0 isn't a valid LUID, so no
            // need to pass an explicit OK flag.
            auto luid = luidStr.toULongLong();
            if(!luid)
            {
                qWarning() << "Failed to parse interface LUID"
                    << luidStr;
                return;
            }

            auto pAdapter = WinInterfaceMonitor::getAdapterForLuid(luid);
            if(pAdapter)
            {
                qInfo() << "Found adapter" << *pAdapter;
                resolve(std::move(pAdapter));
            }
        }
    };
}

bool WireguardServiceBackend::_doingInitialCleanup{false};

const QString &WireguardServiceBackend::pipePath()
{
    static const auto _pipePath =
        QStringLiteral(R"(\\.\pipe\ProtectedPrefix\Administrators\WireGuard\%1)")
            .arg(interfaceName);
    return _pipePath;
}

void WireguardServiceBackend::cleanFile(const Path &file, const QString &traceName)
{
    // Remove a stale config file if it existed
    if(QFile::remove(file))
    {
        qWarning() << "Removed leftover" << traceName << "file" << file;
    }
    // We couldn't remove the file; if it's not there this is normal.  (This is
    // a filesystem race, but it only affects tracing.)
    else if(QFile::exists(file))
    {
        qWarning() << "Can't remove leftover" << traceName << "file" << file;
    }
    else
    {
        qInfo() << "No leftover" << traceName << "file to remove -" << file;
    }
}

Async<void> WireguardServiceBackend::asyncCleanup()
{
    cleanFile(Path::WireguardConfigFile, QStringLiteral("config"));
    cleanFile(Path::WireguardInterfaceFile, QStringLiteral("interface"));

    // Stop the WireGuard service if it is running, then attempt to clean up
    openWgServiceState();
    Q_ASSERT(wgServiceState());  // Postcondition of openWgServiceState()
    TraceStopwatch stopwatch{"Stopping WireGuard"};
    return wgServiceState()->stopIfRunning();
}

void WireguardServiceBackend::cleanup()
{
    // Should only be called once at startup
    if(_doingInitialCleanup)
    {
        qWarning() << "Already doing initial cleanup";
        return;
    }

    _doingInitialCleanup = true;
    // Stop the service if it was running.  This is asynchronous, just fire and
    // forget - even if we try to connect before the state change completes, the
    // start request will be properly serialized with the start (it could still
    // fail if the service is stuck stopping, which we handle as a normal
    // failure).
    //
    // Like startService(), this can still block for up to 30 seconds if SCM is
    // busy, which will just block the daemon.
    return asyncCleanup()
        .timeout(cleanupTimeout)
        // Try to clean the interface up even if the task above times out or
        // fails
        ->next([](const Error &err)
        {
            // After ensuring that the service is stopped (or timing out), try
            // to delete the wgpia0 interface if it already existed.  This can
            // happen if we didn't cleanly shut down while connecte, such as if
            // the OS crashes or power is lost.
            Exec::cmd(Path::WireguardServiceExecutable, {QStringLiteral("/cleaninterface"), interfaceName});

            _doingInitialCleanup = false;
            qInfo() << "WireGuard initial cleanup finished";
        })
        ->runUntilFinished();
}

WireguardServiceBackend::WireguardServiceBackend()
{
}

WireguardServiceBackend::~WireguardServiceBackend()
{
    // This backend doesn't have any state of its own; shutdown() issues the
    // request to stop the service since it is asynchronous.
}

auto WireguardServiceBackend::createInterface(wg_device &wgDev,
                                              const QPair<QHostAddress, int> &peerIpNet)
    -> Async<std::shared_ptr<NetworkAdapter>>
{
    if(_doingInitialCleanup)
    {
        qWarning() << "Cannot connect to WireGuard yet, initial cleanup is still occurring";
        return Async<std::shared_ptr<NetworkAdapter>>::reject({HERE, Error::Code::WireguardCreateDeviceFailed});
    }

    // Write the config file
    {
        ConfigWriter conf{Path::WireguardConfigFile};
        auto endl{conf.endl};

        conf << "[Interface]" << endl;
        conf << "Address = " << peerIpNet.first.toString() << "/" << peerIpNet.second << endl;

        // Unimplemented/ignored flags:
        // - WGDEVICE_REPLACE_PEERS - no effect, this is initial configuration
        // - WGDEVICE_HAS_PUBLIC_KEY - specify private key instead
        // - WGDEVICE_HAS_FWMARK - not applicable on Windows
        Q_ASSERT(!(wgDev.flags & WGDEVICE_HAS_PUBLIC_KEY));
        Q_ASSERT(!(wgDev.flags & WGDEVICE_HAS_FWMARK));
        if(wgDev.flags & WGDEVICE_HAS_PRIVATE_KEY)
            conf << "PrivateKey = " << wgKeyToB64(wgDev.private_key) << endl;

        if(wgDev.flags & WGDEVICE_HAS_LISTEN_PORT)
            conf << "ListenPort = " << wgDev.listen_port << endl;

        conf << endl; // Blank line; just for readability

        for(const wg_peer *pPeer = wgDev.first_peer; pPeer; pPeer = pPeer->next_peer)
        {
            conf << "[Peer]" << endl;
            // Unimplemented/ignored flags:
            // - WGPEER_REMOVE_ME - does not make sense in initial config
            // - WGPEER_REPLACE_ALLOWEDIPS - no effect; initial config
            // - WGPEER_HAS_PRESHARED_KEY - not used in PIA
            Q_ASSERT(!(pPeer->flags & WGPEER_REMOVE_ME));
            Q_ASSERT(!(pPeer->flags & WGPEER_HAS_PRESHARED_KEY));

            conf << "Endpoint = " << pPeer->endpoint << endl;

            if(pPeer->flags & WGPEER_HAS_PUBLIC_KEY)
                conf << "PublicKey = " << wgKeyToB64(pPeer->public_key) << endl;

            if(pPeer->flags & WGPEER_HAS_PERSISTENT_KEEPALIVE_INTERVAL)
            {
                conf << "PersistentKeepalive = "
                    << pPeer->persistent_keepalive_interval << endl;
            }

            conf << "AllowedIPs =";
            char delim = ' ';
            for(const wg_allowedip *pIp = pPeer->first_allowedip; pIp; pIp = pIp->next_allowedip)
            {
                conf << delim;

                // We don't want the Wireguard service to create killswitch WFP
                // rules, we do that ourselves.  Pass 0/0 as 0/1, 128/1 instead.
                if(pIp->family == AF_INET && pIp->ip4.s_addr == 0 && pIp->cidr == 0)
                    conf << "0.0.0.0/1,128.0.0.0/1";
                else
                    conf << *pIp;
                delim = ',';
            }

            conf << endl;
        }

        if(conf.invalid())
        {
            qWarning() << "Could not create config file for WireGuard device";
            return Async<std::shared_ptr<NetworkAdapter>>::reject({HERE, Error::Code::WireguardCreateDeviceFailed});
        }
    }

    // Before trying to start the WireGuard service, ensure that it was stopped.
    // If a prior run tried to start WireGuard and was unable to stop it, this
    // ensures that we are not stuck unable to start WireGuard since it was
    // already running - subsequent attempts will try to stop it and continue if
    // it can be stopped.
    openWgServiceState();
    Q_ASSERT(wgServiceState());  // Postcondition of openWgServiceState()
    TraceStopwatch stopwatch{"Stopping WireGuard if it was running"};
    return wgServiceState()->stopIfRunning()
        ->next([](const Error &err)
        {
            // Start the service asynchronously.  However, it is still possible for
            // StartServiceW() to block for up to 30 seconds if SCM is busy.  If that
            // happens, we let it block the daemon.  We could put this on a worker
            // thread, but then start/stop controls would just queue up on the worker
            // thread, which is not much better.
            TraceStopwatch stopwatch{"Starting WireGuard"};
            openWgServiceState();
            Q_ASSERT(wgServiceState()); // Postcondition of openWgServiceState()
            return wgServiceState()->startService();
        })
        // If starting the service failed, hint to the daemon to re-check
        // whether WinTUN is installed (this is how it fails if WinTUN is
        // missing, it goes directly from StartPending to StopPending).
        ->except([](const Error &err) -> Async<std::shared_ptr<NetworkAdapter>>
        {
            // Starting the service succeeded, but finding the
            // network adapter failed.  Hint to the daemon to
            // check the WinTUN state.
            auto pDaemon = g_daemon;
            if(pDaemon)
                pDaemon->wireguardServiceFailed();
            throw err;
        })
        ->next([](const Error &err) -> Async<std::shared_ptr<NetworkAdapter>>
        {
            // Remove the config file, prevent the user from accidentally
            // starting the service with stale configuration
            if(QFile::remove(Path::WireguardConfigFile))
                qInfo() << "Removed" << Path::WireguardConfigFile;
            else
                qWarning() << "Unable to remove" << Path::WireguardConfigFile;

            if(err)
            {
                // Startup failed, reject with a more specific error
                qWarning() << "Couldn't start WireGuard service:" << err;
                return Async<std::shared_ptr<NetworkAdapter>>::reject({HERE, Error::Code::WireguardCreateDeviceFailed});
            }

            // The service was started.  The service reports that it's started
            // after opening the named pipe, so we don't delay waiting for the
            // named pipe to show up.
            //
            // The interface doesn't show up immediately though, the service
            // signals that it has started before creating the interface to
            // avoid SCM deadlocks.
            qInfo() << "WireGuard started, find the WinTUN interface";
            return Async<WinTunInterfaceTask>::create();
        })
        ->then([](const std::shared_ptr<NetworkAdapter> &pAdapter)
        {
            // The connection succeeded and we found the WinTUN interface.
            // Hint to the daemon that it should re-check the WinTUN state if it
            // thought WinTUN was not installed.
            auto pDaemon = g_daemon;
            if(pDaemon)
                pDaemon->wireguardConnectionSucceeded();
            return pAdapter;
        });
}

auto WireguardServiceBackend::getStatus() -> Async<WgDevPtr>
{
    return Async<WinLocalSocketTask>::create(pipePath())
        ->then([](const std::shared_ptr<QLocalSocket> &pSocket)
        {
            Q_ASSERT(pSocket);  // Postcondition of WinLocalSocketTask (rejects otherwise)
            return Async<WireguardDeviceStatusTask>::create(pSocket);
        })
        ->then([](const std::shared_ptr<WgDevStatus> &pDev)
        {
            Q_ASSERT(pDev); // Postcondition of WireguardDeviceStatusTask
            // Like WireguardGoBackend, return an aliased shared pointer
            return WgDevPtr{pDev, &pDev->device()};
        });
}

Async<void> WireguardServiceBackend::shutdown()
{
    return asyncCleanup();
}

#include "wireguardservicebackend.moc"
