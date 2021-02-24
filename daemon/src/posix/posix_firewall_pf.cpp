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

#include "common.h"
#line HEADER_FILE("posix/posix_firewall_pf.cpp")

#ifdef Q_OS_MACOS

#include "posix_firewall_pf.h"
#include "path.h"
#include "brand.h"
#include "processrunner.h"
#include "exec.h"
#include "configwriter.h"

#include <QProcess>

static QString kRootAnchor = QStringLiteral(BRAND_IDENTIFIER);
static QByteArray kPfWarning = "pfctl: Use of -f option, could result in flushing of rules\npresent in the main ruleset added by the system at startup.\nSee /etc/pf.conf for further details.\n";

namespace
{
    // Restart strategy for stub resolver
    const RestartStrategy::Params stubResolverRestart{std::chrono::milliseconds(100), // Min restart delay
                                                      std::chrono::seconds(3), // Max restart delay
                                                      std::chrono::seconds(30)}; // Min "successful" run time

    // Run Unbound on an auxiliary loopback address configured to return
    // NXDOMAIN for everything.  Used to block DNS in a way that still allows
    // mDNSResponder to bring up the physical interface.
    class StubUnbound
    {
    public:
        StubUnbound();
        ~StubUnbound();

    public:
        bool enable(bool enabled);
        void setMethod(const QString &method);

    private:
        ProcessRunner _unboundRunner;
        QString _stubMethod;
    };

    StubUnbound::StubUnbound()
        : _unboundRunner{stubResolverRestart}, _stubMethod{QStringLiteral("NX")}
    {
        _unboundRunner.setObjectName(QStringLiteral("unbound-stub"));
    }

    bool StubUnbound::enable(bool enabled)
    {
        if(enabled == _unboundRunner.isEnabled())
            return false; // No change, nothing to do

        if(enabled)
        {
            qInfo() << "Initialize DNS stub with method" << _stubMethod;

            {
                ConfigWriter conf{Path::UnboundDnsStubConfigFile};
                conf << "server:" << conf.endl;
                conf << "    logfile: \"\"" << conf.endl;   // Log to stderr
                conf << "    edns-buffer-size: 4096" << conf.endl;
                conf << "    max-udp-size: 4096" << conf.endl;
                conf << "    qname-minimisation: yes" << conf.endl;
                conf << "    interface: 127.0.0.1@8073" << conf.endl;
                conf << "    interface: ::1@8073" << conf.endl;
                // This server shouldn't do any queries, bind it to the loopback
                // interface
                conf << "    outgoing-interface: 127.0.0.1" << conf.endl;
                conf << "    verbosity: 1" << conf.endl;
                // We can drop user/group rights on this server because it doesn't
                // have to make any queries
                conf << "    username: \"nobody\"" << conf.endl;
                conf << "    do-daemonize: no" << conf.endl;
                conf << "    use-syslog: no" << conf.endl;
                conf << "    hide-identity: yes" << conf.endl;
                conf << "    hide-version: yes" << conf.endl;
                // By default, unbound only allows queries from localhost.  Due to
                // the PF redirections used to redirect outgoing queries to this
                // server, it will receive packets on the loopback interface with
                // the source IPs of other interfaces, so we need to allow any host.
                // This doesn't open up access to remote hosts.
                conf << "    access-control: 0.0.0.0/0 allow" << conf.endl;
                conf << "    access-control: ::/0 allow" << conf.endl;
                conf << "    directory: \"" << Path::InstallationDir << "\"" << conf.endl;
                conf << "    pidfile: \"\"" << conf.endl;
                conf << "    chroot: \"\"" << conf.endl;

                if(_stubMethod == QStringLiteral("NX"))
                    conf << "    local-zone: \".\" static" << conf.endl;
                else if(_stubMethod == QStringLiteral("Refuse"))
                    conf << "    local-zone: \".\" always_refuse" << conf.endl;
                else
                {
                    conf << "    local-zone: \".\" redirect" << conf.endl;
                    conf << "    local-data: \". 30 IN NS localhost.\"" << conf.endl;
                    conf << "    local-data: \". 30 IN SOA localhost. nobody.invalid. 1 30 30 60 30\"" << conf.endl;
                    conf << "    local-data: \". 30 IN A 0.0.0.0\"" << conf.endl;
                    conf << "    local-data: \". 30 IN AAAA ::\"" << conf.endl;
                }
            }
            _unboundRunner.enable(Path::UnboundExecutable,
                                  {"-c", Path::UnboundDnsStubConfigFile});
            // Don't need to flush the DNS cache when enabling the stub resolver
            return false;
        }
        else
        {
            _unboundRunner.disable();
            QFile::remove(Path::UnboundDnsStubConfigFile);
            // Flush the DNS cache in case the bogus responses were cached
            return true;
        }
    }

