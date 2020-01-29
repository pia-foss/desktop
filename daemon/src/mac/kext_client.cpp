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

void KextClient::showError(QString funcName)
{
    qWarning() << QStringLiteral("%1 Error (code: %2) %3").arg(funcName).arg(errno).arg(qPrintable(qt_error_string(errno)));
}

void KextClient::initiateConnection(const FirewallParams &params, QString tunnelDeviceName,
                                    QString tunnelDeviceLocalAddress, QString tunnelDeviceRemoteAddress)
{
    qInfo() << "Attempting to connect to kext";
    if(_state == State::Connected)
    {
        qWarning() << "Already connected to kext, disconnecting before reconnecting.";
        shutdownConnection();
        QThread::msleep(200);
    }

    qInfo() << "sizeof(ProcQuery) is: " << sizeof(ProcQuery);

    ctl_info ctl_info = {};

    int sock = ::socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
    if(sock < 0)
    {
        showError("::socket");
        return;
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
        return;
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
        return;
    }

    // We're successfully connected so save the socket fd to our ivar
    _sockFd = sock;
    _state = State::Connected;

    updateSplitTunnel(params, tunnelDeviceName, tunnelDeviceLocalAddress,
                      tunnelDeviceRemoteAddress, params.excludeApps,
                      params.vpnOnlyApps);

    _readNotifier = new QSocketNotifier(sock, QSocketNotifier::Read);
    connect(_readNotifier, &QSocketNotifier::activated, this, &KextClient::readFromSocket);

    qInfo() << "Successfully connected to kext!";
}

// NOTE: we do not need to send existing PIDs for "vpnOnly" apps - this is because:
// vpnOnly apps are automatically whitelisted when routeDefault=true as they're on the VPN (which is allowed)
// and when routeDefault=false the IP filter is disabled anyway (we don't need a kernel firewall as the killswitch cannot function when routeDefault=false)
//
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

void KextClient::removeCurrentBoundRoute(const QString &ipAddress, const QString &interfaceName)
{

    if(ipAddress.isEmpty() || interfaceName.isEmpty())
    {
        qInfo() << "No bound route to remove!";
        return;
    }

    qInfo() << "Removing bound route";
    QString routeCmd = QStringLiteral("route delete 0.0.0.0 %1 -ifscope %2")
        .arg(ipAddress, interfaceName);
    qInfo() << "Executing" << routeCmd;
    ::shellExecute(routeCmd);
}

void KextClient::createBoundRoute(const QString &ipAddress, const QString &interfaceName)
{
    qInfo() << "Adding new bound route";
    QString routeCmd = QStringLiteral("route add -net 0.0.0.0 %1 -ifscope %2")
        .arg(ipAddress, interfaceName);
    qInfo() << "Executing" << routeCmd;
    ::shellExecute(routeCmd);
}

void KextClient::updateFirewall(QString ipAddress)
{
    if(ipAddress.isEmpty())
    {
        qInfo() << "Removing firewall rule - empty split tunnel IP address";
        PFFirewall::setAnchorWithRules(kSplitTunnelAnchorName, false, {});
    }
    else
    {
        qInfo() << "Updating the firewall rule for new ip" << ipAddress;
        PFFirewall::setAnchorWithRules(kSplitTunnelAnchorName,
            true, { QStringLiteral("pass out from %1 no state").arg(ipAddress)});
    }
}

void KextClient::updateIp(QString ipAddress)
{
    u_int32_t ip_address = 0;
    // If the IP address is empty, send 0 to the kernel extension.  This keeps
    // the kext in sync with the firewall rule above - the purpose of this IP is
    // to tell the kext what IP we have permitted through the firewall that it
    // needs to apply the per-app filter to, and at this point we have disabled
    // that rule.
    if(!ipAddress.isEmpty())
        ::inet_pton(AF_INET, qPrintable(ipAddress), &ip_address);
    qInfo() << "Sending ip address to kext:" << ipAddress;
    ::setsockopt(_sockFd, SYSPROTO_CONTROL, PIA_IP_SET, &ip_address, sizeof(ip_address));
}

