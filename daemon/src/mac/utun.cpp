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
#line SOURCE_FILE("mac/utun.cpp")

#include <net/if_utun.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sys_domain.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <libproc.h>

#include "mac/utun.h"

uint UTun::mtu() const
{
    if(isOpen())
    {
        struct ifreq ifr{};
        ::strlcpy(ifr.ifr_name, qPrintable(name()), sizeof(ifr.ifr_name));

        // SIOCSIFMTU == Socket IO Control Get InterFace MTU
        if(!::ioctl(_fd, SIOCGIFMTU, &ifr))
        {
            return ifr.ifr_mtu;
        }
    }
    return 0;
}

uint UTun::setMtu(uint mtu)
{
    if(isOpen())
    {
        struct ifreq ifr{};
        ifr.ifr_mtu = mtu;
        ::strlcpy(ifr.ifr_name, qPrintable(name()), sizeof(ifr.ifr_name));

        // SIOCSIFMTU == Socket IO Control Set InterFace MTU
        if(!::ioctl(_fd, SIOCSIFMTU, &ifr))
        {
            return mtu;
        }
    }
    return 0;
}

void UTun::close()
{
    if(isOpen())
    {
        qInfo() << "Closing open tunnel";
        ::close(_fd);
        _fd = -1;
        _unitNumber = -1;
    }
    else
    {
        qInfo() << "No open tunnel to close";
    }
}

UTun UTun::create()
{
    const int baseNumber = 8; // Start at utun7
    int unitNumber = 0;

    UTun utun;
    // Attempt to find a usable unit number (i.e utunN)
    for(int i = 0; i < 25; ++i)
    {
        unitNumber = baseNumber + i;
        qInfo() << "Trying unit_number:" << unitNumber;
        if(utun.open(unitNumber) != -1)
            return utun;
    }

    qWarning() << "Could not open utun device!";
    return {};
}

int UTun::open(int unitNumber)
{
    struct sockaddr_ctl sc{};
    struct ctl_info ctlInfo{};

    // Request a utun device
    ::strlcpy(ctlInfo.ctl_name, UTUN_CONTROL_NAME, sizeof(ctlInfo.ctl_name));

    int fd = ::socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
    if(fd < 0)
    {
        qWarning() << "Unable to open system socket for utun device:" << ErrnoTracer{errno};
        return -1;
    }

    // Prevent the socket being inherited by child processes
    if(::fcntl(fd, F_SETFD, FD_CLOEXEC))
        qWarning() << "fcntl failed setting FD_CLOEXEC:" << ErrnoTracer{errno};

    // Convert Kernel control name to id
    // This updates the ctlInfo.ctl_id field
    if(::ioctl(fd, CTLIOCGINFO, &ctlInfo) == -1)
    {
        qWarning() << "Unable to get system socket info for utun device:" << ErrnoTracer{errno};
        ::close(fd);
        return -1;
    }

    // Configure the tunnel device
    sc.sc_len = sizeof(sc);
    sc.sc_id = ctlInfo.ctl_id;
    sc.sc_family = AF_SYSTEM;
    sc.ss_sysaddr = AF_SYS_CONTROL;
    sc.sc_unit = unitNumber;         // utun<unitNumber>

    // Setup && configure the utun
    if(::connect(fd, reinterpret_cast<sockaddr*>(&sc), sizeof(sc)) < 0)
    {
        qWarning() << "Unable to connect system socket for utun device:" << ErrnoTracer{errno};
        ::close(fd);
        return -1;
    }

    _unitNumber = unitNumber;
    _fd = fd;
    return fd;
}
