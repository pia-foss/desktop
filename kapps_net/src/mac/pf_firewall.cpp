// Copyright (c) 2023 Private Internet Access, Inc.
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


#include <kapps_net/net.h>
#include <unistd.h>
#include <kapps_core/src/logger.h>
#include "pf_firewall.h"
#include "../firewall.h"
#include <kapps_core/src/newexec.h>
#include <kapps_core/src/util.h>

namespace kapps { namespace net {

namespace
{
    const std::string kPfWarning = "pfctl: Use of -f option, could result in flushing of rules\npresent in the main ruleset added by the system at startup.\nSee /etc/pf.conf for further details.\n";
    kapps::core::Executor _pfExecutor{KAPPS_CORE_CURRENT_CATEGORY, kPfWarning};
}

int PFFirewall::execute(const std::string& command, bool ignoreErrors)
{
    return _pfExecutor.bash(command, ignoreErrors);
}

bool PFFirewall::isPFEnabled()
{
    return 0 == execute(qs::format("test -s '%/pf.token' && pfctl -s References | grep -qFf '%/pf.token'", _config.daemonDataDir, _config.daemonDataDir), true);
}

void PFFirewall::installRootAnchors()
{
    KAPPS_CORE_INFO() << "Installing PF root anchors";

    // Append our NAT anchors by reading back and re-applying NAT rules only
    auto insertNatAnchors = qs::format(
        "( "
            R"(pfctl -sn | grep -v '%/*'; )"   // Translation rules (includes both nat and rdr, despite the modifier being 'nat')
            R"(echo 'nat-anchor "%/*"'; )"     // PIA's translation anchors
            R"(echo 'rdr-anchor "%/*"'; )"
            R"(echo 'load anchor "%" from "%/pf/%.conf"'; )" // Load the PIA anchors from file
        ") | pfctl -N -f -", _rootAnchor, _rootAnchor, _rootAnchor,_rootAnchor, _config.resourceDir, _rootAnchor);

    execute(insertNatAnchors);

    // Append our filter anchor by reading back and re-applying filter rules
    // only.  pfctl -sr also includes scrub rules, but these will be ignored
    // due to -R.
    auto insertFilterAnchor = qs::format(
        "( "
            R"(pfctl -sr | grep -v '%/*'; )"   // Filter rules (everything from pfctl -sr except 'scrub')
            R"(echo 'anchor "%/*"'; )"         // PIA's filter anchors
            R"(echo 'load anchor "%" from "%/pf/%.conf"'; )" // Load the PIA anchors from file
        " ) | pfctl -R -f -", _rootAnchor, _rootAnchor, _rootAnchor, _config.resourceDir, _rootAnchor);
     execute(insertFilterAnchor);
}

bool PFFirewall::isRootAnchorLoaded(const std::string &modifier)
{
    // Our Root anchor is loaded if:
    // 1. It is is included among the top-level anchors
    // 2. It is not empty (i.e it contains sub-anchors)
    return 0 == execute(qs::format("pfctl -s % | grep -q '%' && pfctl -q -a '%' -s % 2> /dev/null | grep -q .", modifier, _rootAnchor, _rootAnchor, modifier));
}

bool PFFirewall::areAllRootAnchorsLoaded()
{
    return isRootAnchorLoaded("nat") &&
        isRootAnchorLoaded("rules");
}

bool PFFirewall::areAnyRootAnchorsLoaded()
{
    return isRootAnchorLoaded("nat") ||
        isRootAnchorLoaded("rules");
}

std::string PFFirewall::getMacroArgs(const MacroPairs& macroPairs)
{
    std::vector<std::string> macroVec;
    for(const auto &pair : macroPairs)
        macroVec.push_back(qs::format("-D%=%", pair.first, pair.second));

    return qs::joinVec(macroVec, " ");
}

void PFFirewall::install()
{
    // remove hard-coded (legacy) pia anchor from /etc/pf.conf if it exists
    execute(qs::format("if grep -Fq '%' /etc/pf.conf ; then echo \"`cat /etc/pf.conf | grep -vF '%'`\" > /etc/pf.conf ; fi", _rootAnchor));

    // Clean up any existing rules if they exist.
    uninstall();

    // Sleep for 1/100th of a second. Without this small sleep
    // pfctl gets overwhelmed and will ignore the installRootAnchors
    timespec waitTime{0, 10'000'000};
    ::nanosleep(&waitTime, nullptr);

    installRootAnchors();
    execute(qs::format("pfctl -E 2>&1 | grep -F 'Token : ' | cut -c9- > '%/pf.token'", _config.daemonDataDir));
}

void PFFirewall::uninstall()
{
    KAPPS_CORE_INFO() << "Uninstalling PF root anchor";

    // Flush our rules if any of our root anchors are loaded
    if (areAnyRootAnchorsLoaded())
        execute(qs::format("pfctl -q -a '%' -F all", _rootAnchor));

    if (isPFEnabled())
    {
        execute(qs::format("test -f '%/pf.token' && pfctl -X `cat '%/pf.token'` && rm '%/pf.token'",
            _config.daemonDataDir, _config.daemonDataDir, _config.daemonDataDir));
    }
}

bool PFFirewall::isInstalled()
{
    return isPFEnabled() && areAllRootAnchorsLoaded();
}

void PFFirewall::enableAnchor(const kapps::core::StringSlice &anchor,
                              const kapps::core::StringSlice &modifier,
                              const MacroPairs &macroPairs)
{
    execute(qs::format("if pfctl -q -a '%/%' -s % 2> /dev/null | grep -q . ; then echo '%: ON' ; else echo '%: OFF -> ON' ; pfctl -q -a '%/%' -F all % -f '%/pf/%.%.conf' ; fi", _rootAnchor, anchor, modifier, anchor,
        anchor, _rootAnchor, anchor, getMacroArgs(macroPairs), _config.resourceDir, _rootAnchor, anchor));
}

void PFFirewall::disableAnchor(const kapps::core::StringSlice &anchor,
                               const kapps::core::StringSlice &modifier)
{
    execute(qs::format("if ! pfctl -q -a '%/%' -s % 2> /dev/null | grep -q . ; then echo '%: OFF' ; else echo '%: ON -> OFF' ; pfctl -q -a '%/%' -F all ; fi", _rootAnchor, anchor, modifier, anchor, anchor, _rootAnchor, anchor));
}

void PFFirewall::setAnchorEnabled(const kapps::core::StringSlice &anchor,
                                  const kapps::core::StringSlice &modifier, bool enable,
                                  const MacroPairs &macroPairs)
{
    if(enable)
        enableAnchor(anchor, modifier, macroPairs);
    else
        disableAnchor(anchor, modifier);
}

void PFFirewall::setAnchorTable(const kapps::core::StringSlice &anchor,
                                bool enabled, const kapps::core::StringSlice &table,
                                const std::vector<std::string>& items)
{
    if(enabled)
        execute(qs::format("pfctl -q -a '%/%' -t '%' -T replace %", _rootAnchor, anchor, table, qs::joinVec(items, " ")));
    else
        execute(qs::format("pfctl -q -a '%/%' -t '%' -T kill", _rootAnchor, anchor, table), true);
}

void PFFirewall::setFilterEnabled(const kapps::core::StringSlice &anchor,
                                  bool enable, const MacroPairs &macroPairs)
{
    setAnchorEnabled(anchor, "rules", enable, macroPairs);
}

void PFFirewall::setFilterWithRules(const kapps::core::StringSlice &anchor,
                                    bool enabled, const std::vector<std::string> &ruleList)
{
    if(!enabled)
        return (void)execute(qs::format("pfctl -q -a '%/%' -F all", _rootAnchor, anchor), true);
    else
        return (void)execute(qs::format("echo -e \"%\" | pfctl -q -a '%/%' -f -", qs::joinVec(ruleList, "\n"), _rootAnchor, anchor));
}

void PFFirewall::setTranslationEnabled(const kapps::core::StringSlice &anchor,
                                       bool enable, const MacroPairs &macroPairs)
{
    setAnchorEnabled(anchor, "nat", enable, macroPairs);
}

void PFFirewall::ensureRootAnchorPriority()
{
    // We check whether our filter appears last in the ruleset. If it does not,
    // then reinstall PIA anchors (this happens atomically).
    // We don't check for priority of the nat/rdr anchors specifically, but
    // these are less likely to have conflicts, and a conflict would likely
    // also occur for filter rules anyway.
    // Appearing last ensures priority.
    if(execute(qs::format("pfctl -sr | tail -1 | grep -qF '%'", _rootAnchor)) != 0)
    {
        KAPPS_CORE_INFO() << "Reinstall PIA root anchors, priority was overridden";
        installRootAnchors();
    }
}

void PFFirewall::flushState()
{
    execute("pfctl -F states");
}

}}