    void StubUnbound::setMethod(const QString &method)
    {
        if(method == _stubMethod)
            return; // No change, nothing to do

        _stubMethod = method;

        // If we're enabled; reload with the new method.  We need to regenerate
        // the config file, and the ProcessRunner has to be explicitly disabled/
        // enabled since the command-line parameters don't actually change.
        if(_unboundRunner.isEnabled())
        {
            enable(false);
            enable(true);
        }
    }

    StubUnbound::~StubUnbound()
    {
        enable(false);  // Remove the config file
    }

    StubUnbound &getStubDns()
    {
        static StubUnbound _stubUnbound;
        return _stubUnbound;
    }
}

int PFFirewall::execute(const QString& command, bool ignoreErrors)
{
    static QLoggingCategory stdoutCategory("pf.stdout");
    static QLoggingCategory stderrCategory("pf.stderr");

    QProcess p;
    p.start(QStringLiteral("/bin/bash"), { QStringLiteral("-c"), command }, QProcess::ReadOnly);
    p.closeWriteChannel();
    int exitCode = waitForExitCode(p);
    auto out = p.readAllStandardOutput().trimmed();
    auto err = p.readAllStandardError().replace(kPfWarning, "").trimmed();
    if ((exitCode != 0 || !err.isEmpty()) && !ignoreErrors)
        qWarning().noquote().nospace() << "(" << exitCode << ") $ " << command;
    else if (false)
        qDebug().noquote().nospace() << "(" << exitCode << ") $ " << command;
    if (!out.isEmpty()) qCInfo(stdoutCategory).noquote() << out;
    if (!err.isEmpty()) qCWarning(stderrCategory).noquote() << err;
    return exitCode;
}

bool PFFirewall::isPFEnabled()
{
    return 0 == execute(QStringLiteral("test -s '%1/pf.token' && pfctl -s References | grep -qFf '%1/pf.token'").arg(Path::DaemonDataDir), true);
}

