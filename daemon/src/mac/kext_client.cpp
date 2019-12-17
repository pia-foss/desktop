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

#include "common.h"
#line SOURCE_FILE("mac/kext_client.cpp")

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <fcntl.h>
#include <libproc.h>             // for proc_pidpath()
#include <QSocketNotifier>
#include <QProcess>
#include <QThread>
#include "posix/posix_firewall_pf.h"
#include "mac/mac_constants.h"
#include "kext_client.h"
#include "daemon.h"
#include "path.h"

// setsockopt() identifiers for our Kext socket.
// setsockopt() is used to transfer data between userland/kernel
#define PIA_IP_SET               1
#define PIA_MSG_REPLY            2
#define PIA_REMOVE_APP           3
#define PIA_WHITELIST_PIDS       4
#define PIA_WHTIELIST_PORTS      5
#define PIA_FIREWALL_STATE       6

namespace
{
    RegisterMetaType<QVector<QString>> qStringVector;
    RegisterMetaType<OriginalNetworkScan> qNetScan;
    RegisterMetaType<FirewallParams> qFirewallParams;

    const QString kSplitTunnelAnchorName = "150.allowExcludedApps";
}

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

void PidFinder::findPidPorts(pid_t pid, QVector<WhitelistPort> &ports)
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

        // Don't care about anything other than TCP.
        // It seems that TCP sockets may sometimes be indicated as SOCKINFO_IN,
        // we don't use anything from the TCP-specific socket info so this is
        // fine, identify TCP sockets by checking the IP protocol.
        if(socketInfo.psi.soi_kind != SOCKINFO_IN && socketInfo.psi.soi_kind != SOCKINFO_TCP)
            continue;
        if(socketInfo.psi.soi_protocol != IPPROTO_TCP)
            continue;

        if(socketInfo.psi.soi_proto.pri_in.insi_vflag == INI_IPV4)
        {
            // The local address can be 0, but the port must be valid
            if(socketInfo.psi.soi_proto.pri_in.insi_lport > 0)
            {
                ports.push_back({socketInfo.psi.soi_proto.pri_in.insi_laddr.ina_46.i46a_addr4.s_addr,
                                 static_cast<uint32_t>(socketInfo.psi.soi_proto.pri_in.insi_lport)});
                qInfo() << "added:" << ports.back().source_ip << ports.back().source_port << ports.size();
            }
        }
        else if(socketInfo.psi.soi_proto.pri_in.insi_vflag == INI_IPV6)
        {
            // Store an IPv6 socket if it's the "any" address (and has a valid
            // port)
            const auto &in6addr = socketInfo.psi.soi_proto.pri_in.insi_laddr.ina_6.s6_addr;
            bool isAny = std::all_of(std::begin(in6addr), std::end(in6addr), [](auto val){return val == 0;});
            if(isAny && socketInfo.psi.soi_proto.pri_in.insi_lport)
            {
                ports.push_back({0, static_cast<uint32_t>(socketInfo.psi.soi_proto.pri_in.insi_lport)});
                qInfo() << "added:" << ports.back().source_ip << ports.back().source_port << ports.size();
            }
        }
    }
}

QSet<pid_t> PidFinder::pids()
{
    int totalPidCount = 0;
    QSet<pid_t> pidsForPaths;
    QVector<pid_t> allPidVector;
    allPidVector.resize(maxPids);

    // proc_listallpids() returns the total number of PIDs in the system
    // (assuming that maxPids is > than the total PIDs, otherwise it returns maxPids)
     totalPidCount = proc_listallpids(allPidVector.data(), maxPids * sizeof(pid_t));

    for(int i = 0; i != totalPidCount; ++i)
    {
        pid_t pid = allPidVector[i];

        // Add the PID to our set if matches one of the paths
        if(matchesPath(pid))
            pidsForPaths.insert(pid);
    }

    return pidsForPaths;
}

QVector<WhitelistPort> PidFinder::ports(const QSet<pid_t> &pids)
{
    QVector<WhitelistPort> ports;
    for(const auto &pid : pids)
    {
        auto oldSize = ports.size();
        findPidPorts(pid, ports);
        if(pids.size() != oldSize)
            qInfo() << "PID" << pid << "-" << (ports.size() - oldSize) << "ports";
    }
    return ports;
}

