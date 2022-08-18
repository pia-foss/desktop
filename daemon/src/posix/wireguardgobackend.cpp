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

#include <common/src/common.h>
#line SOURCE_FILE("wireguardgobackend.cpp")

#include "wireguardgobackend.h"
#include <common/src/builtin/path.h>
#include <common/src/async.h>
#include <common/src/exec.h>
#include "brand.h"
#include <unistd.h>
#include <memory>

namespace
{
    // The restart strategy for wireguard-go doesn't really matter, since we
    // abort if the process fails at all.
    RestartStrategy::Params wgGoRestartParams{std::chrono::milliseconds{100},
                                              std::chrono::seconds{1},
                                              std::chrono::milliseconds{100}};

    // How long to wait for each individual attempt to connect to the local
    // socket
    std::chrono::seconds localSocketAttemptTimeout{1};
}

// Task to get the device name once wireguard-go is started on Mac.
//
// On Linux, the device name is fixed, but on Mac we let the kernel pick the
// device name, so we have to get the result from a file written by
// wireguard-go.
//
// The task only reject in some catastrophic cases (such as file content that's
// unexpectedly long); a timeout should usually be applied with
// Async::timeout().
//
// (If failures occur in wireguard-go that prevent it from writing an interface
// name, we expect wireguard-go to shut down, which will abandon the attempt and
// abort this task.)
class MacInterfaceNameTask : public Task<QString>
{
    Q_OBJECT

public:
    MacInterfaceNameTask();

private:
    // Check for the interface name file, resolve if it exists and contains a
    // device name
    void checkInterfaceFile();

private:
    RecursiveWatcher _devFileWatcher;
};

// Task to connect to a local socket asynchronously.
//
// If the connection can be established, the task is resolved with a
// QLocalSocket in the Connected state.  Slots should be connected synchronously
// in order to connect to the socket's error/state signals, this ensures the
// caller will receive any subsequent errors or state changes.
//
// Note that it is not safe to destroy the QLocalSocket during slots connected
// to the Task (since it is not safe to destroy QLocalSocket during its
// signals).
class LocalSocketTask : public Task<std::shared_ptr<QLocalSocket>>
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("localsockettask")

public:
    // Start an asynchronous connection.  The socket name is passed to
    // QLocalSocket - it can be a complete (platform-dependent) path, or a name
    // that QLocalSocket will map to a path.
    explicit LocalSocketTask(const QString &name);

private:
    void socketError(QLocalSocket::LocalSocketError err);
    void socketDisconnected();
    void socketConnected();

private:
    std::unique_ptr<QLocalSocket> _pSocket;
};

// Task to connect to a local socket that may not have been created yet (or even
// whose parent directory may not have been created yet).
//
// Attempts to connect to the socket immediately.  If this fails, will retry
// when any change is observed in the containing directory or its ancestors (to
// detect when the socket may have been created).  If the socket exists but the
// connection is refused, will start short-polling the socket (this happens with
// WG on some Linux systems, PIA tries to connect so quickly that WG isn't ready
// yet).
//
// Rejected if the timeout elapses.
class PendingLocalSocketTask : public Task<std::shared_ptr<QLocalSocket>>
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("pendinglocalsockettask")

public:
    // Start trying to connect.  The socket must be specified with an absolute
    // path (in order to watch its parent directories).
    PendingLocalSocketTask(Path socketPath);

private:
    void directoryChanged();
    void beginAttempt();

private:
    Path _socketPath;
    RecursiveWatcher _dirWatcher;
    // When an attempt is ongoing, it's held here
    Async<void> _pAttempt;
    // If a directory change is observed during a connection attempt, we should
    // try again as soon as that attempt completes.
    bool _alreadyChanged;
};

MacInterfaceNameTask::MacInterfaceNameTask()
    : _devFileWatcher{Path::WireguardInterfaceFile}
{
    connect(&_devFileWatcher, &RecursiveWatcher::check, this,
            &MacInterfaceNameTask::checkInterfaceFile);
    checkInterfaceFile();
}

