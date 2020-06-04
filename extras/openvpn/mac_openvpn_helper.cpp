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

#include "builtin/util.h"
#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QThread>
#include "brand.h"

#include <unistd.h>
#include <sys/sysctl.h>

enum ProcessErrorBehavior
{
    HaltOnErrors,
    IgnoreErrors,
};

QStringList g_servicesWithDisabledIPv6;

QString g_watcherPlistPath = QStringLiteral("/Library/Application Support/" BRAND_IDENTIFIER "/watcher.plist");
QString g_watcherPlist = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n"
            "  <dict>\n"
            "    <key>Label</key>\n"
            "    <string>" BRAND_IDENTIFIER ".watcher</string>\n"
            "    <key>ProgramArguments</key>\n"
            "    <array>\n"
            "      <string>%1</string>\n"
            "    </array>\n"
            "    <key>EnvironmentVariables</key>\n"
            "    <dict>\n"
            "      <key>script_type</key>\n"
            "      <string>watch-notify</string>\n"
            "    </dict>\n"
            "    <key>StandardErrorPath</key>\n"
            "    <string>/Library/Application Support/" BRAND_IDENTIFIER "/watcher.log</string>\n"
            "    <key>WatchPaths</key>\n"
            "    <array>\n"
            "      <string>/Library/Preferences/SystemConfiguration</string>\n"
            "    </array>\n"
            "  </dict>\n"
            "</plist>\n");

void restoreIPv6();
void restoreConfiguration();


pid_t getParentPID()
{
    pid_t pid = getpid();

    struct kinfo_proc info;
    size_t length = sizeof(struct kinfo_proc);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
    if (sysctl(mib, 4, &info, &length, NULL, 0) < 0)
        return -1;
    if (length == 0)
        return -1;
    return info.kp_eproc.e_ppid;
}

QByteArray execute(const QString& command, ProcessErrorBehavior errorBehavior = HaltOnErrors)
{
    QProcess p;
    p.start(command, QProcess::ReadOnly);
    p.closeWriteChannel();
    int exitCode = waitForExitCode(p);
    if (exitCode == 0 || errorBehavior == IgnoreErrors)
        return p.readAllStandardOutput();
    else
    {
        qCritical("Command failed with exit code %d: %s", exitCode, qPrintable(command));
        throw exitCode;
    }
}

QByteArray execute(const QString& executable, const QStringList& arguments, ProcessErrorBehavior errorBehavior = HaltOnErrors)
{
    QProcess p;
    p.start(executable, arguments, QProcess::ReadOnly);
    p.closeWriteChannel();
    int exitCode = waitForExitCode(p);
    if (exitCode == 0 || errorBehavior == IgnoreErrors)
        return p.readAllStandardOutput();
    else
    {
        qCritical("Command failed with exit code %d: %s [%s]", exitCode, qPrintable(executable), qPrintable(arguments.join(QLatin1String(", "))));
        throw exitCode;
    }
}

QByteArray scutil(const QStringList& commands, ProcessErrorBehavior errorBehavior = HaltOnErrors)
{
    QProcess p;
    p.start("scutil");
    p.write(commands.join('\n').toUtf8());
    p.waitForBytesWritten();
    p.closeWriteChannel();
    int exitCode = waitForExitCode(p);
    if (exitCode == 0 || errorBehavior == IgnoreErrors)
        return p.readAllStandardOutput();
    else
    {
        qCritical("scutil failed with exit code %d", exitCode);
        throw exitCode;
    }
}

QJsonValue scutilParse(const QString& text, int* offset = nullptr, const QString& indent = QString())
{
    int off = offset ? *offset : 0;
    QRegExp re(QStringLiteral("^(?:(<dictionary>|<array>) \\{\\n(?:((?:%1  [^\\n]*\\n)*)%1\\}\\n))|^([^\\n]*)\\n").arg(indent));
    if (off == re.indexIn(text, off, QRegExp::CaretAtOffset))
    {
        if (offset)
            *offset = off + re.matchedLength();
        if (!re.cap(1).isEmpty())
        {
            bool isArray = re.cap(1) == QLatin1String("<array>");
            QJsonObject dictionary;
            QJsonArray array;
            QString members = re.cap(2);
            QString indent2 = indent + QLatin1String("  ");
            QRegExp re2(QStringLiteral("^%1(\\S+) : ").arg(indent2));
            for (int i = 0; i < members.length();)
            {
                if (i == re2.indexIn(members, i, QRegExp::CaretAtOffset))
                {
                    i += re2.matchedLength();
                    QJsonValue member = scutilParse(members, &i, indent2);
                    if (member.isUndefined())
                        return QJsonValue::Undefined;
                    if (isArray)
                        array.insert(re2.cap(1).toInt(), member);
                    else
                        dictionary.insert(re2.cap(1), member);
                }
                else
                {
                    qCritical() << "Failed to parse scutil output";
                    return QJsonValue::Undefined;
                }
            }
            if (isArray)
                return array;
            else if (dictionary.contains(QLatin1String("PIAEmpty")))
                return QJsonValue::Null;
            else
                return dictionary;
        }
        else
        {
            return re.cap(3).trimmed();
        }
    }
    else if (text == QLatin1String("  No such key\n"))
    {
        return QJsonValue::Null;
    }
    else
    {
        qCritical() << "Failed to parse scutil output";
        return QJsonValue::Undefined;
    }
}

