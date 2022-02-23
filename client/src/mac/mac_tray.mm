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
#line SOURCE_FILE("mac/mac_tray.mm")

#include "mac_tray.h"
#include "client.h"
#include "platformuistrings.h"
#include "brand.h"
#include "product.h"

#include <QFile>
#include <QHash>
#include <QJsonValue>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QAccessibleActionInterface>
#include <QRectF>
#include <QtMac>

#import <AppKit/AppKit.h>

#include <objc/runtime.h>
#include <cmath>

// A few deprecated symbols are used in this file to be buildable with OS X 10.10.
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

@class NTMActionTarget;
@class NTMStatusBarButton;
@class NTMNotificationDelegate;
@class NTMMenuDelegate;

// NativeTrayMac relies on ARC to manage the lifetimes of the Objective-C
// objects.
static_assert(__has_feature(objc_arc), "NativeTrayMac requires Objective-C ARC");

class NativeTrayMac : public NativeTray
{
private:
    // This destructor just nulls all members.  The public constructor delegates
    // to this one for exception safety.
    NativeTrayMac();

public:
    NativeTrayMac(IconState initialIcon, const QString &initialIconSet);

    // We can't be sure exactly when the NSStatusBarButton and friends will be
    // destroyed, so the NTMActionTarget might outlive the NativeTrayMac.  This
    // destructor is needed to clear its parent property.
    ~NativeTrayMac();

private:
    NSMenu* createMenu(const NativeMenuItem::List& items);
    QRect getScreenBound() const;

public:
    virtual void setIconState(IconState icon, const QString &iconSet) override;
    virtual void showNotification(IconState icon, const QString &title,
                                  const QString &subtitle) override;
    virtual void hideNotification() override;
    virtual void setToolTip(const QString &toolTip) override;
    virtual void setMenuItems(const NativeMenuItem::List& items) override;
    virtual void getIconBound(QRect &iconBound, qreal &screenScale) override;

    // For use by NTMActionTarget only - forwards the clicked action
    // from the NSStatusBarButton.
    void trayClicked();

    // For use by NTMStatusBarButton only - shows the menu for a
    // rightMouseDown event or a mouseDown event with the Ctrl key pressed.
    void trayShowMenu();

    // For use by NTMActionTarget only - forwards a menu item action.
    void menuItemAction(const QString &code);

private:
    NSUserNotificationCenter *_pNotificationCenter;
    // We don't actually care about these delegate references, but we have to
    // store them since the respective 'delegate' properties do not retain refs.
    NTMNotificationDelegate *_pNotificationDelegate;
    NTMMenuDelegate *_pMenuDelegate;
    // Action target stored in the NSStatusBarButton.  When its trayClicked is
    // called, it calls NativeTrayMac::trayClicked().
    NTMActionTarget *_pActionTarget;
    NSStatusBar *_pStatusBar;
    NSStatusItem *_pStatusItem;
    // This is the button embedded into the status bar item, which displays the
    // icon and does most of the cursor interaction work.
    NTMStatusBarButton *_pButton;
    // Pull-down menu shown when right-clicking (or control-clicking) the tray
    // icon.
    NSMenu *_pTrayMenu = nullptr;
    // Menu icon cache.
    QHash<QString, NSImage*> _icons;
};

std::unique_ptr<NativeTray> createNativeTrayMac(NativeTray::IconState initialIcon, const QString &initialIconSet)
{
    return std::unique_ptr<NativeTray>{new NativeTrayMac{initialIcon, initialIconSet}};
}

// Menu item that stores a NativeTray::MenuItem value indicating which one it
// is.
// For whatever reason, it seems that the NSMenuItems in the pop-up menu must
// all use the same target object.  I could not find any documentation on this,
// but all menu items were non-functional when attempting to use different
// targets.
// For this reason, we have to store the menu item type in the menu items
// themselves, we can't use regular NSMenuItems with per-item targets that
// know the menu item types.
@interface NTMMenuItem : NSMenuItem
@property (nonatomic) QString code;
@end

@implementation NTMMenuItem {}
@end

// Action target used to hook up the NSStatusBarButton's click action and the
// NSMenuItem's actions to NativeTrayMac.
@interface NTMActionTarget : NSObject
@property (nonatomic) NativeTrayMac *parent;
 - (void) trayClicked:(id)sender;
 - (void) menuItemAction:(id)sender;
@end

@implementation NTMActionTarget {}

- (void) trayClicked:(id) __unused sender
{
    if(self.parent)
        self.parent->trayClicked();
}