void MacInterfaceNameTask::checkInterfaceFile()
{
    QFile itfFile{Path::WireguardInterfaceFile};
    if(!itfFile.open(QIODevice::ReadOnly))
    {
        qInfo() << "Interface name file does not exist yet, wait for it to be created";
        return;
    }

    // The interface name can be up to 9 characters, there's no reason it should
    // be longer than that.  If we read the entire buffer length of 10
    // characters, the name may have been truncated.
    enum { NameLen = 10 };
    char itfName[NameLen]{};
    qint64 read = itfFile.read(itfName, NameLen);
    if(read < 0)
    {
        // Read error - not expected, but it could be possible we caught the
        // file as it was being created, etc.  Ignore it and try again when
        // we're notified of another change.
        qWarning() << "Unable to read interface name from file, result"
            << read << "-" << itfFile.errorString();
        return;
    }

    if(read == 0)
    {
        // File is empty, probably means it just hasn't been written yet.
        qInfo() << "Interface name file is empty, wait for it to be written";
        return;
    }

    // wireguard-go adds a line break
    if(read > 0 && itfName[read-1] == '\n')
        --read;

    auto interfaceName = QString::fromUtf8(itfName, read);
    if(read >= NameLen)
    {
        // Name is way too long - most likely indicates unexpected content in
        // the file.
        qWarning() << "Interface name too long - read" << read
            << "-" << interfaceName;
        reject({HERE, Error::Code::WireguardCreateDeviceFailed});
    }

    // Otherwise, we got the interface name.
    qInfo() << "Found interface name" << interfaceName;
    resolve(std::move(interfaceName));
}

LocalSocketTask::LocalSocketTask(const QString &name)
{
    _pSocket.reset(new QLocalSocket{});
    connect(_pSocket.get(),
            QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error),
            this, &LocalSocketTask::socketError);
    // We don't expect to get a disconnected signal - we hand off the socket as
    // soon as it's connected - but it's not totally clear that QLocalSocket
    // always emits an error when the connection fails.  Handle this as an
    // error for robustness.
    connect(_pSocket.get(), &QLocalSocket::disconnected, this,
            &LocalSocketTask::socketDisconnected);
    connect(_pSocket.get(), &QLocalSocket::connected, this,
            &LocalSocketTask::socketConnected);
    _pSocket->connectToServer(name);
}

void LocalSocketTask::socketError(QLocalSocket::LocalSocketError err)
{
    Q_ASSERT(isPending());  // Class invariant, socket disconnected when resolved
    Q_ASSERT(_pSocket); // Class invariant, valid when signal is connected
    qWarning() << "Connection to" << _pSocket->fullServerName()
        << "failed, error" << err;
    // Tear down the socket.  We can't delete the socket in this slot, so
    // disconnect signals and delete later.
    _pSocket->disconnect(this);
    _pSocket.release()->deleteLater();
    // Reject with LocalSocketNotFound only if we are sure that the socket did
    // not exist.  This means that PendingLocalSocketTask will wait for a
    // directory change to try to connect again.
    if(err == QLocalSocket::LocalSocketError::ServerNotFoundError)
        reject({HERE, Error::Code::LocalSocketNotFound});
    // Otherwise, reject with LocalSocketCannotConnect, the socket likely exists
    // but we were still unable to connect.  Since we won't necessarily get
    // another filesystem change, PendingLocalSocketTask will start polling.
    else
        reject({HERE, Error::Code::LocalSocketCannotConnect});
    // May have destroyed *this
}

void LocalSocketTask::socketDisconnected()
{
    Q_ASSERT(isPending());  // Class invariant, socket disconnected when resolved
    Q_ASSERT(_pSocket); // Class invariant, valid when signal is connected
    qWarning() << "Unexpected disconnect from" << _pSocket->fullServerName()
        << "before connecting";
    socketError(QLocalSocket::LocalSocketError::UnknownSocketError);
    // May have destroyed *this
}

