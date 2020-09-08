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
#include "ipaddress.h"
#include "exec.h"
#include "brand.h"

#if defined(Q_OS_MACOS)
#include "mac/kext_client.h"
#elif defined(Q_OS_LINUX)
#include "linux/proc_tracker.h"
#include "linux/linux_routing.h"
#include "linux/linux_fwmark.h"
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
#if defined(Q_OS_LINUX)
    , _resolvconfWatcher{QStringLiteral("/etc/resolv.conf")}
#endif
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
    connect(this, &Daemon::networksChanged, this, &PosixDaemon::updateExistingDNS);
    connect(&_resolvconfWatcher, &FileWatcher::changed, this, &PosixDaemon::updateExistingDNS);
    updateExistingDNS();

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
    connect(_connection, &VPNConnection::stateChanged, this,
        [this](VPNConnection::State state)
        {
            if(state == VPNConnection::State::Connected)
                _macDnsMonitor.enableMonitor();
            else
                _macDnsMonitor.disableMonitor();
        });
    connect(&_kextMonitor, &KextMonitor::kextStateChanged, this,
            [this](DaemonState::NetExtensionState extState)
            {
                state().netExtensionState(qEnumToString(extState));
            });
    state().netExtensionState(qEnumToString(_kextMonitor.lastState()));

    PFFirewall::setMacDnsStubMethod(_settings.macStubDnsMethod());

    connect(&_settings, &DaemonSettings::macStubDnsMethodChanged, this,
            [this](){PFFirewall::setMacDnsStubMethod(_settings.macStubDnsMethod());});
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

    // Ensure bound routes are cleaned up and split tunnel is shutdown
    updateBoundRoute({});
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

#ifdef Q_OS_LINUX
void PosixDaemon::updateExistingDNS()
{
    qInfo() << "Networks changed, updating existing DNS";
    const auto netScan = originalNetwork();
    const QString linkTarget = QFile::symLinkTarget(QStringLiteral("/etc/resolv.conf"));
    const QString resolvBackup = Path::DaemonDataDir / QStringLiteral("pia.resolv.conf");
    auto match = QRegularExpression{"systemd"}.match(linkTarget);

    // Connected or disconnected
    QStringList rawDnsList;
    if(match.hasMatch())
    {
        // systemd-resolve was replaced with resolvectl in newer versions of
        // systemd
        if(0 == Exec::bash(QStringLiteral("which resolvectl"), true))
        {
            qInfo() << "Saving existingDNS for systemd using resolvectl";
            QString output = Exec::bashWithOutput(QStringLiteral("resolvectl dns | grep %1 | cut -d ':' -f 2-").arg(netScan.interfaceName()));
            rawDnsList = output.split(' ');
        }
        else
        {
            qInfo() << "Saving existingDNS for systemd using systemd-resolve";
            QString output = Exec::bashWithOutput(QStringLiteral("systemd-resolve --status"));
            auto outputLines = output.split('\n');
            // Find the section for this interface
            QString interfaceSectRegex = QStringLiteral(R"(^Link \d+ \()") + netScan.interfaceName() + R"(\)$)";
            int lineIdx = outputLines.indexOf(QRegularExpression{interfaceSectRegex});
            if(lineIdx < 0)
                lineIdx = outputLines.size();
            qInfo() << "Interface" << netScan.interfaceName() << "starts on line"
                << lineIdx << "/" << outputLines.size();
            // Look for the "DNS Servers:" line, then capture the first DNS
            // server from that line and all subsequent DNS servers.
            QRegularExpression dnsServerLine{R"(^( *DNS Servers: )(.*)$)"};
            QString valueIndent;
            while(++lineIdx < outputLines.size())
            {
                // If the line starts with "Link ", it's the next link section,
                // we're done.
                if(outputLines[lineIdx].startsWith("Link "))
                {
                    qInfo() << "Done on line" << lineIdx << "- starts next link";
                    break;
                }

                // If we previously found the "DNS Servers:" line, look for
                // subsequent servers by checking if the line is indented to the
                // "value" column.  Otherwise, it's the next field.  (DNS server
                // values contain colons if they're IPv6 addresses, so looking
                // for a line starting with "   <name>:" wouldn't work.)
                if(!valueIndent.isEmpty())
                {
                    if(outputLines[lineIdx].startsWith(valueIndent))
                    {
                        qInfo() << "Found additional DNS server line:" << outputLines[lineIdx];
                        rawDnsList.push_back(outputLines[lineIdx].mid(valueIndent.size()));
                    }
                    else
                    {
                        qInfo() << "Done on line" << lineIdx << "- starts next value";
                        break;
                    }
                }
                else
                {
                    auto dnsServerMatch = dnsServerLine.match(outputLines[lineIdx]);
                    if(dnsServerMatch.hasMatch())
                    {
                        // It's the "DNS Servers:" line - capture the indent
                        // level and the first value.
                        valueIndent = QString{dnsServerMatch.captured(1).size(), ' '};
                        rawDnsList.push_back(dnsServerMatch.captured(2));
                        qInfo() << "Found first DNS server line:"
                            << outputLines[lineIdx] << "- value indent length is"
                            << valueIndent.size();
                    }
                }
            }
        }
    }

    // When connected
    else if(QFile::exists(resolvBackup))
    {
        qInfo() << "Saving existing DNS non-systemd";
        QString output = Exec::bashWithOutput(QStringLiteral("cat %1 | awk '$1==\"nameserver\" { printf \"%s \", $2; }'").arg(resolvBackup));
        rawDnsList = output.split(' ');
    }

    // When not yet connected
    else
    {
        qInfo() << "Saving existing DNS non-systemd";
        QString output = Exec::bashWithOutput(QStringLiteral("cat /etc/resolv.conf | awk '$1==\"nameserver\" { printf \"%s \", $2; }'"));
        rawDnsList = output.split(' ');
    }

    std::vector<quint32> dnsIps;

    for (auto dnsServer : rawDnsList)
    {
        bool ok{ false };
        quint32 address = QHostAddress{ dnsServer }.toIPv4Address(&ok);

        if (ok)
            dnsIps.push_back(address);
    }

    qInfo() << "Existing DNS ips";

    for (auto i : dnsIps)
    {
        qInfo() << QHostAddress{ i }.toString();
    }

    _state.existingDNSServers(dnsIps);
}
#endif

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

