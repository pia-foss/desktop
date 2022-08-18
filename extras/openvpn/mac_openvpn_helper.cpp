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

#include <common/src/builtin/util.h>
#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>
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
void restoreConfiguration();
void applyConfiguration(const QString &primaryService, int killPid, QStringList dnsServers, QString domain, QStringList winsServers);

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

QString detectPrimaryService()
{
    QString primaryService = scutilGet(QStringLiteral("State:/Network/Global/IPv4")).value(QLatin1String("PrimaryService")).toString();
    qInfo() << "Primary service:" << primaryService;
    return primaryService;
}

void saveAndApplyConfiguration(int killPid, QStringList dnsServers, QString domain, QStringList winsServers)
{
    // Store the setup parameters so we can apply the configuration again if the
    // DNS configuration is changed
    scutil({
        QStringLiteral("d.init"),
        QStringLiteral("d.add killPid %1").arg(killPid),
        QStringLiteral("d.add dnsServers * %1").arg(dnsServers.join(' ')),
        QStringLiteral("d.add domain %1").arg(domain),
        QStringLiteral("d.add winsServers * %1").arg(winsServers.join(' ')),
        QStringLiteral("set State:/Network/PrivateInternetAccess/SetupParams")
    });

    QString primaryService{detectPrimaryService()};
    if(primaryService.isEmpty())
    {
        qWarning() << "No primary service - cannot apply DNS yet, will apply later when network is connected";
        return;
    }

    applyConfiguration(primaryService, killPid, dnsServers, domain, winsServers);
}

void reapplyConfiguration(const QString &primaryService, const QJsonObject &setupParams)
{
    uint killPid = setupParams.value(QLatin1String("killPid")).toString().toUInt();
    QStringList dnsServers = arrayToStringList(setupParams.value(QLatin1String("dnsServers")).toArray());
    QString domain = setupParams.value(QLatin1String("domain")).toString();
    QStringList winsServers = arrayToStringList(setupParams.value(QLatin1String("winsServers")).toArray());

    applyConfiguration(primaryService, killPid, dnsServers, domain, winsServers);
}

void applyConfiguration(const QString &primaryService, int killPid, QStringList dnsServers, QString domain, QStringList winsServers)
{
    // Wait until network settings have settled
    QThread::msleep(200);

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
    commands << QStringLiteral("d.add Service %1").arg(primaryService);
    commands << QStringLiteral("d.add Addresses * %1").arg(originalAddresses.join(' '));
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
}

