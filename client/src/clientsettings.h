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
#line HEADER_FILE("clientsettings.h")

#ifndef CLIENTSETTINGS_H
#define CLIENTSETTINGS_H

#include "json.h"
#include <QVector>

// Settings properties of the client.  Unlike DaemonSettings, these settings are
// client-specific, for various reasons:
// - They're per-user settings, like 'theme', 'favoriteLocations', 'recentLocations'
// - They're used before the daemon is loaded, like 'language'
//
// These are persisted in client.json.
//
// When adding settings here, consider whether the setting needs to be handled
// specially in ClientInterface::resetSettings().  By default, it reapplies the
// defaults listed here.
// - Some settings aren't really "settings" from the user's perspective and
//   shouldn't be reset (last used version, migration flag, recent locations).
// - Some settings have nontrivial "default" values and shouldn't actually use
//   the default values listed here (language)
class ClientSettings : public NativeJsonObject
{
    Q_OBJECT

public:
    static const std::initializer_list<QString> &iconThemeValues();
    static const QString &iconThemeDefault();
    static const QString &dashboardFrameDefault();

public:
    ClientSettings();

public:
    // Whether daemon-stored client settings have been migrated.
    // A few settings (theme, favoriteLocations, recentLocations) were stored
    // daemon-side before client settings were implemented.  Since this was
    // released in beta, we need to migrate those to client-side settings.
    //
    // If migrateDaemonSettings = false, then the client will migrate those
    // settings once it connects to the daemon, then it sets this setting to
    // true so they will not be migrated again.
    //
    // Also, since the default for this is true, the defaults for the
    // ClientSettings fields that are migrated don't really matter - their
    // defaults in DaemonSettings are used, since those values are migrated
    // over.
    JsonField(bool, migrateDaemonSettings, true)

    JsonField(bool, connectOnLaunch, false) // Connect when first client connects
    JsonField(bool, desktopNotifications, true) // Show desktop notifications

    // Theme name for the client
    JsonField(QString, themeName, QStringLiteral("dark"))
    // Region sort key for the client
    JsonField(QString, regionSortKey, QStringLiteral("latency"))
    // The user's favorite locations (chosen explicitly from the UI).
    JsonField(QVector<QString>, favoriteLocations, {})
    // The user's recently-used regions (set automatically by the client).
    // This is stored in order of use.  (Most recent is first.)
    JsonField(QStringList, recentLocations, {})

    JsonField(QVector<QString>, primaryModules, QVector<QString>::fromList({QStringLiteral("region"), QStringLiteral("ip")}))
    JsonField(QVector<QString>, secondaryModules, QVector<QString>::fromList({QStringLiteral("quickconnect"), QStringLiteral("performance"), QStringLiteral("usage"), QStringLiteral("settings"), QStringLiteral("account")}))
    JsonField(QString, iconSet, ::ClientSettings::iconThemeDefault(), ::ClientSettings::iconThemeValues())

    // Client's chosen language.
    // This is really an entire locale, because it also controls
    // date/time/number presentation.
    // Initially this defaults to the empty string; the client finds the system
    // language to initially set this.
    JsonField(QString, language, QStringLiteral(""))

    // Dashboard frame style - windowed or popup
    JsonField(QString, dashboardFrame, ::ClientSettings::dashboardFrameDefault(), {"window", "popup"})

    // Store the last version the client remembers running as.
    JsonField(QString, lastUsedVersion, QStringLiteral(""))

    JsonField(bool, disableHardwareGraphics, false)

    // The amount of time to snooze, this setting stores the value
    // last entered by the user using the Snooze module.
    // Value is number of seconds. Default is 5 min
    JsonField(int, snoozeDuration, 300);
};

// A language (potentially) supported by the client
class ClientLanguage : public NativeJsonObject
{
    Q_OBJECT

public:
    ClientLanguage() {}
    ClientLanguage(const ClientLanguage &other) {*this = other;}
    ClientLanguage &operator=(const ClientLanguage &other)
    {
        locale(other.locale());
        displayName(other.displayName());
        available(other.available());
        rtlMirror(other.rtlMirror());
        return *this;
    }

public:
    // The locale associated with the language - the value that would be used in
    // the 'language' field of ClientSettings
    JsonField(QString, locale, {})
    // The language's display name.  Normally it is the native name (English,
    // Deutsch, Italiano, etc.), but if no fonts are available for this
    // language's script, it will be the English name.
    JsonField(QString, displayName, {})
    // Whether the language can be selected (languages that haven't been
    // translated yet can't actually be chosen)
    JsonField(bool, available, false)
    // Whether the language mirrors for RTL
    JsonField(bool, rtlMirror, false)

public:
    bool operator==(const ClientLanguage &other) const
    {
        return locale() == other.locale() &&
            displayName() == other.displayName() &&
            available() == other.available() &&
            rtlMirror() == other.rtlMirror();
    }
    bool operator!=(const ClientLanguage &other) const {return !(*this==other);}
};

class SystemApplication : public NativeJsonObject
{
    Q_OBJECT
public:
    SystemApplication() {}
    SystemApplication(QString _path, QString _name, QStringList _folders)
    {
        name(std::move(_name));
        path(std::move(_path));
        folders(std::move(_folders));
    }
    SystemApplication(const SystemApplication &other) {*this = other;}

    SystemApplication &operator=(const SystemApplication &other)
    {
        name(other.name());
        path(other.path());
        folders(other.folders());
        includedApps(other.includedApps());
        return *this;
    }

public:
    // The name of the application
    JsonField(QString, name, {});
    // Path to the file represented by this application.  This is platform
    // specific:
    // - Windows: Absolute path to a Start Menu shell link
    // - Mac: Absolute path to an app bundle
    JsonField(QString, path, {});
    // Folder groupings used to sort the list of applications.  Empty if the
    // item is in the root of the Start Menu / Applications folder; otherwise
    // the list of folder display names.
    JsonField(QStringList, folders, {});
    // Some special entries include other apps - those apps' display names are
    // specified in includedApps
    JsonField(QStringList, includedApps, {});

public:
    bool operator==(const SystemApplication &other) const
    {
        return name() == other.name() && path() == other.path() &&
            folders() == other.folders() && includedApps() == other.includedApps();
    }
    bool operator!=(const SystemApplication &other) const {return !(*this==other);}


};

// State properties of the client.  This is data provided by the native client
// code that can't be directly manipulated, and these aren't persisted.
class ClientState : public NativeJsonObject
{
    Q_OBJECT

public:
    ClientState();

public:
    // Languages presented as choices in the UI.  Never changes after startup.
    JsonField(QVector<ClientLanguage>, languages, {})
    // The language currently in use.  Might differ from the 'language' setting
    // if that setting isn't a valid language.
    JsonField(ClientLanguage, activeLanguage, {})

    JsonField(bool, clientHasBeenUpdated, false)
    JsonField(bool, firstRunFlag, false)
    JsonField(bool, quietLaunch, false)
    // Whether safe graphics are currently in use right now.  (This only changes
    // after a restart, so it may be different from the disableHardwareGraphics
    // setting.)  This only applies on Windows/Linux.
    JsonField(bool, usingSafeGraphics, false)
    // Whether the client has been UAC-elevated on Windows.  This triggers a
    // warning in the UI.
    JsonField(bool, winIsElevated, false)
};

#endif
