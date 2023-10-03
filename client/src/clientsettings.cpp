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
#include "clientsettings.h"
#include "brand.h"
#include <kapps_core/src/corejson.h>
#include <nlohmann/json.hpp>

const std::initializer_list<QString> &ClientSettings::iconThemeValues()
{
    static const std::initializer_list<QString> values {
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
        "auto", "light", "dark", "colored"
        // Windows and macOS default to auto (follow system theme) and have
        // dark/light/colored/classic choices
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
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
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
    : NativeJsonObject{DiscardUnknownProperties},
      _activeLanguageTag{"en", {}, "US"}
{
    connect(this, &ClientState::activeLanguageChanged, this,
        &ClientState::updateActiveLanguageTag);
    updateActiveLanguageTag();
}

void ClientState::updateActiveLanguageTag()
{
    try
    {
        _activeLanguageTag = kapps::regions::Bcp47Tag{activeLanguage().locale().toStdString()};
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Current language" << activeLanguage().locale()
            << "is not a valid BCP-47 tag, defaulting to en-US";
        _activeLanguageTag = kapps::regions::Bcp47Tag{"en", {}, "US"};
    }
}

QString ClientState::getTranslatedName(const QString &id,
    const std::unordered_map<QString, kapps::regions::DisplayText> &names) const
{
    auto itName = names.find(id);
    if(itName == names.end())
    {
        // This region/country isn't known - this isn't great, but displaying
        // the ID is better than an empty string at this point.
        // Of course, the best solution to this is for all regions and countries
        // to have metadata.  Otherwise, regions/countries lacking metadata need
        // to be filtered out earlier before we get to this point.
        return id;
    }

    return qs::toQString(itName->second.getLanguageText(_activeLanguageTag));
}

void ClientState::setRegionsMetadata(const QJsonObject &metadata)
{
    // Tolerate an empty metadata object since we get that initially before the
    // daemon connection is up
    if(metadata.isEmpty())
    {
        _regionNames.clear();
        _countryNames.clear();
        _countryPrefixes.clear();
        return;
    }

    try
    {
        auto j = nlohmann::json::parse(QJsonDocument{metadata}.toJson());
        std::unordered_map<QString, kapps::regions::DisplayText> regionNames;
        std::unordered_map<QString, kapps::regions::DisplayText> countryNames;
        std::unordered_map<QString, kapps::regions::DisplayText> countryPrefixes;

        for(const auto &[id, region] : kapps::core::jsonObject(j.at("regionDisplays")).items())
        {
            regionNames.emplace(qs::toQString(id),
                region.at("name").get<kapps::regions::DisplayText>());
        }
        for(const auto &[code, country] : kapps::core::jsonObject(j.at("countryDisplays")).items())
        {
            countryNames.emplace(qs::toQString(code),
                country.at("name").get<kapps::regions::DisplayText>());

            countryPrefixes.emplace(qs::toQString(code),
                country.at("prefix").get<kapps::regions::DisplayText>());
        }

        _regionNames = std::move(regionNames);
        _countryNames = std::move(countryNames);
        _countryPrefixes = std::move(countryPrefixes);
    }
    catch(const std::exception &ex)
    {
        KAPPS_CORE_WARNING() << "Can't interpret regions metadata from daemon:"
            << ex.what();
        // Keep whatever we had before
    }
}