struct ProcQuery
{
    KextClient::CommandType command;
    char needs_reply;
    char app_path[PATH_MAX];
    int pid;
    int accept;
    uint32_t id;
    uint32_t source_ip;
    uint32_t source_port;
    uint32_t dest_ip;
    uint32_t dest_port;

    // SOCK_STREAM or SOCK_DGRAM (tcp or udp)
    int socket_type;
};

void KextClient::showError(QString funcName)
{
    qWarning() << QStringLiteral("%1 Error (code: %2) %3").arg(funcName).arg(errno).arg(qPrintable(qt_error_string(errno)));
}

int KextClient::connectToKext(const OriginalNetworkScan &netScan, const FirewallParams &params)
{
    qInfo() << "Attempting to connect to kext";
    if(_state == State::Connected)
    {
        qWarning() << "Already connected to kext, disconnecting before reconnecting.";
        shutdownConnection();
    }

    qInfo() << "sizeof(ProcQuery) is: " << sizeof(ProcQuery);

    ctl_info ctl_info = {};

    int sock = ::socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
    if(sock < 0)
    {
        showError("::socket");
        return -1;
    }

    // Set close on exec flag, to prevent socket being inherited by
    // child processes (such as openvpn). If we fail to do this
    // the Kext thinks it stil has an open connection after we close it here,
    // due to the open file descriptor in the openvpn sub process
    if(::fcntl(sock, F_SETFD, FD_CLOEXEC))
        showError("::fcntl");

    strncpy(ctl_info.ctl_name, "com.privateinternetaccess.PiaKext", sizeof(ctl_info.ctl_name));

    if(::ioctl(sock, CTLIOCGINFO, &ctl_info) == -1)
    {
        showError("::ioctl");
        ::close(sock);
        return -1;
    }

    sockaddr_ctl sc = {};

    sc.sc_len = sizeof(sockaddr_ctl);
    sc.sc_family = AF_SYSTEM;
    sc.ss_sysaddr = SYSPROTO_CONTROL;
    sc.sc_id = ctl_info.ctl_id;
    sc.sc_unit = 0;

    if(::connect(sock, reinterpret_cast<sockaddr*>(&sc), sizeof(sockaddr_ctl)))
    {
        showError("::connect");
        ::close(sock);
        return -1;
    }

    // We're successfully connected so save the socket fd to our ivar
    _sockFd = sock;
    _state = State::Connected;

    updateNetwork(netScan, params);
    sendExistingPids(params.excludeApps);
    updateExcludedApps(params.excludeApps);

    _readNotifier = new QSocketNotifier(sock, QSocketNotifier::Read);
    connect(_readNotifier, &QSocketNotifier::activated, this, &KextClient::readFromSocket);

    qInfo() << "Successfully connected to kext!";

    return 0;
}

// Send PIDs to Kext for the excluded apps running prior to enabling split tunnel.
// This is necessary as existing connections must be excluded in a different way
// We can't just attach to the socket as the sockets already exist and are connected,
// instead we must analyse the IP traffic.
void KextClient::sendExistingPids(const QVector<QString> &excludedApps)
{
    std::array<pid_t, PidFinder::maxPids> pids{};
    PidFinder finder{excludedApps};
    auto pidSet = finder.pids();

    // Copy across from our set to an array
    std::copy(pidSet.begin(), pidSet.end(), begin(pids));

    // Tell the kext
    ::setsockopt(_sockFd, SYSPROTO_CONTROL,
        PIA_WHITELIST_PIDS, pids.data(), pids.size() * sizeof(pid_t));

    // Get open TCP sockets for those PIDs too
    auto portsVec = finder.ports(pidSet);
    std::array<WhitelistPort, PidFinder::maxPorts> ports{};
    std::copy(portsVec.begin(), portsVec.end(), ports.begin());
    ::setsockopt(_sockFd, SYSPROTO_CONTROL, PIA_WHTIELIST_PORTS, ports.data(),
                 ports.size() * sizeof(ports[0]));
}

