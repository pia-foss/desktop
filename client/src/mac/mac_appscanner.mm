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
#line SOURCE_FILE("mac_appscanner.mm")

#include "mac_appscanner.h"
#include "mac/mac_constants.h"
#include "path.h"
#import <AppKit/AppKit.h>
#include <QtMac>
#include <QFileInfo>
#include <QDirIterator>
#include <unordered_set>

namespace
{
    // A number of Mac apps all use a shared WebKit framework daemon to do
    // their network requests (they seem to use local XPC to communicate with
    // that daemon).
    //
    // This means we can't exclude these apps individuall - excluding the app
    // bundle itself has no effect; excluding the WebKit framework excludes all
    // of them.
    //
    // We display a catch-all "WebKit apps" entry so users can still exclude
    // them as a group and hide each of their individual entries.
    const QString safariPath{QStringLiteral("/Applications/Safari.app")};
    const QString webkitAppsDisplay{QStringLiteral("WebKit applications")};
}

QString getMacAppName(const QString &path)
{
    if(path == webkitFrameworkPath)
        return webkitAppsDisplay;

    QFileInfo fi(path);
    return fi.fileName().replace(QStringLiteral(".app"), QStringLiteral(""));
}

QPixmap getIconForAppBundle(const QString &path, const QSize &size)
{
    NSImage *img = [[NSWorkspace sharedWorkspace] iconForFile:path.toNSString()];

    const int width = size.width() > 0 ? size.width() : 40;
    const int height = size.height () > 0 ? size.height () : 40;

    NSBitmapImageRep * bmp = [[NSBitmapImageRep alloc]
          initWithBitmapDataPlanes:NULL
          pixelsWide:width
          pixelsHigh:height
          bitsPerSample:8
          samplesPerPixel:4
          hasAlpha:YES
          isPlanar:NO
          colorSpaceName:NSDeviceRGBColorSpace
          bitmapFormat:NSAlphaFirstBitmapFormat
          bytesPerRow:0
          bitsPerPixel:0
          ];
    [NSGraphicsContext saveGraphicsState];

    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:bmp]];

    // assume NSImage nsimage
    [img drawInRect:NSMakeRect(0,0,width,height) fromRect:NSZeroRect operation: NSCompositeSourceOver fraction: 1];

    [NSGraphicsContext restoreGraphicsState];

    QPixmap qpixmap = QtMac::fromCGImageRef([bmp CGImage]);

    return qpixmap;
}

QPixmap MacAppIconProvider::requestPixmap(const QString &id, QSize *, const QSize &requestedSize)
{
    // Qt doesn't decode the id after extracting it from the URI
    QString path = QUrl::fromPercentEncoding(id.toUtf8());
    // Use Safari's icon for the "WebKit Apps" entry
    if(path == webkitFrameworkPath)
        return getIconForAppBundle(safariPath, requestedSize);
    return getIconForAppBundle(path, requestedSize);
}

MacAppScanner::MacAppScanner()
{

}

void MacAppScanner::scanApplications()
{
  qWarning () << "Tried to scan applications on mac. This isn't supported";
}
