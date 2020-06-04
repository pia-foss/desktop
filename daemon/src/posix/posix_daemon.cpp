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
#line HEADER_FILE("posix/posix_daemon.cpp")

#include "posix_daemon.h"

#include "posix.h"
#include "posix_firewall_pf.h"
#include "posix_firewall_iptables.h"
#include "path.h"
#include "exec.h"
#include "brand.h"

#if defined(Q_OS_MACOS)
#include "mac/kext_client.h"
#elif defined(Q_OS_LINUX)
#include "linux/proc_tracker.h"
#endif

#include <QFileSystemWatcher>
#include <QSocketNotifier>

#include <initializer_list>

#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>

static void ignoreSignals(std::initializer_list<int> sigs)
{
    for (int sig : sigs)
        ::signal(sig, SIG_IGN);
}

#define VPN_GROUP BRAND_CODE "vpn"

void setUidAndGid()
{
    // Make sure we're running as root:VPN_GROUP
    uid_t uid = geteuid();
    if (uid != 0)
    {
        struct passwd* pw = getpwuid(uid);
        qFatal("Running as user %s (%d); must be root.", pw && pw->pw_name ? pw->pw_name : "<unknown>", uid);
    }
    struct group* gr = getgrnam(VPN_GROUP);
    if (!gr)
    {
        qFatal("Group '" VPN_GROUP "' does not exist.");
        return;
    }

    // Set both real and effective GID.  They both must be set so the process
    // doesn't look like a setgid process, which would prevent child processes
    // from using $ORIGIN in their RPATH.
    if (setegid(gr->gr_gid) == -1 || setgid(gr->gr_gid) == -1)
    {
        qFatal("Failed to set group id to %d (%d: %s)", gr->gr_gid, errno, qPrintable(qt_error_string(errno)));
    }
    // Set the setgid bit on the support tool binary
    [](const char* path, gid_t gid) {
        if (chown(path, 0, gid) || chmod(path, 02755))
        {
            qWarning("Failed to exclude support tool from killswitch (%d: %s)", errno, qPrintable(qt_error_string(errno)));
        }
    } (qUtf8Printable(Path::SupportToolExecutable), gr->gr_gid);
}

// Only used by MacOs - Linux uses routing policies and packet tagging instead
class PosixRouteManager : public RouteManager
{
public:
    virtual void addRoute(const QString &subnet, const QString &gatewayIp, const QString &interfaceName, uint32_t metric=0) const override;
    virtual void removeRoute(const QString &subnet, const QString &gatewayIp, const QString &interfaceName) const override;
};

void PosixRouteManager::addRoute(const QString &subnet, const QString &gatewayIp, const QString &interfaceName, uint32_t metric) const
{
#if defined(Q_OS_MACOS)
    qInfo() << "Adding bypass route for" << subnet;
    Exec::cmd("route", {"add", "-net", subnet, gatewayIp});
#endif
}

void PosixRouteManager::removeRoute(const QString &subnet, const QString &gatewayIp, const QString &interfaceName) const
{
#if defined(Q_OS_MACOS)
    qInfo() << "Removing bypass route for" << subnet;
    Exec::cmd("route", {"delete", "-net", subnet, gatewayIp});
#endif
}

PosixDaemon::PosixDaemon()
    : Daemon{}, _enableSplitTunnel{false},
      _subnetBypass{std::make_unique<PosixRouteManager>()}
{
    connect(&_signalHandler, &UnixSignalHandler::signal, this, &PosixDaemon::handleSignal);
    ignoreSignals({ SIGPIPE });

#ifdef Q_OS_MACOS
    PFFirewall::install();

    prepareSplitTunnel<KextClient>();
#endif

#ifdef Q_OS_LINUX
    IpTablesFirewall::install();

    // There's no installation required for split tunnel on Linux (!)
    _state.netExtensionState(qEnumToString(DaemonState::NetExtensionState::Installed));

    // Check for the WireGuard kernel module
    connect(&_linuxModSupport, &LinuxModSupport::modulesUpdated, this,
            &PosixDaemon::checkLinuxModules);
    checkLinuxModules();

    prepareSplitTunnel<ProcTracker>();
#endif

    auto daemonBinaryWatcher = new QFileSystemWatcher(this);
    daemonBinaryWatcher->addPath(Path::DaemonExecutable);
    connect(daemonBinaryWatcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        if (!QFile::exists(Path::DaemonExecutable))
        {
            qInfo() << "Daemon executable appears to have been deleted; shutting down...";
#ifdef Q_OS_MAC
            // On macOS, also remove us from launchctl and remove the LaunchDaemon plist
            QProcess::startDetached(QStringLiteral("/bin/launchctl remove " BRAND_IDENTIFIER ".daemon"));
            QFile::remove(QStringLiteral("/Library/LaunchDaemons/" BRAND_IDENTIFIER ".daemon.plist"));
#endif
            stop();
        }
    });

