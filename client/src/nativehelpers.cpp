// Copyright (c) 2021 Private Internet Access, Inc.
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
#line SOURCE_FILE("nativehelpers.cpp")

#include "nativehelpers.h"
#include "balancetext.h"
#include "clientsettings.h"
#include "client.h"
#include "path.h"
#include "version.h"
#include "brand.h"
#include "exec.h"
#include "locations.h"

#include <QProcess>
#include <QStringList>
#include <QFile>
#include <QGuiApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QCursor>
#include <QLibrary>

#ifdef Q_OS_MACOS
#include "mac/mac_install.h"
#include "mac/mac_window.h"
#include "mac/mac_loginitem.h"
#include "mac/mac_tray.h"
#endif

#ifdef Q_OS_WIN
#include "win/win_objects.h"
#include "win/win_registry.h"
#include "win/win_resources.h"
#include "win/win_tray.h"
// DriverStatus enum needed for driver reinstallation
#include "../../extras/installer/win/tap_inl.h"
#include <VersionHelpers.h>
#endif

#ifdef Q_OS_LINUX
#include "linux/linux_scaler.h"
#include "linux/linux_loginitem.h"
#include "linux/linux_window.h"
#endif

namespace
{
#if defined(Q_OS_MAC)
    bool macStartInstaller(const QString &installer)
    {
        // On Mac, we have to decompress the download, since it's a compressed
        // app bundle.
        QDir updateDir{Path::ClientUpdateDir};
        if(!updateDir.removeRecursively() ||
           !QDir{}.mkpath(Path::ClientUpdateDir))
        {
            qCritical() << "Can't clean update directory"
                << Path::ClientUpdateDir;
            return false;
        }

        // Unzip the bundle by shelling out to 'unzip'
        QStringList unzipArgs{QStringLiteral("-q"), installer,
                              QStringLiteral("-d"), updateDir.canonicalPath()};
        auto unzipResult = QProcess::execute(QStringLiteral("unzip"), unzipArgs);
        if(unzipResult != 0)
        {
            qCritical() << "Can't decompress update - unzip returned"
                << unzipResult << "- arguments were" << unzipArgs;
            return false;
        }

        // There should be exactly app bundle in the update directory now.  We
        // tolerate any other files that the zip might have contained (maybe a
        // future 'readme', etc.), but it can't contain more than one app bundle
        auto bundleList = updateDir.entryList({QStringLiteral("*.app")},
                                               QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot);
        if(bundleList.length() != 1)
        {
            qCritical() << "Can't execute update - found" << bundleList.length()
                << "app bundles after decompressing to"
                << updateDir.canonicalPath();
            return false;
        }

        // Run the app bundle with 'open -n'.  The new installer likely has the
        // same bundle identifier as this app, so '-n' tells macOS to open the
        // new app even if it thinks it's already open.
        QStringList openArgs{QStringLiteral("-n"), updateDir.absoluteFilePath(bundleList[0])};
        auto execResult = QProcess::execute(QStringLiteral("open"), openArgs);
        if(execResult != 0)
        {
            qCritical() << "Can't start update - open returned" << execResult
                << "- arguments were" << openArgs;
            return false;
        }

        // We started the new app bundle - don't wait for it to finish, it'll
        // want to close this client during installation.
        return true;
    }

#elif defined(Q_OS_WIN)
    void winSetIcon(QWindow &window)
    {
        // These are static because the window does not take ownership of the HICON
        // that we give it, we have to keep it alive.  They're function-local
        // statics because this code is linked into unit test modules where these
        // resources might not exist, we must not try to load them until this
        // function is called.  (This matters on CI builds.)
        static IconResource smallAppIcon{IDI_APP, IconResource::Size::Small};
        static IconResource largeAppIcon{IDI_APP, IconResource::Size::Large};

        HWND winHandle = reinterpret_cast<HWND>(window.winId());
        if(!winHandle)
        {
            qWarning() << "Can't get window handle in winSetIcon()";
            return;
        }

        // Set both large and small icons on the window.
        // Qt provides a way to set a window icon, but:
        //  - we're loading this icon from the Windows resource section anyway to
        //    avoid duplicating these assets on Windows, and to use the correct icon
        //    sizes that Windows wants
        //  - Qt would require us to convert these to QPixmaps and put them in a
        //    QIcon just so it can convert them back to HICONs on the fly
        //
        ::SendMessageW(winHandle, WM_SETICON, ICON_SMALL,
                       reinterpret_cast<LPARAM>(smallAppIcon.getHandle()));
        ::SendMessageW(winHandle, WM_SETICON, ICON_BIG,
                       reinterpret_cast<LPARAM>(largeAppIcon.getHandle()));
    }
#endif

