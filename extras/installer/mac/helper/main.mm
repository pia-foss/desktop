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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <xpc/xpc.h>
#include <xpc/connection.h>
#include <syslog.h>
#include <libgen.h>
#include <string>
#include "mac_install_helper.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <Security/CodeSigning.h>
#include <Security/SecCode.h>
#include "brand.h"
#include "version.h"

// Linked in from vpn-installer-sh.cpp, generated from xxd -i on branded install
// script
extern unsigned char vpn_installer_sh[];
extern unsigned int vpn_installer_sh_len;

static_assert(__has_feature(objc_arc), "Requires Objective-C ARC");

static const char g_installScriptRelativePath[] = "/Contents/Resources/vpn-installer.sh";

namespace xpc
{
    int connectionCount = 0;

    // Execute a program (with arguments) and test if it returns success (zero).
    // If the program can't be executed, or if it returns a nonzero exit code,
    // this returns false.
    //
    // If execEnv is not nullptr, then it is passed to putenv() after forking to
    // change the executed process's environment.
    bool execute(char * const argv[], char *execEnv)
    {
        int forkResult = fork();
        errno = 0;

        // fork() failed
        if(forkResult < 0)
        {
            syslog(LOG_ERR, "fork error: %d / %d - %s", forkResult, errno,
                   strerror(errno));
            return false;
        }

        // child - call execv()
        if(forkResult == 0)
        {
            if(execEnv)
                putenv(execEnv);

            int execErr = execv(argv[0], argv);
            syslog(LOG_ERR, "exec error: %d / %d - %s", execErr, errno,
                   strerror(errno));
            exit(-1);
        }

        // parent - watch for child exit using PID
        int status = 0;
        int waitResult = waitpid(forkResult, &status, 0);
        if(waitResult != forkResult)
        {
            syslog(LOG_ERR, "wait error: %d / %d - %s", waitResult, errno,
                   strerror(errno));
            return false;
        }
        if(!WIFEXITED(status))
        {
            syslog(LOG_ERR, "child terminated abnormally - %d", status);
            return false;
        }
        if(WEXITSTATUS(status) != 0)
        {
            syslog(LOG_ERR, "child exited unsuccessfully - %d", WEXITSTATUS(status));
            return false;
        }

        syslog(LOG_NOTICE, "Completed successfully");
        return true;
    }

    // Create a temporary directory (with mkdtemp() - mode 0700) and remove it when
    // done.
    class TempDir
    {
    public:
        // Create a temp directory.  If it fails, valid() returns false after
        // construction.
        TempDir();
        TempDir(TempDir &&other)
        {
            // _path is empty, so this TempDir does not own a temp directory
            *this = std::move(other);
        }
        ~TempDir();
        TempDir &operator=(TempDir &&other)
        {
            std::swap(_path, other._path);
            return *this;
        }

        const std::string &path() const {return _path;}
        bool valid() const {return !_path.empty();}
        explicit operator bool() const {return valid();}

        // Disown the temporary directory - it will no longer be cleaned up
        void disown() {_path.clear();}

    private:
        // Path to the actual directory, if it was created.
        std::string _path;
    };

    TempDir::TempDir()
    {
        char mkdtemplate[] = "/tmp/" BRAND_CODE "_XXXXXX";
        // mkdtemp returns mkdtemplate on success and nullptr on failure
        if(!mkdtemp(mkdtemplate))
            syslog(LOG_ERR, "Unable to create temp directory: %d", errno);
        else
        {
            _path = mkdtemplate;
            syslog(LOG_NOTICE, "Created temp directory %s", _path.c_str());
        }
    }

    TempDir::~TempDir()
    {
        if(valid())
        {
            syslog(LOG_NOTICE, "Remove temp directory %s", _path.c_str());
            // Need mutable strings for argv
            char binRmPath[] = "/bin/rm";
            char rmRfArg[] = "-rf";
            char *argv[] = {binRmPath, rmRfArg, _path.data(), NULL};
            if(!execute(argv, nullptr))
            {
                syslog(LOG_WARNING, "Unable to remove temp directory: %s",
                       _path.c_str());
            }
        }
    }

    void sendReply(xpc_object_t event, MacInstall::Result result)
    {
        syslog(LOG_NOTICE, "Command completed with result %d", result);
        xpc_object_t reply = xpc_dictionary_create_reply(event);
        if(!reply)
        {
            syslog(LOG_WARNING, "Could not reply to request, caller may not have requested reply");
            return;
        }

        xpc_dictionary_set_int64(reply, MacInstall::xpcKeyResult, result);
        xpc_connection_t conn = xpc_dictionary_get_remote_connection(event);
        if(!conn)
        {
            syslog(LOG_WARNING, "Could not reply to request, connection may have been abandoned");
            return;
        }

        xpc_connection_send_message(conn, reply);
    }

