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

#include "brandhelper.h"
#include "common.h"
#line SOURCE_FILE("client.cpp")

#include "client.h"
#include "path.h"
#include "trayiconmanager.h"
#include "nativehelpers.h"
#include "circlemousearea.h"
#include "draghandle.h"
#include "focuscue.h"
#include "windowclipper.h"
#include "windowformat.h"
#include "windowscaler.h"
#include "windowmaxsize.h"
#include "clipboard.h"
#include "flexvalidator.h"
#include "path_interface.h"
#include "semversion.h"
#include "version.h"
#include "brand.h"
#include "nativeacc/nativeacc.h"
#include "splittunnelmanager.h"
#include "appsingleton.h"
#include "apiretry.h"

#if defined(Q_OS_MACOS)
#include "mac/mac_loginitem.h"
#include "mac/mac_language.h"
#include "mac/mac_window.h"
#elif defined(Q_OS_WIN)
#include "win/win_language.h"
#include "win/win_util.h"
#elif defined(Q_OS_LINUX)
#include "linux/linux_language.h"
#endif

#ifdef Q_OS_UNIX
#include "posix/unixsignalhandler.h"
#endif

#include <QGuiApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QPointer>
#include <QProcess>
#include <QQmlContext>
#include <QTimer>
#include <QtGlobal>
#include <QUrl>
#include <QUrlQuery>

QmlCallResult::QmlCallResult(Async<QJsonValue> asyncResult)
    : _asyncResult{std::move(asyncResult)}
{
    Q_ASSERT(_asyncResult); // Ensured by RemoteCallInterface::call()

    // The async result could have already been rejected -
    // RemoteCallInterface::call() does this if nobody is listening to the
    // signal emitted for a new message to avoid leaving the message hanging
    // with no response.
    //
    // If this happens, queue a delayed call to onResultFinished() so we still
    // reject asynchronously.
    if(_asyncResult->isFinished())
    {
        QMetaObject::invokeMethod(this, &QmlCallResult::onResultFinished,
                                  Qt::QueuedConnection);
    }
    else
    {
        connect(_asyncResult.data(), &Task<QJsonValue>::finished, this,
                &QmlCallResult::onResultFinished);
    }
}

void QmlCallResult::reject(const Error &error)
{
    emit rejected(error);
}

void QmlCallResult::onResultFinished()
{
    if(_asyncResult->isResolved())
    {
        // Resolved successfully - pass the result to resolved().
        // QML uses QVariantMap / QVariantList to represent arbitrary JS
        // objects and arrays.  QJsonValue::toVariant() builds this variant
        // representation.
        //
        // Another alternative could be to reserialize as JSON and then
        // deserialize on the QML side.  It's not really clear which of these
        // would be faster - the JSON method would probably involve more CPU
        // overhead to parse and validate the JSON, but the variant method
        // probably involves more dynamic memory allocation, at least for small
        // values.
        emit resolved(_asyncResult->result().toVariant());
    }
    else if(_asyncResult->isRejected())
    {
        reject(_asyncResult->error());
    }
    else
    {
        // The result was abandoned.  Neither result() nor error() is valid.
        reject({HERE, Error::Code::Unknown});
    }
}

bool ClientTranslator::load(const QLocale &locale)
{
    // Look for a file like ':/translations/client.en_US.qm'
    return QTranslator::load(locale, QStringLiteral("client"),
                             QStringLiteral("."),
                             QStringLiteral(":/translations"));
}

const QString ClientInterface::_pseudotranslationLocale{QStringLiteral("ro")};
const QString ClientInterface::_pseudotranslationRtlLocale{QStringLiteral("ps")};