void KextClient::updateKextFirewall(const FirewallParams &params, bool isConnected)
{
    FirewallState updatedState { params.blockAll, params.allowLAN, params.defaultRoute, isConnected };

    qInfo() << "Updating Kext firewall, new state is: killswitchActive:"
            << updatedState.killswitchActive
            << "allowLAN:"
            << updatedState.allowLAN
            << "routeDefault:"
            << params.defaultRoute
            << "isConnected:"
            << isConnected;

    if(updatedState.allowLAN != _firewallState.allowLAN || updatedState.killswitchActive != _firewallState.killswitchActive
        || updatedState.defaultRoute != _firewallState.defaultRoute || updatedState.isConnected != _firewallState.isConnected)
    {
        qInfo() << "Sending updated firewall state to kext";
        ::setsockopt(_sockFd, SYSPROTO_CONTROL, PIA_FIREWALL_STATE, &updatedState, sizeof(FirewallState));
        _firewallState = updatedState;
    }
}

void KextClient::updateNetwork(const FirewallParams &params, QString tunnelDeviceName,
                               QString tunnelDeviceLocalAddress, QString tunnelDeviceRemoteAddress)
{
    qInfo() << "Updating Network";

    // The kext maintains its own IP filter firewall and needs to be kept in sync with our pf firewall
    updateKextFirewall(params, !tunnelDeviceName.isEmpty());

    // We *always* update the bound route as it's possible a user connected to another network
    // with the same gateway IP/interface/IP. In this case the system may wipe our bound route so we have to recreate it.
    // Note this does result in constantly deleting/recreating the bound route for every firewall change but it doesn't appear
    // to affect connectivity of apps, so it should be ok.
    if(_previousNetScan.isValid())
        removeCurrentBoundRoute(_previousNetScan.gatewayIp(), _previousNetScan.interfaceName());

    // If we're connected, create a bound route for the physical interface, so
    // bound sockets for excluded apps will route correctly.
    // Don't create this when we're not connected; the network info may not be
    // up to date, and it would create a routing conflict.
    if(params.splitTunnelNetScan.isValid())
    {
        createBoundRoute(params.splitTunnelNetScan.gatewayIp(), params.splitTunnelNetScan.interfaceName());

        // If we were ignoring excluded apps before (since we didn't have this
        // info), enumerate them now to permit through the firewall
        if(!_previousNetScan.isValid())
            sendExistingPids(_excludedApps);
    }

    // If the VPN does not have the default route, create a bound route for the
    // VPN interface, so sockets explicitly bound to that interface will work
    // (they could be bound by our kext, bound by the daemon, or bound by an app
    // that is configured to use the VPN interface).
    if(!params.defaultRoute)
    {
        // We don't know the tunnel interface info right away; these are found
        // later.  (Split tunnel is started immediately to avoid blips for apps
        // that bypass the VPN.)
        if(tunnelDeviceName.isEmpty() || tunnelDeviceRemoteAddress.isEmpty())
        {
            qInfo() << "Can't create bound route for VPN network yet - interface:"
                << tunnelDeviceName << "- remote:" << tunnelDeviceRemoteAddress;
        }
        else
        {
            // We don't need to remove the previous bound route as if the IP changes that means the interface went down, which will destroy that route
            createBoundRoute(tunnelDeviceRemoteAddress, tunnelDeviceName);
        }
    }

    if(_previousNetScan.ipAddress() != params.splitTunnelNetScan.ipAddress())
    {
        updateFirewall(params.splitTunnelNetScan.ipAddress());
        updateIp(params.splitTunnelNetScan.ipAddress());
    }

    // Update our network info
    _previousNetScan = params.splitTunnelNetScan;
    _tunnelDeviceName = tunnelDeviceName;
    _tunnelDeviceLocalAddress = tunnelDeviceLocalAddress;
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
    removeCurrentBoundRoute(_previousNetScan.ipAddress(), _previousNetScan.interfaceName());

     _excludedApps = {};
     _vpnOnlyApps = {};

    _sockFd = -1;

    // clear out our network info
    _previousNetScan = {};

    // Ensure we reset our Kext firewall state
    _firewallState = {};
    _tunnelDeviceName.clear();

    // Discard the trace cache
    qInfo() << "Discarding" << _tracedResponses.size() << "trace cache entries";
    for(const auto &tracedResponse : _tracedResponses)
    {
        qInfo() << "Discarding trace cache for PID"
            << tracedResponse.response.pid << "-"
            << tracedResponse.response.app_path << "-"
            << tracedResponse.count << "responses";
    }
    _tracedResponses.clear();

    _state = State::Disconnected;
    qInfo() << "Successfully disconnected from Kext";
}