#ifdef Q_OS_MAC
    connect(&_kextMonitor, &KextMonitor::kextStateChanged, this,
            [this](DaemonState::NetExtensionState extState)
            {
                state().netExtensionState(qEnumToString(extState));
            });
    state().netExtensionState(qEnumToString(_kextMonitor.lastState()));
#endif

#ifdef Q_OS_LINUX
    // Activate some routing components handled by IpTablesFirewall only when
    // the daemon is active
    connect(this, &PosixDaemon::firstClientConnected, this, []()
        {
            qInfo() << "Daemon is active, activate Linux routing rules";
            IpTablesFirewall::activate();
        });
    connect(this, &PosixDaemon::lastClientDisconnected, this, []()
        {
            qInfo() << "Daemon is inactive, deactivate Linux routing rules";
            IpTablesFirewall::deactivate();
        });
#endif
}

PosixDaemon::~PosixDaemon()
{
#ifdef Q_OS_MACOS
    PFFirewall::uninstall();
#endif

#ifdef Q_OS_LINUX
    IpTablesFirewall::uninstall();
#endif

    // Ensure split tunnel is shutdown
    toggleSplitTunnel({});
}

std::shared_ptr<NetworkAdapter> PosixDaemon::getNetworkAdapter()
{
    return {};
}

void PosixDaemon::handleSignal(int sig) Q_DECL_NOEXCEPT
{
    qInfo("Received signal %d", sig);
    switch (sig)
    {
    case SIGINT:
    case SIGTERM:
    case SIGHUP:
        stop();
        break;
    default:
        break;
    }
}

#if defined(Q_OS_LINUX)
// Update the 100.vpnTunOnly rule with the current tunnel device name and local
// address.  If it's updated and the anchor should be enabled, returns true.
// Otherwise, returns false - the anchor should be disabled.
static bool updateVpnTunOnlyAnchor(bool hasConnected, QString tunnelDeviceName, QString tunnelDeviceLocalAddress)
{
    if(hasConnected)
    {
        if(tunnelDeviceName.isEmpty() || tunnelDeviceLocalAddress.isEmpty())
        {
            qWarning() << "Not enabling 100.vpnTunOnly rule, do not have tunnel device config:"
                << tunnelDeviceName << "-" << tunnelDeviceLocalAddress;
            return false;
        }

        qInfo() << "Enabling 100.vpnTunOnly rule for tun device"
            << tunnelDeviceName << "-" << tunnelDeviceLocalAddress;
        IpTablesFirewall::replaceAnchor(
            IpTablesFirewall::IPv4,
            QStringLiteral("100.vpnTunOnly"),
            {
                QStringLiteral("! -i %1 -d %2 -m addrtype ! --src-type LOCAL -j DROP")
                .arg(tunnelDeviceName, tunnelDeviceLocalAddress),
            },
            IpTablesFirewall::kRawTable
        );
        return true;
    }

    return false;
}
#endif

#if defined(Q_OS_MACOS)
// Figure out which subnets we need to bypass for the allowSubnets rule on MacOs
static QStringList subnetsToBypass(const FirewallParams &params)
{
    if(params.bypassIpv6Subnets.isEmpty())
        // No IPv6 subnets, so just return IPv4
        return QStringList{params.bypassIpv4Subnets.toList()};
    else
        // If we have any IPv6 subnets then Whitelist link-local/broadcast IPv6 ranges too.
        // These are required by IPv6 Neighbor Discovery
        return QStringList{"fe80::/10", "ff00::/8"}
               + (params.bypassIpv4Subnets + params.bypassIpv6Subnets).toList();
}
#endif