ClientInterface::ClientInterface(bool hasExistingSettingsFile,
                                 const QJsonObject &initialSettings,
                                 GraphicsMode gfxMode, bool quietLaunch)
{
    _settings.readJsonObject(initialSettings);

    _state.firstRunFlag(!hasExistingSettingsFile);
    _state.quietLaunch(quietLaunch);

    loadLanguages();

    // If the language hasn't been set yet, default to the system language.
    if(_settings.language().isEmpty())
        _settings.language(getFirstRunLanguage());

    QCoreApplication *pApp = QCoreApplication::instance();
    Q_ASSERT(pApp); // Ensured by clientMain(); Client outlives QCoreApplication
    pApp->installTranslator(&_currentTranslation);

    setTranslation(_settings.language());

    connect(&_settings, &ClientSettings::languageChanged, this,
            [this](){setTranslation(_settings.language());});

    // Read the last used version, and set the current version as the last used version
    QString lastUsedVersion = _settings.lastUsedVersion();
    QString currentVersion(PIA_VERSION);
    _settings.lastUsedVersion(currentVersion);


    try {
        // For older versions which don't write 'lastUsedVersion' we can
        // guess that the client was upgraded if there was indeed an existing settings file
        // but last used version was empty.
        //
        // For fresh installs - hasExistingSettingsFile would be false
        // For well-behaved upgrades - lastUsedVersion will not be empty and will be handled by checking semantic version
        //
        //
        // Do note that if it doesn't have an existing file, and `lastUsedVersion` is indeed empty
        // then SemVersion will throw an exception, but this is the expected behaviour and the value
        // of `clientHasBeenUpdated` will remain `false`.
        //
        if((hasExistingSettingsFile && lastUsedVersion.isEmpty()) || (SemVersion(currentVersion).compare(SemVersion(lastUsedVersion)) > 0))
            _state.clientHasBeenUpdated(true);
    } catch (...) {
    }

    // clientMain() already enabled safe mode if necessary, but set the client
    // state and apply the permanent setting if --safe-mode was given.
    switch(gfxMode)
    {
        case GraphicsMode::PersistSafe:
            _settings.disableHardwareGraphics(true);
            writeSettings();
            Q_FALLTHROUGH();
        case GraphicsMode::Safe:
            _state.usingSafeGraphics(true);
            break;
        default:
        case GraphicsMode::Normal:
            break;  // Nothing to do
    }

#if defined(Q_OS_WIN)
    WinHandle procToken{};
    TOKEN_ELEVATION_TYPE elevationType{TokenElevationTypeDefault};
    DWORD returnedSize{0};
    if(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, procToken.receive()) &&
       ::GetTokenInformation(procToken.get(), TokenElevationType, &elevationType,
                             sizeof(elevationType), &returnedSize) &&
       returnedSize == sizeof(elevationType))
    {
        qInfo() << "Client elevation status:" << elevationType;
        _state.winIsElevated(elevationType == TokenElevationTypeFull);
    }
    else
    {
        qWarning() << "Unable to determine client elevation type -"
            << "hToken:" << procToken.get() << "- returnedSize:"
            << returnedSize << "-" << SystemError{HERE};
    }
#endif
}

ClientInterface::~ClientInterface()
{
    QCoreApplication *pApp = QCoreApplication::instance();
    Q_ASSERT(pApp); // Ensured by clientMain(); Client outlives QCoreApplication
    pApp->removeTranslator(&_currentTranslation);
}

void ClientInterface::writeSettings()
{
    writeProperties(_settings.toJsonObject(), Path::ClientSettingsDir, "clientsettings.json");
}

void ClientInterface::addKnownLanguage(QVector<ClientLanguage> &languages,
                                       const QString &locale,
                                       const QString &displayName,
                                       bool rtlMirror)
{
    ClientLanguage newLang;
    newLang.locale(locale);
    newLang.displayName(displayName);
    newLang.available(true);
    newLang.rtlMirror(rtlMirror);

    languages.push_back(newLang);
}

void ClientInterface::addCheckedLanguage(QVector<ClientLanguage> &languages,
                                         const QString &locale,
                                         const QString &nativeName,
                                         const QString &englishName,
                                         QFontDatabase::WritingSystem writingSystem,
                                         bool rtlMirror)
{
    ClientLanguage newLang;
    newLang.locale(locale);
    newLang.rtlMirror(rtlMirror);

    if(!QFontDatabase{}.families(writingSystem).isEmpty())
    {
        // This script is supported, use the native name and allow this language
        newLang.displayName(nativeName);
        newLang.available(true);
    }
    else
    {
        // Not supported, use the English name and do not allow this language
        newLang.displayName(englishName);
        newLang.available(false);
    }

    languages.push_back(newLang);
}

