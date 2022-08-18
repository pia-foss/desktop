// Copyright (c) 2022 Private Internet Access, Inc.
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

#include <common/src/common.h>
#line HEADER_FILE("posix/posix_daemon.cpp")

#include "posix_daemon.h"

#include "posix.h"
#include <common/src/builtin/path.h>
#include <kapps_core/src/ipaddress.h>
#include <common/src/exec.h>
#include "brand.h"
#include <common/src/locations.h>

#if defined(Q_OS_MACOS)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if defined(Q_OS_LINUX)
#include <kapps_net/src/linux/linux_cn_proc.h>
#include <kapps_net/src/linux/linux_cgroup.h>
#endif

#include <QFileSystemWatcher>
#include <QSocketNotifier>
#include <QVersionNumber>

#include <initializer_list>

#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <kapps_net/src/firewall.h>

#include <QDir>

#define VPN_GROUP BRAND_CODE "vpn"

void setUidAndGid()
{
    // Make sure we're running as root:VPN_GROUP
    uid_t uid = geteuid();
    if (uid != 0)
    {
        struct passwd* pw = getpwuid(uid);
        qFatal().nospace() << "Running as user "
            << (pw && pw->pw_name ? pw->pw_name : "<unknown>") << " (" << uid
            << "); must be root.";
    }
    struct group* gr = getgrnam(VPN_GROUP);
    if (!gr)
    {
        qFatal() << "Group '" VPN_GROUP "' does not exist.";
        return;
    }

    // Set both real and effective GID.  They both must be set so the process
    // doesn't look like a setgid process, which would prevent child processes
    // from using $ORIGIN in their RPATH.
    if (setegid(gr->gr_gid) == -1 || setgid(gr->gr_gid) == -1)
    {
        qFatal().nospace() << "Failed to set group id to "
            << gr->gr_gid << " (" << errno << ": " << qt_error_string(errno)
            << ")";
    }
    // Set the setgid bit on the support tool binary
    [](const char* path, gid_t gid) {
        if (chown(path, 0, gid) || chmod(path, 02755))
        {
            qWarning().nospace() << "Failed to exclude support tool from killswitch ("
                << errno << ": " << qt_error_string(errno) << ")";
        }
    } (qUtf8Printable(Path::SupportToolExecutable), gr->gr_gid);
}

PosixDaemon::PosixDaemon()
#if defined(Q_OS_LINUX)
    : _resolvconfWatcher{QStringLiteral("/etc/resolv.conf")}
#endif
{
    kapps::net::FirewallConfig config{};
    config.daemonDataDir = Path::DaemonDataDir;
    config.resourceDir = Path::ResourceDir;
    config.executableDir = Path::ExecutableDir;
    config.installationDir = Path::InstallationDir;
    config.brandInfo.code = BRAND_CODE;
    config.brandInfo.identifier = BRAND_IDENTIFIER;

#if defined(Q_OS_LINUX)
    config.brandInfo.cgroupBase = BRAND_LINUX_CGROUP_BASE;
    config.brandInfo.fwmarkBase = BRAND_LINUX_FWMARK_BASE;
    config.bypassFile = Path::VpnExclusionsFile;
    config.vpnOnlyFile = Path::VpnOnlyFile;
    config.defaultFile = Path::ParentVpnExclusionsFile;
#elif defined(Q_OS_MACOS)
    config.unboundDnsStubConfigFile = Path::UnboundDnsStubConfigFile;
    config.unboundExecutableFile = Path::UnboundExecutable;
#endif

    KAPPS_CORE_INFO() << "Configuring firewall";
    _pFirewall.emplace(config);

    connect(&_signalHandler, &UnixSignalHandler::signal, this, &PosixDaemon::handleSignal);

    // There's no installation required for split tunnel on Mac or Linux (!)
    _state.netExtensionState(qEnumToString(StateModel::NetExtensionState::Installed));

#ifdef Q_OS_LINUX
    //IpTablesFirewall::install();

    // Check for the WireGuard kernel module
    connect(&_linuxModSupport, &LinuxModSupport::modulesUpdated, this,
            &PosixDaemon::checkLinuxModules);
    connect(this, &Daemon::networksChanged, this, &PosixDaemon::updateExistingDNS);
    connect(&_resolvconfWatcher, &FileWatcher::changed, this, &PosixDaemon::updateExistingDNS);
    updateExistingDNS();

    checkLinuxModules();

    //prepareSplitTunnel<ProcTracker>();
#endif

    checkFeatureSupport();

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

    connect(this, &Daemon::aboutToConnect, this, &PosixDaemon::onAboutToConnect);

#ifdef Q_OS_MAC
    connect(_connection, &VPNConnection::stateChanged, this,
        [this](VPNConnection::State state)
        {
            if(state == VPNConnection::State::Connected)
                _macDnsMonitor.enableMonitor();
            else
                _macDnsMonitor.disableMonitor();
        });
#endif
}