    // The local date, time, and date-time formats are cached - the client must
    // be restarted to pick up changes in the system locale.  In the absence of
    // actually detecting changes to the system locale, this is preferred over
    // incompletely changing strings to the new format as they're re-rendered.

    QString localDateTimeFormat()
    {
        static const QString format{QLocale::system().dateTimeFormat(QLocale::FormatType::ShortFormat)};
        return format;
    }
    QString localDateFormat()
    {
        static const QString format{QLocale::system().dateFormat(QLocale::FormatType::ShortFormat)};
        return format;
    }
    QString localTimeFormat()
    {
        static const QString format{QLocale::system().timeFormat(QLocale::FormatType::ShortFormat)};
        return format;
    }
    // Render a UTC time specified by a timestamp in milliseconds to a local
    // time using the format string given.
    QString renderTimestampToLocal(qint64 timestampMs, const QString &format)
    {
        const auto &timestamp =
            QDateTime::fromMSecsSinceEpoch(timestampMs, Qt::TimeSpec::UTC).toLocalTime();
        return QLocale::system().toString(timestamp, format);
    }
}

NativeHelpers::NativeHelpers()
{
    connect(static_cast<QGuiApplication*>(QGuiApplication::instance()),
        &QGuiApplication::focusWindowChanged, this,
        &NativeHelpers::onFocusWindowChanged);
    connect(Logger::instance(), &Logger::configurationChanged, this,
        &NativeHelpers::logToFileChanged);
}

void NativeHelpers::initDashboardPopup(QQuickWindow *pDashboard)
{
    if(!pDashboard)
    {
        qWarning() << "Invalid value for pDashboard in initDashboardWindow()";
        return;
    }

    // Set window icons too - these aren't usually used for the popup dashboard,
    // but they might be used by window switchers, etc.
    initDecoratedWindow(pDashboard);

    qInfo() << "requested alpha:" << pDashboard->requestedFormat().alphaBufferSize()
        << "- actual alpha:" << pDashboard->format().alphaBufferSize();

#if defined(Q_OS_MACOS)
    macSetAllWorkspaces(*pDashboard);
#endif
}

void NativeHelpers::initDecoratedWindow(QQuickWindow *pWindow)
{
    if(!pWindow)
    {
        qWarning() << "Invalid value for pWindow in initDecoratedWindow()";
        return;
    }

    // Tell Qt to release any resources that it can when they're not needed.
    // It's normal to run PIA in the background for a long time with no
    // windows open, try not to hog resources.
    pWindow->setPersistentSceneGraph(false);
    pWindow->setPersistentOpenGLContext(false);

#if defined(Q_OS_WIN)
    winSetIcon(*pWindow);
#elif defined(Q_OS_LINUX)
    linuxSetWindowIcon(*pWindow);
#endif
}

void NativeHelpers::itemStackAfter(QQuickItem *pFirst, QQuickItem *pSecond)
{
    if(!pFirst || !pSecond)
    {
        qWarning() << "Invalid item in itemStackAfter():" << pFirst << pSecond;
        return;
    }

    pSecond->stackAfter(pFirst);
}