void PosixDaemon::applyFirewallRules(const FirewallParams& params)
{
    const auto &netScan{params.netScan};
    // TODO: Just one more tiny step of refactoring needed :)
#if defined(Q_OS_MACOS)
    // double-check + ensure our firewall is installed and enabled. This is necessary as
    // other software may disable pfctl before re-enabling with their own rules (e.g other VPNs)
    if (!PFFirewall::isInstalled()) PFFirewall::install();

    PFFirewall::ensureRootAnchorPriority();
    PFFirewall::setAnchorEnabled(QStringLiteral("000.allowLoopback"), params.allowLoopback);
    PFFirewall::setAnchorEnabled(QStringLiteral("100.blockAll"), params.blockAll);
    PFFirewall::setAnchorEnabled(QStringLiteral("200.allowVPN"), params.allowVPN);
    PFFirewall::setAnchorEnabled(QStringLiteral("250.blockIPv6"), params.blockIPv6);
    PFFirewall::setAnchorEnabled(QStringLiteral("290.allowDHCP"), params.allowDHCP);
    PFFirewall::setAnchorEnabled(QStringLiteral("299.allowIPv6Prefix"), netScan.hasIpv6() && params.allowLAN);
    PFFirewall::setAnchorTable(QStringLiteral("299.allowIPv6Prefix"), netScan.hasIpv6() && params.allowLAN, QStringLiteral("ipv6prefix"), {
        // First 64 bits is the IPv6 Network Prefix
        QStringLiteral("%1/64").arg(netScan.ipAddress6())});
    PFFirewall::setAnchorEnabled(QStringLiteral("300.allowLAN"), params.allowLAN);
    PFFirewall::setAnchorEnabled(QStringLiteral("305.allowSubnets"), params.enableSplitTunnel);
    PFFirewall::setAnchorTable(QStringLiteral("305.allowSubnets"), params.enableSplitTunnel, QStringLiteral("subnets"), subnetsToBypass(params));
    PFFirewall::setAnchorEnabled(QStringLiteral("310.blockDNS"), params.blockDNS);
    PFFirewall::setAnchorTable(QStringLiteral("310.blockDNS"), params.blockDNS, QStringLiteral("dnsaddr"), params.effectiveDnsServers);
    PFFirewall::setAnchorEnabled(QStringLiteral("350.allowHnsd"), params.allowHnsd);
    PFFirewall::setAnchorEnabled(QStringLiteral("400.allowPIA"), params.allowPIA);
#elif defined(Q_OS_LINUX)

   // double-check + ensure our firewall is installed and enabled
    if(!IpTablesFirewall::isInstalled()) IpTablesFirewall::install();

    // Note: rule precedence is handled inside IpTablesFirewall
    IpTablesFirewall::ensureRootAnchorPriority();

    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("000.allowLoopback"), params.allowLoopback);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("100.blockAll"), params.blockAll);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("200.allowVPN"), params.allowVPN);

    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::IPv6, QStringLiteral("250.blockIPv6"), params.blockIPv6);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("290.allowDHCP"), params.allowDHCP);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::IPv6, QStringLiteral("299.allowIPv6Prefix"), netScan.hasIpv6() && params.allowLAN);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("300.allowLAN"), params.allowLAN);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("305.allowSubnets"), params.enableSplitTunnel);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("310.blockDNS"), params.blockDNS);

    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::IPv4, QStringLiteral("320.allowDNS"), params.blockDNS);

    // block VpnOnly packets when the VPN is not connected
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("340.blockVpnOnly"), !_state.vpnEnabled());
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("350.allowHnsd"), params.allowHnsd && params.defaultRoute);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("350.cgAllowHnsd"), params.allowHnsd && !params.defaultRoute);

    // Allow PIA Wireguard packets when PIA is allowed.  These come from the
    // kernel when using the kernel module method, so they aren't covered by the
    // allowPIA rule, which is based on GID.
    // This isn't needed for OpenVPN or userspace WG, but it doesn't do any
    // harm.
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("390.allowWg"), params.allowPIA);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("400.allowPIA"), params.allowPIA);

    // Update and apply our rules to ensure VPN packets are only accepted on the
    // tun interface, mitigates CVE-2019-14899: https://seclists.org/oss-sec/2019/q4/122
    bool enableVpnTunOnly = updateVpnTunOnlyAnchor(params.hasConnected,
                                                   _state.tunnelDeviceName(),
                                                   _state.tunnelDeviceLocalAddress());
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::IPv4,
                                       QStringLiteral("100.vpnTunOnly"),
                                       enableVpnTunOnly,
                                       IpTablesFirewall::kRawTable);

    // Update dynamic rules that depend on info such as the adapter name and/or DNS servers
    _firewall.updateRules(params);
