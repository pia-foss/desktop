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
#line SOURCE_FILE("mac/mac_window.mm")

#include "mac_window.h"
#import <AppKit/AppKit.h>
#include <QWindow>
#include <QtMac>
#include <QSize>

namespace
{
    NSWindow *macGetNativeWindow(const QWindow &window)
    {
        // Casting from int to NSView * under ARC requires two steps - a cast
        // from int to *, then a bridge cast from a non-retainable pointer to a
        // retaininable pointer.
        void *pNativeViewVoid = reinterpret_cast<void*>(window.winId());
        // We don't hang on to the NSView *, so we can just bridge this cast
        // without retaining or releasing any references.
        // (The NSWindow * that we end up returning is handled normally by ARC,
        // the caller could hang on to it if they wanted to.)
        NSView *pNativeView = (__bridge NSView*)(pNativeViewVoid);
        return [pNativeView window];
    }
}

void macSetAllWorkspaces(QWindow &window)
{
    NSWindow *pNativeWindow = macGetNativeWindow(window);
    [pNativeWindow setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces];

    qInfo() << "window is opaque:" << (pNativeWindow.opaque);
    if(pNativeWindow.contentView)
    {
        qInfo() << "content view is opaque:" << (pNativeWindow.contentView.opaque);
    }
    else
    {
        qInfo() << "could not find content view";
    }
}

void enableShowInDock () {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

void disableShowInDock () {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
}

class MacWindowMetrics : public NativeWindowMetrics
{
public:
    virtual double calcScreenScaleFactor(const PlatformScreens::Screen &) const override
    {
        // No scaling on OS X.
        return 1.0;
    }
    virtual QMarginsF calcDecorationSize(const QWindow &window,
                                         double screenScale) const override;
};

std::unique_ptr<NativeWindowMetrics> macCreateWindowMetrics()
{
    return std::make_unique<MacWindowMetrics>();
}

CGFloat nsRectTop(const NSRect &rect)
{
    return rect.origin.y + rect.size.height;
}
CGFloat nsRectRight(const NSRect &rect)
{
    return rect.origin.x + rect.size.width;
}

QMarginsF MacWindowMetrics::calcDecorationSize(const QWindow &window,
                                               double screenScale) const
{
    // No window scaling is applied on OS X.
    Q_ASSERT(screenScale == 1.0);
    Q_UNUSED(screenScale);  // Unused when asserts are disabled.

    NSWindow *pNativeWindow = macGetNativeWindow(window);
    // If the window hasn't been created yet, return default empty margins.
    if(!pNativeWindow)
        return {};
    // Use an arbitrary content rectangle to compute the size.
    NSRect contentRect{{0, 0}, {300, 300}};
    auto frameRect = [pNativeWindow frameRectForContentRect:contentRect];

    QMarginsF decMargins;
    // Note that Cocoa rects start from the bottom-left and grow toward the
    // top-right, so the Y coordinates here are handled accordingly
    decMargins.setLeft(contentRect.origin.x - frameRect.origin.x);
    decMargins.setTop(nsRectTop(frameRect) - nsRectTop(contentRect));
    decMargins.setRight(nsRectRight(frameRect) - nsRectRight(contentRect));
    decMargins.setBottom(contentRect.origin.y - frameRect.origin.y);
    return decMargins;
}

void macCheckAppDeactivate()
{
    NSApplication *pApp = [NSApplication sharedApplication];
    if(!pApp)
        return;

    // We just hid one of the PIA client windows.  If no other window became
    // focused, hide the app to focus the next app.  (Otherwise, AppKit seems to
    // leave our app as active, but with no focused window, which is odd.)
    //
    // Normally, this means there are no visible windows, so NSApplication.hide
    // just focuses the next app (there's nothing to hide).
    NSWindow *pKeyWindow = pApp.keyWindow;
    if(!pKeyWindow && pApp.active)
    {
        qInfo() << "No window focused - deactivating app";
        [pApp hide:pApp];
        return;
    }

    qInfo() << "Focused window:" << QString::fromNSString(pKeyWindow.title);
}