void NativeHelpers::releaseWindowResources(QQuickWindow *pWindow)
{
    // Tell Qt to release graphical resources for this window that are no
    // longer needed.
    if(pWindow)
        pWindow->releaseResources();
}

QString NativeHelpers::getClientVersion()
{
    return QStringLiteral(PIA_VERSION);
}

bool NativeHelpers::getStartOnLogin()
{
    return getStartOnLoginSetting();
}

void NativeHelpers::setStartOnLogin(bool enabled)
{
    // Statics can't be marked Q_INVOKABLE, provide a non-static shim function
    applyStartOnLoginSetting(enabled);
}

bool NativeHelpers::resourceExists(const QString &path)
{
    // From QML, resources are accessed with paths beginning with "qrc:/", but
    // QFile expects resource paths to just begin with ":/"
    if(!path.startsWith(QStringLiteral("qrc:/")))
    {
        qWarning() << "Invalid resource path:" << path;
        return false;
    }

    // Skip the "qrc" prefix and start with ":/"
    return QFile::exists(path.mid(3));
}

QString NativeHelpers::readResourceText(const QString &path)
{
    if(!resourceExists(path))
    {
        qWarning() << "Invalid resource path:" << path;
        return QStringLiteral("");
    }

    QFile file(path.mid(3));
    file.open(QIODevice::ReadOnly);
    QString result = QString::fromUtf8(file.readAll());
    file.close();
    return result;
}

void NativeHelpers::quitAndUninstall()
{
#if defined(Q_OS_WIN)
    if (QProcess::startDetached(Path::ExecutableDir / "uninstall.exe", {}))
    {
        qInfo() << "Quit due to uninstall";
        QCoreApplication::quit();
    }
#elif defined(Q_OS_MACOS)
    if (macExecuteUninstaller())
    {
        qInfo() << "Quit due to uninstall";
        QCoreApplication::quit();
    }
#elif defined(Q_OS_LINUX)
    if(runInTerminal(Path::ExecutableDir / BRAND_CODE "-uninstall.sh"))
    {
        qInfo() << "Quit due to uninstall";
        QCoreApplication::quit();
    }
#endif
}

void NativeHelpers::setDockVisibility(bool enabled)
{
#ifdef Q_OS_MACOS
    if(enabled) {
        enableShowInDock();
    }
    else {
        disableShowInDock();
    }
#endif
}


auto NativeHelpers::getPlatform() const -> Platform
{
#if defined(Q_OS_MACOS)
    return Platform::MacOS;
#elif defined(Q_OS_WIN)
    return Platform::Windows;
#elif defined(Q_OS_LINUX)
    return Platform::Linux;
#else
    #error NativeHelpers::getPlatform() not implemented for this platform
#endif
}

QString NativeHelpers::getProductName() const
{
    // Out-of-line because it depends on version.h; minimize build impact of
    // changing version (which happens on branch checkout)
    return QStringLiteral(PIA_PRODUCT_NAME);
}

bool NativeHelpers::getIncludeFeatureHandshake() const
{
    return INCLUDE_FEATURE_HANDSHAKE;
}

bool NativeHelpers::getLogToFile()
{
  return Logger::instance()->logToFile();
}

void NativeHelpers::crashClient()
{
    qInfo () << "Intentionally crashing client";
    // This is implemented via an export from common to ensure that we see both
    // the PIA client executable and pia-clientlib in the stack trace.
    testCrash();
}

void NativeHelpers::trimComponentCache()
{
    if(g_client)
        g_client->trimComponentCache();
}

void NativeHelpers::writeDummyLogs()
{
    qDebug () << "Writing client dummy logs";
    for (int i = 0; i < 10000; i ++ ) {
        qDebug () << "Writing dummy logs to fill up space (" << i << "/10000)";
    }
}