void LocalSocketTask::socketConnected()
{
    Q_ASSERT(isPending());  // Class invariant, socket disconnected when resolved
    Q_ASSERT(_pSocket); // Class invariant, valid when signal is connected
    // Disconnect signals from the socket; hand over the socket now that it's
    // connected
    _pSocket->disconnect(this);
    // Move it to a shared pointer; Task requires a copiable value type
    resolve(std::shared_ptr<QLocalSocket>{_pSocket.release()});
    // May have destroyed *this
}

PendingLocalSocketTask::PendingLocalSocketTask(Path socketPath)
    : _socketPath{std::move(socketPath)},
      _dirWatcher{_socketPath.parent()},    // Watch the parent directory
      _alreadyChanged{false}
{
    connect(&_dirWatcher, &RecursiveWatcher::check, this,
        &PendingLocalSocketTask::directoryChanged);

    // Begin an attempt now
    beginAttempt();
}

void PendingLocalSocketTask::directoryChanged()
{
    if(_pAttempt)
    {
        qInfo() << "Change occurred while already connecting to"
            << _socketPath << "- will try again if this attempt fails";
        qInfo() << "_pAttempt:" << QString::number(reinterpret_cast<quint64>(_pAttempt.get()), 16);
        _alreadyChanged = true;
        return;
    }

    qInfo() << "Observed directory change, attempt to connect to"
        << _socketPath;
    beginAttempt();
}

void PendingLocalSocketTask::beginAttempt()
{
    Q_ASSERT(!_pAttempt);   // Ensured by caller
    Q_ASSERT(!_alreadyChanged); // Class invariant, only set when _pAttempt is set

    auto pAttempt = Async<LocalSocketTask>::create(_socketPath)
        .timeout(localSocketAttemptTimeout)
        ->next(this, [this](const Error &error, const std::shared_ptr<QLocalSocket> &result)
            {
                // If the connection was established, we're done
                if(!error)
                {
                    resolve(result);
                    // May have destroyed *this
                    return;
                }

                qWarning() << "Attempt for" << _socketPath << "failed -"
                    << error;

                _pAttempt.reset();
                // If we already observed a change, try again right now.
                if(_alreadyChanged)
                {
                    qInfo() << "Already observed a change, try to connect to"
                        << _socketPath << "again";
                    _alreadyChanged = false;
                    beginAttempt();
                }
                else if(error.code() == Error::Code::LocalSocketNotFound)
                {
                    // Socket definitely doesn't exist yet, wait for a directory
                    // change before trying again
                    qInfo() << "Wait for" << _socketPath
                        << "to be created before trying again";
                }
                else
                {
                    // The socket likely exists at this point (definitely true
                    // if we got QLocalSocket::LocalSocketError::ConnectionRefused,
                    // but might be true with some other ambiguous errors.
                    // We won't necessarily get another filesystem change, so
                    // poll again in 100ms.
                    qInfo() << "Connection failed, but socket" << _socketPath
                        << "may exist, wait briefly and try again";
                    QTimer::singleShot(100, this, [this]()
                    {
                        qInfo() << "Recheck" << _socketPath
                            << "now since the socket was present for the last attempt";
                        directoryChanged();
                    });
                }
            });
    // The attempt can complete synchronously, in which case the handler was
    // already called, and could have resolved the task.  Only store pAttempt if
    // it is not resolved yet.
    if(pAttempt->isPending())
        _pAttempt = std::move(pAttempt);
}