- (void) menuItemAction:(id) sender
{
    NTMMenuItem *menuItem = (NTMMenuItem*)sender;
    if(self.parent)
        self.parent->menuItemAction(menuItem.code);
}

@end

// Delegate used to implement click-to-dismiss on notifications.
@interface NTMNotificationDelegate : NSObject <NSUserNotificationCenterDelegate>
@end

@implementation NTMNotificationDelegate {}
- (void) userNotificationCenter:(NSUserNotificationCenter*)center
        didActivateNotification:(NSUserNotification*)notification
{
    if(center && notification)
        [center removeDeliveredNotification:notification];
}
@end

// This is the derived NSStatusBarButton used by NativeTrayMac.
// We have to derive from NSStatusBarButton to intercept some mouse events and
// override drawRect.
// For simplicity, since NTMActionTarget already has a parent pointer, we keep
// an Objective-C reference to it and use its parent pointer to forward these
// events.
@interface NTMStatusBarButton : NSStatusBarButton
@property (nonatomic) NTMActionTarget *buttonTarget;
// The status item created by NativeTrayMac - needed to draw the background
@property (nonatomic) NSStatusItem *statusItem;
- (void)initCustomActions;
@end

@implementation NTMStatusBarButton
{
    // We provide a custom accessibility action for the show-menu action.
    //
    // Mac tray icons don't normally have different left- and right-click
    // behavior, but ours does, so it's sensible that this has to be expressed
    // as a custom action.
    //
    // (The non-custom accessibilityPeformShowMenu selector doesn't do anything,
    // it seems the actual tray icon elements are proxies provided by the OS,
    // and they do not proxy this action.)
    //
    // However, NSAccessibilityCustomAction (and the accessibilityCustomActions
    // property) are only available on 10.13 or later.  To ensure the app still
    // runs (and builds) on 10.11-10.12, we probe for
    // NSAccessibilityCustomAction at runtime, and this
    // NSArray<NSAccessibilityCustomAction*> is statically typed as an
    // NSArray<id>
    NSArray<id> *customActions;
}

- (void)initCustomActions
{
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300
    if(@available(macOS 10.13, *))
    {
        // Get a localized action name
        QString showMenuName = PlatformUIStrings::macTrayAccShowMenu();

        NSAccessibilityCustomAction *showMenuAction = nil;
        showMenuAction = [[NSAccessibilityCustomAction alloc]
            initWithName:showMenuName.toNSString()
            target:self
            selector:@selector(accShowMenu:)];

        customActions = @[showMenuAction];
    }
#endif
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];

    [self initCustomActions];

    return self;
}

- (void) callParentShowMenu
{
    if(self.buttonTarget && self.buttonTarget.parent)
        self.buttonTarget.parent->trayShowMenu();
}

- (NSArray<id>*) accessibilityCustomActions
{
    if(@available(macOS 10.13, *))
    {
        NSArray<NSAccessibilityCustomAction*> *baseActions = [super accessibilityCustomActions];
        if(baseActions && customActions)
            return [baseActions arrayByAddingObjectsFromArray:customActions];
    }

    return customActions;
}

- (void) mouseDown:(NSEvent*) theEvent
{
    // If the Ctrl key is down, show the menu instead of clicking the button
    if(theEvent.modifierFlags & NSControlKeyMask)
    {
        [self callParentShowMenu];
    }
    else
    {
        // Otherwise, forward to the normal button implementation.
        [super mouseDown:theEvent];
    }
}

- (void) rightMouseDown:(NSEvent*) __unused theEvent
{
    [self callParentShowMenu];
}

- (void) drawRect:(NSRect)dirtyRect
{
    if(self.statusItem)
        [self.statusItem drawStatusBarBackgroundInRect:dirtyRect withHighlight:self.highlighted];
    [super drawRect:dirtyRect];
}

- (void) accShowMenu:(id) __unused sender
{
    [self callParentShowMenu];
}

@end

// This delegate is used to highlight the icon while the menu is shown.
@interface NTMMenuDelegate : NSObject <NSMenuDelegate>
@property (nonatomic) NSStatusBarButton *button;
@end

@implementation NTMMenuDelegate {}

- (void) menuWillOpen:(NSMenu*) __unused menu
{
    if(self.button)
        [self.button highlight:YES];
}

- (void) menuDidClose:(NSMenu*) __unused menu
{
    if(self.button)
        [self.button highlight:NO];
}
@end