void NativeHelpers::loadDummyCrashDll(const QString &dllName)
{
    Path dllPath = Path::ClientDataDir / dllName;
    QFileInfo fi(dllPath);
    if(!fi.exists()) {
        qWarning () << "Dummy dll does not exist: " << dllPath;
        return;
    }
    QLibrary lib{dllPath};
    lib.load();
    auto pCrash = lib.resolve("crashModule");
    if(!pCrash) {
        qWarning () << "Could not resolve 'crashModule' symbol";
        return;
    }
    pCrash();
}

void NativeHelpers::startLogUploader(const QString &diagnosticsFile)
{
    // Write launch-on-login state for supportability
    qInfo() << "Current launch-on-login:" << getStartOnLoginSetting();

    startSupportTool("logs", diagnosticsFile);
}

void NativeHelpers::wipeLogFile()
{
    g_logger->wipeLogFile();
}


bool NativeHelpers::runInTerminal(const QString &command)
{
    qDebug () << "Running command in terminal" << command;
    // The command _cannot_ have arguments.  Many terminal emulators interpret
    // the command as a program+arguments, but some do not (Terminator,
    // deepin-terminal).
    //
    // None of our scripts need arguments right now, but if they did, we'd have
    // to work around this by passing them through as environment variables
    // instead.
    int result = Exec::cmd(Path::ExecutableDir / "run-in-terminal.sh", {command});
    qDebug () << "Process finished with exit code: " << result;
    if(result != 0)
    {
        emit terminalStartFailed(command);
        return false;
    }

    return true;
}

bool NativeHelpers::launchInstaller(const QString &installer)
{
#if defined(Q_OS_MAC)
    return macStartInstaller(installer);
#elif defined(Q_OS_WIN)
    return QProcess::startDetached(installer, QStringList{});
#elif defined(Q_OS_LINUX)
    return runInTerminal(installer);
#else
    #error "launchInstaller() not implemented for this platform"
#endif
}

bool NativeHelpers::installWireguardKernelModule()
{
#if defined(Q_OS_LINUX)
    // First, check if automatic installation is possible on this distribution
    if(Exec::cmd(Path::ExecutableDir / "install-wireguard.sh", {"--detect"}) == 0)
    {
        // It is, start the actual installation in a terminal emulator.  This
        // only fails if the terminal emulator can't be started; the normal
        // terminalStartFailed() signal is emitted.
        runInTerminal(Path::ExecutableDir / "install-wireguard.sh");
        return true;
    }
#endif
    return false;
}

qint64 NativeHelpers::getMonotonicTime()
{
    // Although QElapsedTimer does not guarantee in general that it uses a
    // system-wide epoch, it does on all platforms we support (it's the time
    // since boot).
    QElapsedTimer timer;
    timer.start();
    return timer.msecsSinceReference();
}

void NativeHelpers::reinstallTap()
{
    reinstallDriver(Driver::Tap, L"tap reinstall");
}

void NativeHelpers::reinstallTun()
{
    reinstallDriver(Driver::WinTun, L"tun reinstall");
}

void NativeHelpers::installWfpCallout()
{
    reinstallDriver(Driver::WfpCallout, L"callout install");
}

void NativeHelpers::reinstallWfpCallout()
{
    reinstallDriver(Driver::WfpCallout, L"callout reinstall");
}

void NativeHelpers::uninstallWfpCallout()
{
    reinstallDriver(Driver::WfpCallout, L"callout uninstall");
}

void NativeHelpers::forceWfpCalloutRebootState()
{
    // For testing UI that appears when the reboot state is active.
    setDriverReinstallStatus(Driver::WfpCallout, QStringLiteral("reboot"));
}

