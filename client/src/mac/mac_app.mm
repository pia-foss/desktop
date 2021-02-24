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
#line SOURCE_FILE("mac/mac_app.mm")

#include "mac_app.h"
#include "client.h"
#import <AppKit/AppKit.h>

// App delegate - handle application-wide events
@interface PiaMacAppDelegate : NSObject<NSApplicationDelegate>
@end
@implementation PiaMacAppDelegate
- (BOOL) applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag
{
    Q_UNUSED(sender);
    Q_UNUSED(flag);
    // There's no hard guarantee that the Client has been created yet (though
    // it's likely)
    if(!Client::instance())
    {
        qWarning() << "Can't handle Mac reopen signal, Client hasn't been created yet";
    }
    else
    {
        qInfo() << "Opening dashboard from Mac reopen signal";
        Client::instance()->openDashboard();
    }

    return NO;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    Q_UNUSED(sender);

    qInfo() << "Exiting due to termination by OS";
    // Notify exit immediately in case clientMain() does not get a chance to
    // clean up, which may happen if the user is logging out
    Client::instance()->notifyExit();
    QCoreApplication::quit();

    return NSTerminateNow;
}
@end

// The NSApplication.delegate property is a weak property; keep the delegate
// alive by holding a reference to it here.
PiaMacAppDelegate *pAppDelegate = nullptr;

void macAppInit()
{
    // This has to occur before the QGuiApplication is created.
    // Qt sets its own app delegate too, but it chains to ours as long as we
    // install it first.  (This is by design, see
    // QCocoaApplicationDelegate.reflectionDelegate.)
    NSApplication *pApp = [NSApplication sharedApplication];
    Q_ASSERT(pApp);
    Q_ASSERT(!pApp.delegate);
    pAppDelegate = [PiaMacAppDelegate alloc];
    pApp.delegate = pAppDelegate;
}