void KextClient::updateSplitTunnel(const FirewallParams &params, QString tunnelDeviceName,
                                   QString tunnelDeviceLocalAddress, QString tunnelDeviceRemoteAddress,
                                   QVector<QString> excludedApps, QVector<QString> vpnOnlyApps)
{
    // Update apps first - updateNetwork() will enumerate the updated bypass
    // apps if we start tracking bypass apps
    updateApps(params.excludeApps, params.vpnOnlyApps);
    updateNetwork(params, tunnelDeviceName, tunnelDeviceLocalAddress, tunnelDeviceRemoteAddress);
}

void KextClient::manageWebKitApps(QVector<QString> &apps)
{
    // If the system WebKit framework is excluded/vpnOnly, add this staged framework
    // path too. Newer versions of Safari use this.
    if(apps.contains(webkitFrameworkPath) &&
        !apps.contains(stagedWebkitFrameworkPath))
    {
        apps.push_back(stagedWebkitFrameworkPath);
    }
}

QVector<QString> KextClient::findRemovedApps(const QVector<QString> &newApps, const QVector<QString> &oldApps)
{
    QVector<QString> removedApps;

    for(const auto &app : oldApps)
    {
        if(!newApps.contains(app))
            removedApps.push_back(app);
    }

    return removedApps;
}

void KextClient::updateApps(QVector<QString> excludedApps, QVector<QString> vpnOnlyApps)
{
    if(_state == State::Disconnected)
    {
        qWarning() << "Cannot update excluded apps, not connected to Kext";
        return;
    }

    // Possibly modify vpnOnly/exclusions vector to handle webkit/safari apps
    manageWebKitApps(excludedApps);
    manageWebKitApps(vpnOnlyApps);

    // If nothing has changed, just return
    if(_excludedApps != excludedApps)
    {
        removeApps(findRemovedApps(excludedApps, _excludedApps));
        _excludedApps = std::move(excludedApps);
        for(const auto &app : _excludedApps) qInfo() << "Excluded Apps:" << app;
    }

    if(_vpnOnlyApps != vpnOnlyApps)
    {
        removeApps(findRemovedApps(vpnOnlyApps, _vpnOnlyApps));
        _vpnOnlyApps = std::move(vpnOnlyApps);
        for(const auto &app : _vpnOnlyApps) qInfo() << "VPN Only Apps:" << app;
    }

    qInfo() << "Updated apps";
}

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
    {
        // Send reply back to Kext using setsockopt()  - ::send() turned out to be unreliable
        setsockopt(socket, SYSPROTO_CONTROL, PIA_MSG_REPLY, &procResponse, sizeof(procResponse));

        // Log the request/response for app verification (i.e should this process be excluded?)
        if(procResponse.accept)
        {
            // Have we recently traced for this PID?
            // A linear search is OK since we limit the size of this container
            auto itTracedResponse = std::find_if(_tracedResponses.begin(), _tracedResponses.end(),
                [&](const auto &traced){return traced.response.pid == procResponse.pid;});
            TracedResponse *pTracedResponse = nullptr;
            if(itTracedResponse != _tracedResponses.end())
            {
                pTracedResponse = &*itTracedResponse;
            }
            else
            {
                // Have not recently traced this PID, add it
                _tracedResponses.push_back({});
                // If the container is over the limit, drop the oldest entry
                if(_tracedResponses.size() > 20)
                {
                    qInfo() << "Discarding trace cache for PID"
                        << _tracedResponses.front().response.pid << "-"
                        << _tracedResponses.front().response.app_path << "-"
                        << _tracedResponses.front().count << "responses";
                    _tracedResponses.pop_front();
                }
                pTracedResponse = &_tracedResponses.back();
            }
            // Consequence of above
            Q_ASSERT(pTracedResponse);
            ++pTracedResponse->count;

            // Trace if:
            // - this is one of the first 3 responses
            // - every hundredth response after that
            // - accept, rule_type, or bind_ip have changed
            if(pTracedResponse->count <= 3 || (pTracedResponse->count % 100) == 0 ||
               pTracedResponse->response.accept != procResponse.accept ||
               pTracedResponse->response.rule_type != procResponse.rule_type ||
               pTracedResponse->response.bind_ip != procResponse.bind_ip)
            {
                qInfo() << QStringLiteral("<KEXT RESPONSE> command: %1 pid: %2 (#%3) message_id: %4 accept: %5 bind_ip: %6 app_path: %7")
                    .arg(qEnumToString(procResponse.command)).arg(procResponse.pid)
                    .arg(pTracedResponse->count).arg(procResponse.id)
                    .arg(procResponse.accept).arg(QHostAddress{ntohl(procResponse.bind_ip)}.toString())
                    .arg(procResponse.command == VerifyApp ? procResponse.app_path : "N/A");
                pTracedResponse->response = procResponse;
            }
        }
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
    procResponse.accept = false;

    // Convert the PID to an app path on disk
    proc_pidpath(procQuery.pid, procResponse.app_path, sizeof(procResponse.app_path));

    // Wrap the app path in a QString for convenience
    QString appPath{procResponse.app_path};

    // Check whether the app is one we want to exclude
    auto matchesPath = [&appPath](const QVector<QString> &apps) {
        return std::any_of(apps.begin(), apps.end(),
        [&appPath](const QString &prefix)
        {
            // On MacOS we exclude apps based on their ".app" bundle,
            // this means we don't match on entire paths, but just on prefixes
            return appPath.startsWith(prefix);
        });
    };

    if(matchesPath(_excludedApps))
    {
        // Ignore excluded apps when not connected; we don't know the current IP
        // address (but they'll route to the physical interface anyway).
        if(_previousNetScan.isValid())
        {
            procResponse.accept = true;
            procResponse.rule_type = RuleType::BypassVPN;
            quint32 addr = QHostAddress{_previousNetScan.ipAddress()}.toIPv4Address();
            procResponse.bind_ip = htonl(addr);
            if(!procResponse.bind_ip)
            {
                qWarning() << "Unable to bind excluded app" << appPath
                    << "- do not have interface IP address";
                procResponse.accept = false;
            }
        }
    }
    else if(matchesPath(_vpnOnlyApps))
    {
        procResponse.accept = true;
        procResponse.rule_type = RuleType::OnlyVPN;
        quint32 addr = QHostAddress{_tunnelDeviceLocalAddress}.toIPv4Address();
        procResponse.bind_ip = htonl(addr);
        // Return 'true' for accept even if the bind_ip is null - this blocks
        // Only VPN apps when we're not connected to the VPN.
    }
}