    // Get the complete path to the installer using the app path from an install
    // or uninstall request.
    std::string getInstallScriptPath(xpc_object_t event)
    {
        std::string installerPath{xpc_dictionary_get_string(event, MacInstall::xpcKeyAppPath)};
        installerPath += g_installScriptRelativePath;
        return installerPath;
    }

    std::string getAppPath(xpc_object_t event)
    {
        std::string appPath{xpc_dictionary_get_string(event, MacInstall::xpcKeyAppPath)};
        return appPath;
    }

    bool dirExists(const char *path)
    {
        struct stat info;

        if(stat( path, &info ) != 0)
            return false;
        else if(info.st_mode & S_IFDIR)
            return true;
        else
            return false;
    }

    // Copy the bundle specified by appPath to a temp directory owned by root
    // (mode 0700).  After the copy is completed, the copy's signature can be
    // validated without TOC/TOU issues, since it is owned by root.
    std::string copyBundle(std::string tmpDir, std::string appPath)
    {
        // Need mutable strings for argv
        char binCpPath[] = "/bin/cp";
        char cpRArg[] = "-R";
        char *argv[] = {binCpPath, cpRArg, appPath.data(), tmpDir.data(), NULL};

        if(!execute(argv, nullptr))
        {
            syslog(LOG_ERR, "Unable to copy app bundle %s to temp directory %s",
                   appPath.c_str(), tmpDir.c_str());
            return {};
        }

        char bnamebuf[MAXPATHLEN];

        tmpDir += '/';
        tmpDir += basename_r(appPath.data(), bnamebuf);
        syslog(LOG_NOTICE, "Copied app bundle %s to %s", appPath.c_str(),
               tmpDir.c_str());
        return tmpDir;
    }

    bool validateBundle(std::string secureAppPath)
    {
        CFURLRef fileRef = CFURLCreateAbsoluteURLWithBytes(kCFAllocatorDefault,
                                                           reinterpret_cast<const UInt8*>(secureAppPath.data()), static_cast<CFIndex>(secureAppPath.length()),
                                                           kCFStringEncodingUTF8, NULL , false);

        SecStaticCodeRef staticCode;
        OSStatus createCodeStatus  = SecStaticCodeCreateWithPath(fileRef, kSecCSDefaultFlags,
                                                       &staticCode);

        CFRelease(fileRef);
        fileRef = nil;
        if(errSecSuccess != createCodeStatus)
        {
            syslog(LOG_NOTICE, "Does not appear to be an app bundle: %s (%d)",
                   secureAppPath.c_str(), errSecSuccess);
            return false;
        }

        OSStatus validateResult = -1;
        SecRequirementRef isLtm  = nil;

        SecRequirementCreateWithString(CFSTR("identifier " BRAND_IDENTIFIER " "
                                             "and certificate leaf[subject.CN] = \"" PIA_CODESIGN_CERT "\" "
                                             "and info [" BRAND_IDENTIFIER ".version] = \"" PIA_VERSION "\""),
                                       kSecCSDefaultFlags, &isLtm);

        // "Strict" validation is necessary to ensure that files haven't been
        // added to the bundle; otherwise an "unsealed bundle root" does not
        // fail code validation
        validateResult = SecStaticCodeCheckValidity(staticCode, kSecCSStrictValidate, isLtm);

        if(validateResult == errSecSuccess)
        {
            syslog(LOG_NOTICE, "Verified signature on bundle %s",
                   secureAppPath.c_str());
            return true;
        }

        syslog(LOG_NOTICE, "Validate failed for bundle %s (%d)", secureAppPath.c_str(),
               validateResult);
        return false;
    }

    bool executeInstaller(std::string bundlePath, std::string clientPidStr,
                          const char *installingUser)
    {
        std::string envInstallingUser{"INSTALLING_USER="};
        if(installingUser)
            envInstallingUser += installingUser;

        std::string installerPath = bundlePath + g_installScriptRelativePath;

        syslog(LOG_NOTICE, "Execute installer: %s with %s (calling PID %s)",
               installerPath.c_str(), envInstallingUser.c_str(),
               clientPidStr.c_str());
        char installCmd[] = "install";
        char *argv[] = {installerPath.data(), installCmd, clientPidStr.data(),
                        nullptr};

        if(execute(argv, envInstallingUser.data()))
        {
            syslog(LOG_NOTICE, "Install Success");
            return true;
        }
        syslog(LOG_NOTICE, "Install Failed");
        return false;
    }

