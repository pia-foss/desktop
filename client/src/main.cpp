// Copyright (c) 2019 London Trust Media Incorporated
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
#line SOURCE_FILE("main.cpp")

#if defined(UNIT_TEST)

// Entry point shouldn't be included for unit test projects
void dummyClientMain() {}

#else

#include "client.h"
#include "clientsettings.h"
#include "path.h"
#include "semversion.h"
#include "version.h"
#include "appsingleton.h"

#include "clientlib.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QResource>
#include <QTranslator>
#include <QFontDatabase>
#include <QFont>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <iostream>
#ifdef Q_OS_LINUX
#include "linux/linux_env.h"
#include "linux/linux_language.h"
#include "linux/linux_shutdown.h"
#include <QApplication>
#endif

#if defined(Q_OS_WIN)
#include "win/win_scaler.h"
#include "win/win_messagereceiver.h"
#include "win/win_util.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

#ifdef Q_OS_MACOS
#include "mac/mac_app.h"
#include "mac/mac_install.h"
#endif

#ifdef PIA_DAEMON
#ifdef Q_OS_WIN
#include "win/win_daemon.h"
#else
#include "posix/posix_daemon.h"
#endif
#endif

#ifdef Q_OS_UNIX
#include "posix/unixsignalhandler.h"
#endif

#if defined(Q_OS_WIN)
// On Windows, we sometimes need to fall back to the software renderer.
bool winShouldUseSoftwareBackend()
{
    // If the user isn't using a composited desktop, use the software backend.
    BOOL dwmEnabled;
    if (::DwmIsCompositionEnabled(&dwmEnabled) == S_OK && !dwmEnabled)
        return true;

    return false;
}
#endif

int clientMain(int argc, char *argv[])
{
    Path::initializePreApp();

    // We never use Qt's built-in high-DPI scaling.  On Windows, it has a number
    // of issues, mostly revolving around per-monitor scaling in 8.1+.  Even on
    // Qt-based Linux environments it has other issues.
    QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);

    bool quietLaunch{false};
    GraphicsMode gfxMode{GraphicsMode::Normal};
    for(auto i=1; i<argc; ++i)
    {
        if(argv[i] && ::strcmp(argv[i], "--quiet") == 0)
            quietLaunch = true;
        else if(argv[i] && ::strcmp(argv[i], "--safe-mode") == 0)
            gfxMode = GraphicsMode::PersistSafe;
#ifdef Q_OS_LINUX
        else if(argv[i] && ::strcmp(argv[i], "--shutdown-socket") == 0)
        {
            // We exec()ed to shutdown the daemon socket - just do that and quit
            if(i+1 < argc)
                return linuxShutdownSocket(argv[i+1], argc, argv);
            qWarning() << "Option requires argument:" << argv[i];
            return -1;
        }
#endif
        else
            qWarning() << "Unknown option:" << argv[i];
    }

    // Check if the client setting "disableHardwareGraphics" is enabled.  This
    // has to be found before Client is created, since we have to set the
    // corresponding app attribute before the QCoreApplication is constructed.
    //
    // To do that, we manually read clientsettings.json if it exists and check
    // this setting manually.
    ClientSettings initialSettings;
    bool hasExistingClientSettings = readProperties(initialSettings, Path::ClientSettingsDir, "clientsettings.json");
    // Also enable safe mode if that setting was already on.  (This also applies
    // the default value for the setting if the client settings didn't exist.)
    if(gfxMode == GraphicsMode::Normal && initialSettings.disableHardwareGraphics())
        gfxMode = GraphicsMode::Safe;

#if defined(Q_OS_WIN)
    if (gfxMode == GraphicsMode::Normal && winShouldUseSoftwareBackend())
    {
        // Use software graphics due to Windows-specific constraints.  (Don't
        // enable the permanent setting though.)
        gfxMode = GraphicsMode::Safe;
    }

    // On Windows, Qt provides this application attribute to load an
    // app-provided software OpenGL implementation.  We ship Qt's Mesa-llvmpipe
    // build as opengl32sw.dll.
    if(gfxMode != GraphicsMode::Normal)
        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
    else
    {
        // Disable shader disk cache as this causes crashes on Intel HD Graphics 620
        // This is fixed in Qt 5.12 where the driver is blacklisted
        // Since we don't use a lot of shaders we shouldn't see a very significant
        // performance impact
        //
        // https://bugreports.qt.io/browse/QTBUG-64697
        // https://codereview.qt-project.org/#/c/238651/
        // https://codereview.qt-project.org/#/c/242235/

        QGuiApplication::setAttribute(Qt::AA_DisableShaderDiskCache, true);
    }
#endif

#ifdef Q_OS_MACOS
    // This initialization occurs before the QGuiApplication is created.
    macAppInit();
    // There's no software graphics feature on Mac OS.  Hardware rendering is
    // most reliable on Mac.
#endif

