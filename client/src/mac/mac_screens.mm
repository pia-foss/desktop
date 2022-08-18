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
#line SOURCE_FILE("mac_screens.mm")

#import <AppKit/AppKit.h>
#include "mac_screens.h"

class MacScreens;

@interface ScreenChangeObserver : NSObject
@property (nonatomic) MacScreens *parent;
 - (void) observeNotification:(NSNotification*)notification;
@end

// Mac implementation of PlatformScreens using NSScreen.  As discussed in
// PlatformScreens, the QScreen implementation on Mac is buggy and prone to
// crashes.
class MacScreens : public PlatformScreens
{
public:
    MacScreens();
    ~MacScreens();

private:
    // Get a QRect bound (with origin at top-left and Y down) from an NSRect
    // describing an NSScreen (with origin at bottom-left and Y up)
    QRect boundFromNsRect(double flipY, const NSRect &rect);
    std::vector<Screen> buildScreens();

public:
    // Used by ScreenChangeObserver and MacScreens only
    void rebuildScreens();

private:
    // ARC reference to screen change observer
    ScreenChangeObserver *_pObserver;
};

@implementation ScreenChangeObserver {}
- (void) observeNotification:(NSNotification*) __unused notification
{
    if(self.parent)
        self.parent->rebuildScreens();
}
@end

MacScreens::MacScreens()
{
    _pObserver = [ScreenChangeObserver alloc];
    _pObserver.parent = this;
    [[NSNotificationCenter defaultCenter]
        addObserver:_pObserver
        selector:@selector(observeNotification:)
        name:NSApplicationDidChangeScreenParametersNotification
        object:NSApplication.sharedApplication];

    rebuildScreens();
}

MacScreens::~MacScreens()
{
    [[NSNotificationCenter defaultCenter]
        removeObserver:_pObserver
        name:NSApplicationDidChangeScreenParametersNotification
        object:NSApplication.sharedApplication];
}

QRect MacScreens::boundFromNsRect(double flipY, const NSRect &rect)
{
    double macTop = rect.origin.y + rect.size.height;
    QRectF floatRect{rect.origin.x, flipY - macTop, rect.size.width, rect.size.height};
    return floatRect.toRect();
}

auto MacScreens::buildScreens() -> std::vector<Screen>
{
    NSArray<NSScreen*> *pAllScreens = NSScreen.screens;
    if(!pAllScreens)
    {
        qWarning() << "Unable to get screens from NSScreen";
        return {};
    }

    if(pAllScreens.count <= 0)
        return {};  // No screens

    std::vector<Screen> newScreens;
    newScreens.reserve(pAllScreens.count);

    // Get the primary screen, which is the first one (per doc, this is
    // different from NSScreen.main, which is actually the screen with the
    // keyboard focus)
    NSScreen *pPrimary = pAllScreens[0];
    // Flip Y coordinates by subtracting from this value - Mac places the origin
    // at the bottom-left of the primary with Y increasing up; flip to origin at
    // top-left with Y increasing down like other platforms.
    double flipY = pPrimary.frame.size.height;
    newScreens.push_back({true, boundFromNsRect(flipY, pPrimary.frame),
                          boundFromNsRect(flipY, pPrimary.visibleFrame)});

    for(unsigned i=1; i<pAllScreens.count; ++i)
    {
        newScreens.push_back({false, boundFromNsRect(flipY, pAllScreens[i].frame),
                              boundFromNsRect(flipY, pAllScreens[i].visibleFrame)});
    }

    return newScreens;
}

void MacScreens::rebuildScreens()
{
    updateScreens(buildScreens());
}

std::unique_ptr<PlatformScreens> createMacScreens()
{
    return std::unique_ptr<PlatformScreens>{new MacScreens{}};
}