    void actionInstall(xpc_object_t event, xpc_connection_t connection)
    {
        MacInstall::Result result = MacInstall::Result::Error;

        std::string appPath{getAppPath(event)};

        pid_t clientPid = xpc_connection_get_pid(connection);
        auto clientPidStr = std::to_string(clientPid);

        TempDir tmpDir;
        if(tmpDir)  // Failure traced by TempDir
        {
            std::string copiedAppPath = copyBundle(tmpDir.path(), appPath);
            if(copiedAppPath.empty())
            {
                syslog(LOG_ERR, "Failed to copy bundle: %s", appPath.c_str());
            }
            else if(!validateBundle(copiedAppPath))
            {
                syslog(LOG_ERR, "Failed to validate signature of bundle: %s", appPath.c_str());
            }
            else if(!executeInstaller(copiedAppPath, std::move(clientPidStr),
                                      xpc_dictionary_get_string(event, MacInstall::xpcKeyInstallingUser)))
            {
                syslog(LOG_ERR, "Installer completed unsuccessfully from bundle: %s", appPath.c_str());
            }
            else
            {
                result = MacInstall::Result::Success;

                // TODO - Need to remove original app bundle from user process
            }
        }

        sendReply(event, result);
    }

    bool makeTmpSubdir(std::string &tmpDir, const char *pSubdirName)
    {
        tmpDir += '/';
        tmpDir += pSubdirName;
        if(mkdir(tmpDir.c_str(), S_IRUSR | S_IWUSR | S_IXUSR))
        {
            syslog(LOG_ERR, "Failed to create directory: %s (%d)",
                   tmpDir.c_str(), errno);
            return false;
        }
        return true;
    }

    std::string createUninstallFile(std::string tmpPath)
    {
        // Out of paranoia, put the uninstall script in /tmp/pia_XXXXXX/app/Contents/Resources/.
        // It always uninstalls from the standard install location, but we don't
        // want it to think the app bundle path is /, just in case.
        if(!makeTmpSubdir(tmpPath, "app") ||
           !makeTmpSubdir(tmpPath, "Contents") ||
           !makeTmpSubdir(tmpPath, "Resources"))
        {
            // Traced by makeTempSubdir
            return {};
        }

        tmpPath += "/vpn-installer.sh";

        FILE *fp = fopen(tmpPath.c_str(), "w");
        if(!fp)
        {
            syslog(LOG_ERR, "Failed to open uninstall file %s (%d)",
                   tmpPath.c_str(), errno);
            return {};
        }
        if(fwrite(vpn_installer_sh, 1, vpn_installer_sh_len, fp) != vpn_installer_sh_len)
        {
            syslog(LOG_NOTICE, "Failed to write uninstall path %s",
                   tmpPath.c_str());
            fclose(fp);
            return {};
        }
        if(fclose(fp))
        {
            syslog(LOG_ERR, "Failed to close uninstall file %s (%d)",
                   tmpPath.c_str(), errno);
            return {};
        }

        // Add the execute bit.  The default umask already prevented group/other
        // from having write access, which is important to ensure that there's
        // no window where the file could be modified.
        if(chmod(tmpPath.c_str(), S_IRUSR | S_IWUSR | S_IXUSR) != 0)
        {
            syslog(LOG_ERR, "Failed to set executable bit on uninstaller %s (%d)",
                   tmpPath.c_str(), errno);
            return {};
        }

        syslog(LOG_NOTICE, "Wrote uninstaller %s", tmpPath.c_str());
        return tmpPath;
    }

    void actionUninstall(xpc_object_t event, xpc_connection_t connection)
    {
        MacInstall::Result result = MacInstall::Result::Error;

        TempDir tmpDir;
        if(tmpDir)  // Failure traced by TempDir
        {
            auto installerPath = createUninstallFile(tmpDir.path());
            // Overwrite the contents of uninstall file with the install script
            if(installerPath.empty())
            {
                syslog(LOG_ERR, "Unable to create uninstaller");
            }
            else
            {
                pid_t clientPid = xpc_connection_get_pid(connection);
                auto clientPidStr = std::to_string(clientPid);

                syslog(LOG_NOTICE, "Execute uninstaller: %s (client PID %s)",
                       installerPath.c_str(), clientPidStr.c_str());
                char uninstallArg[] = "scheduleuninstall";
                std::string uninstallTmpDir{tmpDir.path()};
                char *argv[] = {installerPath.data(), uninstallArg,
                    clientPidStr.data(), uninstallTmpDir.data(), nullptr};

                if(execute(argv, nullptr))
                {
                    // The 'scheduleuninstall' succeeded, the uninstall proceeds
                    // asynchronously - the uninstall script is responsible for
                    // removing this temp directory now.
                    tmpDir.disown();
                    result = MacInstall::Result::Success;
                }
            }
        }

        sendReply(event, result);
    }