namespace
{
    NSImage *loadNativeImage(const QString &imagePath, bool templateImg)
    {
        // Open the resource file and read it into a QByteArray.
        QFile imageRes{imagePath};
        if(!imageRes.open(QIODevice::OpenModeFlag::ReadOnly))
            return nullptr;
        // Read the data, put it in a QByteArray, then get an NSData from there.
        QByteArray qtImageData{imageRes.readAll()};
        imageRes.close();
        NSData *pImageData = qtImageData.toNSData();

        // Create an NSImage using the file data.
        NSImage *pNativeImage = [[NSImage alloc] initWithData:pImageData];

        // Set the template image flag, which allows the OS to colorize the icon
        // (used for the classic Mac icons, which are black + alpha only).
        [pNativeImage setTemplate:templateImg];

        // Set the image's size to the desired size, which leaves a small margin
        // at the top and bottom of the status bar.
        int statusBarHeight = [[NSStatusBar systemStatusBar] thickness];
        // This is the typical margin used by system icons (3.0 on each side)
        CGFloat iconHeight = statusBarHeight - 6.0;
        // Scale uniformly to this size
        qInfo() << "Desired height:" << iconHeight << "- actual size:" << pNativeImage.size.width << "x" << pNativeImage.size.height;
        CGFloat iconWidth = pNativeImage.size.width * iconHeight / pNativeImage.size.height;
        pNativeImage.size = NSSize{iconWidth, iconHeight};
        return pNativeImage;
    }

    bool isMacDarkModeEnabled()
    {
        QProcess defaultsProcess;
        // Note that we can't use QProcess::execute() here, because it forwards
        // the child process's stdout to our own stdout - we would not be able
        // to read it.
        defaultsProcess.start(QStringLiteral("defaults read -g AppleInterfaceStyle"),
                              QProcess::ReadOnly);
        int exitCode = waitForExitCode(defaultsProcess);
        return (exitCode == 0 && defaultsProcess.readAllStandardOutput().trimmed() == QByteArrayLiteral("Dark"));
    }



    NSImage *loadIconForState(NativeTray::IconState icon, const QString &iconSet)
    {
        bool templateImg = false;
        QString baseName;
        // Make sure the theme name is sane; ensures a sane fallback to 'auto'
        // if an invalid theme name is present in settings
        if(iconSet == QStringLiteral("light") || iconSet == QStringLiteral("dark") ||
            iconSet == QStringLiteral("colored"))
        {
            baseName = iconSet;
        }
        else if(BRAND_HAS_CLASSIC_TRAY && iconSet == QStringLiteral("classic"))
        {
            baseName = iconSet;
            templateImg = true;
        }
        else { // "auto" or fallback
            baseName = QStringLiteral("monochrome");
            templateImg = true;
        }

        QString stateName;
        switch(icon)
        {
        case NativeTray::IconState::Alert:
            stateName = QStringLiteral("alert");
            break;
        case NativeTray::IconState::Disconnected:
            stateName = QStringLiteral("down");
            break;
        case NativeTray::IconState::Connected:
            stateName = QStringLiteral("connected");
            break;
        case NativeTray::IconState::Disconnecting:
            stateName = QStringLiteral("disconnecting");
            break;
        case NativeTray::IconState::Connecting:
            stateName = QStringLiteral("connecting");
            break;
        case NativeTray::IconState::Snoozed:
            stateName = QStringLiteral("snoozed");
            break;
        default:
            // Impossible, handled all states (ensures valid stateName below)
            Q_ASSERT(false);
            break;
        }

        QString resource = QStringLiteral(":/img/tray/wide-%1-%2.png").arg(baseName).arg(stateName);
        // Should always end up with a valid file name here - consequence of
        // the checks above on the theme and state, and all these resources
        // must exist
        Q_ASSERT(QFile::exists(resource));

        return loadNativeImage(resource, templateImg);
    }

    // Map an NSPoint from native coordinates to Qt coordinates.
    // OS X uses a different coordinate system than Qt.  OS X places the origin
    // at the *bottom*-left corner of the primary monitor, and Y increases up.
    // Qt places the origin at the *top*-left corner of the primary monitor, and
    // Y increases down.
    QPointF pointNativeToQt(const NSPoint &nativePoint)
    {
        // To convert these coordinates, we need to find the primary screen's
        // height.  The first entry in NSScreen.screens is the primary screen.
        // (This is specifically documented -
        // https://developer.apple.com/documentation/appkit/nsscreen/1388393-screens?language=objc)
        NSArray<NSScreen*> *screens = NSScreen.screens;
        // It might be possible to have 0 screens if all monitors were
        // disconnected.  This is only used in response to click events, so it's
        // probably not possible to actually receive a click event in this case,
        // so return the point unmodified.
        if(screens.count < 1)
        {
            qWarning() << "No screens found; cannot map point:"
                << nativePoint.x << "," << nativePoint.y;
            return {nativePoint.x, nativePoint.y};
        }
        CGFloat mainMonitorHeight = screens[0].frame.size.height;
        CGFloat mappedY = mainMonitorHeight - nativePoint.y;
        return {nativePoint.x, mappedY};
    }