void ClientInterface::loadLanguages()
{
    // Probe for the languages that are actually available and populate
    // ClientState::languages()
    QVector<ClientLanguage> languages;

    // This is the order the languages appear in the UI.
    // The 'u' prefixes on the name strings are required for MSVC - even with
    // '/utf-8' something in the QStringLiteral macro still causes these to be
    // interpreted as Windows-1252 otherwise.
    addKnownLanguage(languages, QStringLiteral("en-US"), QStringLiteral(u"English"));
    addCheckedLanguage(languages, QStringLiteral("zh-Hans"), QStringLiteral(u"简体中文"), QStringLiteral("Simplified Chinese"), QFontDatabase::WritingSystem::SimplifiedChinese);
    addCheckedLanguage(languages, QStringLiteral("zh-Hant"), QStringLiteral(u"繁體中文"), QStringLiteral("Traditional Chinese"), QFontDatabase::WritingSystem::TraditionalChinese);
    addKnownLanguage(languages, QStringLiteral("da"), QStringLiteral(u"Dansk"));
    addKnownLanguage(languages, QStringLiteral("nl"), QStringLiteral(u"Nederlands"));
    addKnownLanguage(languages, QStringLiteral("fr"), QStringLiteral(u"Français"));
    addKnownLanguage(languages, QStringLiteral("de"), QStringLiteral(u"Deutsch"));
    addKnownLanguage(languages, QStringLiteral("it"), QStringLiteral(u"Italiano"));
    addCheckedLanguage(languages, QStringLiteral("ja"), QStringLiteral(u"日本語"), QStringLiteral("Japanese"), QFontDatabase::WritingSystem::Japanese);
    addCheckedLanguage(languages, QStringLiteral("ko"), QStringLiteral(u"한국어"), QStringLiteral("Korean"), QFontDatabase::WritingSystem::Korean);
    addKnownLanguage(languages, QStringLiteral("nb"), QStringLiteral(u"Norsk (Bokmål)"));
    addKnownLanguage(languages, QStringLiteral("pl"), QStringLiteral(u"Polski"));
    addKnownLanguage(languages, QStringLiteral("pt-BR"), QStringLiteral(u"Português (Brasil)"));
    addKnownLanguage(languages, QStringLiteral("ru"), QStringLiteral(u"Русский"));
    addKnownLanguage(languages, QStringLiteral("es-MX"), QStringLiteral(u"Español (México)"));
    addKnownLanguage(languages, QStringLiteral("sv"), QStringLiteral("Svenska"));
    addCheckedLanguage(languages, QStringLiteral("th"), QStringLiteral(u"ไทย"), QStringLiteral("Thai"), QFontDatabase::WritingSystem::Thai);
    addKnownLanguage(languages, QStringLiteral("tr"), QStringLiteral(u"Türkçe"));
    addCheckedLanguage(languages, QStringLiteral("ar"), QStringLiteral(u"العربية"), QStringLiteral("Arabic"), QFontDatabase::WritingSystem::Arabic, true);

    // Only add the pseudolocalization languages in debug builds.  Their .ts
    // files are excluded from release builds.
    // These use real locales instead of something user-assigned like en_ZZ,
    // because QLocale ignores user-defined codes.
#ifdef _DEBUG
    addKnownLanguage(languages, _pseudotranslationLocale, QStringLiteral("Pseudo-translated"));
    addKnownLanguage(languages, _pseudotranslationRtlLocale, QStringLiteral("Pseudo-translated (RTL)"), true);
#endif

    _state.languages(languages);
}

