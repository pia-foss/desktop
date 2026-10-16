// Copyright (c) 2026 Private Internet Access, Inc.
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
#line SOURCE_FILE("win/win_console.cpp")

#include "win_console.h"
#include "win_service.h"
#include "win_wintun.h"
#include <common/src/builtin/path.h>
#include "win.h"
#include "version.h"
#include "product.h"
#include "brand.h"
#include <common/src/exec.h>
#include "../../../extras/installer/win/tap_inl.h"
#include "../../../extras/installer/win/callout_inl.h"
#include "../../../extras/installer/win/dco_inl.h"

#include <QCoreApplication>
#include <QDirIterator>
#include <QProcessEnvironment>
#include <QStringList>
#include <QProcess>

#define _SETUPAPI_VER _WIN32_WINNT_WIN7
#include <setupapi.h>

static BOOL WINAPI ctrlHandler(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        QTextStream(stderr) << "Terminating..." << Qt::endl;
        QMetaObject::invokeMethod(g_console, &WinConsole::stopDaemon);
        return TRUE;
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        QTextStream(stderr) << "Terminating..." << Qt::endl;
        QMetaObject::invokeMethod(g_console, &WinConsole::stopDaemon);
        // Preferably we ought ot wait for termination here, but no clean way to do it yet
        return FALSE;
    default:
        return FALSE;
    }
}

static QString getTapInfPath()
{
    return QDir::toNativeSeparators(Path::TapDriverDir / (IsWindows10OrGreater() ? "win10" : "win7") / "OemVista.inf");
}

static QString getWfpCalloutInfPath()
{
    return QDir::toNativeSeparators(Path::WfpCalloutDriverDir / (IsWindows10OrGreater() ? "win10" : "win7") / "PiaWfpCallout.inf");
}

static QString getDcoInfPath()
{
    // The INF filename, its service name, and the device hardware ID it declares
    // all have to agree with the signed driver payload in deps/dco
    // (DCO_DEVICE_NAME).  The payload is staged in win10/win11 subdirectories,
    // like the WFP callout driver above.
    const auto subdir = IsWindows11() ? QStringLiteral("win11") : QStringLiteral("win10");
    return QDir::toNativeSeparators(Path::DcoDriverDir / subdir /
                                    QString::fromWCharArray(DCO_DEVICE_NAME L".inf"));
}

static QString getTapInstallPath()
{
    return QDir::toNativeSeparators(Path::ExecutableDir / "tapinstall.exe");
}

WinConsole::WinConsole(QObject* parent)
    : QObject(parent)
    , _arguments(QCoreApplication::arguments())
{

}

int WinConsole::installTapDriver(bool force)
{
    return ::installTapDriver(qUtf16Printable(getTapInfPath()), false, force, false);
}

int WinConsole::uninstallTapDriver()
{
    return ::uninstallTapDriver(true, false);
}

int WinConsole::reinstallTapDriver()
{
    ::uninstallTapDriver(false, false);
    return ::installTapDriver(qUtf16Printable(getTapInfPath()), false, true, false);
}

int WinConsole::uninstallWintunDriver()
{
    WintunModule wintun;

    bool rebootRequiredWireGuard{false}, rebootRequiredOpenVPN{false};
    BOOL rebootRequiredUninstall{FALSE};

    if(!wintun.deleteDriver())
    {
        qWarning() << "WintunDeleteDriver failed to delete WinTUN driver";
        return DriverStatus::DriverUninstallFailed;
    }
    if(rebootRequiredWireGuard || rebootRequiredOpenVPN || rebootRequiredUninstall)
    {
        qInfo() << "WinTUN uninstall requested a reboot";
        return DriverStatus::DriverUninstalledReboot;
    }
    return DriverStatus::DriverUninstalled;
}

int WinConsole::createWintunAdapter()
{
    WintunModule wintun;

    bool rebootRequired{false};
    auto adapter = wintun.recreateAdapter(WintunData::pOpenVPNName);

    if(!adapter)
        return DriverStatus::DriverInstallFailed;
    if(rebootRequired)
        return DriverStatus::DriverInstalledReboot;
    return DriverStatus::DriverInstalled;
}