const QStringList KextMonitor::_kextLogStreamParams{"stream", "--predicate", "senderImagePath CONTAINS \"PiaKext\""};

KextMonitor::KextMonitor()
  : _lastState{NetExtensionState::Unknown}, _loaded{false},
    _kextLogStream{{std::chrono::milliseconds(0), std::chrono::seconds(5), std::chrono::seconds(5)}}
{
    _kextLogStream.setObjectName(QStringLiteral("kext log stream"));

    // React to disk logging enable/disable
    Q_ASSERT(Logger::instance());   // Created before PosixDaemon
    connect(Logger::instance(), &Logger::configurationChanged, this,
        [this](bool logToFile, const QStringList&)
        {
            if(logToFile)
            {
                if(_loaded)
                {
                    qInfo() << "Logging enabled while loaded, start streaming kext log";
                    _kextLogStream.enable(QStringLiteral("log"), _kextLogStreamParams);
                }
            }
            else
            {
                if(_kextLogStream.isEnabled())
                {
                    qInfo() << "Logging disabled while loaded, stop streaming kext log";
                    _kextLogStream.disable();
                }

                // Discard trace logs
                _kextLog.clear();
                _oldKextLog.clear();
            }
        });

    // Store log output
    connect(&_kextLogStream, &ProcessRunner::stdoutLine, this,
        [this](const QByteArray &line)
        {
            if(_kextLog.size() + line.size() + 1 > LogChunkSize)
            {
                // Bump the old log
                _oldKextLog = std::move(_kextLog);
                _kextLog.clear();
            }

            // If this is the first trace (since startup or since debug logging
            // was enabled), or if the log was bumped, reserve the chunk size
            if(_kextLog.isEmpty())
                _kextLog.reserve(LogChunkSize);

            _kextLog += line;
            _kextLog += '\n';
        });

    // Ensure the kernel extension is not loaded from a prior run.
    // Previous versions of PIA could errantly load the kext when trying to test
    // it (the kextutil call was missing -no-load), and there was a rare race
    // condition that could prevent the kernel extension from unloading at
    // shutdown (if the daemon did not disconnect before exhausing all retries).
    //
    // As a result, if the user is upgrading to this build, the kernel extension
    // might already be loaded.
    qInfo() << "Unload kext if it was loaded by a prior run";
    unloadKext();
}