    // Map an NSRect from native coordinates to Qt coordinates.
    // This just maps the origin using pointNativeToQt, then offsets by the
    // rect's height to move that origin to the rect's top-left corner from the
    // bottom.
    QRectF rectNativeToQt(const NSRect &nativeRect)
    {
        QPointF mappedPoint{pointNativeToQt(nativeRect.origin)};
        return {mappedPoint.x(), mappedPoint.y() - nativeRect.size.height,
                nativeRect.size.width, nativeRect.size.height};
    }
}

QString getMacAutoIcon () {
    if(isMacDarkModeEnabled()) {
      return QStringLiteral("qrc:/img/tray/mac/light-connected.png");
    } else {
      return QStringLiteral("qrc:/img/tray/mac/dark-connected.png");
    }
}

NativeTrayMac::NativeTrayMac()
    : _pActionTarget{}, _pStatusItem{}, _pButton{}
{
}

// The public constructor delegates to the private constructor above because
// our destructor does essential cleanup that still needs to happen even if
// something throws during this constructor.
// The parent-ownership relationship could probably be factored out into an RAII
// object, but it gets pretty convoluted pretty quickly since it's trying to
// rectify two very different object models.
NativeTrayMac::NativeTrayMac(NativeTray::IconState initialIcon, const QString &initialIconSet)
    : NativeTrayMac{}
{
    _pNotificationCenter = NSUserNotificationCenter.defaultUserNotificationCenter;
    _pNotificationDelegate = [NTMNotificationDelegate alloc];
    _pNotificationCenter.delegate = _pNotificationDelegate;
    // Create the action target and set the parent.
    _pActionTarget = [NTMActionTarget alloc];
    _pActionTarget.parent = this;

    // Get the status bar
    _pStatusBar = [NSStatusBar systemStatusBar];

    // Create a status item
    _pStatusItem = [_pStatusBar statusItemWithLength:NSSquareStatusItemLength];

    // Create a status bar button
    NSImage *pInitialIcon = loadIconForState(initialIcon, initialIconSet);
    // This is the default frame that would have been used by
    // NSButton.buttonWithImage:target:action:.  It doesn't really matter since
    // the button will be bounded by the status item.
    NSRect frame = {{0, 0}, {40, 32}};
    _pButton = [[NTMStatusBarButton alloc] initWithFrame:frame];
    [_pButton setButtonType:NSMomentaryPushButton];
    _pButton.image = pInitialIcon;
    _pButton.imagePosition = NSImageOnly;
    _pButton.buttonTarget = _pActionTarget;
    _pButton.target = _pActionTarget;
    _pButton.action = @selector(trayClicked:);
    _pButton.statusItem = _pStatusItem;
    _pButton.imageScaling = NSScaleNone;
    _pButton.bordered = NO;
    // The button would normally indicate a 'press' by graying out the icon
    // (only visible for us on the colored part of the 'connected' icon).
    // Turn this off since we want the normal menu bar behavior of changing the
    // background color.
    NSButtonCell *pButtonCell = (NSButtonCell*)_pButton.cell;
    pButtonCell.highlightsBy = NSNoCellMask;

    // Put the status bar button into the status item.
    // Note that we are using the deprecated view property, not the newer button
    // and menu properties, because it is not possible to conditionally show a
    // pull-down menu with the newer properties.
    //
    // If a menu set into the menu property, it is always shown for left-clicks;
    // there would be no way to show the dashboard directly for left-clicks.
    //
    // The deprecated popUpMenu() method is the only way to conditionally show
    // the menu, and this method requires the status item to have a view.
    _pStatusItem.view = _pButton;
    _pStatusItem.highlightMode = YES;

    // Set up the menu delegate to customize the top level menu.
    _pMenuDelegate = [NTMMenuDelegate alloc];
    _pMenuDelegate.button = _pButton;

    // Update the localized text in the status bar button's accessibility
    // annotations when the app retranslates
    QObject::connect(Client::instance(), &Client::retranslate, this,
        [this]()
        {
            if(_pButton)
                [_pButton initCustomActions];
        });

}