QString ClientInterface::matchOsLanguage(const QString &osLang)
{
    // The incoming OS language is a language tag, which in general can have a
    // large number of fields.
    //
    // Use a QLocale to parse out the language and script.  (The script is
    // needed to correctly match zh-Hans and zh-Hant.)
    //
    // Note that QLocale is very picky about the country/region of the incoming
    // tag; it will ignore it if it doesn't think it's a valid country for that
    // language.  For example, OS X will give 'zh-Hant-CN' if you select
    // Traditional Chinese with region China, but Qt doesn't like that - a
    // QLocale{"zh-Hant-CN"} will still say its country is Taiwan.
    //
    // QLocale _is_ smart enough though to pick the correct script for plain
    // "zh-TW" and "zh-CN", which occur on Windows and Linux.
    //
    // Fortunately, we do not have more than one localization for any
    // particular language+script combination right now, so we can just check
    // the language+script and ignore the country.  This works both for exact
    // matches and an approximate fallback (for example, matching "es" or
    // "es-US", to "es-MX").

    QLocale osLangLocale{osLang};
    qInfo() << "OS language" << osLang << "- parsed as language:" << osLangLocale.language() << "and script:" << osLangLocale.script();

    for(const auto &language : _state.languages())
    {
        // Similarly, use QLocale to match up the language and script for the
        // candidate language.
        QLocale supportedLangLocale{language.locale()};
        if(osLangLocale.language() == supportedLangLocale.language() &&
           osLangLocale.script() == supportedLangLocale.script())
        {
            return language.locale();
        }
    }

    // No match
    return {};
}

QString ClientInterface::getFirstRunLanguage()
{
    // Invariant; languages have been set up by ctor (and there's always at
    // least en-US)
    Q_ASSERT(!_state.languages().isEmpty());
    // Invariant; default language is available (it's en-US, which is built-in)
    Q_ASSERT(_state.languages()[0].available());

    // Get the OS languages.
    // Use native APIs to do this, QLocale::system() is pretty broken.
    // In particular, on Mac OS at least, if your region isn't set to a region
    // that Qt thinks is OK for that language, it'll just return en-US.  It
    // seems to just return 'zh' for 'zh-Hant' (and probably 'zh-Hans' too).  It
    // also can't represent the list of language preferences that most OSes
    // provide.
    QList<QString> osLangs =
#if defined(Q_OS_MAC)
        macGetDisplayLanguages();
#elif defined(Q_OS_WIN)
        winGetDisplayLanguages();
#elif defined(Q_OS_LINUX)
        linuxGetDisplayLanguages();
#endif

    qInfo() << "System display languages are" << osLangs;

    for(const auto &osLang : osLangs)
    {
        const auto &match = matchOsLanguage(osLang);
        if(!match.isEmpty())
        {
            qInfo() << "Matched system language" << osLang
                << "to supported language" << match;
            return match;
        }
    }

    // There's no match, use the default
    qInfo() << "Did not match any system languages, use the default language";
    return _state.languages()[0].locale();
}

void ClientInterface::setTranslation(const QString &locale)
{
    // Check if this is actually an available language - use the default instead
    // if it isn't.
    const auto &languages = _state.languages();
    Q_ASSERT(!languages.isEmpty()); // Always have at least en-US

    auto itLocaleLang = std::find_if(languages.begin(), languages.end(),
                                     [&](const auto &lang){return lang.locale() == locale;});
    if(itLocaleLang == languages.end() || !itLocaleLang->available())
    {
        qWarning() << "Locale" << locale << "is not available, using default";
        itLocaleLang = languages.begin();
    }

    _currentLocale = QLocale{itLocaleLang->locale()};
    _currentTranslation.load(_currentLocale);
    qInfo() << "Changed to locale" << itLocaleLang->locale();
    _state.activeLanguage(*itLocaleLang);
    emit retranslate();
}

bool ClientInterface::applySettings(const QJsonObject &settings)
{
    qDebug().noquote() << "Applying settings:" << QJsonDocument(settings).toJson(QJsonDocument::Compact);

    bool success = _settings.assign(settings);

    if(!success)
        qWarning() << "Not all settings applied:" << *_settings.error();

    // Write out the new settings.
    // Unlike Daemon, we don't need to defer this and batch writes, client
    // settings aren't changed rapidly like DaemonData.
    writeSettings();

    return success;
}