void KextClient::removeCurrentBoundRoute()
{
    // Nothing to do, so return - this will happen when we first start up the KextClient
    if(_previousNetScan.gatewayIp().isEmpty() || _previousNetScan.interfaceName().isEmpty())
    {
        qInfo() << "Did not find bound route, nothing to remove";
        return;
    }

    qInfo() << "Removing bound route";
    QString routeCmd = QStringLiteral("route delete 0.0.0.0 %1 -ifscope %2")
        .arg(_previousNetScan.gatewayIp()).arg(_previousNetScan.interfaceName());
    qInfo() << "Executing" << routeCmd;
    ::shellExecute(routeCmd);
}

void KextClient::createBoundRoute(const OriginalNetworkScan &updatedScan)
{
    if(updatedScan.gatewayIp().isEmpty() || updatedScan.interfaceName().isEmpty())
    {
        qInfo() << "Not creating bound route rule - gatewayIp or interfaceName is empty!";
        return;
    }

    qInfo() << "Adding new bound route";
    QString routeCmd = QStringLiteral("route add -net 0.0.0.0 %1 -ifscope %2")
        .arg(updatedScan.gatewayIp()).arg(updatedScan.interfaceName());
    qInfo() << "Executing" << routeCmd;
    ::shellExecute(routeCmd);
}

void KextClient::updateFirewall(QString ipAddress)
{
    if(ipAddress.isEmpty())
    {
        qInfo() << "Not updating firewall rule - ipAddress is empty!";
        return;
    }

    qInfo() << "Updating the firewall rule for new ip" << ipAddress;
    PFFirewall::setAnchorWithRules(kSplitTunnelAnchorName,
        true, { QStringLiteral("pass out from %1 no state").arg(ipAddress)});
}

void KextClient::updateIp(QString ipAddress)
{
    if(ipAddress.isEmpty())
    {
        qInfo() << "Not updating IP - ipAddress is empty!";
        return;
    }

    u_int32_t ip_address = 0;
    ::inet_pton(AF_INET, qPrintable(ipAddress), &ip_address);
    qInfo() << "Sending ip address to kext:" << ipAddress;
    ::setsockopt(_sockFd, SYSPROTO_CONTROL, PIA_IP_SET, &ip_address, sizeof(ip_address));
}

void KextClient::updateKextFirewall(const FirewallParams &params)
{
    FirewallState updatedState { params.blockAll, params.allowLAN };

    qInfo() << "Updating Kext firewall, new state is: killswitchActive:"
            << updatedState.killswitchActive
            << "allowLAN:"
            << updatedState.allowLAN;

    if(updatedState.allowLAN != _firewallState.allowLAN || updatedState.killswitchActive != _firewallState.killswitchActive)
    {
        qInfo() << "Sending updated firewall state to kext";
        ::setsockopt(_sockFd, SYSPROTO_CONTROL, PIA_FIREWALL_STATE, &updatedState, sizeof(FirewallState));
        _firewallState = updatedState;
    }
}

void KextClient::updateNetwork(const OriginalNetworkScan &updatedScan, const FirewallParams &params)
{
    qInfo() << "Updating Network";

    // The kext maintains its own IP filter firewall and needs to be kept in sync with our pf firewall
    updateKextFirewall(params);

    // We *always* update the bound route as it's possible a user connected to another network
    // with the same gateway IP/interface/IP. In this case the system may wipe our bound route so we have to recreate it.
    // Note this does result in constantly deleting/recreating the bound route for every firewall change but it doesn't appear
    // to affect connectivity of apps, so it should be ok.
    removeCurrentBoundRoute();
    createBoundRoute(updatedScan);

    if(_previousNetScan.ipAddress() != updatedScan.ipAddress())
    {
        updateFirewall(updatedScan.ipAddress());
        updateIp(updatedScan.ipAddress());
    }

    // Update our network info
    _previousNetScan = updatedScan;
}

void KextClient::initiateConnection(const OriginalNetworkScan &netScan, const FirewallParams &params)
{
    int retryCount = 5;
    for(int i=1; i < retryCount+1; ++i)
    {
        int rv = connectToKext(netScan, params);

        // Only retry if connectToKext failed with errno set to EBUSY (errno is thread-safe)
        if(rv == 0 || errno != EBUSY)
            return;

        QThread::msleep(100);
        qWarning() << "Failed to connect to Kext, trying again. " << i << "of" << retryCount;
    }

    qWarning() << "Giving up. Failed to connect to Kext";
}