QJsonObject scutilGet(const QString& path, bool* present = nullptr)
{
    QString data = QString(scutil({ QStringLiteral("open"), QStringLiteral("show %1").arg(path), QStringLiteral("quit") }, IgnoreErrors));
    QJsonValue value = scutilParse(data);
    if (present)
        *present = value.isObject();
    return value.toObject();
}

bool scutilExists(const QString& path)
{
    bool exists;
    QJsonObject data = scutilGet(path, &exists);
    return exists && !data.contains(QStringLiteral("PIAEmpty"));
}

QStringList arrayToStringList(const QJsonArray& array)
{
    QStringList result;
    for (const QJsonValue& value : array)
        result += value.toString();
    return result;
}

void flushDNSCache()
{
    QProcess::execute(QStringLiteral("dscacheutil -flushcache"));
    QProcess::execute(QStringLiteral("discoveryutil udnsflushcaches"));
    QProcess::execute(QStringLiteral("discoveryutil mdnsflushcache"));
    QProcess::execute(QStringLiteral("killall -HUP mDNSResponder"));
    QProcess::execute(QStringLiteral("killall -HUP mDNSResponderHelper"));
}

void disableIPv6()
{
    QStringList networkServices;
    for (const QString& service : QString(execute(QStringLiteral("networksetup -listallnetworkservices"))).split('\n', QString::SkipEmptyParts).mid(1))
    {
        // Skip disabled services
        if (service.startsWith('*'))
            continue;
        networkServices += service;
    }
    qInfo() << "Network services:" << networkServices;

    // Disable IPv6 on any services where it is set to "Automatic"
    QStringList servicesWithDisabledIPv6;
    for (const QString& service : networkServices)
    {
        if (QString(execute(QStringLiteral("networksetup"), { QStringLiteral("-getinfo"), service })).split('\n', QString::SkipEmptyParts).contains(QLatin1String("IPv6: Automatic")))
        {
            execute(QStringLiteral("networksetup"), { QStringLiteral("-setv6off"), service });
            servicesWithDisabledIPv6 += service;
        }
    }
    qInfo() << "Disabled IPv6 on services:" << servicesWithDisabledIPv6;

    g_servicesWithDisabledIPv6 = servicesWithDisabledIPv6;
}

void restoreIPv6()
{
    for (const QString& service : g_servicesWithDisabledIPv6)
    {
        execute(QStringLiteral("networksetup"), { QStringLiteral("-setv6automatic"), service }, IgnoreErrors);
    }
    qInfo() << "Restored IPv6 on services:" << g_servicesWithDisabledIPv6;

    g_servicesWithDisabledIPv6.clear();
}

