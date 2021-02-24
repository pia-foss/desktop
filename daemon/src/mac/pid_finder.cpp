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

#include "pid_finder.h"
#include <unistd.h>
#include <QtAlgorithms>

bool PidFinder::matchesPath(pid_t pid)
{
    QString appPath = pidToPath(pid);

    // Check whether the app is one we want to exclude
    return std::any_of(_paths.begin(), _paths.end(),
        [&appPath](const QString &prefix) {
            // On MacOS we exclude apps based on their ".app" bundle,
            // this means we don't match on entire paths, but just on prefixes
            return appPath.startsWith(prefix);
        });
}

QString PidFinder::pidToPath(pid_t pid)
{
    char path[PATH_MAX] = {0};
    proc_pidpath(pid, path, sizeof(path));

    // Wrap in QString for convenience
    return QString{path};
}

template <typename Func_T>
void PidFinder::findPidAddresses(pid_t pid, IPVersion ipVersion, Func_T func)
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

        socket_fdinfo socketInfo{};
        size = proc_pidfdinfo(pid, fd.proc_fd, PROC_PIDFDSOCKETINFO,
                              &socketInfo, sizeof(socketInfo));
        if(size != sizeof(socketInfo))
        {
            qWarning() << "Failed to inspect descriptor" << fd.proc_fd << "of"
                << pid << "- got size" << size << "- expected" << sizeof(socketInfo);
            continue;
        }

        // Don't care about anything other than TCP/UDP.
        // It seems that TCP sockets may sometimes be indicated with
        // soi_kind==SOCKINFO_IN instead of SOCKINFO_TCP.
        // we don't use anything from the TCP-specific socket info so this is
        // fine, identify sockets by checking the IP protocol.
        if(!(socketInfo.psi.soi_protocol == IPPROTO_TCP || socketInfo.psi.soi_protocol == IPPROTO_UDP))
            continue;

        if(ipVersion == IPv4 && socketInfo.psi.soi_proto.pri_in.insi_vflag & INI_IPV4)
        {
            // The local address can be 0, but the port must be valid
            if(socketInfo.psi.soi_proto.pri_in.insi_lport > 0)
            {
                // lport is network order, so convert to host
                const auto ip = ntohl(socketInfo.psi.soi_proto.pri_in.insi_laddr.ina_46.i46a_addr4.s_addr);
                const auto port = ntohs(socketInfo.psi.soi_proto.pri_in.insi_lport);

                // We're not interested in loopback related ports as loopback packets don't get routed anyway
                // so should be invisible to split tunnel
                if(!QHostAddress{ip}.isLoopback())
                    func(ip, port);
                //ports.push_back(ntohs(static_cast<uint32_t>(socketInfo.psi.soi_proto.pri_in.insi_lport)));
            }
        }
        else if(socketInfo.psi.soi_proto.pri_in.insi_vflag & INI_IPV6)
        {
            // Store an IPv6 socket if it's the "any" address (and has a valid
            // port)
            if(ipVersion == IPv4)
            {
                const auto &in6addr = socketInfo.psi.soi_proto.pri_in.insi_laddr.ina_6.s6_addr;
                bool isAny = std::all_of(std::begin(in6addr), std::end(in6addr), [](auto val) { return val == 0; });
                if(isAny && socketInfo.psi.soi_proto.pri_in.insi_lport)
                {
                    // lport is network order, so convert to host
                    const auto port = ntohs(socketInfo.psi.soi_proto.pri_in.insi_lport);
                    func(0, port);
                    //ports.push_back(ntohs(static_cast<uint32_t>(socketInfo.psi.soi_proto.pri_in.insi_lport)));
                }
            }
            else if(ipVersion == IPv6)
            {
                if(socketInfo.psi.soi_proto.pri_in.insi_lport > 0)
                {
                    const auto &ip6 = socketInfo.psi.soi_proto.pri_in.insi_laddr.ina_6;
                    const auto port = ntohs(socketInfo.psi.soi_proto.pri_in.insi_lport);

                    // lport is network order, so convert to host

                    // TODO: actually pass the real ip6 address here, not just 0
                    func(0, port);
                    //ports.push_back(ntohs(static_cast<uint32_t>(socketInfo.psi.soi_proto.pri_in.insi_lport)));
                }
            }
        }
    }
}

pid_t PidFinder::pidForPort(quint16 port, IPVersion ipVersion)
{
    return pidFor([&, this](const auto &pid) {
        QSet<quint16> ports;
        findPidAddresses(pid, ipVersion, [&ports](const auto &ip, const auto &port) {
            ports << port;
        });
        return ports.contains(port);
    });
}

QSet<pid_t> PidFinder::pids()
{
    return pidsFor([this](const auto &pid) { return matchesPath(pid); });
}

QSet<quint16> PidFinder::ports(const QSet<pid_t> &pids, IPVersion ipVersion)
{
    QSet<quint16> ports;
    for(const auto &pid : pids)
        findPidAddresses(pid, ipVersion, [&ports](const auto &ip, const auto &port) {
            ports << port;
        });

    return ports;
}

QSet<AddressAndPort> PidFinder::addresses4(const QSet<pid_t> &pids)
{
    QSet<AddressAndPort> addresses;
    for(const auto &pid : pids)
        findPidAddresses(pid, PidFinder::IPv4, [&addresses](const auto &ip, const auto &port) {
            addresses.insert({static_cast<quint32>(ip), port});
        });

    return addresses;
}
