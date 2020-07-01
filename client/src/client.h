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
#line HEADER_FILE("client.h")

#ifndef CLIENT_H
#define CLIENT_H
#pragma once

#include "clientsettings.h"
#include "clientqmlcontext.h"
#include "daemonconnection.h"
#include "settings.h"
#include "nativehelpers.h"
#include "preconnectstatus.h"
#include "appsingleton.h"

#include <QFontDatabase>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QTranslator>
#include <QWindow>
#include <atomic>

// QmlCallResult exposes the result of a RemoteCallInterface::call() in a way
// that can be bound to QML.  On the QML side, this is bound to callbacks (since
// Qt's JS engine does not support Promise.)
//
// QmlCallResult will emit exactly one signal to either resolved or rejected
// (unless the remote party never responds to the request).  Signal handlers
// should be connected synchronously after QmlCallResult is created; the
// completion signal will be emitted asynchronously.
class QmlCallResult : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("client")

public:
    // Create the QmlCallResult with an Async<QJsonValue> returned by
    // RemoteCallInterface::call().
    QmlCallResult(Async<QJsonValue> asyncResult);

signals:
    // The request succeeded and returned a value.  The parameter is the
    // result of the call.
    void resolved(QVariant resultValue);
    // The request failed.  This signal is emitted with a native Error object,
    // which QML code then uses to create a JS Error.
    void rejected(Error nativeError);

private:
    void reject(const Error &error);

private slots:
    void onResultFinished();

private:
    Async<QJsonValue> _asyncResult;
};

// Expose relevant properties of the daemon as a QML class, merely mirroring
// the native singleton.
class DaemonInterface : public QObject
{
    Q_OBJECT
public:
    explicit DaemonInterface(DaemonConnection* daemonConnection, QObject* parent = nullptr);

    bool isConnected() const { return _daemonConnection->isConnected(); }

public:
#define MemberProperty(type, name) \
    private: type* get_##name() { return &_daemonConnection->name; } \
    Q_PROPERTY(type* name READ get_##name FINAL CONSTANT)

    MemberProperty(DaemonData, data)
    MemberProperty(DaemonAccount, account)
    MemberProperty(DaemonSettings, settings)
    MemberProperty(DaemonState, state)
#undef MemberProperty

    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged FINAL)

signals:
    void connectedChanged(bool isConnected);
    void error(const Error& error);

public slots:
    // Generic call mechanism for any RPC function from QML.
    // This is only intended to be used by QML; it returns a QmlCallResult that
    // the caller must free.  (The QML engine requires this to be an owning raw
    // pointer.)
    Q_REQUIRED_RESULT QmlCallResult *qmlCall(const QString &method, const QJsonArray &params)
    {
        return new QmlCallResult(_daemonConnection->call(method, params));
    }
    // Generic post mechanism for any RPC function
    void post(const QString& method, const QJsonArray& params) { _daemonConnection->post(method, params); }

private:
    DaemonConnection* _daemonConnection;
};

// Just a QTranslator that fills in the file name, separator, suffix, etc. when
// calling load().
class ClientTranslator : public QTranslator
{
public:
    using QTranslator::QTranslator;
    bool load(const QLocale &locale);
};

class Client;

// Client graphics modes - chosen by clientMain() but passed through to
// ClientInterface
enum class GraphicsMode
{
    // Normal graphics (Hardware if available, still may fall back to
    // software if hardware is unavailable.  Not considered safe mode,
    // still uses composited popup dashboard if possible.)
    Normal,
    // Safe graphics mode (Always software, never composite dashboard)
    Safe,
    // Safe graphics mode, and force persistent setting on (triggered by
    // --safe-mode)
    PersistSafe,
};

// Provides access to Client settings and state to QML
class ClientInterface : public QObject
{
    Q_OBJECT

private:
    // Locales that we use for pseudotranslation
    static const QString _pseudotranslationLocale, _pseudotranslationRtlLocale;

public:
    // The contents of clientsettings.json (and a flag indicating whether it
    // actually existed) are passed in from clientMain() since it has to
    // pre-read the settings at startup.
    // They're passed as a QJsonObject rather than a ClientSettings so we don't
    // have to maintain copy operations for ClientSettings (with a subtle
    // failure mode that settings could work but wouldn't be reloaded).
    ClientInterface(bool hasExistingSettingsFile,
                    const QJsonObject &initialSettings, GraphicsMode gfxMode,
                    bool quietLaunch);
    ~ClientInterface();

