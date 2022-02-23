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

#include "common.h"
#line SOURCE_FILE("mac/mac_language.mm")

#include "mac_language.h"
#import <AppKit/AppKit.h>
#include <QString>
#include <QList>

QList<QString> macGetDisplayLanguages()
{
    QList<QString> languages;
    NSArray<NSString*> *osLangs = [NSLocale preferredLanguages];

    for(unsigned i=0; i<[osLangs count]; ++i)
    {
        languages.push_back(QString::fromNSString(osLangs[i]));
    }
    return languages;
}

QString macLocaleUpper(const QString &text, const QLocale &locale)
{
    NSLocale *macLocale = [NSLocale localeWithLocaleIdentifier:locale.bcp47Name().toNSString()];
    if(!macLocale)
    {
        qWarning() << "Can't create native locale for ID" << locale.bcp47Name();
        return text;
    }

    NSString *macOrig = text.toNSString();
    NSString *macUpper = nil;
    if(macOrig)
        macUpper = [macOrig uppercaseStringWithLocale:macLocale];
    if(!macUpper)
    {
        qWarning() << "Unable to upper case string" << text << "in locale" << locale.bcp47Name();
        return text;
    }

    return QString::fromNSString(macUpper);
}