NSMenu* NativeTrayMac::createMenu(const NativeMenuItem::List& items)
{
    NSMenu *pMenu = [[NSMenu alloc] initWithTitle:@""];
    pMenu.autoenablesItems = false;
    for (auto& item : items)
    {
        if (item->separator())
        {
            [pMenu addItem:[NSMenuItem separatorItem]];
            continue;
        }
        NTMMenuItem *pMenuItem = [[NTMMenuItem alloc] initWithTitle:item->text().toNSString()
                                 action:@selector(menuItemAction:)
                                 keyEquivalent:@""];
        pMenuItem.target = _pActionTarget;
        pMenuItem.code = item->code();
        pMenuItem.enabled = item->enabled();
        pMenuItem.state = item->checked() == true ? NSControlStateValueOn : NSControlStateValueOff;
        if (!item->icon().isEmpty())
        {
            auto it = _icons.find(item->icon());
            if (it != _icons.end())
                pMenuItem.image = *it;
            else
            {
                QString file = item->icon();
                if (file.startsWith("qrc:/")) file.remove(0, 3);
                auto pixmap = QPixmap(file);
                if (!pixmap.isNull())
                {
                    int max = pixmap.width() > pixmap.height() ? pixmap.width() : pixmap.height();
                    NSImage* image = QtMac::toNSImage(pixmap);
                    [image setSize:NSMakeSize(16 * pixmap.width() / max, 16 * pixmap.height() / max)];
                    _icons.insert(item->icon(), image);
                    pMenuItem.image = image;
                }
            }
        }
        if (!item->children().isEmpty())
        {
            pMenuItem.submenu = createMenu(item->children());
        }
        [pMenu addItem:pMenuItem];
    }
    return pMenu;
}

void NativeTrayMac::setMenuItems(const NativeMenuItem::List& items)
{
    _pTrayMenu = createMenu(items);
    _pTrayMenu.delegate = _pMenuDelegate;
}

NativeTrayMac::~NativeTrayMac()
{
    // Remove the status item - ARC only releases our own reference to the item,
    // it doesn't destroy it since the status bar still holds on to it.
    if(_pStatusBar && _pStatusItem)
        [_pStatusBar removeStatusItem:_pStatusItem];

    // Clear out the action target's parent, since it may outlive this object.
    if(_pActionTarget)
        _pActionTarget.parent = nullptr;
}

QRect NativeTrayMac::getScreenBound() const
{
    // Get the bound of the view.  It only provides one bound, so we assume that
    // it's the bound on the screen where the icon was just clicked.
    NSRect boundInView = _pButton.bounds;
    // Convert to window coordinates, then to screen coordinates
    NSRect boundInWindow = [_pButton convertRect:boundInView toView:nil];
    // Although this method is called 'convertRectToScreen', it actually returns
    // an origin relative to the entire virtual desktop, not relative to the
    // screen's origin.
    NSRect boundInDesktop = [_pButton.window convertRectToScreen:boundInWindow];

    QRectF qtBounds{rectNativeToQt(boundInDesktop)};
    return qtBounds.toRect();
}

void NativeTrayMac::setIconState(IconState icon, const QString &iconSet)
{
    NSImage *pIconImage = loadIconForState(icon, iconSet);
    _pButton.image = pIconImage;
}

void NativeTrayMac::showNotification(IconState, const QString &title,
                                     const QString &subtitle)
{
    hideNotification();

    // The IconState here is ignored on OS X.  The large icon can't be
    // customized (with public APIs anyway).  We could show it as a small icon
    // in the message text, but it's already right next to the tray icon anyway.

    NSUserNotification *pNotification = [[NSUserNotification alloc] init];
    pNotification.title = title.toNSString();
    pNotification.subtitle = subtitle.toNSString();
    pNotification.informativeText = QStringLiteral(PIA_PRODUCT_NAME).toNSString();

    [_pNotificationCenter deliverNotification:pNotification];
}

void NativeTrayMac::hideNotification()
{
    [_pNotificationCenter removeAllDeliveredNotifications];
}

void NativeTrayMac::setToolTip(const QString& toolTip)
{
    _pButton.toolTip = toolTip.toNSString();
}

void NativeTrayMac::getIconBound(QRect &iconBound, qreal &screenScale)
{
    iconBound = getScreenBound();
    screenScale = 1.0;
}

void NativeTrayMac::trayClicked()
{
    // There's never any scaling on OS X, the OS handles it for us.
    emit leftClicked(getScreenBound(), 1.0);
}

void NativeTrayMac::trayShowMenu()
{
    if (_pTrayMenu)
        [_pStatusItem popUpStatusItemMenu:_pTrayMenu];
}

void NativeTrayMac::menuItemAction(const QString &code)
{
    emit menuItemSelected(code);
}