void applyConfiguration(bool shouldDisableIPv6, int killPid, QStringList dnsServers, QString domain, QStringList winsServers)
{
    // If needed, disable IPv6 on all services where it's set to Automatic
    if (shouldDisableIPv6)
        disableIPv6();
    else
        restoreIPv6();

    // Wait until network settings have settled
    QThread::msleep(200);

    QString primaryService = scutilGet(QStringLiteral("State:/Network/Global/IPv4")).value(QLatin1String("PrimaryService")).toString();
    qInfo() << "Primary service:" << primaryService;

    QByteArray oldDNS = scutil({ QStringLiteral("open"), QStringLiteral("show State:/Network/Global/DNS"), QStringLiteral("quit") });

    QJsonObject primaryIPv4 = scutilGet(QStringLiteral("State:/Network/Service/%1/IPv4").arg(primaryService));
    QJsonObject primarySetupDNS = scutilGet(QStringLiteral("Setup:/Network/Service/%1/DNS").arg(primaryService));
    QJsonObject primarySetupSMB = scutilGet(QStringLiteral("Setup:/Network/Service/%1/SMB").arg(primaryService));

    QStringList originalAddresses = arrayToStringList(primaryIPv4.value(QLatin1String("Addresses")).toArray());
    QStringList staticDNSServers = arrayToStringList(primarySetupDNS.value(QLatin1String("ServerAddresses")).toArray());
    QStringList staticDomains = arrayToStringList(primarySetupDNS.value(QLatin1String("SearchDomains")).toArray());
    QStringList staticWINSAddresses = arrayToStringList(primarySetupSMB.value(QLatin1String("WINSAddresses")).toArray());
    QString staticWorkgroup = primarySetupSMB.value(QLatin1String("Workgroup")).toString();

    bool useDynamicWINS = !winsServers.isEmpty() && staticWINSAddresses.isEmpty();
    bool overrideDNS = !dnsServers.isEmpty() || !domain.isEmpty();

    // Make all other configuration changes in a single scutil command
    QStringList commands;
    commands << QStringLiteral("open");

    // Store general PIA properties for the down script to use
    commands << QStringLiteral("d.init");
    commands << QStringLiteral("d.add PID %1").arg(killPid);
    commands << QStringLiteral("d.add Service %1").arg(primaryService);
    commands << QStringLiteral("d.add Addresses * %1").arg(originalAddresses.join(' '));
    if (!g_servicesWithDisabledIPv6.isEmpty())
        commands << QStringLiteral("d.add ServicesWithDisabledIPv6 * %1").arg(g_servicesWithDisabledIPv6.join(' '));
    if (overrideDNS)
        commands << QStringLiteral("d.add OverrideDNS ? TRUE");
    commands << QStringLiteral("set State:/Network/PrivateInternetAccess");

    // Save a backup of the DNS state
    commands << QStringLiteral("d.init");
    commands << QStringLiteral("d.add PIAEmpty ? TRUE");
    commands << QStringLiteral("get State:/Network/Service/%1/DNS").arg(primaryService);
    commands << QStringLiteral("set State:/Network/PrivateInternetAccess/OldStateDNS");

    // Save a backup of the DNS setup
    commands << QStringLiteral("d.init");
    commands << QStringLiteral("d.add PIAEmpty ? TRUE");
    commands << QStringLiteral("get Setup:/Network/Service/%1/DNS").arg(primaryService);
    commands << QStringLiteral("set State:/Network/PrivateInternetAccess/OldSetupDNS");

    // Save a backup of the SMB state
    commands << QStringLiteral("d.init");
    commands << QStringLiteral("d.add PIAEmpty ? TRUE");
    commands << QStringLiteral("get State:/Network/Service/%1/SMB").arg(primaryService);
    commands << QStringLiteral("set State:/Network/PrivateInternetAccess/OldStateSMB");

    // Overwrite the DNS state/setup
    if(overrideDNS)
    {
        commands << QStringLiteral("d.init");
        commands << QStringLiteral("get State:/Network/Service/%1/DNS").arg(primaryService);
        commands << QStringLiteral("get Setup:/Network/Service/%1/DNS").arg(primaryService); // manual DNS settings, no-op if not present
        if (!domain.isEmpty())
            commands << QStringLiteral("d.add DomainName %1").arg(domain);
        if (!dnsServers.isEmpty())
            commands << QStringLiteral("d.add ServerAddresses * %1").arg(dnsServers.join(' '));
        commands << QStringLiteral("set State:/Network/Service/%1/DNS").arg(primaryService);
        commands << QStringLiteral("set Setup:/Network/Service/%1/DNS").arg(primaryService);
        commands << QStringLiteral("set State:/Network/PrivateInternetAccess/DNS");
    }

    // Overwrite SMB state
    commands << QStringLiteral("d.init");
    if (!staticWorkgroup.isEmpty())
        commands << QStringLiteral("d.add Workgroup %1").arg(staticWorkgroup);
    if (useDynamicWINS)
        commands << QStringLiteral("d.add WINSAddresses * %1").arg(winsServers.join(' '));
    commands << QStringLiteral("set State:/Network/Service/%1/SMB").arg(primaryService);

    // Execute changes
    commands << QStringLiteral("quit");
    scutil(commands);

    // Wait for the settings to propagate
    // TODO: we can use the notification API for this later
    for (int i = 0; ; i++)
    {
        if (i == 20)
        {
            qWarning() << "Failed to get propagated DNS settings";
            break;
        }
        if (oldDNS != scutil({ QStringLiteral("open"), QStringLiteral("show State:/Network/Global/DNS"), QStringLiteral("quit") }))
        {
            qInfo() << "DNS settings propagated successfully after" << (i * 100) << "ms";
            break;
        }
        QThread::msleep(100);
    }

    // Flush all DNS caches now that the new settings are live
    flushDNSCache();

    qInfo() << "Finished applying settings";

    // Write plist for the network config watcher and launch it
    QFile plist(g_watcherPlistPath);
    if (plist.open(QFile::WriteOnly))
    {
        QTextStream(&plist) << g_watcherPlist.arg(QCoreApplication::applicationFilePath());
        plist.close();
        execute(QStringLiteral("/bin/launchctl"), { QStringLiteral("load"), g_watcherPlistPath }, IgnoreErrors);
    }
}