void ClientInterface::resetSettings()
{
    // Reset settings by applying default settings
    QJsonObject defaultsJson = ClientSettings{}.toJsonObject();

    // Do not reset these values - remove them before applying the defaults

    // Migration flag - don't re-migrate old values after a reset
    defaultsJson.remove(QStringLiteral("migrateDaemonSettings"));
    // Favorites and recents - not presented as "settings" in the client
    defaultsJson.remove(QStringLiteral("favoriteLocations"));
    defaultsJson.remove(QStringLiteral("recentLocations"));
    // Modules lists - not presented as settings, also the UI does not handle
    // external changes to the setting
    defaultsJson.remove(QStringLiteral("primaryModules"));
    defaultsJson.remove(QStringLiteral("secondaryModules"));
    // regionSortKey - not presented as a setting
    defaultsJson.remove(QStringLiteral("regionSortKey"));
    // Last used version - not a setting
    defaultsJson.remove(QStringLiteral("lastUsedVersion"));

    // Help page settings are not reset, as they were most likely changed for
    // troubleshooting.
    defaultsJson.remove(QStringLiteral("disableHardwareGraphics"));

    // These values have nontrivial defaults

    // Language - detected by ClientInterface ctor; depends on the OS languages
    // and the supported languages in this build
    defaultsJson.insert(QStringLiteral("language"), getFirstRunLanguage());

    applySettings(defaultsJson);
}

QString ClientInterface::localeUpperCase(const QString &text) const
{
    // Qt's ICU support is not enabled by default on Mac/Windows.  Use native
    // APIs instead.
#if defined(Q_OS_MAC)
    return macLocaleUpper(text, _currentLocale);
#elif defined(Q_OS_WIN)
    return winLocaleUpper(text, _currentLocale);
#elif defined(Q_OS_LINUX)
    // ICU support is enabled and deployed on Linux, QLocale is fine.
    return _currentLocale.toUpper(text);
#else
    // QLocale might work for future platforms, but it would need to be checked.
    #error "localeUpperCase() not implemented for new platform"
#endif
}

void ClientInterface::migrateFromDaemon(const DaemonSettings &daemonSettings)
{
    if(_settings.migrateDaemonSettings())
    {
        _settings.connectOnLaunch(daemonSettings.connectOnLaunch());
        _settings.desktopNotifications(daemonSettings.desktopNotifications());
        _settings.themeName(daemonSettings.themeName());
        _settings.favoriteLocations(daemonSettings.favoriteLocations());
        _settings.recentLocations(daemonSettings.recentLocations());
        _settings.primaryModules(daemonSettings.primaryModules());
        _settings.secondaryModules(daemonSettings.secondaryModules());
        _settings.migrateDaemonSettings(false);

        qDebug().noquote() << "Settings after migration:" << QJsonDocument(_settings.toJsonObject()).toJson(QJsonDocument::Compact);
        writeSettings();
    }
}

std::atomic<qintptr> Client::_currentDaemonSocket{-1};

