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

#include "port_finder.h"
#include "ipaddress.h"
#include <unistd.h>
#include <QtAlgorithms>


namespace
{
bool isIpv4Local(const Ipv4Address &addr)
{
    return addr.isLoopback() || addr.isPrivateUse();
}

bool isIpv6Local(const Ipv6Address &ipAddr, const QPair<QHostAddress, int> localIpv6Subnet)
{
    QHostAddress addr = ipAddr;
    if(addr.protocol() != QAbstractSocket::IPv6Protocol)
        return false;

    return addr.isLoopback() || addr.isLinkLocal() || addr.isUniqueLocalUnicast() ||
        // Verify the subnet is valid
        // Then check if the ip address contains the "Network Prefix"
        (localIpv6Subnet.first.protocol() == QAbstractSocket::IPv6Protocol && addr.isInSubnet(localIpv6Subnet));
}

// Thin wrapper around socket_info for convenience
class SocketInfo
{
public:
    explicit SocketInfo(socket_info *pSocketInfo)
    : _pSocketInfo{pSocketInfo}
    {}

    SocketInfo(const SocketInfo&) = delete;
    SocketInfo(SocketInfo&&) = delete;

    //Ipv4
    std::uint32_t localIp4() const {return isIpv4() ? ntohl(inetInfo().insi_laddr.ina_46.i46a_addr4.s_addr) : 0;}
    std::uint32_t remoteIp4() const {return isIpv4() ? ntohl(inetInfo().insi_faddr.ina_46.i46a_addr4.s_addr) : 0;}
    // Ipv6 (return by reference as the members are quite large)
    const auto& localIp6() const {return isIpv6() ? inetInfo().insi_laddr.ina_6.s6_addr : _nullIpv6Address;}
    const auto& remoteIp6() const {return isIpv6() ? inetInfo().insi_faddr.ina_6.s6_addr : _nullIpv6Address;}
    bool isIpv6AnyAddress() const;
    std::uint16_t localPort() const {return ntohs(inetInfo().insi_lport);}
    std::uint32_t remotePort() const {return ntohs(inetInfo().insi_fport);}
    int protocol() const {return _pSocketInfo->soi_protocol;}
    bool isIpv4() const {return isIpVersion(INI_IPV4);}
    bool isIpv6() const {return isIpVersion(INI_IPV6);}
private:
    std::uint8_t isIpVersion(std::uint8_t flag) const {return inetInfo().insi_vflag & flag;}
    const in_sockinfo& inetInfo() const {return _pSocketInfo->soi_proto.pri_in;}
private:
    unsigned char _nullIpv6Address[16]{};
    // Use a pointer to avoid an expensive copy.
    // The lifetime of an instance of SocketInfo should be
    // equal to the lifetime of the associated socket_info, so this should be fine.
    socket_info *_pSocketInfo{nullptr};
};

bool SocketInfo::isIpv6AnyAddress() const
{
    if(isIpv4()) return false;

    const auto &in6addr{inetInfo().insi_laddr.ina_6.s6_addr};
    return std::all_of(std::begin(in6addr), std::end(in6addr), [](auto val) { return val == 0; });
}

template <typename Func_T>
void addressesForPid(pid_t pid, IPVersion ipVersion, Func_T func)
{
    // Get the buffer size needed
    int size = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, nullptr, 0);
    if(size <= 0)
        return;

