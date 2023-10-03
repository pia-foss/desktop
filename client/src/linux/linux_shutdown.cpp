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

#include <common/src/common.h>
#line SOURCE_FILE("linux_shutdown.cpp")

#include "linux_shutdown.h"
#include "../client.h"
#include <common/src/ipc.h>
#include <common/src/jsonrpc.h>
#include <common/src/builtin/path.h>
#include <common/src/builtin/util.h>
#include <unistd.h>

namespace
{
    // We can't unregister an atexit() handler, and they're still called after
    // main() returns normally, so we just have to deactivate it using a flag.
    std::atomic<bool> _cleanShutdown{false};

    // The UTF-8 executable path for argv[0] is set up ahead of time when the
    // process is still in a sane state.
    QByteArray _clientExecutablePath;
}

// On Linux/X11, at shutdown, the client exec()s itself with --shutdown-socket
// to do a clean shutdown of the daemon socket.
//
// This is bit kludgey, but it's the best we can do with Xlib.  OS logouts on
// virtually all desktop environments just kill the X11 connection rather than
// send SIGTERM.  When that happens, either Xlib or Qt immediately bails out
// with exit(), which can happen on any thread or even more than one thread at
// the same time.
//
// At that point, there is no hope for rescuing the current process into any
// sane state where we could reliably send a disconnect to the server, so we
// exec() in order to ensure that all threads are shut down and we can safely
// start an event loop (which is necessary to ensure that the data actually
// reaches the daemon).
int linuxShutdownSocket(char *socketFdArg, int &argc, char **argv)
{
    // QCoreApplication changes argc/argv, parse this first
    bool socketFdOk = false;
    qlonglong socketFdLL = QString{socketFdArg}.toLongLong(&socketFdOk);
    if(!socketFdOk || socketFdLL < 0 || socketFdLL > std::numeric_limits<qintptr>::max())
    {
        qWarning() << "Invalid socket:" << socketFdArg;
        return -1;
    }

    qintptr socketFd = static_cast<qintptr>(socketFdLL);

    // Minimal setup for an event loop and logging
    QCoreApplication app{argc, argv};
    Path::initializePostApp();
    Logger logSingleton{Path::ClientLogFile};

    // Create an IPC connection and client-side interface using the exiting
    // socket descriptor
    LocalMethodRegistry methods;
    ThreadedLocalIPCConnection ipc{nullptr};
    ClientSideInterface rpc{&methods, nullptr};

    // Hook up the IPC connection to the RPC interface
    QObject::connect(&ipc, &IPCConnection::messageReceived, &rpc, &ClientSideInterface::processMessage);
    QObject::connect(&rpc, &ClientSideInterface::messageReady, &ipc, &IPCConnection::sendMessage);
    QObject::connect(&ipc, &IPCConnection::messageError, &rpc, &ClientSideInterface::requestSendError);

    // If the connection succeeds, call notifyExit, then quit when that finishes
    QObject::connect(&ipc, &IPCConnection::connected, &app,
        [&app, &rpc]()
        {
            qInfo() << "Connected to daemon, notifying exit";
            rpc.callWithParams(QStringLiteral("notifyClientDeactivate"), {})
                ->notify(&app, [&](const Error &error, const QJsonValue &)
                    {
                        if(error)
                            qWarning() << "Failed to notify exit:" << error;
                        else
                            qInfo() << "Notification completed";
                        app.quit();
                    });
        });
    // If the connection fails for some reason, just quit
    QObject::connect(&ipc, &IPCConnection::error, &app,
        [&app, socketFd]()
        {
            qInfo() << "Failed to connect to daemon using socket descriptor" << socketFd;
            app.quit();
        });
    // Time out if the daemon does not respond
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &app, [&app]()
        {
            qInfo() << "Notification timed out";
            app.quit();
        });
    timeout.start(500);

    ipc.connectToSocketFd(socketFd);

    return app.exec();
}

// On Linux/X11, as describe above, either Xlib or Qt may decide to kill the
// whole process with exit() if the X connection is lost.
//
// Since this happens during a normal OS logout, try to tell the daemon that
// this is an intentional shutdown.
void linuxHandleExit()
{
    _clientExecutablePath = Path::ClientExecutable.str().toUtf8();

    std::atexit([]()
    {
#ifdef PIA_CRASH_REPORTING
        // Don't try to take crash dumps at this point.  The process may already
        // be corrupt since Xlib can call exit() while other threads are running
        // and even from more than one thread.
        stopCrashReporting();
#endif

        if(_cleanShutdown.load())
            return;

        // Exec the client in-place to try to tell the daemon that this is an
        // intentional shutdown.  The process may be corrupt, so we need to
        // avoid heap allocations, logging, synchronization, etc.
        qintptr socketFd = Client::getCurrentDaemonSocket();
        if(socketFd < 0)
            return;

        // Duplicate the descriptor so it survives exec() (the original has
        // FD_CLOEXEC)
        socketFd = dup(static_cast<int>(socketFd));
        if(socketFd < 0)
            return;

        // Set up args
        // digits10 + 2 ==> 1 extra char to hold any possible qintptr, and 1 null char
        constexpr std::size_t convBufSize = std::numeric_limits<qintptr>::digits10 + 2;
        char socketFdString[convBufSize]{};
        int convResult = snprintf(socketFdString, convBufSize, "%llu", static_cast<unsigned long long>(socketFd));
        if(convResult <= 0 || convResult >= static_cast<int>(convBufSize))
            return;

        char shutdownSocketArg[] = "--shutdown-socket";
        char *argv[] = {_clientExecutablePath.data(), shutdownSocketArg, socketFdString, nullptr};
        execv(_clientExecutablePath.data(), argv);
    });
}

// clientMain() is exiting cleanly and already handled shutdown, deactivate the
// atexit() handler
void linuxCleanlyExited()
{
    _cleanShutdown.store(true);
}