Client::Client(bool hasExistingSettingsFile, const QJsonObject &initialSettings,
               GraphicsMode gfxMode, bool quietLaunch)
    : _daemon(new DaemonConnection(this))
    , _daemonInterface{_daemon}
    , _nativeHelpers{}
    , _preConnectStatus{*_daemon}
    , _clientInterface{hasExistingSettingsFile, initialSettings, gfxMode,
                       quietLaunch}
    , _qmlContext{_engine}
    , _notifyActivateResult{}
    , _activated{false}
    , _mainUiLoaded{false}
{
    QQmlEngine::setObjectOwnership(&_daemonInterface, QQmlEngine::ObjectOwnership::CppOwnership);
    QQmlEngine::setObjectOwnership(&_nativeHelpers, QQmlEngine::ObjectOwnership::CppOwnership);
    QQmlEngine::setObjectOwnership(&_preConnectStatus, QQmlEngine::ObjectOwnership::CppOwnership);
    QQmlEngine::setObjectOwnership(&_clientInterface, QQmlEngine::ObjectOwnership::CppOwnership);

    // Install _qmlContext as the global context for all QML code
    QQmlContext *pGlobalContext = _engine.rootContext();
    Q_ASSERT(pGlobalContext);   // QQmlEngine always has a root context
    // There can't already be a context object (Qt's globals are not implemented
    // with a context object)
    Q_ASSERT(!pGlobalContext->contextObject());
    pGlobalContext->setContextObject(&_qmlContext);

    qmlRegisterType<DaemonData>("PIA.NativeDaemon.Data", 1, 0, "NativeDaemonData");
    qmlRegisterType<DaemonAccount>("PIA.NativeDaemon.Account", 1, 0, "NativeDaemonAccount");
    qmlRegisterType<DaemonSettings>("PIA.NativeDaemon.Settings", 1, 0, "NativeDaemonSettings");
    qmlRegisterType<DaemonState>("PIA.NativeDaemon.State", 1, 0, "NativeDaemonState");

    qmlRegisterType<ClientSettings>("PIA.NativeClient.Settings", 1, 0, "NativeClientSettings");

    qmlRegisterType<TrayIconManager>("PIA.Tray", 1, 0, "TrayIconManager");
    qmlRegisterType<TrayMetrics>(); // Not instantiated by QML.
    // QmlCallResult objects are used by QML but not instantiated by it.
    qmlRegisterType<QmlCallResult>();
    // The Error type is used by QML to refer to Error::Code values.
    // Note that we call this type 'NativeError' in QML because 'Error' refers
    // to the JS Error class.
    qmlRegisterUncreatableType<Error>("PIA.Error", 1, 0, "NativeError", "Can't create Error from QML");

    qmlRegisterType<CircleMouseArea>("PIA.CircleMouseArea", 1, 0, "CircleMouseArea");
    qmlRegisterType<FocusCue>("PIA.FocusCue", 1, 0, "FocusCue");
    qmlRegisterType<DragHandle>("PIA.DragHandle", 1, 0, "DragHandle");
    qmlRegisterType<FlexValidator>("PIA.FlexValidator", 1, 0, "FlexValidator");

    qmlRegisterSingletonType<DaemonInterface>("PIA.NativeDaemon", 1, 0, "NativeDaemon",
        [](auto, auto) -> QObject* {return &Client::instance()->_daemonInterface;});
    qmlRegisterSingletonType<NativeHelpers>("PIA.NativeHelpers", 1, 0, "NativeHelpers",
        [](auto, auto) -> QObject* {return &Client::instance()->_nativeHelpers;});
    qmlRegisterSingletonType<PreConnectStatus>("PIA.PreConnectStatus", 1, 0, "PreConnectStatus",
        [](auto, auto) -> QObject* {return &Client::instance()->_preConnectStatus;});
    qmlRegisterSingletonType<ClientInterface>("PIA.NativeClient", 1, 0, "NativeClient",
        [](auto, auto) -> QObject* {return &Client::instance()->_clientInterface;});
    qmlRegisterSingletonType<Clipboard>("PIA.Clipboard", 1, 0, "Clipboard",
        [](auto, auto) -> QObject* {return new Clipboard;});
    qmlRegisterSingletonType<PathInterface>("PIA.PathInterface", 1, 0, "PathInterface",
        [](auto, auto) -> QObject* {return new PathInterface;});
    qmlRegisterSingletonType<BrandHelper>("PIA.BrandHelper", 1, 0, "BrandHelper",
        [](auto, auto) -> QObject* {return new BrandHelper;});
    qmlRegisterSingletonType<SplitTunnelManager>("PIA.SplitTunnelManager", 1, 0, "SplitTunnelManager",
        [](auto, auto) -> QObject* {return new SplitTunnelManager;});

    qmlRegisterType<WindowClipper>("PIA.WindowClipper", 1, 0, "WindowClipper");
    qmlRegisterUncreatableType<WindowFormat>("PIA.WindowFormat", 1, 0, "WindowFormat", "WindowFormat is an attaching type only");
    qmlRegisterType<WindowScaler>("PIA.WindowScaler", 1, 0, "WindowScaler");
    qmlRegisterType<WindowMaxSize>("PIA.WindowMaxSize", 1, 0, "WindowMaxSize");
    qmlRegisterType<WorkspaceChange>("PIA.WorkspaceChange", 1, 0, "WorkspaceChange");

    NativeAcc::init();

    connect(&g_daemonSettings, &DaemonSettings::debugLoggingChanged, this, []() {
        const auto& value = g_daemonSettings.debugLogging();
        if (value == nullptr)
            g_logger->configure(false, {});
        else
            g_logger->configure(true, *value);
    });

    connect(g_logger, &Logger::configurationChanged, this, [](bool logToFile, const QStringList& filters) {
        if (!logToFile)
        {
            // Wipe log file only after the configuration is updated. This is because
            // the file isn't closed until the config change is emitted
            g_logger->wipeLogFile();
        }
    });

    connect(&_clientInterface, &ClientInterface::retranslate, this,
            [this](){_qmlContext.retranslate();});

#ifdef Q_OS_UNIX
    auto *signalHandler = new UnixSignalHandler();
    connect(signalHandler, &UnixSignalHandler::sigUsr1, this, [this]() {
      qDebug () << "Showing dashboard because received SIGUSR1";
      this->openDashboard();
      this->checkForURL();
    });
    connect(signalHandler, &UnixSignalHandler::sigInt, this, [this]() {
      qInfo() << "Exit due to SIGINT";
      QCoreApplication::quit();
    });
    connect(signalHandler, &UnixSignalHandler::sigTerm, this, [this]() {
      qInfo() << "Exit due to SIGTERM";
      QCoreApplication::quit();
    });
#endif

    // Emit a signal from Client that native components can connect to if
    // necessary
    connect(&_clientInterface, &ClientInterface::retranslate, this,
            &Client::retranslate);
}

