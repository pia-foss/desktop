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
#line SOURCE_FILE("linux/linux_language.cpp")

#include "linux_language.h"
#include <QProcessEnvironment>
#include <QString>
#include <QList>

namespace
{
    QList<QString> linuxLanguages;
}

void linuxLanguagePreAppInit()
{
    // Read the LANGUAGE environment variable
    //
    // This is a colon-delimited list of languages, see
    // https://www.gnu.org/software/gettext/manual/html_node/gettext_2.html#Locale-Environment-Variables
    // 'gettext' also supports other variables like LC_ALL, LC_MESSAGE, and
    // LANG, but LANGUAGE support seems to be pretty widespread so this is the
    // only one we're reading for now.
    const auto &procEnv = QProcessEnvironment::systemEnvironment();
    const auto &languageEnv = procEnv.value(QStringLiteral("LANGUAGE"));

    linuxLanguages = languageEnv.split(':', QString::SplitBehavior::SkipEmptyParts);

    // Wipe out the LANGUAGE variable.  Qt otherwise detects this and applies
    // very subtle tweaks for some reason.
    //
    // We don't want this at all; we want the same (correct!) behavior when you
    // select a given language regardless of the system language.
    //
    // Specifically, with LANGUAGE=ar, all drop-downs have incorrect padding at
    // one side (left in LTR, right in RTL), due to the drop down doing
    // something to try to right-align the indicator.  This happens regardless
    // of the app language selected.  It doesn't have anything to do with the
    // way we implement RTL mirroring, and it doesn't seem to happen on any
    // other platform.
    qunsetenv("LANGUAGE");
}

const QList<QString> &linuxGetDisplayLanguages()
{
    return linuxLanguages;
}