void NativeHelpers::reinstallDriver(Driver type, const wchar_t *commandParams)
{
#ifdef Q_OS_WIN
    if (_reinstallDriverNotifier)
        return;

    // Unfortunately a normal QProcess won't work with UAC, and
    // QProcess::startDetached won't let us wait for the result.
    // Awkwardly work around this with ShellExecuteEx.

    SHELLEXECUTEINFOW info {};
    info.cbSize = sizeof(SHELLEXECUTEINFOW);
    info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
    info.lpFile = qUtf16Printable(Path::DaemonExecutable);
    info.lpParameters = commandParams;
    info.lpDirectory = qUtf16Printable(Path::ExecutableDir);
    info.nShow = SW_HIDE;

    if (ShellExecuteExW(&info) && info.hProcess)
    {
        setDriverReinstallStatus(type, QStringLiteral("working"));

        _reinstallDriverNotifier = new QWinEventNotifier(info.hProcess, this);
        QObject::connect(_reinstallDriverNotifier, &QObject::destroyed, [hProcess = info.hProcess]() {
            CloseHandle(hProcess);
        });
        QObject::connect(_reinstallDriverNotifier, &QWinEventNotifier::activated, this, [this, type](HANDLE hProcess) {
            _reinstallDriverNotifier->deleteLater();
            _reinstallDriverNotifier = nullptr;

            DWORD exitCode;
            if (!GetExitCodeProcess(hProcess, &exitCode) || exitCode == STILL_ACTIVE)
                exitCode = DriverInstallFailed;
            switch (exitCode)
            {
            case DriverUpdated:
            case DriverUpdateNotNeeded:
            case DriverInstalled:
            case DriverUninstalled:
                setDriverReinstallStatus(type, QStringLiteral("success"));
                break;
            case DriverUpdatedReboot:
            case DriverInstalledReboot:
            case DriverUninstalledReboot:
                setDriverReinstallStatus(type, QStringLiteral("reboot"));
                break;
            default:
                setDriverReinstallStatus(type, QStringLiteral("error"));
                break;
            }
        });
    }
    else if (info.hInstApp != (HINSTANCE)SE_ERR_ACCESSDENIED)
    {
        setDriverReinstallStatus(type, QStringLiteral("error"));
    }
    else
    {
        // SE_ERR_ACCESSDENIED occurs if the user declines the UAC prompt.  Use
        // a different status than "error" so we don't show an error message in
        // the client.
        setDriverReinstallStatus(type, QStringLiteral("denied"));
    }
#endif
}

QPoint NativeHelpers::getMouseCursorPosition()
{
  return QCursor::pos();
}

void NativeHelpers::initScaling()
{
#ifdef Q_OS_LINUX
    LinuxWindowScaler::initScaleFactor();
#endif
    // No effect on any other platform.  Windows handles scale changes properly;
    // Mac scaling is handled by the OS.
}

double NativeHelpers::balanceWrappedText(double maxWidth, int fontPixelSize,
                                         const QString &text) const
{
    return BalanceText::balanceWrappedText(maxWidth, fontPixelSize, text);
}

QString NativeHelpers::openDaemonLog()
{
#ifdef Q_OS_LINUX
    if(QProcess::startDetached(QStringLiteral("/usr/bin/xdg-open"), QStringList{Path::DaemonLogFile}))
        return {};  // Success
    // Otherwise, return the daemon log path to indicate failure and show the
    // path to the user
#endif
    return Path::DaemonLogFile;
}

QStringList NativeHelpers::iconThemeValues()
{
    return ClientSettings::iconThemeValues();
}