#ifdef Q_OS_LINUX
    // On Linux, software graphics are provided by telling libgl to load a
    // software implementation even if hardware is available.  Qt doesn't
    // implement AA_UseSoftwareOpenGL on Linux.
    if(gfxMode != GraphicsMode::Normal)
        ::qputenv("LIBGL_ALWAYS_SOFTWARE", QByteArrayLiteral("1"));

    // This has to occur before creating the QApplication.
    LinuxEnv::preAppInit();
    linuxLanguagePreAppInit();

    // Linux still uses the QSystemTrayIcon implementation from Qt.Widgets, so
    // it needs a QApplication instead of a plain QGuiApplication.
    QApplication app{argc, argv};
#else
    QGuiApplication app{argc, argv};
#endif
    Path::initializePostApp();
#ifdef PIA_CRASH_REPORTING
    initCrashReporting();
    monitorDaemonDumps();
#else
    qInfo() << "Not initializing crash handler - built without crash reporting support.";
#endif

#ifdef Q_OS_UNIX
    UnixSignalHandler::init();
#endif
    Logger logSingleton{Path::ClientLogFile};
    QGuiApplication::setApplicationDisplayName(QStringLiteral(PIA_PRODUCT_NAME));
    QGuiApplication::setQuitOnLastWindowClosed(false);

    AppSingleton runGuard(Path::ClientExecutable);
    qint64 runningInstancePid = runGuard.isAnotherInstanceRunning();
    if(runningInstancePid > 0) {
        qWarning () << "Exiting because another instance appears to be running";
#ifdef Q_OS_UNIX
        UnixSignalHandler::sendSignalUsr1(runningInstancePid);
#endif

#ifdef Q_OS_WIN
        broadcastMessage(L"WM_PIA_SHOW_DASHBOARD");
#endif
        app.quit();
        return -1;
    }

#if defined(Q_OS_WIN)
    // Exit if the uninstaller tells us to
    MessageReceiver messageReceiver;
#endif

    // Note that we need app.applicationDirPath() on Windows too, because the
    // working directory isn't always set to the application directory (such as
    // when we're launched by the installer, or from the Start menu)
    QResource::registerResource(Path(app.applicationDirPath()) /
            #if defined(Q_OS_MACOS)
                ".." / "Resources" /
            #elif defined(Q_OS_LINUX)
                ".." / "etc" /
            #endif
                QStringLiteral("translations.rcc")
                );

#ifdef PIA_DAEMON
    // Run an embedded version of the daemon as part of the client-portable
    // project; the client needs to run as root, but doesn't need any installed
    // background service. Since it's not multi-process based, this version is
    // significantly easier to debug.
    //
    // TODO: Offer a configurable version that communicates directly with the
    // daemon, without relying on the same local sockets IPC.
    //
    QStringList daemonArguments;
    bool daemonStopped = false;
#ifdef Q_OS_WIN
    WinDaemon daemon(daemonArguments);
#else
    setUidAndGid();
    PosixDaemon daemon(daemonArguments);
#endif
    QObject::connect(&daemon, &Daemon::stopped, [&] { daemonStopped = true; });
    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&] { if (!daemonStopped) daemon.stop(); });
    daemon.start();
#endif

    // --safe-mode was already applied if it's set, but we still pass the flag to
    // Client so it can set the permanent client setting (including writing the
    // settings out to disk).
    Client client{hasExistingClientSettings, initialSettings.toJsonObject(),
                  gfxMode, quietLaunch};

#ifdef Q_OS_MACOS
    // Check for installation on Mac.  (On Mac, the downloaded app bundle just
    // runs the client binary, this is how the install occurs.)
    //
    // This depends on Path initialization, etc., but we do this before starting
    // the main event loop since it may decide to exit if an install occurred.
    macCheckInstallation();
#endif

    client.setupFonts();

    client.queueNotification(&Client::init);

    // These notifications occur when the user session is ending.  Depending on
    // OS and version, we might get some or all of these, and Qt may
    // terminate the app before main() gets a chance to actually shut down.
    // For resiliency, we send notifyExit to the daemon immediately in each
    // notification so it knows that this is an intentional shutdown (even if
    // it's not totally "clean").  The daemon is fine with multiple
    // notifyExit calls.
    QObject::connect(&app, &QGuiApplication::commitDataRequest, &client, [&]()
    {
        qInfo() << "Session is ending (received commit data request)";
        client.notifyExit();
    });

    QObject::connect(&app, &QGuiApplication::aboutToQuit, &client, [&]()
    {
        qInfo() << "App is about to quit";
        client.notifyExit();
    });

#ifdef Q_OS_LINUX
    linuxHandleExit();
#endif

    int result = app.exec();
    qInfo() << "QGuiApplication has exited:" << result;

    // Issue an exit notification if we haven't already, and try to let it
    // complete (for up to 500ms).
    client.notifyExitAndWait();

#ifdef Q_OS_LINUX
    linuxCleanlyExited();
#endif

#ifdef PIA_DAEMON
    if (!daemonStopped)
    {
        QObject::connect(&daemon, &Daemon::stopped, [] { QGuiApplication::quit(); });
        daemon.stop();
        int daemonResult = app.exec();
        if (!result) result = daemonResult;
    }
#endif

    return result;
}

int main(int argc, char *argv[])
{
    return runClient(true, argc, argv, &clientMain);
}

#endif