int WinConsole::installDcoDriver()
{
    return ::installDcoDriver(qUtf16Printable(getDcoInfPath()), DCO_DEVICE_NAME, qUtf16Printable(getTapInstallPath()));
}

int WinConsole::uninstallDcoDriver()
{
    return ::uninstallDcoDriver(qUtf16Printable(getDcoInfPath()), DCO_DEVICE_NAME, qUtf16Printable(getTapInstallPath()));
}

int WinConsole::reinstallDcoDriver()
{
    auto uninstallResult =
        ::uninstallDcoDriver(qUtf16Printable(getDcoInfPath()), DCO_DEVICE_NAME, qUtf16Printable(getTapInstallPath()));
    qInfo() << "Uninstall result:" << uninstallResult;
    if(uninstallResult == DriverStatus::DriverUninstalledReboot)
    {
        // Uninstall requires a reboot to complete.  Don't try to install again
        // yet, it would fail since the driver file is in use.
        qInfo() << "Restart the computer to complete uninstallation, then install again.";
        return uninstallResult;
    }
    return ::installDcoDriver(qUtf16Printable(getDcoInfPath()), DCO_DEVICE_NAME, qUtf16Printable(getTapInstallPath()));
}

int WinConsole::installCalloutDriver()
{
    return ::installCalloutDriver(qUtf16Printable(getWfpCalloutInfPath()), false);
}

int WinConsole::uninstallCalloutDriver()
{
    return ::uninstallCalloutDriver(qUtf16Printable(getWfpCalloutInfPath()), false);
}

int WinConsole::reinstallCalloutDriver()
{
    auto uninstallResult = ::uninstallCalloutDriver(qUtf16Printable(getWfpCalloutInfPath()), false);
    qInfo() << "Uninstall result:" << uninstallResult;
    if(uninstallResult == DriverStatus::DriverUninstalledReboot)
    {
        // Uninstall requires a reboot to complete.  Don't try to install again
        // yet, it would fail since the driver file is in use.
        qInfo() << "Restart the computer to complete uninstallation, then install again.";
        return uninstallResult;
    }
    return ::installCalloutDriver(qUtf16Printable(getWfpCalloutInfPath()), false);
}