static void updateForwardedRoutes(const FirewallParams &params, const QString &tunnelDeviceName, bool shouldBypassVpn)
{
    const auto &netScan = params.netScan;

    // If routed traffic is configured to bypass, create the default gateway
    // route in this table all the time, which ensures that it isn't briefly
    // routed into the VPN while the connection is coming up.
    if(shouldBypassVpn)
        Exec::bash(QStringLiteral("ip route replace default via %1 dev %2 table %3").arg(netScan.gatewayIp(), netScan.interfaceName(), Routing::forwardedTable));
    // Otherwise, create the VPN route for this traffic once connected.  This
    // doesn't need to be active while disconnected - the "use VPN" mode of
    // routed traffic intentionally permits traffic when disconnected, setting
    // KS=Always blocks it correctly with the blackhole route if desired.
    else if(params.hasConnected)
        Exec::bash(QStringLiteral("ip route replace default dev %1 table %2").arg(tunnelDeviceName, Routing::forwardedTable));
    // Routed = Use VPN, and not connected
    else
        Exec::bash(QStringLiteral("ip route delete default table %2").arg(Routing::forwardedTable));

    // Add blackhole fall-back route to block all forwarded traffic if killswitch is on (and disconnected)
    if(params.leakProtectionEnabled)
        Exec::bash(QStringLiteral("ip route replace blackhole default metric 32000 table %1").arg(Routing::forwardedTable));
    else
        Exec::bash(QStringLiteral("ip route delete blackhole default metric 32000 table %1").arg(Routing::forwardedTable));

    // Blackhole IPv6 for forwarded connections too, for IPv6 leak protection and killswitch
    if(params.blockIPv6)
        Exec::bash(QStringLiteral("ip -6 route replace blackhole default metric 32000 table %1").arg(Routing::forwardedTable));
    else
        Exec::bash(QStringLiteral("ip -6 route delete blackhole default metric 32000 table %1").arg(Routing::forwardedTable));
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

    PFFirewall::setTranslationEnabled(QStringLiteral("000.natVPN"), params.hasConnected);
    PFFirewall::setFilterEnabled(QStringLiteral("000.allowLoopback"), params.allowLoopback);
    PFFirewall::setFilterEnabled(QStringLiteral("100.blockAll"), params.blockAll);
    PFFirewall::setFilterEnabled(QStringLiteral("200.allowVPN"), params.allowVPN);
    PFFirewall::setFilterEnabled(QStringLiteral("250.blockIPv6"), params.blockIPv6);

    PFFirewall::setFilterEnabled(QStringLiteral("290.allowDHCP"), params.allowDHCP);
    PFFirewall::setFilterEnabled(QStringLiteral("299.allowIPv6Prefix"), netScan.hasIpv6() && params.allowLAN);
    PFFirewall::setAnchorTable(QStringLiteral("299.allowIPv6Prefix"), netScan.hasIpv6() && params.allowLAN, QStringLiteral("ipv6prefix"), {
        // First 64 bits is the IPv6 Network Prefix
        QStringLiteral("%1/64").arg(netScan.ipAddress6())});
    PFFirewall::setFilterEnabled(QStringLiteral("300.allowLAN"), params.allowLAN);
    PFFirewall::setFilterEnabled(QStringLiteral("305.allowSubnets"), params.enableSplitTunnel);
    PFFirewall::setAnchorTable(QStringLiteral("305.allowSubnets"), params.enableSplitTunnel, QStringLiteral("subnets"), subnetsToBypass(params));

    // On Mac, there are two DNS leak protection modes depending on whether we
    // are connected.
    //
    // These modes are needed to handle quirks in mDNSResponder.  It has been
    // observed sending DNS packets on the physical interface even when the
    // DNS server is properly routed via the VPN.  When resuming from sleep, it
    // also may prevent traffic from being sent over the physical interface
    // until it has received DNS responses over that interface (which we don't
    // want to allow to prevent leaks).
    //
    // - When connected, 310.blockDNS blocks access to UDP/TCP 53 on servers
    //   other than the configured DNS servers, and UDP/TCP 53 to the configured
    //   servers is forced onto the tunnel, even if the sender had bound to the
    //   physical interface.
    // - In any other state, 000.stubDNS redirects all UDP/TCP 53 to a local
    //   resolver that just returns NXDOMAIN for all queries.  This should
    //   satisfy mDNSResponder without creating leaks.
    bool macBlockDNS{false}, macStubDNS{false};
    QStringList localDnsServers, tunnelDnsServers;

    // In addition to the normal case (connected) some versions of macOS
    // set the PrimaryService Key to empty when switching networks.
    // In this case we want to use stubDNS to work-around this behavior.
    if(_state.connectionState() == QStringLiteral("Connected") &&
       !_state.macosPrimaryServiceKey().isEmpty())
    {
        macBlockDNS = params.blockDNS;
        for(const auto &address : params.effectiveDnsServers)
        {
            QHostAddress parsed{address};
            if(isModernInfraDns(parsed) || !isIpv4Local(parsed))
                tunnelDnsServers.push_back(address);
            else
                localDnsServers.push_back(address);
        }
    }
    else
    {
        macStubDNS = params.blockDNS;
    }

    PFFirewall::setTranslationEnabled(QStringLiteral("000.stubDNS"), macStubDNS);
    if(PFFirewall::setDnsStubEnabled(macStubDNS))
    {
        // Schedule a DNS cache flush since the DNS stub was disabled; important
        // if the user disables the VPN while in this state.
        _connection->scheduleMaceDnsCacheFlush();
    }
    PFFirewall::setFilterEnabled(QStringLiteral("310.blockDNS"), macBlockDNS, { {"interface", _state.tunnelDeviceName()} });
    PFFirewall::setAnchorTable(QStringLiteral("310.blockDNS"), macBlockDNS, QStringLiteral("localdns"), localDnsServers);
    PFFirewall::setAnchorTable(QStringLiteral("310.blockDNS"), macBlockDNS, QStringLiteral("tunneldns"), tunnelDnsServers);
    PFFirewall::setFilterEnabled(QStringLiteral("311.stubDNS"), macStubDNS);
    PFFirewall::setFilterEnabled(QStringLiteral("350.allowHnsd"), params.allowResolver);
    PFFirewall::setFilterEnabled(QStringLiteral("400.allowPIA"), params.allowPIA);

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
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::IPv4, QStringLiteral("100.protectLoopback"), true);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::IPv4, QStringLiteral("80.splitDNS"), params.hasConnected && params.enableSplitTunnel, IpTablesFirewall::kNatTable);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::IPv4, QStringLiteral("90.snatDNS"), params.hasConnected && params.enableSplitTunnel, IpTablesFirewall::kNatTable);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::IPv4, QStringLiteral("80.fwdSplitDNS"), params.hasConnected && params.enableSplitTunnel, IpTablesFirewall::kNatTable);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::IPv4, QStringLiteral("90.fwdSnatDNS"), params.hasConnected && params.enableSplitTunnel, IpTablesFirewall::kNatTable);

    // block VpnOnly packets when the VPN is not connected
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("340.blockVpnOnly"), !_state.vpnEnabled());
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("350.allowHnsd"), params.allowResolver && params.defaultRoute);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("350.cgAllowHnsd"), params.allowResolver && !params.defaultRoute);

    // Allow PIA Wireguard packets when PIA is allowed.  These come from the
    // kernel when using the kernel module method, so they aren't covered by the
    // allowPIA rule, which is based on GID.
    // This isn't needed for OpenVPN or userspace WG, but it doesn't do any
    // harm.
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("390.allowWg"), params.allowPIA);
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("400.allowPIA"), params.allowPIA);

    // Mark forwarded packets in all cases (so we can block when KS is on)
    IpTablesFirewall::setAnchorEnabled(IpTablesFirewall::Both, QStringLiteral("100.tagFwd"), true, IpTablesFirewall::kMangleTable);

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

    // Update routes for forwarded packets (i.e docker)
    updateForwardedRoutes(params, _state.tunnelDeviceName(),
        params.enableSplitTunnel && !_settings.routedPacketsOnVPN());