    QVector<proc_fdinfo> fds;
    fds.resize(size / sizeof(proc_fdinfo));
    // Get the file descriptors
    size = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, fds.data(), fds.size() * sizeof(proc_fdinfo));
    fds.resize(size / sizeof(proc_fdinfo));

    for(const auto &fd : fds)
    {
        if(fd.proc_fdtype != PROX_FDTYPE_SOCKET)
            continue;   // Don't care about anything besides sockets

        socket_fdinfo socketFdInfo{};
        size = proc_pidfdinfo(pid, fd.proc_fd, PROC_PIDFDSOCKETINFO,
                              &socketFdInfo, sizeof(socketFdInfo));
        if(size != sizeof(socketFdInfo))
        {
            qWarning() << "Failed to inspect descriptor" << fd.proc_fd << "of"
                << pid << "- got size" << size << "- expected" << sizeof(socketFdInfo);
            continue;
        }

        // Use an OOP wrapper for convenience
        const SocketInfo socketInfo{&socketFdInfo.psi};

        // Don't care about anything other than TCP/UDP.
        // It seems that TCP sockets may sometimes be indicated with
        // soi_kind==SOCKINFO_IN instead of SOCKINFO_TCP.
        // we don't use anything from the TCP-specific socket info so this is
        // fine, identify sockets by checking the IP protocol.
        if(!(socketInfo.protocol() == IPPROTO_TCP || socketInfo.protocol() == IPPROTO_UDP))
            continue;

        if(ipVersion == IPv4 && socketInfo.isIpv4())
        {
            // The local address can be 0, but the port must be valid
            if(socketInfo.localPort() > 0)
                func(socketInfo);
        }
        else if(socketInfo.isIpv6())
        {
            // Store an IPv6 socket if it's the "any" address (and has a valid
            // port)
            if(ipVersion == IPv4)
            {
                if(socketInfo.isIpv6AnyAddress() && socketInfo.localPort() > 0)
                    func(socketInfo);
            }
            else if(ipVersion == IPv6)
            {
                if(socketInfo.localPort() > 0)
                    func(socketInfo);
            }
        }
    }
}
}

bool PortFinder::matchesPath(const QVector<QString> &paths, pid_t pid)
{
    QString appPath = pidToPath(pid);

    // Check whether the app is one we want to exclude
    return std::any_of(paths.begin(), paths.end(),
        [&appPath](const QString &prefix) {
            // On MacOS we exclude apps based on their ".app" bundle,
            // this means we don't match on entire paths, but just on prefixes
            return appPath.startsWith(prefix);
        });
}

QString PortFinder::pidToPath(pid_t pid)
{
    char path[PATH_MAX]{};
    proc_pidpath(pid, path, sizeof(path));

    // Wrap in QString for convenience
    return QString{path};
}

pid_t PortFinder::pidForPort(quint16 port, IPVersion ipVersion)
{
    return pidFor([&](const auto &pid) {
        QSet<quint16> ports;
        addressesForPid(pid, ipVersion, [&](const auto &socketInfo) {
            ports << socketInfo.localPort();
        });
        return ports.contains(port);
    });
}

QSet<pid_t> PortFinder::pids(const QVector<QString> &paths)
{
    return pidsFor([&](const auto &pid) { return matchesPath(paths, pid); });
}

PortSet PortFinder::ports(const QSet<pid_t> &pids, IPVersion ipVersion, const OriginalNetworkScan &netScan)
{
    QSet<quint16> ports;
    for(const auto &pid : pids)
        addressesForPid(pid, ipVersion, [&](const auto &socketInfo) {
            // We skip connections used for LAN communication
            // This is because on-link LAN connections should never go into the tunnel anyway
            // And off-link LAN should have a subnet bypass setup so it shouldn't go into the tunnel either.
            if(socketInfo.isIpv4())
            {
                if(!isIpv4Local({socketInfo.remoteIp4()})) ports << socketInfo.localPort();
            }
            else if(socketInfo.isIpv6())
            {
                // Find the on-link ipv6 network
                const auto parsedIpv6Subnet{QHostAddress::parseSubnet(QStringLiteral("%1/64").arg(netScan.ipAddress6()))};
                if(!isIpv6Local({socketInfo.remoteIp6()}, parsedIpv6Subnet)) ports << socketInfo.localPort();
            }
        });

    return ports;
}

PortSet PortFinder::ports(const QVector<QString> &paths, IPVersion ipVersion, const OriginalNetworkScan &netScan)
{
    return ports(pids(paths), ipVersion, netScan);
}

QSet<AddressAndPort> PortFinder::addresses4(const QVector<QString> &paths)
{
    QSet<AddressAndPort> addresses;
    for(const auto &pid : pids(paths))
        addressesForPid(pid, IPv4, [&addresses](const auto &socketInfo) {
            addresses.insert({static_cast<std::uint32_t>(socketInfo.localIp4()), socketInfo.localPort()});
        });

    return addresses;
}