QString NativeHelpers::iconPreviewResource(const QString &theme)
{
#if defined(Q_OS_MACOS)
    if(theme == QStringLiteral("auto"))
        return QStringLiteral("qrc:/img/tray/wide-monochrome-connected.png");
    if(theme == QStringLiteral("light"))
        return QStringLiteral("qrc:/img/tray/wide-light-connected.png");
    if(theme == QStringLiteral("dark"))
        return QStringLiteral("qrc:/img/tray/wide-dark-connected.png");
    if(theme == QStringLiteral("colored"))
        return QStringLiteral("qrc:/img/tray/wide-colored-connected.png");
    if(theme == QStringLiteral("classic"))
        return QStringLiteral("qrc:/img/tray/wide-classic-connected.png");
#elif defined(Q_OS_WIN)
    if(theme == QStringLiteral("light"))
    {
        if(TrayIconLoader::useOutlineIcons())
            return QStringLiteral("qrc:/img/tray/square-light-with-dark-outline-connected.png");
        else
            return QStringLiteral("qrc:/img/tray/square-light-no-outline-connected.png");
    }
    if(theme == QStringLiteral("colored"))
    {
        if(TrayIconLoader::useOutlineIcons())
            return QStringLiteral("qrc:/img/tray/square-colored-with-dark-outline-connected.png");
        else
            return QStringLiteral("qrc:/img/tray/square-colored-no-outline-connected.png");
    }
    if(theme == QStringLiteral("classic"))
        return QStringLiteral("qrc:/img/tray/square-classic-connected.png");
#elif defined(Q_OS_LINUX)
    if(theme == QStringLiteral("light"))
        return QStringLiteral("qrc:/img/tray/square-light-no-outline-connected.png");
    if(theme == QStringLiteral("dark"))
        return QStringLiteral("qrc:/img/tray/square-dark-no-outline-connected.png");
    if(theme == QStringLiteral("colored"))
        return QStringLiteral("qrc:/img/tray/square-colored-no-outline-connected.png");
    if(theme == QStringLiteral("classic"))
        return QStringLiteral("qrc:/img/tray/square-classic-connected.png");
#else
    #error "Unknown platform - NativeHelpers::iconPreviewResource()"
#endif
    return {};
}

QString NativeHelpers::encodeUriComponent(const QString &value)
{
    // QUrl's percent-encoding function forces us to do an extra UTF-8->UTF-16
    // conversion
    return QString::fromUtf8(QUrl::toPercentEncoding(value));
}

void NativeHelpers::requestDashboardReopen()
{
  emit dashboardOpenRequested();
}

void NativeHelpers::checkAppDeactivate()
{
#ifdef Q_OS_MAC
    macCheckAppDeactivate();
#endif
}

QString NativeHelpers::getBestLocationForCountry(QObject *pDaemonStateObj,
                                                 const QString &countryCode)
{
    const DaemonState *pDaemonState = dynamic_cast<const DaemonState*>(pDaemonStateObj);
    if(!pDaemonState)
        return {};
    NearestLocations nearest{pDaemonState->availableLocations()};
    const auto &countryLower = countryCode.toLower();
    const auto &pBestInCountry = nearest.getBestMatchingLocation([&countryLower](const Location &loc)
    {
        return loc.country().toLower() == countryLower;
    });
    if(pBestInCountry)
        return pBestInCountry->id();
    return {};
}

void NativeHelpers::applyStartOnLoginSetting(bool enabled)
{
    try
    {
        qInfo() << "Applying launch-on-login:" << enabled;
#if defined(Q_OS_WIN)
        winSetLaunchAtLogin(enabled);
#elif defined(Q_OS_MACOS)
        macSetLaunchAtLogin(enabled);
#elif defined(Q_OS_LINUX)
        linuxSetLaunchAtLogin(enabled);
#endif
        bool newState = getStartOnLoginSetting();
        qInfo() << "Applied launch-on-login:" << enabled << "- new state:"
            << newState;
    }
    catch(const Error &ex)
    {
        qWarning() << "Unable to apply launch-on-login setting" << enabled << ":" << ex;
    }
    catch(const std::exception &ex)
    {
        qWarning() << "Unable to apply launch-on-login setting" << enabled << ":" << ex.what();
    }
    catch(...)
    {
        qWarning() << "Unable to apply launch-on-login setting" << enabled << ": unknown exception";
    }
}