void restoreConfiguration()
{
    // Unload the watcher if it is present
    QFile plist(g_watcherPlistPath);
    if (plist.exists())
    {
        execute(QStringLiteral("/bin/launchctl"), { QStringLiteral("unload"), g_watcherPlistPath }, IgnoreErrors);
        plist.remove();
    }

    //bool configExists = 0 == QProcess::execute(QStringLiteral("scutil"), { QStringLiteral("-w"), QStringLiteral("State:/Network/PrivateInternetAccess") });
    QJsonObject config = scutilGet(QStringLiteral("State:/Network/PrivateInternetAccess"));

    QString service = config.value(QLatin1String("Service")).toString();
    //QStringList originalAddresses = arrayToStringList(config.value(QLatin1String("Addresses")).toArray());
    QStringList servicesWithDisabledIPv6 = arrayToStringList(config.value(QLatin1String("ServicesWithDisabledIPv6")).toArray());

    for (const QString& service : servicesWithDisabledIPv6)
    {
        if (!g_servicesWithDisabledIPv6.contains(service))
            g_servicesWithDisabledIPv6.append(service);
    }

    QStringList commands;
    commands << QStringLiteral("open");

    auto restoreConfigKey = [&commands](const QString& savedKey, const QString& destKey) {
        if (scutilExists(savedKey))
        {
            commands << QStringLiteral("get %1").arg(savedKey);
            commands << QStringLiteral("set %1").arg(destKey);
        }
        else
        {
            commands << QStringLiteral("remove %1").arg(destKey);
        }
        commands << QStringLiteral("remove %1").arg(savedKey);
    };

    restoreConfigKey(QStringLiteral("State:/Network/PrivateInternetAccess/OldStateDNS"), QStringLiteral("State:/Network/Service/%1/DNS").arg(service));
    restoreConfigKey(QStringLiteral("State:/Network/PrivateInternetAccess/OldSetupDNS"), QStringLiteral("Setup:/Network/Service/%1/DNS").arg(service));
    restoreConfigKey(QStringLiteral("State:/Network/PrivateInternetAccess/OldStateSMB"), QStringLiteral("State:/Network/Service/%1/SMB").arg(service));

    commands << QStringLiteral("remove State:/Network/PrivateInternetAccess/DNS");
    commands << QStringLiteral("remove State:/Network/PrivateInternetAccess");

    commands << QStringLiteral("quit");
    scutil(commands, IgnoreErrors);

    QThread::msleep(200);

    flushDNSCache();

    restoreIPv6();

    qInfo() << "Finished restoring settings";
}

