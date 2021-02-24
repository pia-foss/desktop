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
#line SOURCE_FILE("clientsettings.cpp")

#include "clientsettings.h"
#include "brand.h"

const std::initializer_list<QString> &ClientSettings::iconThemeValues()
{
    static const std::initializer_list<QString> values {
#if defined(Q_OS_MACOS)
        // Mac defaults to auto and has dark/light/colored/classic choices
        "auto", "light", "dark", "colored"
#elif defined(Q_OS_WIN)
        // Win just has light/colored/classic
        "light", "colored"
#elif defined(Q_OS_LINUX)
        // Linux has light/dark/colored/classic, but no 'auto'
        "light", "dark", "colored"
#else
    #error "Unknown platform - ClientSettings::iconSet"
#endif

    // The "classic" tray icon theme isn't provided by all brands
#if BRAND_HAS_CLASSIC_TRAY
            , "classic"
#endif
        };

    return values;
}

const QString &ClientSettings::iconThemeDefault()
{
    static const QString defTheme =
#if defined(Q_OS_MACOS)
        QStringLiteral("auto")
#else
        QStringLiteral("light")
#endif
        ;

    return defTheme;
}

const QString &ClientSettings::dashboardFrameDefault()
{
    static const QString defFrame =
#if defined(Q_OS_LINUX)
        QStringLiteral("window")
#else
        QStringLiteral("popup")
#endif
        ;
    return defFrame;
}

ClientSettings::ClientSettings()
    : NativeJsonObject{SaveUnknownProperties}
{
}

ClientState::ClientState()
    : NativeJsonObject{DiscardUnknownProperties}
{
}