    void eventHandler(xpc_object_t event, xpc_connection_t connection)
    {
        if(xpc_get_type(event) == XPC_TYPE_ERROR)
        {
            if(event == XPC_ERROR_CONNECTION_INVALID)
            {
                // This is normal when connection is lost or closed
                // Nothing to do - we don't have any state to clean up
                --connectionCount;
                syslog(LOG_NOTICE, "Client connection closed (%d connections)",
                       connectionCount);
                // Exit if there are no other connections (this is an on-demand
                // service)
                if(connectionCount <= 0)
                {
                    syslog(LOG_NOTICE, "Service exiting");
                    // There doesn't seem to be any other way to exit when using
                    // dispatch_main()
                    exit(0);
                }
            }
            else if(event == XPC_ERROR_TERMINATION_IMMINENT)
            {
                syslog(LOG_NOTICE, "Client connection about to close - %s",
                       xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
            }
            else
            {
                syslog(LOG_NOTICE, "Connection received error - %s",
                       xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
            }
            return;
        }

        int64_t action = xpc_dictionary_get_int64(event, MacInstall::xpcKeyAction);
        switch(action)
        {
            case MacInstall::Action::Invalid:
                syslog(LOG_ERR, "Message did not contain valid action");
                break;
            case MacInstall::Action::Install:
                actionInstall(event, connection);
                break;
            case MacInstall::Action::Uninstall:
                actionUninstall(event, connection);
                break;
            default:
                syslog(LOG_ERR, "Unknown action: %lld", action);
                break;
        }
    }

    void connectionHandler(xpc_object_t event)
    {
        auto type = xpc_get_type(event);

        if(type == XPC_TYPE_ERROR)
        {
            syslog(LOG_NOTICE, "Listener received error - %s",
                   xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
        }
        else if(type == XPC_TYPE_CONNECTION)
        {
            xpc_connection_t connection = reinterpret_cast<xpc_connection_t>(event);
            // New connection - set the event handler
            xpc_connection_set_event_handler(connection, ^(xpc_object_t event)
            {
                eventHandler(event, connection);
            });
            // Start handling messages on the connection
            xpc_connection_resume(connection);
            ++connectionCount;
            syslog(LOG_NOTICE, "Client connection opened (%d connections)", connectionCount);
        }
    }
}

int main(int, char**)
{
    openlog(MacInstall::serviceIdentifier, LOG_CONS|LOG_PID, LOG_USER);

    // Certain tools like launchctl behave differently depending on the real
    // user ID.  Although this tool is now launched as a launch daemon and
    // should already have the real user ID set to root, set this explicitly for
    // robustness.
    if (setgid(getegid()))
    {
        syslog(LOG_ERR, "Unable to set real GID.");
        return -1;
    }
    if (setuid(geteuid()))
    {
        syslog(LOG_ERR, "Unable to set real UID.");
        return -2;
    }

    // Set up the XPC service used by the client to trigger an install/uninstall
    xpc_connection_t xpcService = xpc_connection_create_mach_service(MacInstall::serviceIdentifier,
                                                                     dispatch_get_main_queue(),
                                                                     XPC_CONNECTION_MACH_SERVICE_LISTENER);
    if(!xpcService)
    {
        syslog(LOG_ERR, "Failed to create Mach service.");
        return -3;
    }

    // Set up the connection handler for the listener connection
    xpc_connection_set_event_handler(xpcService, ^(xpc_object_t event)
    {
        xpc::connectionHandler(event);
    });

    syslog(LOG_NOTICE, "Starting install helper service");

    // Start handling events
    xpc_connection_resume(xpcService);

    dispatch_main();

    // This doesn't seem possible, there doesn't seem to be a way to cause
    // dispatch_main() to return - it's here for supportability in case this
    // happens somehow.
    syslog(LOG_NOTICE, "Service exiting; event loop ended");

    return 0;
}