void configurationChanged()
{
    bool present;
    QJsonObject data = scutilGet(QStringLiteral("State:/Network/PrivateInternetAccess"), &present);

    if (!present)
    {
        qInfo() << "No PIA network configuration found; aborting";
        return;
    }

    uint pid = data.value(QLatin1String("PID")).toString().toUInt();
    QString service = data.value(QLatin1String("Service")).toString();
    QStringList originalAddresses = arrayToStringList(data.value(QLatin1String("Addresses")).toArray());
    QStringList currentAddresses = arrayToStringList(scutilGet(QStringLiteral("State:/Network/Service/%1/IPv4").arg(service)).value(QLatin1String("Addresses")).toArray());
    QJsonObject originalDNS = scutilGet(QStringLiteral("State:/Network/PrivateInternetAccess/OldStateDNS"));
    QJsonObject intendedDNS = scutilGet(QStringLiteral("State:/Network/PrivateInternetAccess/DNS"));
    QJsonObject currentDNS = scutilGet(QStringLiteral("State:/Network/Service/%1/DNS").arg(service));

    // Check if IP address has changed
    if (originalAddresses != currentAddresses)
    {
        qInfo() << "IP address has changed; killing connection";
        goto killConnection;
    }

    // Check if DNS has changed
    if (data.value(QLatin1String("OverrideDNS")).toString() == QLatin1String("TRUE"))
    {
        if (currentDNS != intendedDNS)
        {
            // If we merely changed back to the original DNS, reapply settings
            if (currentDNS == originalDNS)
            {
                qInfo() << "DNS reverted to original setting; reapplying";
                scutil({
                           QStringLiteral("open"),
                           QStringLiteral("get State:/Network/PrivateInternetAccess/DNS"),
                           QStringLiteral("set State:/Network/Service/%1/DNS").arg(service),
                           QStringLiteral("set Setup:/Network/Service/%1/DNS").arg(service),
                           QStringLiteral("quit"),
                       }, IgnoreErrors);
            }
            else
            {
                qInfo() << "DNS has changed; killing connection";
                goto killConnection;
            }
        }
    }

    return;

killConnection:
    if (pid)
    {
        execute(QStringLiteral("kill %1").arg(pid), IgnoreErrors);
    }
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    qInfo() << "Arguments:" << app.arguments();
    qInfo() << "Environment:" << env.toStringList();

    putenv(const_cast<char*>("PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"));

    QString scriptType = env.value(QStringLiteral("script_type"));

    try
    {
        if (scriptType == QLatin1String("up"))
        {
            QStringList dnsServers;
            QStringList winsServers;
            QString domain;

            bool shouldDisableIPv6 = env.value(QStringLiteral("ifconfig_ipv6_remote")) == QLatin1String("::1");

            // If the local network connection goes down, we kill a process to
            // cause a reconnect.  The default is the parent of this script,
            // which is OpenVPN itself for OpenVPN.  The kill_pid variable
            // overrides this, and 0 prevents us from killing anything.
            int killPid{0};
            QString killPidEnvVal{env.value(QStringLiteral("kill_pid"))};
            if(!killPidEnvVal.isEmpty())
                killPid = killPidEnvVal.toUInt();   // 0 if the value is invalid
            else
                killPid = getParentPID();

            // Parse command-line arguments
            for(int i=1; i<argc; ++i)
            {
                if(argv[i] == QStringLiteral("--dns"))
                {
                    if(i+1 < argc)
                    {
                        dnsServers = QString(argv[i+1]).split(':');
                        ++i;
                    }
                }
                else if(argv[i] == QStringLiteral("--"))
                {
                    // Done with PIA args; OpenVPN args follow
                    break;
                }
                else
                {
                    qWarning() << "Unknown option" << argv[i];
                }
            }

            qInfo().noquote().nospace() << "Using device:" << env.value(QStringLiteral("dev"))
                << " local_address:" << env.value(QStringLiteral("ifconfig_local"))
                << " remote_address:" << env.value(QStringLiteral("route_vpn_gateway"));

            // Parse all foreign_option_{i} variables
            for (int i = 1; ; i++)
            {
                QString option = env.value(QStringLiteral("foreign_option_%1").arg(i));
                if (option.isEmpty())
                    break;
                if (option.startsWith(QLatin1String("dhcp-option DNS ")))
                    dnsServers += option.mid(16);
                else if (option.startsWith(QLatin1String("dhcp-option WINS ")))
                    winsServers += option.mid(17);
                else if (option.startsWith(QLatin1String("dhcp-option DOMAIN ")))
                    domain = option.mid(19);
            }
            // If DNS is being overridden, and no domain was specified, use a
            // default.  Don't touch the domain if we are keeping the existing
            // DNS setting.
            if (domain.isEmpty() && !dnsServers.isEmpty())
                domain = QStringLiteral("openvpn");

            try
            {
                applyConfiguration(shouldDisableIPv6, killPid, dnsServers, domain, winsServers);
            }
            catch (int exitCode)
            {
                qWarning() << "Error during configuration, trying to roll back...";
                restoreConfiguration();
                return exitCode;
            }
        }
        else if (scriptType == QLatin1String("down"))
        {
            restoreConfiguration();
        }
        else if (scriptType == QLatin1String("watch-notify"))
        {
            configurationChanged();
        }
    }
    catch (int exitCode)
    {
        return exitCode;
    }

    return 0;
}
