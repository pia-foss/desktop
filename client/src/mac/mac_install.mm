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
#line SOURCE_FILE("mac/mac_install.mm")

#import "mac_install.h"

#include "path.h"
#include "brand.h"
#include "product.h"

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QStringList>
#include <QVector>

#import <AppKit/AppKit.h>
#import <security/Authorization.h>
#import <Security/Security.h>
#import <ServiceManagement/ServiceManagement.h>
#import <stdlib.h>
#import <dlfcn.h>

enum class MacAuthorizeResult
{
    Success,
    JobDisabledFailure,
    OtherFailure,
};

namespace xpc
{
    // Connect to the install helper and send an Install or Uninstall request.
    // Synchronously waits for the reply.
    MacInstall::Result sendRequest(MacInstall::Action action, const QString &appPath)
    {
        xpc_connection_t xpcConn = xpc_connection_create_mach_service(MacInstall::serviceIdentifier,
                                                                      NULL,
                                                                      XPC_CONNECTION_MACH_SERVICE_PRIVILEGED);

        if(!xpcConn)
        {
            qError() << "Failed to open XPC connection to install helper";
            return MacInstall::Result::Error;
        }

        xpc_connection_set_event_handler(xpcConn, ^(xpc_object_t event)
        {
            auto type = xpc_get_type(event);
            if(type == XPC_TYPE_ERROR)
            {
                qError() << "Error in connection handler:"
                    << xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION);
            }
        });

        xpc_connection_resume(xpcConn);

        xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_int64(request, MacInstall::xpcKeyAction, action);

        // Pass the path to the app bundle to install/uninstall
        QByteArray appPathUtf8{appPath.toUtf8()};
        xpc_dictionary_set_string(request, MacInstall::xpcKeyAppPath,
                                  appPathUtf8.data());

        // For install only, pass the current user name
        if(action == MacInstall::Action::Install)
        {
            QByteArray username = qgetenv("USER");
            const char *usernameData = username.data();
            if(!usernameData)
                usernameData = "";
            xpc_dictionary_set_string(request, MacInstall::xpcKeyInstallingUser,
                                      usernameData);
        }

        qInfo() << "Sending request" << traceEnum(action) << "with path"
            << appPathUtf8;
        xpc_object_t reply = xpc_connection_send_message_with_reply_sync(xpcConn, request);
        auto result = static_cast<MacInstall::Result>(xpc_dictionary_get_int64(reply, MacInstall::xpcKeyResult));
        qInfo() << "Result for" << traceEnum(action) << "-" << traceEnum(result);

        return result;
    }
}