void restoreConfiguration()
{
    QJsonObject config = scutilGet(QStringLiteral("State:/Network/PrivateInternetAccess"));

    QString service = config.value(QLatin1String("Service")).toString();

    QStringList commands;
    commands << QStringLiteral("open");

    QJsonObject intendedDNS = scutilGet(QStringLiteral("State:/Network/PrivateInternetAccess/DNS"));

    auto restoreConfigKey = [&commands, &intendedDNS](const QString& savedKey, const QString& destKey)
    {
        // Only restore the key if the value PIA applied is still there.
        // Otherwise, it has likely been changed by the system to reflect a new
        // network configuration, and we should leave the new configuration.
        //
        // In particular, we shouldn't restore to a "State" key that no longer
        // exists, that commonly happens when a network interface is
        // disconnected while connected to PIA.
        bool destExists;
        QJsonObject destValue = scutilGet(destKey, &destExists);
        // We can restore the backup over this value if it still exists and is still
        // set to the configuration applied by PIA
        if(destExists && destValue == intendedDNS)
        {
            // If we backed up a value (and it's not PIAEmpty), restore it.
            if (scutilExists(savedKey))
            {
                commands << QStringLiteral("get %1").arg(savedKey);
                commands << QStringLiteral("set %1").arg(destKey);
            }
            // Otherwise, our backup was empty, just remove the key to restore
            // that.
            else
            {
                commands << QStringLiteral("remove %1").arg(destKey);
            }
        }
        // Remove the backup key, even if we decided not to restore it.
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

    qInfo() << "Finished restoring settings";
}

void configurationChanged()
{
    bool present;
    QJsonObject setupParams = scutilGet(QStringLiteral("State:/Network/PrivateInternetAccess/SetupParams"), &present);

    if(!present)
    {
        qWarning() << "Can't update configuration, no PIA configuration was found";
        return;
    }

    // Determine what (if any) change has occurred.
    //
    // - If the local IP addresses have changed (including to/from "none"), we
    //   have changed networks.
    //   - If the current method supports roaming, redo the configuration from
    //     scratch - clean up and set up again.
    //   - Otherwise, just kill the connection and clean up.
    // - If the local IPs have not changed, but the DNS configuration has
    //   changed, just reapply our DNS configuration.  This tends to happen on
    //   10.15.4+ - the OS tends to reapply the network's DNS configuration even
    //   though nothing has changed.

    uint killPid = setupParams.value(QLatin1String("killPid")).toString().toUInt();

    QJsonObject data = scutilGet(QStringLiteral("State:/Network/PrivateInternetAccess"));

    QString oldPrimary = data.value(QLatin1String("Service")).toString();
    QStringList originalAddresses = arrayToStringList(data.value(QLatin1String("Addresses")).toArray());
    QString currentPrimary{detectPrimaryService()};
    QStringList currentAddresses = arrayToStringList(scutilGet(QStringLiteral("State:/Network/Service/%1/IPv4").arg(currentPrimary)).value(QLatin1String("Addresses")).toArray());

    // Have the primary service or the local IP addresses changed?
    if(oldPrimary != currentPrimary || originalAddresses != currentAddresses)
    {
        // Yes, the network connection has changed
        qInfo().nospace() << "Network connection has changed from "
            << oldPrimary << " (" << originalAddresses.size()
            << " addresses) to " << currentPrimary << " ("
            << currentAddresses.size() << " addresses)";

        // Does the current connection support roaming?
        if(killPid)
        {
            // Can't roam; kill the connection.
            qInfo() << "Network connection has changed; killing connection";
            execute(QStringLiteral("kill %1").arg(killPid), IgnoreErrors);
        }
        else
        {
            // Roaming is possible - tear down and/or reconfigure DNS.

            // Tear down the old configuration if it was known - this is
            // important to restore the old setup configuration to that
            // interface, even if it's not currently connected.
            if(!oldPrimary.isEmpty() && !originalAddresses.isEmpty())
            {
                qInfo() << "Restore old configuration";
                restoreConfiguration();
            }

            // Configure again if the new interface is ready
            if(!currentPrimary.isEmpty() && !currentAddresses.isEmpty())
            {
                qInfo() << "Reconfigure with new primary interface";
                reapplyConfiguration(currentPrimary, setupParams);
            }
        }
        return;
    }

    // The primary service hasn't changed, but there could be no primary service
    // if we had already observed this state and there still isn't an active
    // network connection.
    if(currentPrimary.isEmpty() || currentAddresses.isEmpty())
    {
        qWarning() << "No primary service - cannot apply DNS yet, will apply later when network is connected";
        return;
    }

    QJsonObject oldStateDNS = scutilGet(QStringLiteral("State:/Network/PrivateInternetAccess/OldStateDNS"));
    QJsonObject oldSetupDNS = scutilGet(QStringLiteral("State:/Network/PrivateInternetAccess/OldSetupDNS"));
    QJsonObject intendedDNS = scutilGet(QStringLiteral("State:/Network/PrivateInternetAccess/DNS"));
    QJsonObject currentStateDNS = scutilGet(QStringLiteral("State:/Network/Service/%1/DNS").arg(currentPrimary));
    QJsonObject currentSetupDNS = scutilGet(QStringLiteral("Setup:/Network/Service/%1/DNS").arg(currentPrimary));

    // Check if DNS has changed
    if (data.value(QLatin1String("OverrideDNS")).toString() == QLatin1String("TRUE"))
    {
        if (currentStateDNS != intendedDNS || currentSetupDNS != intendedDNS)
        {
            // Reapply regardless of the new config, but trace the state for
            // supportability
            qInfo() << "DNS has changed, reapplying";
            qInfo() << "intended DNS:" << QJsonDocument{intendedDNS}.toJson();
            qInfo() << "old state DNS:" << QJsonDocument{oldStateDNS}.toJson();
            qInfo() << "old setup DNS:" << QJsonDocument{oldSetupDNS}.toJson();
            qInfo() << "current state DNS:" << QJsonDocument{currentStateDNS}.toJson();
            qInfo() << "current setup DNS:" << QJsonDocument{currentSetupDNS}.toJson();
            scutil({
                       QStringLiteral("open"),
                       QStringLiteral("get State:/Network/PrivateInternetAccess/DNS"),
                       QStringLiteral("set State:/Network/Service/%1/DNS").arg(currentPrimary),
                       QStringLiteral("set Setup:/Network/Service/%1/DNS").arg(currentPrimary),
                       QStringLiteral("quit"),
                   }, IgnoreErrors);
        }
        else
        {
            qInfo() << "DNS is still configured, nothing to update.";
        }
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

            // If the local network connection goes down, we kill a process to
            // cause a reconnect.  The default is the parent of this script,
            // which is OpenVPN itself for OpenVPN.  The kill_pid variable
            // overrides this, and 0 prevents us from killing anything (0
            // indicates that the connection supports roaming - WireGuard)
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

            qInfo().nospace() << "Using device:" << qUtf8Printable(env.value(QStringLiteral("dev")))
                << " local_address:" << qUtf8Printable(env.value(QStringLiteral("ifconfig_local")))
                << " remote_address:" << qUtf8Printable(env.value(QStringLiteral("route_vpn_gateway")));

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
                saveAndApplyConfiguration(killPid, dnsServers, domain, winsServers);
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
            // Remove the saved setup parameters
            scutil({
                QStringLiteral("remove State:/Network/PrivateInternetAccess/SetupParams"),
            }, ProcessErrorBehavior::IgnoreErrors);
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