void KextClient::teardownFirewall()
{
    // Remove all firewall rules
    PFFirewall::setAnchorWithRules(kSplitTunnelAnchorName, false, QStringList{});
}

void KextClient::shutdownConnection()
{
    qInfo() << "Attempting to disconnect from Kext";
    if(_state == State::Disconnected)
    {
        qWarning() << "Already disconnected from Kext";
        return;
    }

    if(::close(_sockFd) != 0)
    {
        showError("::close");
        return;
    }

    if(_readNotifier)
    {
        _readNotifier->setEnabled(false);
        delete _readNotifier;
    }

    // Remove all firewall rules
    teardownFirewall();
    removeCurrentBoundRoute();

     _excludedApps = {};

    _sockFd = -1;

    // clear out our network info
    _previousNetScan = {};

    // Ensure we reset our Kext firewall state
    _firewallState = {};

    _state = State::Disconnected;
    qInfo() << "Successfully disconnected from Kext";
}

void KextClient::manageWebKitExclusions(QVector<QString> &excludedApps)
{
    // If the system WebKit framework is excluded, exclude this staged framework
    // path too.  Newer versions of Safari use this.
    if(excludedApps.contains(webkitFrameworkPath) &&
        !excludedApps.contains(stagedWebkitFrameworkPath))
    {
        excludedApps.push_back(stagedWebkitFrameworkPath);
    }
}

void KextClient::updateExcludedApps(QVector<QString> excludedApps)
{
    if(_state == State::Disconnected)
    {
        qWarning() << "Cannot update excluded apps, not connected to Kext";
        return;
    }

    // Possibly modify exclusions vector to handle webkit/safari apps
    manageWebKitExclusions(excludedApps);

    // If nothing has changed, just return
    if(_excludedApps == excludedApps)
        return;

    QVector<QString> removedApps;
    for(const auto &app : _excludedApps)
    {
        if(!excludedApps.contains(app))
            removedApps.push_back(app);
    }

    _excludedApps.swap(excludedApps);

    for(const auto &app : _excludedApps)
    {
        qInfo() << "Excluding:" << app;
    }

    removeApps(removedApps);
    qInfo() << "Updated excluded apps";
}

// Function below temporarily out of commission
// Need to implement it in a way that doesn't result in kernel panics
void KextClient::removeApps(const QVector<QString> removedApps)
{
    if(_state == State::Disconnected) return;

    qInfo() << "Attempting to remove apps no longer needed." << removedApps.size() << "apps found.";

    char appName[PATH_MAX] = {};
    for(const auto &app : removedApps)
    {
        strncpy(appName, qPrintable(app), sizeof(appName));
        qInfo() << "Telling kext to remove" << appName << "from fast path.";

        qInfo() << "Length of appName is " << ::strlen(appName);

        if(::strlen(appName) > 0)
            setsockopt(_sockFd, SYSPROTO_CONTROL, PIA_REMOVE_APP, appName, sizeof(appName));
    }
}

void KextClient::readFromSocket(int socket)
{
    ProcQuery procQuery = {};
    ProcQuery procResponse = {};

    ::recv(socket, &procQuery, sizeof(procQuery), 0);

    processCommand(procQuery, procResponse);

    if(procQuery.needs_reply)
        // Send reply back to Kext using setsockopt()  - ::send() turned out to be unreliable
        setsockopt(socket, SYSPROTO_CONTROL, PIA_MSG_REPLY, &procResponse, sizeof(procResponse));

    // Log the request/response for app verification (i.e should this process be excluded?)
    if(procResponse.accept)
    {
        qInfo() << QStringLiteral("<KEXT REQUEST> .command %1 .pid %2 .message_id %3")
            .arg(procQuery.command).arg(procQuery.pid).arg(procQuery.id);

        if(procQuery.needs_reply)
            qInfo() << QStringLiteral("<KEXT RESPONSE> .command %1 .pid %2 .message_id %3 .accept %4 .app_path %5")
                .arg(procResponse.command).arg(procResponse.pid).arg(procResponse.id)
                .arg(procResponse.accept).arg(procResponse.command == VerifyApp ? procResponse.app_path : "N/A");
    }
}