bool macCheckInstallation()
{
#ifdef MACOS_SKIP_INSTALL_CHECK
    return true;
#endif
    int installCheck = QProcess::execute(QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/vpn-installer.sh"), { QStringLiteral("check") });

    if (0 != installCheck)
    {
#ifdef _DEBUG
        if (isDebuggerPresent()) return true;
#endif

        qInfo() << "Installation required -" << installCheck;
        if (macExecuteInstaller())
        {
            // Will get relaunched by script once we die
            exit(0); // TODO: exit cleanly
        }
        else
        {
            // Installation canceled or errored out
            exit(1); // TODO: exit cleanly
        }
    }

    return true;
}

// Execute an elevated command with authorization. Called after AuthorizationCopyRights,
// with the error code of that function in 'err'. The AuthorizationRef will get freed.
MacAuthorizeResult macExecuteWithAuthorization(AuthorizationRef authorizationRef, OSStatus err, MacInstall::Action action, const QString &appPath)
{
    CFErrorRef blessError{nullptr};

    MacAuthorizeResult result = MacAuthorizeResult::OtherFailure;
    if (err == errAuthorizationCanceled)
    {
        qCritical() << "Authorization canceled";
    }
    else if (err != errAuthorizationSuccess)
    {
        qCritical() << "Unknown authorization error:" << err;
    }
    else if (!SMJobBless(kSMDomainSystemLaunchd, CFSTR(BRAND_IDENTIFIER ".installhelper"), authorizationRef, &blessError))
    {
        if(blessError)
        {
            int errCode = CFErrorGetCode(blessError);
            if (errCode == kSMErrorJobMustBeEnabled) {
                result = MacAuthorizeResult::JobDisabledFailure;
            }
            qCritical() << "Failed to authorize installer:" << errCode;
            qCritical() << "domain:" << CFErrorGetDomain(blessError);
            qCritical() << "description:" << CFErrorCopyDescription(blessError);
            qCritical() << "reason:" << CFErrorCopyFailureReason(blessError);
            qCritical() << "recovery:" << CFErrorCopyRecoverySuggestion(blessError);
            CFRelease(blessError);
        }
        else
            qCritical() << "Failed to authorize installer, no error info";
    }
    else if(MacInstall::Result::Success != xpc::sendRequest(action, appPath))
    {
        qInfo() << "Failed to send install request to install helper";
    }
    else
    {
        qInfo() << "Succeeded authorizing installer!";
        result = MacAuthorizeResult::Success;
    }
    AuthorizationFree(authorizationRef, kAuthorizationFlagDefaults);
    return result;
}

MacAuthorizeResult macRequestAuthorization(MacInstall::Action action, const QString &appPath)
{
    if (![NSApp isActive])
        [NSApp activateIgnoringOtherApps:YES];

    AuthorizationRef authorizationRef;
    AuthorizationItem authorizationEnvironmentItems[] = {};
    AuthorizationEnvironment authorizationEnvironment = { 0u, authorizationEnvironmentItems };
    AuthorizationItem authorizationRightsItems[] = {
        { kSMRightBlessPrivilegedHelper, 0, NULL, 0 },
    };
    AuthorizationRights authorizationRights = { sizeof(authorizationRightsItems) / sizeof(*authorizationRightsItems), authorizationRightsItems };
    AuthorizationFlags authorizationFlags = (AuthorizationFlags)(kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights | kAuthorizationFlagPreAuthorize);

    OSStatus err = AuthorizationCreate(NULL, &authorizationEnvironment, kAuthorizationFlagDefaults, &authorizationRef);
    if (err != errAuthorizationSuccess)
    {
        qCritical() << "Failed to initialize authorization context:" << err;
        return MacAuthorizeResult::OtherFailure;
    }

    return macExecuteWithAuthorization(authorizationRef, AuthorizationCopyRights(authorizationRef, &authorizationRights, &authorizationEnvironment, authorizationFlags, NULL), action, appPath);
}

QString macGetUntranslocatedPath(const QString& path)
{
    // This trick is borrowed from https://www.sysnack.com/2016/12/16/untranslocating-apps/

    static auto SecTranslocateIsTranslocatedURL = (Boolean (*)(CFURLRef path, bool *isTranslocated, CFErrorRef * __nullable error)) dlsym(RTLD_DEFAULT, "SecTranslocateIsTranslocatedURL");
    static auto SecTranslocateCreateOriginalPathForURL = (CFURLRef __nullable (*)(CFURLRef translocatedPath, CFErrorRef * __nullable error)) dlsym(RTLD_DEFAULT, "SecTranslocateCreateOriginalPathForURL");

    if (SecTranslocateIsTranslocatedURL && SecTranslocateCreateOriginalPathForURL)
    {
        bool isTranslocated = false;
        NSURL* url = [NSURL fileURLWithPath:path.toNSString()];
        SecTranslocateIsTranslocatedURL((__bridge CFURLRef) url, &isTranslocated, NULL);
        if (isTranslocated)
        {
            NSURL* originalURL = (__bridge NSURL*) SecTranslocateCreateOriginalPathForURL((__bridge CFURLRef) url, NULL);
            if (originalURL)
            {
                return QString::fromNSString(originalURL.path);
            }
        }
    }

    return {};
}

bool macExecuteInstaller()
{
    QString originalAppPath = Path::BaseDir;
    // Check if we're begin translocated and try to identify the original path
    QString originalPath = macGetUntranslocatedPath(QCoreApplication::applicationFilePath());
    if (!originalPath.isEmpty())
    {
        originalAppPath = Path(originalPath) / "../../..";
        // Clear quarantine flag
        QProcess::execute(QStringLiteral("/usr/bin/xattr"), { QStringLiteral("-dr"), QStringLiteral("com.apple.quarantine"), originalAppPath });
    }

    int legacyCheck = QProcess::execute(QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/vpn-installer.sh"), {QStringLiteral("check-legacy-upgrade")});
    if(legacyCheck != 0)
    {
        qInfo() << "Legacy upgrade check was canceled, aborting installation";
        return false;
    }

    MacAuthorizeResult auth_res = macRequestAuthorization(MacInstall::Action::Install, originalAppPath);
    if (auth_res == MacAuthorizeResult::Success)
    {
        // Success - relaunch client from installation
        QProcess::execute(Path::InstallationDir / "Contents/Resources/relaunch.sh", { QString::number(getpid()), originalAppPath });
        return true;
    }

    if (auth_res == MacAuthorizeResult::JobDisabledFailure)
        QProcess::execute(QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/vpn-installer.sh"), {QStringLiteral("show-failure-helper")});
    else
        QProcess::execute(QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/vpn-installer.sh"), {QStringLiteral("show-install-failure")});

    return false;
}

bool macExecuteUninstaller()
{
    return (MacAuthorizeResult::Success == macRequestAuthorization(MacInstall::Action::Uninstall, Path::BaseDir));
}