void PFFirewall::installRootAnchors()
{
    qInfo() << "Installing PF root anchors";

    // Append our NAT anchors by reading back and re-applying NAT rules only
    auto insertNatAnchors = QString{
        "( "
            R"(pfctl -sn | grep -v '%2/*'; )"   // Translation rules (includes both nat and rdr, despite the modifier being 'nat')
            R"(echo 'nat-anchor "%2/*"'; )"     // PIA's translation anchors
            R"(echo 'rdr-anchor "%2/*"'; )"
            R"(echo 'load anchor "%2" from "%1/pf/%2.conf"'; )" // Load the PIA anchors from file
        ") | pfctl -N -f -"
    }.arg(Path::ResourceDir, kRootAnchor);
    execute(insertNatAnchors);

    // Append our filter anchor by reading back and re-applying filter rules
    // only.  pfctl -sr also includes scrub rules, but these will be ignored
    // due to -R.
    auto insertFilterAnchor = QString{
        "( "
            R"(pfctl -sr | grep -v '%2/*'; )"   // Filter rules (everything from pfctl -sr except 'scrub')
            R"(echo 'anchor "%2/*"'; )"         // PIA's filter anchors
            R"(echo 'load anchor "%2" from "%1/pf/%2.conf"'; )" // Load the PIA anchors from file
        " ) | pfctl -R -f -"
    }.arg(Path::ResourceDir, kRootAnchor);
    execute(insertFilterAnchor);
}

bool PFFirewall::isRootAnchorLoaded(const QString &modifier)
{
    // Our Root anchor is loaded if:
    // 1. It is is included among the top-level anchors
    // 2. It is not empty (i.e it contains sub-anchors)
    return 0 == execute(QStringLiteral("pfctl -s %2 | grep -q '%1' && pfctl -q -a '%1' -s %2 2> /dev/null | grep -q .").arg(kRootAnchor, modifier), true);
}

bool PFFirewall::areAllRootAnchorsLoaded()
{
    return isRootAnchorLoaded(QStringLiteral("nat")) &&
        isRootAnchorLoaded(QStringLiteral("rules"));
}

bool PFFirewall::areAnyRootAnchorsLoaded()
{
    return isRootAnchorLoaded(QStringLiteral("nat")) ||
        isRootAnchorLoaded(QStringLiteral("rules"));
}

QString PFFirewall::getMacroArgs(const MacroPairs& macroPairs)
{
    QStringList macroList{};
    for(auto itAttr = macroPairs.begin(); itAttr != macroPairs.end(); ++itAttr)
    {
        macroList << QStringLiteral("-D%1=%2").arg(itAttr.key()).arg(itAttr.value());
    }

    return macroList.join(" ");
}

void PFFirewall::install()
{
    // remove hard-coded (legacy) pia anchor from /etc/pf.conf if it exists
    execute(QStringLiteral("if grep -Fq '%1' /etc/pf.conf ; then echo \"`cat /etc/pf.conf | grep -vF '%1'`\" > /etc/pf.conf ; fi").arg(kRootAnchor));

    // Clean up any existing rules if they exist.
    uninstall();

    installRootAnchors();
    execute(QStringLiteral("pfctl -E 2>&1 | grep -F 'Token : ' | cut -c9- > '%1/pf.token'").arg(Path::DaemonDataDir));
}

void PFFirewall::uninstall()
{
    qInfo() << "Uninstalling PF root anchor";

    // Flush our rules if any of our root anchors are loaded
    if (areAnyRootAnchorsLoaded())
        execute(QStringLiteral("pfctl -q -a '%1' -F all").arg(kRootAnchor));

    if (isPFEnabled())
        execute(QStringLiteral("test -f '%1/pf.token' && pfctl -X `cat '%1/pf.token'` && rm '%1/pf.token'").arg(Path::DaemonDataDir));

}

bool PFFirewall::isInstalled()
{
    return isPFEnabled() && areAllRootAnchorsLoaded();
}

void PFFirewall::enableAnchor(const QString& anchor, const QString &modifier, const MacroPairs &macroPairs)
{
    execute(QStringLiteral("if pfctl -q -a '%1/%2' -s %3 2> /dev/null | grep -q . ; then echo '%2: ON' ; else echo '%2: OFF -> ON' ; pfctl -q -a '%1/%2' -F all %5 -f '%4/pf/%1.%2.conf' ; fi").arg(kRootAnchor, anchor, modifier, Path::ResourceDir, getMacroArgs(macroPairs)));
}

void PFFirewall::disableAnchor(const QString& anchor, const QString &modifier)
{
    execute(QStringLiteral("if ! pfctl -q -a '%1/%2' -s %3 2> /dev/null | grep -q . ; then echo '%2: OFF' ; else echo '%2: ON -> OFF' ; pfctl -q -a '%1/%2' -F all ; fi").arg(kRootAnchor, anchor, modifier));
}

void PFFirewall::setAnchorEnabled(const QString& anchor, const QString &modifier, bool enable, const MacroPairs &macroPairs)
{
    if (enable)
        enableAnchor(anchor, modifier, macroPairs);
    else
        disableAnchor(anchor, modifier);
}

void PFFirewall::setAnchorTable(const QString& anchor, bool enabled, const QString& table, const QStringList& items)
{
    if (enabled)
        execute(QStringLiteral("pfctl -q -a '%1/%2' -t '%3' -T replace %4").arg(kRootAnchor, anchor, table, items.join(' ')));
    else
        execute(QStringLiteral("pfctl -q -a '%1/%2' -t '%3' -T kill").arg(kRootAnchor, anchor, table), true);
}

void PFFirewall::setFilterEnabled(const QString &anchor, bool enable, const MacroPairs &macroPairs)
{
    setAnchorEnabled(anchor, QStringLiteral("rules"), enable, macroPairs);
}

void PFFirewall::setFilterWithRules(const QString& anchor, bool enabled, const QStringList &ruleList)
{
    if(!enabled)
        return (void)execute(QStringLiteral("pfctl -q -a '%1/%2' -F all").arg(kRootAnchor, anchor), true);
    else
        return (void)execute(QStringLiteral("echo -e \"%1\" | pfctl -q -a '%2/%3' -f -").arg(ruleList.join('\n'), kRootAnchor, anchor), true);
}

void PFFirewall::setTranslationEnabled(const QString &anchor, bool enable, const MacroPairs &macroPairs)
{
    setAnchorEnabled(anchor, QStringLiteral("nat"), enable, macroPairs);
}

void PFFirewall::ensureRootAnchorPriority()
{
    // We check whether our filter appears last in the ruleset. If it does not,
    // then reinstall PIA anchors (this happens atomically).
    // We don't check for priority of the nat/rdr anchors specifically, but
    // these are less likely to have conflicts, and a conflict would likely
    // also occur for filter rules anyway.
    // Appearing last ensures priority.
    if(execute(QStringLiteral("pfctl -sr | tail -1 | grep -qF '%1'").arg(kRootAnchor)) != 0)
    {
        qInfo() << "Reinstall PIA root anchors, priority was overridden";
        installRootAnchors();
    }
}

void PFFirewall::setMacDnsStubMethod(const QString &macDnsStubMethod)
{
    getStubDns().setMethod(macDnsStubMethod);
}

bool PFFirewall::setDnsStubEnabled(bool enabled)
{
    return getStubDns().enable(enabled);
}

#endif