Client::~Client()
{
    // Remove the context object since it'll be destroyed before _engine
    // (probably doesn't matter, but play it safe)
    // Install _qmlContext as the global context for all QML code
    QQmlContext *pGlobalContext = _engine.rootContext();
    Q_ASSERT(pGlobalContext);   // QQmlEngine always has a root context
    // Our object must still be the context object
    Q_ASSERT(pGlobalContext->contextObject() == &_qmlContext);
    pGlobalContext->setContextObject(nullptr);
}

void Client::setupFonts()
{
    QFontDatabase::addApplicationFont(":/extra/fonts/Roboto-Regular.ttf");
    QFontDatabase::addApplicationFont(":/extra/fonts/Roboto-Bold.ttf");
    QFontDatabase::addApplicationFont(":/extra/fonts/Roboto-Light.ttf");

    QFont roboto("Roboto");
    roboto.setPixelSize(13);

    QGuiApplication::setFont(roboto);
}

void Client::openDashboard()
{
    _nativeHelpers.requestDashboardReopen();
}

void Client::notifyExit()
{
    if(daemon() && daemon()->isConnected())
    {
        // We're connected to a daemon and about to notify exit.  If the
        // the activate notification is still in progress, abandon it but
        // assume that it completed.
        if(!_activated && _notifyActivateResult)
        {
            qInfo() << "Abandon activation request since client is exiting";
            _notifyActivateResult.abandon();
            _activated = true;
        }

        // If we are activated and have not already started an exit
        // notification, start it now.
        if(_activated && !_notifyActivateResult)
        {
            qDebug () << "Sending deactivate notification";
            auto callResult = daemon()->call(QStringLiteral("notifyClientDeactivate"), {});
            // Use next() instead of notify() so we can abandon if needed.
            _notifyActivateResult = callResult->next(this,
                [this](const Error &error, const QJsonValue &)
                {
                    _notifyActivateResult.reset();
                    if(error)
                    {
                        qWarning() << "Failed to notify intentional exit:" << error;
                    }
                    else
                    {
                        _activated = false;
                    }
                });
        }
    }
    else
    {
        // notifyExit() guarantees that we are either deactivated or
        // deactivating as a postcondition (we cannot be activated or
        // activating).  notifyExitAndWait() relies on this.
        //
        // If we're not connected, we are necessarily in the deactivated state,
        // we cleared these when the daemon connection indicated a change to
        // connected=false.
        Q_ASSERT(!_notifyActivateResult);
        Q_ASSERT(!_activated);
    }
}