#endif

#ifdef Q_OS_MACOS
    // Subnet bypass routing for MacOs
    // Linux doesn't make use of this, it uses packet tagging and routing policies instead.
    _subnetBypass.updateRoutes(params);
#endif

    updateBoundRoute(params);
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
    file.writeText("Overview", diagnosticsOverview());
    file.writeCommand("ifconfig", "ifconfig", emptyArgs);
    file.writeCommand("Routes (netstat -nr)", "netstat", QStringList{QStringLiteral("-nr")});
    file.writeCommand("PF (pfctl -sr)", "pfctl", QStringList{QStringLiteral("-sr")});
    file.writeCommand("PF (pfctl -sR)", "pfctl", QStringList{QStringLiteral("-sR")});
    file.writeCommand("PF (App anchors)", "pfctl", QStringList{QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/*"), QStringLiteral("-sr")});
    file.writeCommand("PF (localdns table)", "pfctl", QStringList{QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/310.blockDNS"), "-t", "localdns", "-T", "show"});
    file.writeCommand("PF (tunneldns table)", "pfctl", QStringList{QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/310.blockDNS"), "-t", "tunneldns", "-T", "show"});
    file.writeCommand("PF (allowed subnets table)", "pfctl", QStringList{QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/305.allowSubnets"), "-t", "subnets", "-T", "show"});
    file.writeCommand("PF (pfctl -sR)", "pfctl", QStringList{QStringLiteral("-sR")});
    file.writeCommand("PF (NAT anchors)", "pfctl", QStringList{QStringLiteral("-sn")});
    file.writeCommand("PF (App NAT anchors)", "pfctl", QStringList{QStringLiteral("-sn"), QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/*")});
    file.writeCommand("PF (000.natVPN)", "pfctl", QStringList{QStringLiteral("-sn"), QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/000.natVPN")});
    file.writeCommand("PF (000.stubDNS)", "pfctl", QStringList{QStringLiteral("-sn"), QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/000.stubDNS")});
    file.writeCommand("dig (dig www.pia.com)", "dig", QStringList{QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("+time=4"), QStringLiteral("+tries=1")});
    file.writeCommand("dig (dig @piadns www.pia.com)", "dig", QStringList{QStringLiteral("@%1").arg(piaLegacyDnsPrimary()), QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("+time=4"), QStringLiteral("+tries=1")});
    file.writeCommand("ping (ping www.pia.com)", "ping", QStringList{QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("-c1"), QStringLiteral("-W1")});
    file.writeCommand("ping (ping 202.222.18.222)", "ping", QStringList{piaLegacyDnsPrimary(),
        QStringLiteral("-c1"), QStringLiteral("-W1"), QStringLiteral("-n")});
    file.writeCommand("System log (last 4s)", "log", QStringList{"show", "--last",  "4s"});
    file.writeCommand("Third-party kexts", "/bin/bash", QStringList{QStringLiteral("-c"), QStringLiteral("kextstat | grep -v com.apple")});
    file.writeText("kext syslog", _kextMonitor.getKextLog());
    file.writeCommand("DNS (scutil --dns)", "scutil", QStringList{QStringLiteral("--dns")});
    file.writeCommand("HTTP Proxy (scutil --proxy)", "scutil", QStringList{QStringLiteral("--proxy")});
    file.writeCommand("scutil (scutil --nwi)", "scutil", QStringList{QStringLiteral("--nwi")});
    scutilDNSDiagnostics(file);
#elif defined(Q_OS_LINUX)
    file.writeCommand("OS Version", "uname", QStringList{QStringLiteral("-a")});
    file.writeText("Overview", diagnosticsOverview());
    file.writeCommand("Distro", "lsb_release", QStringList{QStringLiteral("-a")});
    file.writeCommand("ifconfig", "ifconfig", emptyArgs);
    file.writeCommand("ip addr", "ip", QStringList{QStringLiteral("addr")});
    file.writeCommand("netstat -nr", "netstat", QStringList{QStringLiteral("-nr")});
    // Grab the routing tables from iproute2 also - we hope to change OpenVPN
    // from ifconfig to iproute2 at some point as long as this is always present
    file.writeCommand("ip route show", "ip", QStringList{"route", "show"});
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
    file.writeCommand("dig (dig @piadns www.pia.com)", "dig", QStringList{QStringLiteral("@%1").arg(piaLegacyDnsPrimary()), QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("+time=4"), QStringLiteral("+tries=1")});
    file.writeCommand("ping (ping www.pia.com)", "ping", QStringList{QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("-c1"), QStringLiteral("-W1")});
    file.writeCommand("ping (ping 202.222.18.222)", "ping", QStringList{piaLegacyDnsPrimary(),
        QStringLiteral("-c1"), QStringLiteral("-W1"), QStringLiteral("-n")});
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
    file.writeText("cat piavpnonly: cgroup.procs", Exec::bashWithOutput("cat /sys/fs/cgroup/net_cls/piavpnonly/cgroup.procs"));
    file.writeText("ps -p piavpnonly", Exec::bashWithOutput("cat /sys/fs/cgroup/net_cls/piavpnonly/cgroup.procs | xargs -n1 ps -p"));
    file.writeText("cat piavpnexclusions: cgroup.procs", Exec::bashWithOutput("cat /sys/fs/cgroup/net_cls/piavpnexclusions/cgroup.procs"));
    file.writeText("ps -p piavpnexclusions", Exec::bashWithOutput("cat /sys/fs/cgroup/net_cls/piavpnexclusions/cgroup.procs | xargs -n1 ps -p"));
    file.writeCommand("ip rule list", "ip", QStringList{"rule", "list"});
    file.writeCommand("ip route show table " BRAND_CODE "vpnrt", "ip", QStringList{"route", "show", "table", BRAND_CODE "vpnrt"});
    file.writeCommand("ip route show table " BRAND_CODE "vpnWgrt", "ip", QStringList{"route", "show", "table", BRAND_CODE "vpnWgrt"});
    file.writeCommand("ip route show table " BRAND_CODE "vpnOnlyrt", "ip", QStringList{"route", "show", "table", BRAND_CODE "vpnOnlyrt"});
    file.writeCommand("ip route show table " BRAND_CODE "vpnFwdrt", "ip", QStringList{"route", "show", "table", BRAND_CODE "vpnFwdrt"});
    file.writeCommand("WireGuard Kernel Logs", "bash", QStringList{"-c", "dmesg | grep -i wireguard | tail -n 200"});
    // Info about the wireguard kernel module (whether it is loaded and/or available)
    file.writeCommand("ls -ld /sys/modules/wireguard", "ls", {"-ld", "/sys/modules/wireguard"});
    file.writeCommand("modprobe --show-depends wireguard", "modprobe", {"--show-depends", "wireguard"});
    // Info about libnl libraries
    file.writeCommand("ldconfig -p | grep libnl", "bash", QStringList{"-c", "ldconfig -p | grep libnl"});
#endif
}

void PosixDaemon::updateBoundRoute(const FirewallParams &params)
{
#if defined(Q_OS_MAC)
    auto createBoundRoute = [](const QString &ipAddress, const QString &interfaceName)
    {
        // Checked by caller (OriginalNetworkScan::ipv4Valid() below)
        Q_ASSERT(!ipAddress.isEmpty());
        Q_ASSERT(!interfaceName.isEmpty());

        Exec::bash(QStringLiteral("route add -net 0.0.0.0 %1 -ifscope %2").arg(ipAddress, interfaceName));
    };
    auto removeBoundRoute = [](const QString &ipAddress, const QString &interfaceName)
    {
        // Checked by caller (OriginalNetworkScan::ipv4Valid() below)
        Q_ASSERT(!ipAddress.isEmpty());
        Q_ASSERT(!interfaceName.isEmpty());

        Exec::bash(QStringLiteral("route delete 0.0.0.0 -interface %1 -ifscope %1").arg(interfaceName));
    };


    // When we have connected (even if we're currently reconnecting), create a
    // bound route for the physical interface.  This has two purposes:
    // - Split tunnel - it allows "bypass" apps to bind to the physical
    //   interface
    // - DNS leak protection - we allow apps to try to send DNS packets out the
    //   physical interface toward the configured DNS servers, but then force
    //   them into the tunnel anyway.  (mDNSResponder does this on 10.15.4+.)
    if(params.hasConnected)
    {
        // Remove the previous bound route if it's present and different
        if(_boundRouteNetScan.ipv4Valid() &&
           (_boundRouteNetScan.gatewayIp() != params.netScan.gatewayIp() ||
            _boundRouteNetScan.interfaceName() != params.netScan.interfaceName()))
        {
            qInfo() << "Network has changed from"
                << _boundRouteNetScan.interfaceName() << "/"
                << _boundRouteNetScan.gatewayIp() << "to"
                << params.netScan.interfaceName() << "/"
                << params.netScan.gatewayIp() << "- create new bound route";
            removeBoundRoute(_boundRouteNetScan.gatewayIp(),
                                    _boundRouteNetScan.interfaceName());
        }

        // Add the new bound route.  Do this even if it doesn't seem to have
        // changed, because the route can be lost if the user switches to a new
        // network on the same interface with the same gateway (common with
        // 2.4GHz <-> 5GHz network switching)
        if(params.netScan.ipv4Valid())
        {
            // Trace this only when it appears to be new
            if(!_boundRouteNetScan.ipv4Valid())
            {
                qInfo() << "Creating bound route for new network"
                    << params.netScan.interfaceName() << "/"
                    << params.netScan.gatewayIp();
            }
            createBoundRoute(params.netScan.gatewayIp(), params.netScan.interfaceName());
        }

        _boundRouteNetScan = params.netScan;
    }
    else
    {
        // Remove the bound route if it's there
        if(_boundRouteNetScan.ipv4Valid())
        {
            removeBoundRoute(_boundRouteNetScan.gatewayIp(),
                                    _boundRouteNetScan.interfaceName());
        }
        _boundRouteNetScan = {};
    }
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
        // Inform of Network changes
        // Note we do not check first for _splitTunnelNetScan != params.netScan as
        // it's possible a user connected to a new network with the same gateway and interface and IP (i.e switching from 5g to 2.4g)
        updateSplitTunnel(params, _state.tunnelDeviceName(),
                          _state.tunnelDeviceLocalAddress(),
                          params.excludeApps, params.vpnOnlyApps);
    }

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
