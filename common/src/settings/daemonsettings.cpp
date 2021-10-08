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
#include "daemonsettings.h"
#include "brand.h"

DaemonSettings::DaemonSettings()
    : NativeJsonObject(SaveUnknownProperties)
{
}

const QString DaemonSettings::defaultReleaseChannelGA{QStringLiteral(BRAND_RELEASE_CHANNEL_GA)};
const QString DaemonSettings::defaultReleaseChannelBeta{QStringLiteral(BRAND_RELEASE_CHANNEL_BETA)};
const QStringList DaemonSettings::defaultDebugLogging
{
    QStringLiteral("*.debug=true"),
    QStringLiteral("qt*.debug=false"),
    QStringLiteral("qt*.info=false"),
    QStringLiteral("qt.scenegraph.general*=true"),
    QStringLiteral("qt.rhi.general*=true")
};

const std::unordered_set<QString> &DaemonSettings::settingsExcludedFromReset()
{
    static const std::unordered_set<QString> _settingsExcluded
    {
        // Last daemon version - not a setting
        QStringLiteral("lastUsedVersion"),
        // Location - not presented as a "setting"
        QStringLiteral("location"),
        // Help page settings are not reset, as they were most likely changed for
        // troubleshooting.
        QStringLiteral("debugLogging"),
        QStringLiteral("offerBetaUpdates"),
        // Persist Daemon - not presented as a "setting"
        QStringLiteral("persistDaemon"),
        // Rating / session count - not settings (don't show a rating request
        // again after resetting settings)
        QStringLiteral("ratingEnabled"),
        QStringLiteral("sessionCount"),
        // Last dismissed service message - not a setting (don't show it again)
        QStringLiteral("lastDismissedAppMessageId")
    };
    return _settingsExcluded;
}

QJsonValue DaemonSettings::getDefaultDebugLogging()
{
    QJsonValue value;
    json_cast(defaultDebugLogging, value);
    return value;
}

bool DaemonSettings::validateDNSSetting(const DaemonSettings::DNSSetting& setting)
{
    static const QStringList validDNSSettings {
        "pia",
        "handshake",
        "local",
    };
    static const QRegularExpression validIP(QStringLiteral("^(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)$"));
    QString value;
    if (setting.get(value))
        return value.isEmpty() || validDNSSettings.contains(value);
    QStringList servers;
    if (setting.get(servers))
    {
        if (servers.isEmpty() || servers.length() > 2)
            return false;
        for (const QString& server : servers)
        {
            auto match = validIP.match(server);
            if (!match.hasMatch())
                return false;
            for (int i = 1; i <= 4; i++)
            {
                bool ok;
                if (match.captured(i).toUInt(&ok) > 255 || !ok)
                    return false;
            }
        }
        return true;
    }
    return false;
}
