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
#line HEADER_FILE("nativehelpers.h")

#ifndef NATIVEHELPERS_H
#define NATIVEHELPERS_H
#pragma once

#include <kapps_core/src/winapi.h>
#include <QWindow>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>

#ifdef Q_OS_WIN
#include <QWinEventNotifier>
#endif


// NativeHelpers is a singleton exposed to QML to provide access to some native
// routines.
class NativeHelpers : public QObject
{
    Q_OBJECT

private:
    // Used internally for common "driver reinstallation" logic to identify the
    // TAP or WFP callout driver.
    enum class Driver
    {
        Tap,
        WinTun,
        WfpCallout,
    };

public:
    enum class Platform
    {
        Windows,
        MacOS,
        Linux
    };
    Q_ENUM(Platform)

public:
    NativeHelpers();

public:
    // Find out what platform we're on for platform-specific QML logic.
    // Use sparingly!
    Q_PROPERTY(Platform platform READ getPlatform FINAL CONSTANT)

    // The product name from PIA_PRODUCT_NAME.
    // We do not translate the product name.
    Q_PROPERTY(QString productName READ getProductName FINAL CONSTANT)

    Q_PROPERTY(QString macosProductVersion READ getMacosProductVersion FINAL CONSTANT)

    // Whether the client is currently logging to a file.  This exposes the flag
    // from Logger, which works with or without a daemon connection.
    Q_PROPERTY(bool logToFile READ getLogToFile NOTIFY logToFileChanged)

    // Do native initialization on the popup-style dashboard window.
    // On OS X, sets the dashboard window to appear on all workspaces.
    Q_INVOKABLE void initDashboardPopup(QQuickWindow *pDashboard);

    // Do native initialization on decorated windows - settings and changelog
    // windows (pretty much anything other than the popup dashboard).
    // On Windows, sets the window icons
    Q_INVOKABLE void initDecoratedWindow(QQuickWindow *pWindow);

    // Stack the second item after the first.  See QQuickItem::stackAfter()
    Q_INVOKABLE void itemStackAfter(QQuickItem *pFirst, QQuickItem *pSecond);

    // Release cached graphics resources owned by a window.  See
    // QQuickWindow::releaseResources()
    Q_INVOKABLE void releaseWindowResources(QQuickWindow *pWindow);

    // Get the client's build version.
    Q_INVOKABLE QString getClientVersion();

    // Check if start on login is enabled
    Q_INVOKABLE bool getStartOnLogin();

    // Set the start on login state
    Q_INVOKABLE void setStartOnLogin(bool enabled);

    // Test whether a resource exists.  Pass a path beginning with "qrc:/", such
    // as a path that would be used in an Image.
    Q_INVOKABLE bool resourceExists(const QString &path);

    // Read the contents of a resource file as text
    Q_INVOKABLE QString readResourceText (const QString &path);

    // Quit and launch the uninstaller.
    Q_INVOKABLE void quitAndUninstall();

    Q_INVOKABLE void setDockVisibility (bool enabled);

    // Trigger a crash on the client. Used to test the crash handler
    Q_INVOKABLE void crashClient();

    Q_INVOKABLE void trimComponentCache();
    Q_INVOKABLE void clearComponentCache();

    // Write around 1 mb of dummy logs to test log overflow
    Q_INVOKABLE void writeDummyLogs();

    Q_INVOKABLE void loadDummyCrashDll (const QString &dllName);

// #endif

    // Start the log uploader tool.  Pass the path to the diagnostics file if
    // one was written (or an empty string if not).
    // Most of the time, use Client.startLogUploader() instead, which requests
    // to the daemon to write the diagnostic file.
    Q_INVOKABLE void startLogUploader(const QString &diagnosticsFile);

    // Wipe the client log file
    Q_INVOKABLE void wipeLogFile();

    // Launch an installer binary that was downloaded by the daemon.
    Q_INVOKABLE bool launchInstaller(const QString &installer);

    // On Linux, start a script to install the WireGuard kernel module
    Q_INVOKABLE bool installWireguardKernelModule();

    // Get a monotonic time reference - ms since the system booted
    Q_INVOKABLE qint64 getMonotonicTime();

    // Reinstall the TAP adapter (Windows only)
    Q_INVOKABLE void reinstallTap();
    Q_PROPERTY(QString reinstallTapStatus READ reinstallTapStatus NOTIFY reinstallTapStatusChanged FINAL)

    // Reinstall the WinTUN adapter (Windows only)
    Q_INVOKABLE void reinstallTun();
    Q_PROPERTY(QString reinstallTunStatus READ reinstallTunStatus NOTIFY reinstallTunStatusChanged FINAL)

    // Reinstall the WFP callout driver (Windows only)
    Q_INVOKABLE void installWfpCallout();
    Q_INVOKABLE void reinstallWfpCallout();
    Q_INVOKABLE void uninstallWfpCallout();
    Q_INVOKABLE void forceWfpCalloutRebootState();
    Q_PROPERTY(QString reinstallWfpCalloutStatus READ reinstallWfpCalloutStatus NOTIFY reinstallWfpCalloutStatusChanged FINAL)