void WireguardGoRunner::setupProcess(UidGidProcess &process)
{
    auto env = QProcessEnvironment::systemEnvironment();
#if defined(Q_OS_MAC)
    env.insert(QStringLiteral("WG_TUN_NAME_FILE"), Path::WireguardInterfaceFile);
#elif defined(Q_OS_LINUX)
    // This has the same effect as --foreground, but it also suppresses the
    // "you should use the kernel module" warning
    env.insert(QStringLiteral("WG_PROCESS_FOREGROUND"), QStringLiteral("1"));
#endif
    env.insert(QStringLiteral("LOG_LEVEL"), QStringLiteral("debug"));
    process.setProcessEnvironment(env);
}

void WireguardGoBackend::cleanup()
{
#if defined(Q_OS_LINUX)
    const QString wgSocketPath{QStringLiteral("/var/run/wireguard/") +
                               WireguardBackend::interfaceName +
                               QStringLiteral(".sock")};
    // Remove the socket file if it exists; this causes wireguard-go to shut
    // down.
    if(::unlink(wgSocketPath.toUtf8().constData()))
    {
        if(errno == ENOENT)
        {
            qInfo() << "No existing userspace socket to clean up -"
                << wgSocketPath;
        }
        else
        {
            // This is bad, it indicates that there is a Wireguard tunnel up,
            // probably due to the daemon crashing, and we are not able to
            // remove it.
            qWarning() << "Unable to remove existing socket:" << errno
                << "-" << wgSocketPath;
        }
    }
    else
    {
        // This likely indicates that the daemon had crashed, trace at warning
        qWarning() << "Removed existing userspace socket -" << wgSocketPath;
    }
#elif defined(Q_OS_MACOS)
    // On Mac, we don't know the interface name, since it's a utun### name.  We
    // don't want to risk tearing down any other WG tunnel or utun interface
    // that wasn't started by PIA.  (There could even be a stale
    // Path::WireguardInterfaceFile that refers to some utun interface that's
    // now used by some other process.)
    //
    // The most reasonable remaining option is to just try to terminate any
    // existing 'pia-wireguard-go' process.  This won't interact with a
    // wireguard-go process run by some other product, and on the off chance
    // that somebody does name a process 'pia-wireguard-go', the workaround is
    // just to rename it.
    //
    // We don't expect there to be a leftover process, so if this does
    // successfully kill something, trace a warning.
    if(0 == Exec::cmd(QStringLiteral("killall"), {QStringLiteral("-v"), Files::wireguardGoBasename}))
    {
        qWarning() << "Killed existing" << Files::wireguardGoBasename;
    }
#endif
}

// Dump interface diagonstics when WireGuard fails to assign an address
// This is used to investigate an issue on macOS Catalina where the tunnel interface
// may be arbitrary deleted by the OS.
// see: https://github.com/WireGuard/wireguard-apple/commit/794acb7e724f53043d33eaa68b68607dcdab6222
#ifdef Q_OS_MACOS
static void logInterfaces(const QByteArray &line)
{
    qInfo() << "Detected interface error, dumping ifconfig/netstat settings";
    Exec::cmd(QStringLiteral("ifconfig"), {});
    Exec::cmd(QStringLiteral("netstat"), {"-nr", "-f", "inet"});
    Exec::cmd(QStringLiteral("scutil"), {"--dns"});
    Exec::cmd(QStringLiteral("pfctl"), {"-a", BRAND_IDENTIFIER "/*", "-sr"});
    Exec::cmd(QStringLiteral("pfctl"), {"-a", BRAND_IDENTIFIER "/310.blockDNS",
                                        "-t", "dnsaddr", "-T", "show"});
}
#endif

// Returns a rate-limited wrapper for a given function
// auto debouncedFoo = debounce(foo, 1000);
// debouncedFoo() can then be invoked at most once every 1000 ms
template <typename Func_t>
auto debounce(Func_t func, qint64 timeout)
{
    QDeadlineTimer timer;
    return [=](auto&&...args) mutable {
        // Since hasExpired() is true for a default-constructed object
        // func() will be executed the first time the debounced function is called
        // Successive calls will be rate-limited
        if(timer.hasExpired())
        {
             timer.setRemainingTime(timeout);
             func(std::forward<decltype(args)>(args)...);
        }
    };
}

