// Copyright (c) 2019 London Trust Media Incorporated
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
#include <libgen.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include "linux-launcher-cmds.h"
#include "brand.h"
#include <vector>
#include <string>
#include <iostream>

// fork() and execv().  The real and effective group IDs are overridden after
// the fork to gid.
int fork_execv(gid_t gid, char *filename, char *const argv[])
{
    int forkResult = fork();

    // fork() failed
    if(forkResult < 0)
    {
        std::cerr << "fork err: " << forkResult << " / " << errno << " - "
            << strerror(errno) << std::endl;
        return forkResult;
    }

    // child - call execv()
    if(forkResult == 0)
    {
        // Apply gid as both real and effective
        setregid(gid, gid);

        int execErr = execv(filename, argv);
        std::cerr << "exec err: " << execErr << " / " << errno << " - "
            << strerror(errno) << std::endl;
        std::exit(-1);
    }

    // parent - return PID
    return forkResult;
}

void receiveInvoke(int invokePipeFd)
{
    unsigned char command;
    ssize_t readResult = ::read(invokePipeFd, &command, sizeof(command));
    if(readResult < static_cast<ssize_t>(sizeof(command)))
    {
        std::cerr << "Invoke command read error: " << readResult << std::endl;
        return;
    }

    char clientBin[] = "/opt/" BRAND_CODE "vpn/bin/" BRAND_CODE "-client";
    char safeMode[] = "--safe-mode";

    switch(command)
    {
        case LinuxLauncherCmds::StartClientNormal:
        {
            std::cout << "Starting client normally";
            char *args[] = {clientBin, nullptr};
            // Execute with our real GID (the user's GID), drop the effective
            // GID of piavpn
            fork_execv(getgid(), clientBin, args);
            break;
        }
        case LinuxLauncherCmds::StartClientSafe:
        {
            std::cout << "Starting client in safe mode";
            char *args[] = {clientBin, safeMode, nullptr};
            fork_execv(getgid(), clientBin, args);
            break;
        }
        default:
            std::cerr << "Received unknown command " << command << std::endl;
            break;
    }
}

int main (int argc, char *argv[])
{
    // argv[0] is path to support-tool-launcher
    std::vector<char*> args{argv, argv+argc};

    // Change the command to run the branded support tool
    // The absolute path is hard-coded because this launcher is setgid.  If we
    // accepted either a path or executable name in any form (including via the
    // current directory, path in argv[0], etc.), then it'd allow running
    // arbitrary programs setgid.
    char command[] = "/opt/" BRAND_CODE "vpn/bin/" BRAND_CODE "-support-tool";
    args[0] = command;

    // Create a pipe to communicate with the support tool (since it runs with
    // group piavpn, it asks us to invoke the client, so we can invoke it with
    // the user's group).
    int invokePipe[2]{-1, -1};
    char invokePipeArg[] = "--invoke-pipe";
    std::string invokePipeArgVal;
    if(pipe(invokePipe) != 0)
    {
        std::cerr << "failed to create pipe: " << errno << " - "
            << strerror(errno) << std::endl;
        // Doc doesn't state that the array won't be modified if the call fails;
        // be sure.
        invokePipe[0] = -1;
        invokePipe[1] = -1;
    }
    else
    {
        // Pass the write end to the support tool
        args.push_back(invokePipeArg);
        invokePipeArgVal = std::to_string(invokePipe[1]);
        args.push_back(&invokePipeArgVal[0]);
        // Close the read end on exec()
        fcntl(invokePipe[0], F_SETFD, FD_CLOEXEC);
    }

    // Add a NULL to the end to terminate the array
    args.push_back(nullptr);

    // Start the support tool in the piavpn group (our effective GID) to exclude
    // it from killswitch.
    // Our egid is already piavpn, but this also sets the real group ID.  They
    // have to be the same so the $ORIGIN rpath is used.
    int supportToolPid = fork_execv(getegid(), command, args.data());
    if(supportToolPid < 0)
        return -1;

    if(invokePipe[0] < 0)
    {
        // Failed to open the pipes earlier; we're done.
        return 0;
    }

    // No longer need the write end
    close(invokePipe[1]);
    invokePipe[1] = -1;

    // Just one descriptor to poll - we detect that the support tool exits by
    // the remote end hanging up the pipe
    pollfd pollCfg{};
    pollCfg.fd = invokePipe[0];
    pollCfg.events = POLLIN;
    pollCfg.revents = 0;

    while(true)
    {
        errno = 0;
        if(::poll(&pollCfg, 1, -1) >= 0)
        {
            // If there's data to read, read it, even if the other side has
            // hung up (we'll get to that after we read the data)
            if(pollCfg.revents & POLLIN)
            {
                receiveInvoke(pollCfg.fd);
            }
            else if(pollCfg.revents & POLLHUP)
            {
                // Normal termination
                break;
            }
            else if(pollCfg.revents & (POLLERR | POLLNVAL))
            {
                // Errors - terminate
                return -1;
            }
        }
        else if(errno != EINTR)
        {
            // EINTR indicates a signal while polling, that's fine.  Otherwise,
            // terminate.
            std::cerr << "Terminating due to poll error: " << errno << " - "
                << strerror(errno) << std::endl;
            return -1;
        }
    }

    return 0;
}