    // Get the global mouse cursor position
    Q_INVOKABLE QPoint getMouseCursorPosition();

    // Ensure the scale factor has been initialized (called when the QML code is
    // about to show a window).
    // On Linux, the scale factor initialization is deferred in lieu of actually
    // handling change notifications; this has to be called manually before
    // showing a window (and before the positioning too if the window is being
    // positioned based on its size).
    Q_INVOKABLE void initScaling();

    // Wrap text to a width that provides a balanced appearance in line widths.
    // Mainly used by InfoTip.  See balancetext.h.
    Q_INVOKABLE double balanceWrappedText(double maxWidth, int fontPixelSize,
                                          const QString &text) const;

    // Open the daemon log in a text editor.  Currently implemented for Linux
    // only.
    // On success, returns an empty string.  On failure, returns the daemon log
    // path, so it can be shown to the user.
    Q_INVOKABLE QString openDaemonLog();

    // Get the allowed choices for icon theme (defined by ClientSettings, varies
    // by platform)
    Q_INVOKABLE QStringList iconThemeValues();

    // Get the preview icon resource for an icon theme (varies by platform)
    Q_INVOKABLE QString iconPreviewResource(const QString &theme);

    // V4 lacks encodeUriComponent()
    Q_INVOKABLE QString encodeUriComponent(const QString &value);

    void requestDashboardReopen();

    Q_INVOKABLE void checkAppDeactivate();

    static void applyStartOnLoginSetting(bool enabled);
    static bool getStartOnLoginSetting();

    void openUrl (const QString &path, const QJsonObject &queryItems);

    // Clean an SSID for display in the client - just replaces any control
    // characters with spaces.  Mainly used because '\n' causes display
    // problems.
    Q_INVOKABLE QString cleanSsidDisplay(const QString &ssid);

    // Render dates and times.  In all cases, the value given is a Unix
    // timestamp in milliseconds, in UTC.  (The date part is unimportant for
    // renderTime(), and the time part is unimportant for renderDate().)  The
    // result is a date/time/date-time string in the local time zone.
    //
    // This uses the system locale, not the language selected in PIA.
    Q_INVOKABLE QString renderDateTime(qint64 timestampMs);
    Q_INVOKABLE QString renderDate(qint64 timestampMs);
    Q_INVOKABLE QString renderTime(qint64 timestampMs);

signals:
    // Emitted when the application loses focus (no window in the application is
    // focused).
    void appFocusLost();
    void logToFileChanged();
    void reinstallTapStatusChanged();
    void reinstallTunStatusChanged();
    void reinstallWfpCalloutStatusChanged();
    void terminalStartFailed (const QString &command);
    void dashboardOpenRequested();
    void urlOpenRequested(const QString &path, const QJsonObject &queryItems);


private:
    Platform getPlatform() const;
    QString getProductName() const;
    QString getMacosProductVersion() const;
    bool getLogToFile();
    bool getSplitTunnelSupported() const;
    void onFocusWindowChanged(QWindow *pFocusWindow);
    QString reinstallTapStatus() const { return _reinstallTapStatus; }
    QString reinstallTunStatus() const { return _reinstallTunStatus; }
    QString reinstallWfpCalloutStatus() const {return _reinstallWfpCalloutStatus;}
    bool runInTerminal(const QString &command);

    // Reinstall a Windows driver
    void reinstallDriver(Driver type, const wchar_t *commandParams);
    // Set either the TAP or WFP Callout driver reinstallation status.
    void setDriverReinstallStatus(Driver type, const QString &status);

    // Check if we can emit a queued URL event now
    void checkQueuedUrlEvent();

    // A signal has been connected - queue a call to checkQueuedUrlEvent() if it
    // is the urlOpenRequested signal
    virtual void connectNotify(const QMetaMethod &signal) override;

private:
#ifdef Q_OS_WIN
    // Notifier used for any driver reinstall (only one can be in progress at a
    // time)
    QWinEventNotifier* _reinstallDriverNotifier = nullptr;
#endif
    // Status of TAP adapter reinstallation
    QString _reinstallTapStatus;
    // Status of WinTUN adapter reinstallation
    QString _reinstallTunStatus;
    // Status of WFP callout driver reinstallation (or installation if this is
    // the first time)
    QString _reinstallWfpCalloutStatus;

    // If openUrl() is called before the client QML is ready to handle this
    // signal, the path and query parameters are held here until a connection is
    // made to urlOpenRequested().  In particular, this occurs on macOS if the
    // client is not open when a URL is clicked (this both launches the client
    // and quickly sends it an event to open a URL).
    QString _queuedOpenUrlPath;
    QJsonObject _queuedOpenUrlQuery;
};

#endif // MAC_INSTALL_H