WireguardGoBackend::WireguardGoBackend()
    : _wgGoPid{0}
{
    _wgGoRunner.emplace(wgGoRestartParams);

#ifdef Q_OS_MACOS
    // Rate-limit logIntefaces method to max invocations of once a second
    auto debouncedLogInterfaces = debounce(logInterfaces, 1000);
#endif

    connect(_wgGoRunner.ptr(), &ProcessRunner::stdoutLine, this,
        [](const QByteArray &line){qInfo() << "wireguard-go:" << line.data();});
#ifdef Q_OS_MACOS
    connect(_wgGoRunner.ptr(), &ProcessRunner::stdoutLine, this,
        [=](const QByteArray &line) mutable {
            if(line.contains("can't assign requested address"))
                debouncedLogInterfaces(line);
        });
#endif
    connect(_wgGoRunner.ptr(), &ProcessRunner::started, this,
            &WireguardGoBackend::wgGoStarted);
    connect(_wgGoRunner.ptr(), &ProcessRunner::failed, this,
            &WireguardGoBackend::wgGoFailed);
    _wgGoRunner->setObjectName(QStringLiteral("wireguard-go"));
#ifdef Q_OS_MAC
    // If there was an old interface file hanging around, remove it
    QFile::remove(Path::WireguardInterfaceFile);
#endif
}

WireguardGoBackend::~WireguardGoBackend()
{
    // If an async connection attempt was ongoing, abandon it (prevents spurious
    // warnings)
    if(_pConnectAttempt)
        _pConnectAttempt.abandon();

    // Tear down _wgGoRunner to be sure the process has exited before removing
    // the interface file (in case shutdown races with wireguard-go startup)
    _wgGoRunner.clear();

#ifdef Q_OS_MAC
    // Remove the interface file if it was created
    QFile::remove(Path::WireguardInterfaceFile);
#endif
}

void WireguardGoBackend::wgGoStarted(qint64 pid)
{
    if(_wgGoPid)
    {
        // Not expected, we stop the ProcessRunner if wireguard-go dies.
        qWarning() << "Unexpected restart of wireguard-go: PID" << _wgGoPid
            << "->" << pid;
        return;
    }
    qInfo() << "wireguard-go PID:" << pid;
    _wgGoPid = pid;
}

void WireguardGoBackend::wgGoFailed()
{
    _wgGoRunner->disable();
    // If we're shutting down, resolve the shutdown task.  Queue this result, we
    // can't tear down the QProcess or ProcessRunner during this signal.
    if(_pShutdownTask)
    {
        qInfo() << "wireguard-go exited during shutdown";
        QMetaObject::invokeMethod(this, [this]()
        {
            Q_ASSERT(_pShutdownTask);   // Class invariant, never cleared once set
            _pShutdownTask->resolve();
        }, Qt::ConnectionType::QueuedConnection);
    }
    else
    {
        qWarning() << "wireguard-go exited unexpectedly";
        // As above, queue the error handling since we can't tear down the
        // process or runner here
        QMetaObject::invokeMethod(this, [this]()
        {
            raiseError({HERE, Error::Code::WireguardProcessFailed});
        }, Qt::ConnectionType::QueuedConnection);
    }
}