void KextClient::processCommand(const ProcQuery &procQuery, ProcQuery &procResponse)
{
    switch(procQuery.command)
    {
    case VerifyApp:
        verifyApp(procQuery, procResponse);
        break;
    default:
        qWarning() << "Command not recognized, got:" << procQuery.command;
        break;
    }
}

void KextClient::verifyApp(const ProcQuery &procQuery, ProcQuery &procResponse)
{
    // Copy basic details across from the query object
    procResponse.id = procQuery.id;
    procResponse.command = procQuery.command;
    procResponse.pid = procQuery.pid;

    // Convert the PID to an app path on disk
    proc_pidpath(procQuery.pid, procResponse.app_path, sizeof(procResponse.app_path));

    // Wrap the app path in a QString for convenience
    QString appPath{procResponse.app_path};

    // Check whether the app is one we want to exclude
    bool result = std::any_of(_excludedApps.begin(), _excludedApps.end(),
        [&appPath](const QString &prefix)
        {
            // On MacOS we exclude apps based on their ".app" bundle,
            // this means we don't match on entire paths, but just on prefixes
            return appPath.startsWith(prefix);
        });

    procResponse.accept = result;
}

KextMonitor::KextMonitor()
  : _lastState {NetExtensionState::Unknown}
{
}

int KextMonitor::runProc(const QString &cmd, const QStringList &args)
{
  QProcess proc;
  proc.setProgram(cmd);
  proc.setArguments(args);

  proc.start();
  proc.waitForFinished();


  qDebug () << "Running proc: " << cmd << args;
  qDebug () << "Exit code: " << proc.exitCode();
  qDebug () << "Stdout: " << QString::fromLatin1(proc.readAllStandardOutput());
  qDebug () << "Stderr: " << QString::fromLatin1(proc.readAllStandardError());

  return proc.exitCode();
}

void KextMonitor::updateState(int exitCode)
{
    NetExtensionState newState{exitCode == 0 ? NetExtensionState::Installed : NetExtensionState::NotInstalled};
    qInfo() << "Result -" << exitCode << "->" << qEnumToString(newState);

    if(newState != _lastState)
    {
        qInfo() << "Detected kext state:" << qEnumToString(newState);
        _lastState = newState;
        emit kextStateChanged(_lastState);
    }
}

void KextMonitor::checkState()
{
  qDebug () << "Checking kext load state";
  int exitCode = runProc(QStringLiteral("/usr/bin/kextutil"),
                         QStringList() << QStringLiteral("-print-diagnostics")
                                       << Path::SplitTunnelKextPath);
  updateState(exitCode);
}

bool KextMonitor::loadKext()
{
  int exitCode = runProc(QStringLiteral("/sbin/kextload"), QStringList() << Path::SplitTunnelKextPath);
  // Update state.  If the setting is enabled without having tested the state,
  // this may be the first time we learn the actual state of the extension.
  updateState(exitCode);

  // 0 indicates successful load
  return exitCode == 0;
}

bool KextMonitor::unloadKext()
{
  int exitCode = 0;

  // Unloading our kext may take a few attempts under normal conditions.
  // If we have any sockets filters still attached inside the kext they need to first be unregistered. The first unload attempt does this.
  // Assuming we successfully unregister the socket filters, our second (or shortly there after) attempt should succeed.
  // If we still cannot unload the kext after our final attempt then we have a legitimate error, so return this to the caller.
  int retryCount = 5;
  for(int i = 0; i < retryCount; ++i)
  {
      exitCode = runProc(QStringLiteral("/sbin/kextunload"), QStringList() << Path::SplitTunnelKextPath);

      // No error after first unload attempt, so early-exit
      if(exitCode == 0)
          break;

      qWarning() << "Unable to unload Kext, exit code:" << exitCode << "Retrying" << i+1 << "of" << retryCount;
      // Wait a little bit before we try to unload again
      // (to give the socket filters time to unregister)
      QThread::msleep(200);
  }

  // Don't update state; the unload result doesn't indicate the installation
  // state of the kext

  // 0 indicates successful load
  return exitCode == 0;
}