void Client::notifyExitAndWait()
{
    notifyExit();

    if(!_notifyActivateResult)
    {
        // Postcondition of notifyExit(), if there's no request in progress, we
        // are deactivated (it would have started a deactivate request
        // otherwise)
        Q_ASSERT(!_activated);

        qInfo() << "Shutdown already complete, no need to wait any longer";
    }
    else
    {
        // Postcondition of notifyExit() - there can't be an _activate_ request
        // in-flight at this point, _notifyActivateRequest is necessarily a
        // _deactivate_ request
        Q_ASSERT(_activated);

        qInfo() << "An exit notification is in progress, try to let it complete";

        QEventLoop eventLoop;

        // Exit after 500 ms or if the exit call completes
        QTimer timeout;
        timeout.setSingleShot(true);
        connect(&timeout, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
        timeout.start(500);

        auto eventLoopExitTask = _notifyActivateResult->next(&eventLoop,
            [&eventLoop](const Error &){eventLoop.quit();});

        eventLoop.exec(QEventLoop::ExcludeUserInputEvents);

        // If the event loop here ends due to timeout, eventLoopExitTask logs an
        // abandon warning.
    }
}

void Client::handleURL(const QString &resourceUrl)
{
    qDebug () << "Handling URL " << ApiResource{resourceUrl};
    QUrl url{resourceUrl};
    if(url.scheme() != BRAND_CODE "vpn") {
        qWarning () << "Invalid scheme " << url.scheme();
        return;
    }

    QUrlQuery query{url};
    auto items = query.queryItems();
    QJsonObject queryObject;
    for(auto i = items.begin(); i != items.end(); ++i) {
        auto queryItem = *i;
        queryObject.insert(queryItem.first, queryItem.second);
    }

    _nativeHelpers.openUrl(url.path(), queryObject);
}

void Client::checkForURL()
{
    auto urlCheck = AppSingleton::instance();
    QString url = urlCheck->getLaunchResource();
    handleURL(url);
}

void Client::loadQml(const QString &qmlResource)
{
    auto prevRootCount = _engine.rootObjects().size();
    _engine.load(QUrl(qmlResource));
    if (_engine.rootObjects().size() == prevRootCount)
    {
        qCritical() << "Failed to load QML resource" << qmlResource;
        QCoreApplication::quit();
    }
}

void Client::createSplashScreen()
{
    loadQml(QStringLiteral("qrc:/components/main-splash.qml"));
}

void Client::createMainWindow()
{
    SplitTunnelManager::installImageHandler(&_engine);
    loadQml(QStringLiteral("qrc:/components/main.qml"));
}

void Client::init()
{
    createSplashScreen();

    connect(_daemon, &DaemonConnection::socketConnected, this, [](qintptr socketFd)
        {
            _currentDaemonSocket.store(socketFd);
        });

    connect(_daemon, &DaemonConnection::connectedChanged, this, &Client::daemonConnectedChanged);
    // These errors happen normally if the daemon isn't up yet (which is common
    // at boot or after install), trace at warning level.
    connect(_daemon, &DaemonConnection::error, this, [this](const Error& error) { qWarning() << error.errorString(); });
    _daemon->connectToDaemon();
}

void Client::daemonConnectedChanged(bool connected)
{
    // If we are now connected, tell the daemon that this is an interactive
    // client connection
    if(connected)
    {
        // Can't be active or have an in-flight request, because this would have
        // been preceded by a change with connected=false which resets these
        Q_ASSERT(!_notifyActivateResult);
        Q_ASSERT(!_activated);

        qDebug () << "Sending activate notification";
        auto callResult = daemon()->call(QStringLiteral("notifyClientActivate"), {});
        // Use next() instead of notify() so we can abandon if needed.
        _notifyActivateResult = callResult->next(this,
            [this](const Error &error, const QJsonValue &)
            {
                _notifyActivateResult.reset();
                if(error)
                {
                    qWarning() << "Failed to activate:" << error;
                }
                else
                {
                    _activated = true;
                }

                // If this is the first time we have connected and activated,
                // load the main client UI.
                if(!_mainUiLoaded)
                {
                    qInfo() << "Client connected, loading UI";
                    _mainUiLoaded = true;

                    // If we need to migrate daemon-side client settings, do
                    // that before loading the client QML - some initial load
                    // code validates the settings, like for the module lists.
                    _clientInterface.migrateFromDaemon(_daemon->settings);
                    createMainWindow();
                }
            });

    }
    // If we lost the connection, discard any exit notification that occurred
    // on the prior connection.
    else
    {
        // This connection is gone
        _currentDaemonSocket.store(-1);
        _notifyActivateResult.abandon();
        _activated = false;
    }
}

DaemonInterface::DaemonInterface(DaemonConnection* daemonConnection, QObject* parent)
    : QObject(parent)
    , _daemonConnection(daemonConnection)
{
    connect(_daemonConnection, &DaemonConnection::connectedChanged, this, &DaemonInterface::connectedChanged);
    connect(_daemonConnection, &DaemonConnection::error, this, &DaemonInterface::error);
}
