// Copyright (c) 2024 Private Internet Access, Inc.
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
#line SOURCE_FILE("win/win_language.cpp")

#include "win_language.h"
#include "win_objects.h"
#include <common/src/win/win_util.h>
#include <QString>
#include <QList>
#include <kapps_core/src/winapi.h>

QList<QString> winGetDisplayLanguages()
{
    ULONG langBufSize = 0;
    if(!::GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME|MUI_MERGE_USER_FALLBACK,
                                        nullptr, nullptr, &langBufSize))
    {
        qInfo() << "Can't get UI language list length:" << ::GetLastError();
        return {};
    }

    QVector<WCHAR> langBuf;
    // QVector uses signed sizes for some reason
    langBuf.resize(static_cast<int>(langBufSize));
    // Get the list of UI languages.
    // Note that although this returns a list, it does _not_ correspond to the
    // Windows 10 preferred language list, it corresponds to the Windows display
    // language setting.
    // (The preferred language list is probably specific to UWP, it does not
    // appear to be usable from Win32.)
    if(!::GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME|MUI_MERGE_USER_FALLBACK,
                                        nullptr, langBuf.data(), &langBufSize))
    {
        qInfo() << "Can't get UI language list:" << ::GetLastError();
        return {};
    }
    langBuf.resize(static_cast<int>(langBufSize));

    // Convert the null-delimited (and double-null-terminated) language list to
    // a QVector<QString>
    QList<QString> languages;
    int startPos = 0;
    while(langBuf.size() > startPos && langBuf[startPos])
    {
        int endPos = startPos;
        // Find the null terminator (or the end of the string if it is somehow
        // missing)
        while(langBuf.size() > endPos && langBuf[endPos])
            ++endPos;

        languages.push_back(QString::fromWCharArray(&langBuf[startPos], endPos-startPos));

        // Advance startPos to the start of the next string, or to the second
        // null terminator if this was the last one
        startPos = endPos + 1;
    }

    return languages;
}

QString winLocaleUpper(const QString &text, const QLocale &locale)
{
    // Empty string case; required because 0 is interpreted as an error result
    // from LCMapStringEx().
    // Assumes that a non-empty string can never map to an empty string...which
    // is probably not true in general, but should be true for the fixed strings
    // that this is used for.  (There's no way to differentiate an error result
    // from LCMapStringEx() in that case.)
    if(text.isEmpty())
        return {};

    const auto &localeName = locale.bcp47Name();

    // Determine the required buffer size
    int resultSize = ::LCMapStringEx(qstringWBuf(localeName),
                                     LCMAP_UPPERCASE|LCMAP_LINGUISTIC_CASING,
                                     qstringWBuf(text), text.size(),
                                     nullptr, 0, nullptr, nullptr, 0);

    std::vector<WCHAR> outBuf;
    outBuf.resize(resultSize);
    // Convert the string into that buffer
    resultSize = ::LCMapStringEx(qstringWBuf(localeName),
                                 LCMAP_UPPERCASE|LCMAP_LINGUISTIC_CASING,
                                 qstringWBuf(text), text.size(),
                                 outBuf.data(), outBuf.size(), nullptr, nullptr,
                                 0);

    if(resultSize <= 0)
    {
        qWarning() << "Unable to upper case" << text << "in locale"
            << localeName << "ret:" << resultSize << "err:" << ::GetLastError();
        return text;
    }

    auto result = QString::fromWCharArray(outBuf.data(), resultSize);
    return result;
}
