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
#line HEADER_FILE("posix/posix_firewall_pf.h")

#ifndef POSIX_FIREWALL_PF_H
#define POSIX_FIREWALL_PF_H
#pragma once

#ifdef Q_OS_MACOS

#include <QString>
#include <QStringList>

class PFFirewall
{
    CLASS_LOGGING_CATEGORY("pf")

private:
    using MacroPairs = QHash<QString, QString>;
private:
    static int execute(const QString &command, bool ignoreErrors = false);
    static bool isPFEnabled();
    static void installRootAnchors();
    // Test if a specific type of root anchor is loaded and non-empty
    static bool isRootAnchorLoaded(const QString &modifier);
    static bool areAllRootAnchorsLoaded();
    static bool areAnyRootAnchorsLoaded();
    static QString getMacroArgs(const MacroPairs& macroPairs);

public:
    static void install();
    static void uninstall();
    static bool isInstalled();
    static void enableAnchor(const QString &anchor, const QString &modifier, const MacroPairs &macroPairs);
    static void disableAnchor(const QString &anchor, const QString &modifier);
    static void setAnchorEnabled(const QString &anchor, const QString &modifier, bool enable, const MacroPairs &macroPairs);
    static void setAnchorTable(const QString &anchor, bool enabled, const QString &table, const QStringList &items);

    // Manipulate anchors containing filter rules
    static void setFilterEnabled(const QString &anchor, bool enable, const MacroPairs &macroPairs={});
    static void setFilterWithRules(const QString &anchor, bool enabled, const QStringList &rules);

    // Manipulate anchors containing translation rules
    static void setTranslationEnabled(const QString &anchor, bool enable);

    static void ensureRootAnchorPriority();

    // Update the DNS stub used when blocking DNS
    static void setMacDnsStubMethod(const QString &macDnsStubMethod);
    static bool setDnsStubEnabled(bool enabled);
};

#endif

#endif // POSIX_FIREWALL_PF_H