int WinConsole::run()
{
    auto args = _arguments;

    auto unrecognizedCommand = [&] {
        qCritical() << "Unrecognized command:" << args.mid(1);
        QTextStream(stdout)
                << "Unrecognized command; type '" << QFileInfo(args.at(0)).baseName() << " help' for a list of available commands." << Qt::endl;
        return 1;
    };

    auto sendCheckDriverHint = []
    {
        // Signal to the daemon to recheck the callout driver state.  We
        // have to send it an RPC to do this, but pia-service.exe does
        // not link to the client library, so invoke piactl.exe.
        Exec::cmd(Path::InstallationDir / BRAND_CODE "ctl.exe",
                  {QStringLiteral("-u"), QStringLiteral("checkdriver")});
        // Result traced by cmd(), nothing to do if it fails
    };

    if (args.count() > 1)
    {
        #define MATCH_ARG(i, text) (0 == args.at(i).compare(QLatin1String(text), Qt::CaseInsensitive))
        Path logFilePath = Path::ConfigLogFile;
        // "run" runs the daemon normally, use the daemon log file in that case.
        // Use the setup log file for all other modes, which may run
        // concurrently with the daemon.
        if(MATCH_ARG(1, "run"))
            logFilePath = Path::DaemonLogFile;

        Logger logSingleton{logFilePath};

        try
        {
            if (MATCH_ARG(1, "help") || MATCH_ARG(1, "/?"))
                return showHelp();
            else if (MATCH_ARG(1, "run"))
                return runDaemon();
            else if (MATCH_ARG(1, "install"))
                return WinService::installService();
            else if (MATCH_ARG(1, "uninstall"))
                return WinService::uninstallService();
            else if (MATCH_ARG(1, "start"))
                return WinService::startService();
            else if (MATCH_ARG(1, "stop"))
                return WinService::stopService();
            else if (MATCH_ARG(1, "tap") && args.count() > 2)
            {
                if (MATCH_ARG(2, "install"))
                    return WinConsole::installTapDriver();
                else if (MATCH_ARG(2, "uninstall"))
                    return WinConsole::uninstallTapDriver();
                else if (MATCH_ARG(2, "reinstall"))
                    return WinConsole::reinstallTapDriver();
                else
                    return unrecognizedCommand();
            }
            else if (MATCH_ARG(1, "tun") && args.count() > 2)
            {
                if (MATCH_ARG(2, "uninstall"))
                    return WinConsole::uninstallWintunDriver();
                else if (MATCH_ARG(2, "create"))
                    return WinConsole::createWintunAdapter();
                else
                    return unrecognizedCommand();
            }
            else if (MATCH_ARG(1, "callout") && args.count() > 2)
            {
                int result = 0;
                if (MATCH_ARG(2, "install"))
                    result = WinConsole::installCalloutDriver();
                else if(MATCH_ARG(2, "uninstall"))
                    result = WinConsole::uninstallCalloutDriver();
                else if (MATCH_ARG(2, "reinstall"))
                    result = WinConsole::reinstallCalloutDriver();
                else
                    return unrecognizedCommand();

                sendCheckDriverHint();
                return result;
            }
            else if (MATCH_ARG(1, "dco") && args.count() > 2)
            {
                int result = 0;
                if (MATCH_ARG(2, "install"))
                    result = WinConsole::installDcoDriver();
                else if (MATCH_ARG(2, "uninstall"))
                    result = WinConsole::uninstallDcoDriver();
                else if (MATCH_ARG(2, "reinstall"))
                    result = WinConsole::reinstallDcoDriver();
                else
                    return unrecognizedCommand();

                sendCheckDriverHint();
                return result;
            }
            else
                return unrecognizedCommand();
            #undef MATCH_ARG
        }
        catch (const Error& error)
        {
            qCritical() << error;
            switch (error.systemCode())
            {
            case ERROR_SERVICE_EXISTS:
            case ERROR_SERVICE_DOES_NOT_EXIST:
            case ERROR_SERVICE_ALREADY_RUNNING:
            case ERROR_SERVICE_NOT_ACTIVE:
                return 2;
            case ERROR_SERVICE_MARKED_FOR_DELETE:
            case ERROR_AUTHENTICODE_PUBLISHER_NOT_TRUSTED:
            case ERROR_AUTHENTICODE_TRUST_NOT_ESTABLISHED:
                return 3;
            default:
                return 1;
            }
        }
    }
    else
    {
        return showHelp();
    }
}

int WinConsole::runDaemon()
{
    _daemon = new WinDaemon(this);
    QObject::connect(_daemon, &Daemon::started, &QCoreApplication::exec);
    QObject::connect(_daemon, &Daemon::stopped, &QCoreApplication::quit);
    BOOL ctrlHandlerInstalled = SetConsoleCtrlHandler(ctrlHandler, TRUE);
    _daemon->start();
    if (ctrlHandlerInstalled) SetConsoleCtrlHandler(ctrlHandler, FALSE);
    return 0;
}

void WinConsole::stopDaemon()
{
    _daemon->stop();
}

int WinConsole::showHelp()
{
    QTextStream(stdout)
            << PIA_PRODUCT_NAME << " Service v" << QString::fromStdString(Version::semanticVersion()) << Qt::endl
            << Qt::endl
            << "Usage:" << Qt::endl
            << "  " BRAND_CODE "-service <command>" << Qt::endl
            << Qt::endl
            << "Available commands:" << Qt::endl
            << "  install        Install service" << Qt::endl
            << "  uninstall      Uninstall service" << Qt::endl
            << "  start          Start service" << Qt::endl
            << "  stop           Stop service" << Qt::endl
            << "  run            Run interactively" << Qt::endl
            << "  tun uninstall  Uninstall WinTUN driver" << Qt::endl
            << "  tun create     Create WinTUN adapter" << Qt::endl
            << "  callout install Install WFP Callout driver" << Qt::endl
            << "  callout uninstall Uninstall WFP Callout driver" << Qt::endl
            << "  callout reinstall (Re)install WFP Callout driver" << Qt::endl
               ;
    return 0;
}