auto WireguardGoBackend::createInterface(wg_device &wgDev,
                                         const QPair<QHostAddress, int> &)
    -> Async<std::shared_ptr<NetworkAdapter>>
{
    // Can't run this if already started.
    if(_wgGoRunner->isEnabled() || !_interfaceName.isEmpty() || !_wgSocketPath.isEmpty())
    {
        qWarning() << "WireguardGoBackend started twice";
        throw Error{HERE, Error::Code::Unknown};
    }

    // Start wireguard-go.  Use the desired interface name if possible; on Mac
    // we have to use "utun" to pick a utun device.
    _wgGoRunner->enable(Path::WireguardGoExecutable,
                       {QStringLiteral("--foreground"),
#ifdef Q_OS_MAC
                        QStringLiteral("utun")
#else
                        interfaceName
#endif
                       });

    // ProcessRunner::started() is emitted synchronously the first time it is
    // enabled - we have already received the wireguard-go PID.
    Q_ASSERT(_wgGoPid);

    // Get the interface name, then start trying to connect
#ifdef Q_OS_MAC
    return Async<MacInterfaceNameTask>::create()
#else
    return Async<QString>::resolve(interfaceName)
#endif
        ->then(this, [this](const QString &newItfName)
        {
            _interfaceName = newItfName;
            _wgSocketPath = QStringLiteral("/var/run/wireguard/") +
                            _interfaceName + QStringLiteral(".sock");
            return Async<PendingLocalSocketTask>::create(_wgSocketPath);
        })
        ->then(this, [this, devConfig = WgDevStatus{wgDev}](const std::shared_ptr<QLocalSocket> &pSocket)
        {
            Q_ASSERT(pSocket);  // Postcondition of PendingLocalSocketTask; rejects otherwise

            // Configure the interface
            return Async<WireguardConfigDeviceTask>::create(pSocket, devConfig.device());
        })
        ->then(this, [this](int returnedErrno)
            {
                if(returnedErrno)
                {
                    qWarning() << "WireGuard device" << _interfaceName
                        << "could not be configured, returned errno"
                        << returnedErrno;
                }
                else
                {
                    qInfo() << "WireGuard device configured successfully";
                }
            })
        ->then(this, [this](){return std::make_shared<NetworkAdapter>(_interfaceName);});
}

auto WireguardGoBackend::getStatus() -> Async<WgDevPtr>
{
    if(_wgSocketPath.isEmpty())
    {
        // Never got the interface name
        return Async<WgDevPtr>::reject({HERE, Error::Code::WireguardCreateDeviceFailed});
    }

    // wireguard-go only allows one IPC request per connection (it closes the
    // socket after the first request), so we have to create a new connection
    // for each stat poll.
    //
    // However, we don't retry the local socket or watch for it to be created
    // during stat polls; it should stay up after the connection is established
    // (if it's gone, consider the connection lost).
    return Async<LocalSocketTask>::create(_wgSocketPath)
        ->then([](const std::shared_ptr<QLocalSocket> &pSocket)
        {
            Q_ASSERT(pSocket);  // Postcondition of LocalSocketTask (rejects otherwise)
            return Async<WireguardDeviceStatusTask>::create(pSocket);
        })
        ->then([](const std::shared_ptr<WgDevStatus> &pDev)
        {
            Q_ASSERT(pDev); // Postcondition of WireguardDeviceStatusTask
            // Return an aliased shared pointer - dereferences to a wg_device,
            // but frees the complete WgDevStatus
            return WgDevPtr{pDev, &pDev->device()};
        });
}

Async<void> WireguardGoBackend::shutdown()
{
    // If an async connection attempt was ongoing, abandon it (prevents spurious
    // warnings)
    if(_pConnectAttempt)
        _pConnectAttempt.abandon();

    // Allow wireguard-go to cleanly shut down.
    // Set _pShutdownTask, even if it's resolved immediately, because this
    // indicates that shutdown() was called (sanity-checked by destructor)
    _pShutdownTask = Async<void>::create();

    Q_ASSERT(_wgGoRunner);  // Class invariant (only cleared on shutdown)
    if(_wgGoRunner->terminate())
    {
        qInfo() << "Signaled wireguard-go to terminate";
    }
    else
    {
        qInfo() << "wireguard-go process as not running, shutdown complete";
        _pShutdownTask->resolve();
        _wgGoRunner->disable();
    }

    return _pShutdownTask;
}

#include "wireguardgobackend.moc"