bool NativeHelpers::getStartOnLoginSetting()
{
    try
    {
#if defined(Q_OS_WIN)
        return winLaunchAtLogin();
#elif defined(Q_OS_MACOS)
        return macLaunchAtLogin();
#elif defined(Q_OS_LINUX)
        return linuxLaunchAtLogin();
#endif
    }
    catch(const Error &ex)
    {
        qWarning() << "Unable to check launch-on-login setting:" << ex;
    }
    catch(const std::exception &ex)
    {
        qWarning() << "Unable to check launch-on-login setting:" << ex.what();
    }
    catch(...)
    {
        qWarning() << "Unable to check launch-on-login setting: unknown exception";
    }
    return false;
}

void NativeHelpers::openUrl(const QString &path, const QJsonObject &queryItems)
{
    if(isSignalConnected(QMetaMethod::fromSignal(&NativeHelpers::urlOpenRequested)))
    {
        qInfo() << "QML handler is ready, emit URL event for" << path << "now";
        emit urlOpenRequested(path, queryItems);
    }
    else
    {
        qInfo() << "QML handler is not ready, queue URL event for" << path;
        if(!_queuedOpenUrlPath.isEmpty())
        {
            qWarning() << "Discarding existing queued URL event for" << _queuedOpenUrlPath;
        }
        _queuedOpenUrlPath = path;
        _queuedOpenUrlQuery = queryItems;
    }
}

QString NativeHelpers::cleanSsidDisplay(const QString &ssid)
{
    QString cleaned{ssid};
    for(QChar &qc : cleaned)
    {
        ushort &cval{qc.unicode()};
        // Replace control codes (anything < 0x20, or 0x7F 'DEL'), with spaces
        if(cval < 0x20 || cval == 0x7F)
            cval = ' ';
    }
    return cleaned;
}

QString NativeHelpers::renderDateTime(qint64 timestampMs)
{
    return renderTimestampToLocal(timestampMs, localDateTimeFormat());
}

QString NativeHelpers::renderDate(qint64 timestampMs)
{
    return renderTimestampToLocal(timestampMs, localDateFormat());
}

QString NativeHelpers::renderTime(qint64 timestampMs)
{
    return renderTimestampToLocal(timestampMs, localTimeFormat());
}

void NativeHelpers::onFocusWindowChanged(QWindow *pFocusWindow)
{
    if(!pFocusWindow)
        emit appFocusLost();
}

void NativeHelpers::setDriverReinstallStatus(Driver type, const QString &status)
{
    switch(type)
    {
        case Driver::Tap:
            _reinstallTapStatus = status;
            emit reinstallTapStatusChanged();
            break;
        case Driver::WinTun:
            _reinstallTunStatus = status;
            emit reinstallTunStatusChanged();
            break;
        case Driver::WfpCallout:
            _reinstallWfpCalloutStatus = status;
            emit reinstallWfpCalloutStatusChanged();
            break;
    }
}

void NativeHelpers::checkQueuedUrlEvent()
{
    if(!_queuedOpenUrlPath.isEmpty())
    {
        qInfo() << "Re-process queued URL event for" << _queuedOpenUrlPath << "now";
        QString path = std::move(_queuedOpenUrlPath);
        QJsonObject query = std::move(_queuedOpenUrlQuery);
        _queuedOpenUrlPath = QString{};
        _queuedOpenUrlQuery = QJsonObject{};
        // If, for whatever reason, there isn't anything connected to the
        // signal right now, this will just queue up the request again until a
        // connection is made
        openUrl(path, query);
    }
}

void NativeHelpers::connectNotify(const QMetaMethod &signal)
{
    if(signal == QMetaMethod::fromSignal(&NativeHelpers::urlOpenRequested))
    {
        // This isn't necessarily called on the main thread, so we can't do
        // much here but schedule a call to a method on our thread
        qInfo() << "Observed connection to urlOpenRequested, check for queued URL event on main thread";
        QMetaObject::invokeMethod(this, &NativeHelpers::checkQueuedUrlEvent,
                                  Qt::ConnectionType::QueuedConnection);
    }
}