int KextMonitor::runProc(const QString &cmd, const QStringList &args,
                         QString *pStdErr)
{
  QProcess proc;
  proc.setProgram(cmd);
  proc.setArguments(args);

  proc.start();
  proc.waitForFinished();

  qDebug () << "Running proc: " << cmd << args;
  qDebug () << "Exit code: " << proc.exitCode();
  qDebug () << "Stdout: " << QString::fromLatin1(proc.readAllStandardOutput());
  QString stdErr = QString::fromLatin1(proc.readAllStandardError());
  qDebug () << "Stderr: " << stdErr;
  // If the caller wants the stderr output, provide it
  if(pStdErr)
    *pStdErr = std::move(stdErr);

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
  // kextutil -print-diagnostics would still load the kext on its own (just with
  // more diagnostics), -no-load is needed also to only test it without loading
  int exitCode = runProc(QStringLiteral("/usr/bin/kextutil"),
                         {QStringLiteral("-no-load"),
                          QStringLiteral("-print-diagnostics"),
                          Path::SplitTunnelKextPath});
  updateState(exitCode);
}

bool KextMonitor::loadKext()
{
    if(_loaded)
    {
        qWarning() << "Already loaded kext - nothing to do";
        return true;
    }

    // Start streaming kext logs if enabled
    // Do this before loading the kext to capture startup
    Q_ASSERT(Logger::instance());   // Created before PosixDaemon
    if(Logger::instance()->logToFile())
    {
        qInfo() << "Start streaming kext log";
        _kextLogStream.enable(QStringLiteral("log"), _kextLogStreamParams);
    }

    int exitCode = runProc(QStringLiteral("/sbin/kextload"), QStringList() << Path::SplitTunnelKextPath);
    // Update state.  If the setting is enabled without having tested the state,
    // this may be the first time we learn the actual state of the extension.
    updateState(exitCode);

    // 0 indicates successful load
    if(exitCode == 0)
    {
        _loaded = true;
        return true;
    }
    else if(_kextLogStream.isEnabled())
    {
        // Stop streaming kext log; load failed.
        qInfo() << "Stop streaming kext log";
        _kextLogStream.disable();
    }

    return false;
}

bool KextMonitor::unloadKext()
{
    // For robustness, unloadKext() always tries to unload, even if we don't
    // think we have loaded it.  (See KextMonitor::KextMonitor, in some cases
    // prior versions of PIA may have left it loaded.)
    qInfo() << "Unloading kext - currently loaded:" << _loaded;

    int exitCode = 0;

    // Unloading our kext may take a few attempts under normal conditions.
    // If we have any sockets filters still attached inside the kext they need to first be unregistered. The first unload attempt does this.
    // Assuming we successfully unregister the socket filters, our second (or shortly there after) attempt should succeed.
    // If we still cannot unload the kext after our final attempt then we have a legitimate error, so return this to the caller.
    int retryCount = 5;
    for(int i = 0; i < retryCount; ++i)
    {
        QString stderr;
        exitCode = runProc(QStringLiteral("/sbin/kextunload"),
                           {Path::SplitTunnelKextPath}, &stderr);

        if(exitCode == 0)
            break;

        // If it's not loaded to begin with, treat this as success (don't keep
        // trying).
        // '3' is the return code for most errors, so we have to check the
        // error text.
        if(exitCode == 3 && stderr.contains(QStringLiteral("not found for unload request")))
        {
            qInfo() << "Kext was not loaded, done trying to unload";
            exitCode = 0;
        }

        // If we succeeded, we're done
        if(exitCode == 0)
            break;

        if(i+1 < retryCount)
        {
            qWarning() << "Unable to unload Kext, exit code:" << exitCode << "Retrying" << i+1 << "of" << retryCount;
            // Wait a little bit before we try to unload again
            // (to give the socket filters time to unregister)
            QThread::msleep(200);
        }
        else
        {
            qWarning() << "Unable to unload Kext, exit code:" << exitCode << "all" << i+1 << "attempts failed";
        }
    }

    // Don't update state; the unload result doesn't indicate the installation
    // state of the kext

    // 0 indicates successful unload
    if(exitCode == 0)
    {
        // Stop streaming the kext log.
        // In theory we could leave log stream running all the time if debug logging
        // is enabled, but it sometimes stops streaming when the kext is unloaded
        // and reloaded, so for robustness we restart it when reloading the kext.
        if(_kextLogStream.isEnabled())
        {
            qInfo() << "Stop streaming kext log";
            _kextLogStream.disable();
            // Don't discard the log though, keep it in case a report is submitted.
        }

        _loaded = false;
        return true;
    }

    return false;
}

QString KextMonitor::getKextLog() const
{
    return QString::fromUtf8(_oldKextLog) + QString::fromUtf8(_kextLog);
}
