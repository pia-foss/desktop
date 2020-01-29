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

// TODO: Break out firewall handling to a base class that can be used directly
// by the base daemon class, for some common functionality.

class PFFirewall
{
    CLASS_LOGGING_CATEGORY("pf")

private:
    static int execute(const QString &command, bool ignoreErrors = false);
    static bool isPFEnabled();
    static bool isRootAnchorLoaded();

public:
    static void install();
    static void uninstall();
    static bool isInstalled();
    static void enableAnchor(const QString &anchor);
    static void disableAnchor(const QString &anchor);
    static bool isAnchorEnabled(const QString &anchor);
    static void setAnchorEnabled(const QString &anchor, bool enable);
    static void setAnchorTable(const QString &anchor, bool enabled, const QString &table, const QStringList &items);
    static void setAnchorWithRules(const QString &anchor, bool enabled, const QStringList &rules);
    static void ensureRootAnchorPriority();
};

#endif

#endif // POSIX_FIREWALL_PF_H