    Q_PROPERTY(ClientSettings* settings READ get_settings FINAL CONSTANT)
    Q_PROPERTY(ClientState* state READ get_state FINAL CONSTANT)
    ClientSettings *get_settings() {return &_settings;}
    ClientState *get_state() {return &_state;}

private:
    void writeSettings();
    // Add a language that uses a script supported by the embedded Roboto font
    // (so it's always available)
    void addKnownLanguage(QVector<ClientLanguage> &languages,
                          const QString &locale, const QString &displayName,
                          bool rtlMirror = false);
    // Add a language that uses a script other than Latin.
    void addCheckedLanguage(QVector<ClientLanguage> &languages,
                            const QString &locale, const QString &nativeName,
                            const QString &englishName,
                            QFontDatabase::WritingSystem writingSystem,
                            bool rtlMirror = false);
    void loadLanguages();
    QString matchOsLanguage(const QString &osLang);
    QString getFirstRunLanguage();
    void setTranslation(const QString &locale);

public:
    // Apply changes to client-side settings.
    Q_INVOKABLE bool applySettings(const QJsonObject &settings);
    // Reset client-side settings
    Q_INVOKABLE void resetSettings();

    // Upper-case a string using the current locale.
    // Note that this does not introduce a retranslation dependency, usually the
    // caller already has that dependency anyway.  (Use `var dummyDep = uiTr` if
    // necessary.)
    Q_INVOKABLE QString localeUpperCase(const QString &text) const;

    // If migrateDaemonSettings==true, migrate values from DaemonSettings.
    void migrateFromDaemon(const DaemonSettings &daemonSettings);

signals:
    // Emitted when the language changes; tells Client to retranslate the UI.
    void retranslate();

private:
    // Current locale loaded in _currentTranslation - used for locale functions
    QLocale _currentLocale;
    ClientSettings _settings;
    ClientState _state;
    ClientTranslator _currentTranslation;
};

class Client : public QObject, public Singleton<Client>
{
    Q_OBJECT

private:
    // Store the most recent daemon socket's file descriptor so the client can
    // get it on Linux/X11 if Xlib tries to kill the process.
    // Due to Xlib's awful error handling, exit() could be called on any thread
    // or even from multiple threads.  So we store the socket descriptor here
    // to ensure that we can load it without trying to dispatch threads or
    // access the heap.  This creates a race condition if an Xlib exit() races
    // with a daemon reconnect, but there is simply no way to 100% correctly
    // handle Xlib errors.
    static std::atomic<qintptr> _currentDaemonSocket;
public:
    static qintptr getCurrentDaemonSocket() {return _currentDaemonSocket.load();}

public:
    Client(bool hasExistingSettingsFile, const QJsonObject &initialSettings,
           GraphicsMode gfxMode, bool quietLaunch);
    ~Client();

    DaemonConnection* daemon() { return _daemon; }

    void setupFonts();
    ClientInterface* getInterface () { return &_clientInterface; }
    void openDashboard();

    // If needed, notify the daemon that the client is exiting.  (This does not
    // wait for the notification to complete.)
    void notifyExit();

    // After the main event loop ends, notify the daemon if we haven't already
    // done so, and allow for the notification to complete if it's still in
    // progress.
    void notifyExitAndWait();

    void handleURL (const QString &url);
    void checkForURL();

    IMPLEMENT_NOTIFICATIONS(Client)

private:
    void loadQml(const QString &qmlResource);
    void createSplashScreen();
    void createMainWindow();

signals:
    void retranslate();

public slots:
    void init();

protected slots:
    void daemonConnectedChanged(bool connected);

protected:
    DaemonConnection* _daemon;
    DaemonInterface _daemonInterface;
    NativeHelpers _nativeHelpers;
    PreConnectStatus _preConnectStatus;
    ClientInterface _clientInterface;
    QQmlApplicationEngine _engine;
private:
    ClientQmlContext _qmlContext;

    // In-progress requests to notify the daemon of client activation/
    // deactivation are held here (the notifyInteractiveClient / notifyExit
    // requests)
    Async<void> _notifyActivateResult;
    // This indicates whether we have told the daemon that this is an active
    // client.  Between this and _notifyActivateResult, we can discern the state
    // of this notification:
    // _activated | _notifyActivateResult | state
    // -----------|-----------------------|-------
    // false      | valid                 | activation in-progress
    // true       | nullptr               | active (need to deactivate when exiting)
    // true       | valid                 | deactivation in-progress
    // false      | nullptr               | inactive - ready to exit
    bool _activated;
    // The main UI is loaded after the daemon connection is established (the
    // first time) and the client activates itself.  Once we've loaded the UI,
    // don't do it again even if the daemon connection is lost and
    // re-established.
    bool _mainUiLoaded;
};

#define g_client (Client::instance())

#define g_daemonConnection (g_client->daemon())

#define g_daemonConfig (g_daemonConnection->config)
#define g_daemonAccount (g_daemonConnection->account)
#define g_daemonSettings (g_daemonConnection->settings)
#define g_daemonState (g_daemonConnection->state)


#endif // CLIENT_H