#endif

#ifdef Q_OS_MACOS
    // Subnet bypass routing for MacOs
    // Linux doesn't make use of this, it uses packet tagging and routing policies instead.
    _subnetBypass.updateRoutes(params);
#endif

    toggleSplitTunnel(params);
}


QJsonValue PosixDaemon::RPC_installKext()
{
#ifdef Q_OS_MAC
    // Running checkState should perform the installation
    _kextMonitor.checkState();
    // Return the new state in the response (the update would be delivered
    // asynchronously)
    return QJsonValue{qEnumToString(_kextMonitor.lastState())};
#else
    throw Error{HERE, Error::Code::Unknown};    // Not implemented
#endif
}

#ifdef Q_OS_MAC
void scutilDNSDiagnostics(DiagnosticsFile &file)
{
    QString primaryService = Exec::bashWithOutput("echo 'get State:/Network/Global/IPv4\nd.show' | scutil | awk '/PrimaryService/{print $3}'");
    file.writeText("scutil Global:DNS", Exec::bashWithOutput("echo 'get State:/Network/Global/DNS\nd.show' | scutil"));
    file.writeText("scutil Global:IPv4", Exec::bashWithOutput("echo 'get State:/Network/Global/IPv4\nd.show' | scutil"));
    file.writeText("scutil State:PrimaryService:IPv4", Exec::bashWithOutput(QStringLiteral("echo 'get State:/Network/Service/%1/IPv4\nd.show' | scutil").arg(primaryService)));
    file.writeText("scutil State:PrimaryService:DNS", Exec::bashWithOutput(QStringLiteral("echo 'get State:/Network/Service/%1/DNS\nd.show' | scutil").arg(primaryService)));
    file.writeText("scutil Setup:PrimaryService:IPv4", Exec::bashWithOutput(QStringLiteral("echo 'get Setup:/Network/Service/%1/IPv4\nd.show' | scutil").arg(primaryService)));
    file.writeText("scutil Setup:PrimaryService:DNS", Exec::bashWithOutput(QStringLiteral("echo 'get Setup:/Network/Service/%1/DNS\nd.show' | scutil").arg(primaryService)));
    file.writeText("scutil State:PIA", Exec::bashWithOutput("echo 'get State:/Network/PrivateInternetAccess\nd.show' | scutil"));
    file.writeText("scutil State:PIA:DNS", Exec::bashWithOutput("echo 'get State:/Network/PrivateInternetAccess/DNS\nd.show' | scutil"));
    file.writeText("scutil State:PIA:OldStateDNS", Exec::bashWithOutput("echo 'get State:/Network/PrivateInternetAccess/OldStateDNS\nd.show' | scutil"));
    file.writeText("scutil State:PIA:OldSetupDNS", Exec::bashWithOutput("echo 'get State:/Network/PrivateInternetAccess/OldSetupDNS\nd.show' | scutil"));
}
#endif