PosixDaemon::~PosixDaemon()
{
}

std::shared_ptr<NetworkAdapter> PosixDaemon::getNetworkAdapter()
{
    return {};
}

void PosixDaemon::onAboutToConnect()
{
#if defined(Q_OS_MACOS)
    // Firewall::aboutToConnectToVpn() only exists on macOS; it's used to cycle
    // the split tunnel device before connecting when split tunnel is active
    if(_pFirewall)
        _pFirewall->aboutToConnectToVpn();
#endif
}

void PosixDaemon::handleSignal(int sig) Q_DECL_NOEXCEPT
{
    qInfo() << "Received signal" << sig;
    qInfo() << "Deleting firewall rules";
    _pFirewall.clear();
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
    // Use realpath, not QFile::symlinkTarget(), to match the updown script -
    // resolvconf usually uses 2 or more symlinks to reach the actual file from
    // /etc/resolv.conf.
    QString linkTarget = Exec::bashWithOutput(QStringLiteral("realpath /etc/resolv.conf"));
    if(linkTarget.endsWith('\n'))
        linkTarget.remove(linkTarget.size()-1, 1);
    const QString resolvBackup = Path::DaemonDataDir / QStringLiteral("pia.resolv.conf");

    qInfo() << "realpath /etc/resolv.conf ->" << linkTarget;

    // systemd-resolve: connected or disconnected
    QStringList rawDnsList;
    if(linkTarget.contains("systemd"))
    {
        // systemd-resolve was replaced with resolvectl in newer versions of
        // systemd
        if(0 == Exec::bash(QStringLiteral("which resolvectl"), true))
        {
            qInfo() << "Saving existingDNS for systemd using resolvectl";
            QString output = Exec::bashWithOutput(QStringLiteral("resolvectl dns | grep %1 | cut -d ':' -f 2-").arg(QString::fromStdString(netScan.interfaceName())));
            rawDnsList = output.split(' ');
        }
        else
        {
            qInfo() << "Saving existingDNS for systemd using systemd-resolve";
            QString output = Exec::bashWithOutput(QStringLiteral("systemd-resolve --status"));
            auto outputLines = output.split('\n');
            // Find the section for this interface
            QString interfaceSectRegex = QStringLiteral(R"(^Link \d+ \()") + QString::fromStdString(netScan.interfaceName()) + R"(\)$)";
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
    // resolvconf - connected or disconnected
    else if(linkTarget == "/run/resolvconf/resolv.conf")
    {
        qInfo() << "Saving existing DNS - resolvconf";
        // Get the list of resolvconf interfaces according to the priority in
        // interface-order.  Use that to look for 'nameserver' lines while
        // excluding tun interfaces.
        QString interfaceOrder = Exec::bashWithOutput(QStringLiteral("cd /run/resolvconf/interface && /lib/resolvconf/list-records"));
        auto interfaces = interfaceOrder.split('\n');
        for(const auto &itf : interfaces)
        {
            // Ignore tun devices, otherwise look for 'nameserver' lines just
            // like resolv.conf
            if(!itf.startsWith("tun"))
            {
                QString output = Exec::bashWithOutput(QStringLiteral("cat '/run/resolvconf/interface/%1' | awk '$1==\"nameserver\" { printf \"%s \", $2; }'").arg(itf));
                rawDnsList += output.split(' ');
            }
        }
    }
    // resolv.conf - connected
    else if(QFile::exists(resolvBackup))
    {
        qInfo() << "Saving existing DNS - resolv.conf, connected";
        QString output = Exec::bashWithOutput(QStringLiteral("cat %1 | awk '$1==\"nameserver\" { printf \"%s \", $2; }'").arg(resolvBackup));
        rawDnsList = output.split(' ');
    }
    // resolv.conf - disconnected
    else
    {
        qInfo() << "Saving existing DNS - resolv.conf, disconnected";
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

void PosixDaemon::applyFirewallRules(kapps::net::FirewallParams params)
{
    // On POSIX, use the rule paths directly in excludeApps/vpnOnlyApps.
    // macOS rules apply to bundle folders.   Linux rules apply to exact
    // executables, and child processes are addressed by inheriting their
    // parent's cgroup.
    params.excludeApps.reserve(_settings.splitTunnelRules().size());
    params.vpnOnlyApps.reserve(_settings.splitTunnelRules().size());

    for(const auto &rule : _settings.splitTunnelRules())
    {
        qInfo() << "split tunnel rule:" << rule.path() << rule.mode();
        // Ignore anything with a rule type we don't recognize
        if(rule.mode() == QStringLiteral("exclude"))
            params.excludeApps.push_back(rule.path().toStdString());
        else if(rule.mode() == QStringLiteral("include"))
            params.vpnOnlyApps.push_back(rule.path().toStdString());
    }

    // When bypassing by default, force Handshake and Unbound into the VPN with
    // an "include" rule.  (Just routing the Handshake seeds into the VPN is not
    // sufficient; hnsd uses a local recursive DNS resolver that will query
    // authoritative DNS servers, and we want that to go through the VPN.)
    if(params.bypassDefaultApps)
    {
        params.vpnOnlyApps.push_back(Path::HnsdExecutable);
        params.vpnOnlyApps.push_back(Path::UnboundExecutable);
    }

    if(_pFirewall)
        _pFirewall->applyRules(params);
    else
        qInfo() << "Firewall has already been shut down, not applying firewall rules";
}

#ifdef Q_OS_MAC
QString translationDiagnostic()
{
    int syscallRet{0};
    size_t syscallRetSize{sizeof(syscallRet)};
    if(sysctlbyname("sysctl.proc_translated", &syscallRet, &syscallRetSize,
                    nullptr, 0) == -1)
    {
        if(errno == ENOENT) // No such syscall, definitely not translated
            return QStringLiteral("Not translated (ENOENT)");
        return QStringLiteral("Unknown (errno %1)").arg(errno); // Some other error
    }

    switch(syscallRet)
    {
        case 0:
            return QStringLiteral("Not translated");
        case 1:
            return QStringLiteral("Translated");
        default:
            return QStringLiteral("Unknown (returned %1)").arg(syscallRet);
    }
}

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

// Get the ip/port for a 'meta' server so we can use
// it as an endpoint for collecting tcpdump diagnostics.
// The tcpdump diagnostics trace the TCP 3-way handshake
// which is all we are interested in, so it doesn't matter if
// the request fails, so long as the 3-way handshake completes.
// These diagnostics are useful for debugging split tunnel issues on MacOS.
auto tcpdumpMetaEndpoint(const StateModel &state)
{
    struct MetaInfo
    {
        std::uint16_t port;
        QString ip;
    };

    NearestLocations nearest(state.availableLocations());
    QSharedPointer<const Location> pMetaRegion = nearest.getBestMatchingLocation([](const Location &loc)
    {
        return loc.hasService(Service::Meta);
    });

    const Server *pMetaServer{};
    if(pMetaRegion)
        pMetaServer = pMetaRegion->randomServerForService(Service::Meta);

    MetaInfo info{};
    if(pMetaServer)
    {
        info.port = pMetaServer->randomServicePort(Service::Meta);
        info.ip = pMetaServer->ip();
    }

    return info;
}
#endif

void PosixDaemon::writePlatformDiagnostics(DiagnosticsFile &file)
{
    QStringList emptyArgs;

#if defined(Q_OS_MAC)
    // Wipe out pre-existing pcaps
    if(QDir{Path::PcapDir}.exists())
    {
        qInfo() << "Recursively removing pcap folder" << Path::PcapDir;
        QDir{Path::PcapDir}.removeRecursively();
    }
    // Ensure pcap folder exists
    Path::PcapDir.mkpath();

    const auto metaInfo = tcpdumpMetaEndpoint(_state);
    QProcess tcpdumpPhys, tcpdumpUtun7, tcpdumpUtun8, tcpdumpVpn;

    // Collect tcpdump diagnostics
    if(metaInfo.port)
    {
        const auto filterExpression = QStringLiteral("(dst %1 or src %1) and port %2").arg(metaInfo.ip).arg(metaInfo.port);

        auto startTcpDump = [&](QProcess &tcpdumpProcess, const QString &interfaceName, const QString &outputFileName)
        {
            tcpdumpProcess.setProgram("tcpdump");
            tcpdumpProcess.setArguments({filterExpression, "-k", "--immediate-mode", "-vvvUi", interfaceName, "-w", outputFileName});
            tcpdumpProcess.start();
        };
        // Physical interface
        startTcpDump(tcpdumpPhys, _state.originalInterface(), Path::PcapDir / "pia_phys_pcap.txt");
        // Split tunnel interface
        startTcpDump(tcpdumpUtun7, "utun7", Path::PcapDir / "pia_utun7_pcap.txt");
        // Split tunnel interface (alternative - since we cycle between them)
        startTcpDump(tcpdumpUtun8, "utun8", Path::PcapDir / "pia_utun8_pcap.txt");
        // VPN interface - will be empty when not connected
        if(!_state.tunnelDeviceName().isEmpty())
            startTcpDump(tcpdumpVpn, _state.tunnelDeviceName(), Path::PcapDir / "pia_vpn_pcap.txt");
    }
    else
    {
        qWarning() << "Tcpdump diagnostics not collected, could not find a meta endpoint";
    }

    file.writeCommand("OS Version", "sw_vers", emptyArgs);
    file.writeText("Overview", diagnosticsOverview());
    file.writeText("Translation status", translationDiagnostic());
    // Write uname -a, but it's only of limited use - it lies when executed
    // under Rosetta 2 and still says the system is x86_64.
    file.writeCommand("uname -a", "uname", {QStringLiteral("-a")});
    file.writeCommand("ifconfig", "ifconfig", emptyArgs);
    file.writeCommand("Routes (netstat -nr)", "netstat", QStringList{QStringLiteral("-nr")});
    file.writeCommand("PF (pfctl -sr)", "pfctl", QStringList{QStringLiteral("-sr")});
    file.writeCommand("PF (pfctl -sR)", "pfctl", QStringList{QStringLiteral("-sR")});
    file.writeCommand("PF (App anchors)", "pfctl", QStringList{QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/*"), QStringLiteral("-sr")});
    file.writeCommand("PF (500.blockDNS:localdns table)", "pfctl", QStringList{QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/500.blockDNS"), "-t", "localdns", "-T", "show"});
    file.writeCommand("PF (500.blockDNS:tunneldns table)", "pfctl", QStringList{QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/500.blockDNS"), "-t", "tunneldns", "-T", "show"});
    file.writeCommand("PF (allowed subnets table)", "pfctl", QStringList{QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/305.allowSubnets"), "-t", "subnets", "-T", "show"});
    file.writeCommand("PF (450.routeDefaultApps4:lanips table)", "pfctl", QStringList{QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/450.routeDefaultApps4"), "-t", "lanips", "-T", "show"});
    file.writeCommand("PF (450.routeDefaultApps6:lanips table)", "pfctl", QStringList{QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/450.routeDefaultApps6"), "-t", "lanips", "-T", "show"});
    file.writeCommand("PF (pfctl -sR)", "pfctl", QStringList{QStringLiteral("-sR")});
    file.writeCommand("PF (NAT anchors)", "pfctl", QStringList{QStringLiteral("-sn")});
    file.writeCommand("PF (App NAT anchors)", "pfctl", QStringList{QStringLiteral("-sn"), QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/*")});
    file.writeCommand("PF (000.natVPN)", "pfctl", QStringList{QStringLiteral("-sn"), QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/000.natVPN")});
    file.writeCommand("PF (001.natPhys)", "pfctl", QStringList{QStringLiteral("-sn"), QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/001.natPhys")});
    file.writeCommand("PF (000.stubDNS)", "pfctl", QStringList{QStringLiteral("-sn"), QStringLiteral("-a"), QStringLiteral(BRAND_IDENTIFIER "/000.stubDNS")});
    file.writeCommand("dig (dig www.pia.com)", "dig", QStringList{QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("+time=4"), QStringLiteral("+tries=1")});
    file.writeCommand("dig (dig @piadns www.pia.com)", "dig", QStringList{QStringLiteral("@%1").arg(piaModernDnsVpn()), QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("+time=4"), QStringLiteral("+tries=1")});
    file.writeCommand("ping (ping www.pia.com)", "ping", QStringList{QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("-c1"), QStringLiteral("-W1")});
    file.writeCommand("ping (ping piadns)", "ping", QStringList{piaModernDnsVpn(),
        QStringLiteral("-c1"), QStringLiteral("-W1"), QStringLiteral("-n")});
    file.writeCommand("System log (last 4s)", "log", QStringList{"show", "--last",  "4s"});
    file.writeCommand("Third-party kexts", "/bin/bash", QStringList{QStringLiteral("-c"), QStringLiteral("kextstat | grep -v com.apple")});
    // This curl request is used to generate traffic for tcpdump - useful for diagnosing split tunnel issues. We use a meta server endpoint as we control it
    // - we also don't care about the response, just that the TCP threeway handshake takes place
    if(metaInfo.port)
        file.writeText("tcpdump endpoint", Exec::bashWithOutput(QStringLiteral("curl -vI https://%1:%2 --max-time 2 2>&1").arg(metaInfo.ip).arg(metaInfo.port)));
    file.writeCommand("System Extensions", "systemextensionsctl", QStringList{QStringLiteral("list")});
    file.writeCommand("DNS (scutil --dns)", "scutil", QStringList{QStringLiteral("--dns")});
    file.writeCommand("HTTP Proxy (scutil --proxy)", "scutil", QStringList{QStringLiteral("--proxy")});
    file.writeCommand("scutil (scutil --nwi)", "scutil", QStringList{QStringLiteral("--nwi")});
    scutilDNSDiagnostics(file);
    // Even with -detaillevel mini this can take a long time, just capture the
    // specific hardware information we're interested in.
    file.writeCommand("System information", "system_profiler",
        QStringList{QStringLiteral("SPHardwareDataType"), QStringLiteral("SPDisplaysDataType"),
            QStringLiteral("SPMemoryDataType"), QStringLiteral("SPStorageDataType")});
    file.writeCommand("Install log", "cat",
        {QStringLiteral("/Library/Application Support/" BRAND_IDENTIFIER "/install.log")});
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
    file.writeCommand("dig (dig @piadns www.pia.com)", "dig", QStringList{QStringLiteral("@%1").arg(piaModernDnsVpn()), QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("+time=4"), QStringLiteral("+tries=1")});
    file.writeCommand("ping (ping www.pia.com)", "ping", QStringList{QStringLiteral("www.privateinternetaccess.com"),
        QStringLiteral("-c1"), QStringLiteral("-W1")});
    file.writeCommand("ping (ping piadns)", "ping", QStringList{piaModernDnsVpn(),
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
    // Info about the wireguard kernel module (whether it is bed and/or available)
    file.writeCommand("ls -ld /sys/modules/wireguard", "ls", {"-ld", "/sys/modules/wireguard"});
    file.writeCommand("modprobe --show-depends wireguard", "modprobe", {"--show-depends", "wireguard"});
    // Info about libnl libraries
    file.writeCommand("ldconfig -p | grep libnl", "bash", QStringList{"-c", "ldconfig -p | grep libnl"});
    // There's no Install log section on Linux.  The Linux install script does
    // not log to a file.  It does very little compared to the Win/Mac
    // installers, and the few things it does can be easily inspected.
#endif
}

void PosixDaemon::checkFeatureSupport()
{
    // Probably needs to be refactored since wrong errors can end up in 
    // the splitTunnelSupportErrors JsonProperty.
    std::vector<QString> errors;

#ifdef Q_OS_LINUX
    // iptables 1.6.1 is required.
    QProcess iptablesVersion;
    iptablesVersion.start(QStringLiteral("iptables"), QStringList{QStringLiteral("--version")});
    iptablesVersion.waitForFinished();

    bool iptablesDetected = false;
    // To correctly evaluate the exitCode the process must have a NormalExit exitStatus
    // otherwise we assume there was a problem with it.
    if(iptablesVersion.exitStatus() == QProcess::NormalExit)
    {
        // Every non zero exitCode value (1-255) means the command "iptables --version"
        // returned an error.
        if(iptablesVersion.exitCode() == 0)
        {
            iptablesDetected = true;
        }
    }
    // Adding the error to the vpnSupportErrors JsonProperty
    if(!iptablesDetected)
        _state.vpnSupportErrors({QStringLiteral("iptables_missing")});

    qInfo() << "vpnSupportErrors: " << _state.vpnSupportErrors();

    auto output = iptablesVersion.readAllStandardOutput();
    auto outputNewline = output.indexOf('\n');
    // First line only
    if(outputNewline >= 0)
        output = output.left(outputNewline);
    auto match = QRegularExpression{R"(([0-9]+)(\.|)([0-9]+|)(\.|)([0-9]+|))"}.match(output);
    // Note that captured() returns QString{} by default if the pattern didn't
    // match, so these will be 0 by default.
    auto major = match.captured(1).toInt();
    auto minor = match.captured(3).toInt();
    auto patch = match.captured(5).toInt();
    // output.data() is "iptables vX.X.X (nf_tables)" when iptables is installed
    // otherwise it is empty when no iptables package is installed.
    qInfo().nospace() << "iptables version command output " << output.data() 
    << " -> " << major << "." << minor << "." << patch;

    // SemVersion implements a suitable operator<(), we don't use it to parse
    // the version because we're not sure that iptables will always return three
    // parts in its version number though.
    if(SemVersion{major, minor, patch} < SemVersion{1, 6, 1})
        errors.push_back(QStringLiteral("iptables_invalid"));

    // If the network monitor couldn't be created, libnl is missing.  (This was
    // not required in some releases, but it is now used to monitor the default
    // route, mainly because it can change while connected with WireGuard.)
    if(!_pNetworkMonitor)
    {
        errors.push_back(QStringLiteral("libnl_invalid"));
        // This is the only error that also applies to automation, and it only
        // applies on Linux.
        _state.automationSupportErrors({QStringLiteral("libnl_invalid")});
    }

    // This cgroup must be mounted in this location for this feature.
    QFileInfo cgroupFile(Path::ParentVpnExclusionsFile);

    if(!cgroupFile.exists())
    {
        // Try to create the net_cls VFS (if we have no other errors)
        if(errors.empty())
        {
            if(!kapps::net::CGroup::createNetCls(Path::ParentVpnExclusionsFile.parent()))
                errors.push_back(QStringLiteral("cgroups_invalid"));
        }
        else
        {
            errors.push_back(QStringLiteral("cgroups_invalid"));
        }
    }

    // We need proc events in order to detect process invocations
    // (CONFIG_PROC_EVENTS in kconfig).  There's no direct way to check this,
    // and it tends to be disabled on lightweight kernels, like for ARM boards
    // (it requires CONFIG_CONNECTOR=y, and those kernels are often built with
    // CONFIG_CONNECTOR=m).
    //
    // Try to connect and see if we get an initial message.  This is async, so
    // we'll assume the kernel does not support it initially until we get the
    // initial message.
    errors.push_back(QStringLiteral("cn_proc_invalid"));
    qInfo() << "Checking proc event support by connecting to Netlink connector";
    _pCnProcTest.emplace();
    _pCnProcTest->connected = [this]()
    {
        qInfo() << "Proc event received, kernel supports proc events";
        auto errors = _state.splitTunnelSupportErrors();
        auto itNewEnd = std::remove(errors.begin(), errors.end(),
                                    QStringLiteral("cn_proc_invalid"));
        errors.erase(itNewEnd, errors.end());
        _state.splitTunnelSupportErrors(errors);
        // Don't need the netlink connection any more
        _pCnProcTest.clear();
    };

#endif

    if(!errors.empty())
        _state.splitTunnelSupportErrors(errors);
}

#ifdef Q_OS_LINUX
void PosixDaemon::checkLinuxModules()
{
    bool hasWg = _linuxModSupport.hasModule(QStringLiteral("wireguard"));
    _state.wireguardKernelSupport(hasWg);
    qInfo() << "Wireguard kernel module present:" << hasWg;
}
#endif