void PosixDaemon::writePlatformDiagnostics(DiagnosticsFile &file)
{
    QStringList emptyArgs;

#if defined(Q_OS_MAC)
    file.writeCommand("OS Version", "sw_vers", emptyArgs);
    file.writeCommand("ifconfig", "ifconfig", emptyArgs);
    file.writeCommand("PF (pfctl -sr)", "pfctl", QStringList{QStringLiteral("-sr")});
    file.writeCommand("PF (pfctl -sR)", "pfctl", QStringList{QStringLiteral("-sR")});
    file.writeCommand("PF (App anchors)", "pfctl", QStringList{QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/*"), QStringLiteral("-sr")});
    file.writeCommand("PF (dnsaddr table)", "pfctl", QStringList{QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/310.blockDNS"), "-t", "dnsaddr", "-T", "show"});
    file.writeCommand("PF (pfctl -sR)", "pfctl", QStringList{QStringLiteral("-sR")});
    file.writeCommand("dig (dig www.pia.com)", "dig", QStringList{QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("+time=4"), QStringLiteral("+tries=1")});
    file.writeCommand("dig (dig @piadns www.pia.com)", "dig", QStringList{QStringLiteral("@%1").arg(piaLegacyDnsPrimary), QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("+time=4"), QStringLiteral("+tries=1")});
    file.writeCommand("ping (ping www.pia.com)", "ping", QStringList{QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("-c1"), QStringLiteral("-W1")});
    file.writeCommand("ping (ping 202.222.18.222)", "ping", QStringList{piaLegacyDnsPrimary,
        QStringLiteral("-c1"), QStringLiteral("-W1"), QStringLiteral("-n")});
    file.writeCommand("DNS (scutil --dns)", "scutil", QStringList{QStringLiteral("--dns")});
    file.writeCommand("scutil (scutil --proxy)", "scutil", QStringList{QStringLiteral("--proxy")});
    file.writeCommand("scutil (scutil --nwi)", "scutil", QStringList{QStringLiteral("--nwi")});
    scutilDNSDiagnostics(file);
    file.writeCommand("Routes (netstat -nr)", "netstat", QStringList{QStringLiteral("-nr")});
    file.writeCommand("Third-party kexts", "/bin/bash", QStringList{QStringLiteral("-c"), QStringLiteral("kextstat | grep -v com.apple")});
    file.writeText("kext syslog", _kextMonitor.getKextLog());
#elif defined(Q_OS_LINUX)
    file.writeCommand("OS Version", "uname", QStringList{QStringLiteral("-a")});
    file.writeCommand("Distro", "lsb_release", QStringList{QStringLiteral("-a")});
    file.writeCommand("ifconfig", "ifconfig", emptyArgs);
    file.writeCommand("ip addr", "ip", QStringList{QStringLiteral("addr")});
    // Write iptables dumps for each table that PIA uses
    auto dumpIpTables = [&](const QString &table)
    {
        for(const auto &cmd : QStringList{"iptables", "ip6tables"})
        {
            file.writeCommand(QString("%1 -t %2 -S").arg(cmd, table), cmd,
                              {QStringLiteral("-t"), table, QStringLiteral("-S")});
        }
    };
    dumpIpTables(QStringLiteral("filter"));
    dumpIpTables(QStringLiteral("nat"));
    dumpIpTables(QStringLiteral("raw"));
    dumpIpTables(QStringLiteral("mangle"));

    // iptables version - 1.6.1 is required for the split tunnel feature
    file.writeCommand("iptables --version", "iptables", QStringList{QStringLiteral("--version")});
    file.writeCommand("dig (dig www.pia.com)", "dig", QStringList{QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("+time=4"), QStringLiteral("+tries=1")});
    file.writeCommand("dig (dig @piadns www.pia.com)", "dig", QStringList{QStringLiteral("@%1").arg(piaLegacyDnsPrimary), QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("+time=4"), QStringLiteral("+tries=1")});
    file.writeCommand("ping (ping www.pia.com)", "ping", QStringList{QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("-c1"), QStringLiteral("-W1")});
    file.writeCommand("ping (ping 202.222.18.222)", "ping", QStringList{piaLegacyDnsPrimary,
        QStringLiteral("-c1"), QStringLiteral("-W1"), QStringLiteral("-n")});
    file.writeCommand("netstat -nr", "netstat", QStringList{QStringLiteral("-nr")});
    // Grab the routing tables from iproute2 also - we hope to change OpenVPN
    // from ifconfig to iproute2 at some point as long as this is always present
    file.writeCommand("ip route show", "ip", QStringList{"route", "show"});
    file.writeCommand("resolv.conf", "cat", QStringList{QStringLiteral("/etc/resolv.conf")});
    file.writeCommand("ls -l resolv.conf", "ls", QStringList{QStringLiteral("-l"), QStringLiteral("/etc/resolv.conf")});
    file.writeCommand("systemd-resolve --status", "systemd-resolve", QStringList{QStringLiteral("--status")});

    // Relevant only for `resolvconf` DNS (not systemd-resolve)
    // Collect the interface-specific DNS for resolvconf and the order of interfaces.
    // resolvconf works by selecting 3 nameserver lines ordered
    // according to /etc/resolvconf/interface-order. This means if there's already interfaces with higher
    // priority than tun0 wih > 3 nameservers, our own tun0 DNS servers may never be set.
    // The tracing below should reveal if this is the case.
    file.writeCommandIf(QFile::exists(QStringLiteral("/etc/resolvconf/interface-order")),
                        "resolvconf interface-order", "cat",
                        QStringList{QStringLiteral("/etc/resolvconf/interface-order")});

    // Iterate through interfaces and log their DNS servers
    QDir resolvConfInterfaceDir{"/run/resolvconf/interface"};
    if(resolvConfInterfaceDir.exists())
    {
        for(const auto &fileName : resolvConfInterfaceDir.entryList(QDir::Files))
            file.writeCommand(QStringLiteral("resolvconf interface DNS: %1").arg(fileName),
                              "cat", QStringList{QStringLiteral("/run/resolvconf/interface/%1").arg(fileName)});
    }

    // net_cls cgroup required for split tunnel
    file.writeCommand("ls -l <net_cls>", "ls", QStringList{"-l", Path::ParentVpnExclusionsFile.parent()});
    file.writeCommand("ip rule list", "ip", QStringList{"rule", "list"});
    file.writeCommand("ip route show table " BRAND_CODE "vpnrt", "ip", QStringList{"route", "show", "table", BRAND_CODE "vpnrt"});
    file.writeCommand("ip route show table " BRAND_CODE "vpnWgrt", "ip", QStringList{"route", "show", "table", BRAND_CODE "vpnWgrt"});
    file.writeCommand("ip route show table " BRAND_CODE "vpnOnlyrt", "ip", QStringList{"route", "show", "table", BRAND_CODE "vpnOnlyrt"});
    file.writeCommand("WireGuard Kernel Logs", "bash", QStringList{"-c", "dmesg | grep -i wireguard | tail -n 200"});
    // Info about the wireguard kernel module (whether it is loaded and/or available)
    file.writeCommand("ls -ld /sys/modules/wireguard", "ls", {"-ld", "/sys/modules/wireguard"});
    file.writeCommand("modprobe --show-depends wireguard", "modprobe", {"--show-depends", "wireguard"});
    // Info about libnl libraries
    file.writeCommand("ldconfig -p | grep libnl", "bash", QStringList{"-c", "ldconfig -p | grep libnl"});
#endif
}

void PosixDaemon::toggleSplitTunnel(const FirewallParams &params)
{
    qInfo() << "Tunnel device is:" << _state.tunnelDeviceName();
    QVector<QString> excludedApps = params.excludeApps;

    qInfo() <<  "Updated split tunnel - enabled:" << params.enableSplitTunnel
        << "-" << params.netScan;

    // Activate split tunnel if it's supposed to be active and currently isn't
    if(params.enableSplitTunnel && !_enableSplitTunnel)
    {
        qInfo() << "Starting Split Tunnel";
#ifdef Q_OS_MAC
        if(!_kextMonitor.loadKext())
          qWarning() << "Failed to load Kext";
        else
          qInfo() << "Successfully loaded Kext";
#endif

        startSplitTunnel(params, _state.tunnelDeviceName(),
                         _state.tunnelDeviceLocalAddress(),
                         params.excludeApps, params.vpnOnlyApps);
    }
    // Deactivate if it's supposed to be inactive but is currently active
    else if(!params.enableSplitTunnel && _enableSplitTunnel)
    {
        qInfo() << "Shutting down Split Tunnel";
        shutdownSplitTunnel();
#ifdef Q_OS_MAC
        if(!_kextMonitor.unloadKext())
            qWarning() << "Failed to unload Kext";
        else
            qInfo() << "Successfully unloaded Kext";
#endif
    }
    // Otherwise, the current active state is correct, but if we are currently
    // active, update the configuration
    else if(params.enableSplitTunnel)
    {
        // Split tunnel is already running, but network has changed
        if(_splitTunnelNetScan != params.netScan)
        {
            qInfo() << "Split tunnel Network has changed from"
                <<  _splitTunnelNetScan
                << "to:"
                << params.netScan;
        }

        // Inform of Network changes
        // Note we do not check first for _splitTunnelNetScan != params.netScan as
        // it's possible a user connected to a new network with the same gateway and interface and IP (i.e switching from 5g to 2.4g)
        updateSplitTunnel(params, _state.tunnelDeviceName(),
                          _state.tunnelDeviceLocalAddress(),
                          params.excludeApps, params.vpnOnlyApps);
    }
    _splitTunnelNetScan = params.netScan;
    _enableSplitTunnel = params.enableSplitTunnel;
}

#ifdef Q_OS_LINUX
void PosixDaemon::checkLinuxModules()
{
    bool hasWg = _linuxModSupport.hasModule(QStringLiteral("wireguard"));
    _state.wireguardKernelSupport(hasWg);
    qInfo() << "Wireguard kernel module present:" << hasWg;
}
#endif
