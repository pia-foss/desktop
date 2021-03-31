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

#ifndef AUTOMATION_H
#define AUTOMATION_H

#include "common.h"
#include "settings.h"
#include "networkmonitor.h"

// Automation applies a set of rules to the current network detected by
// NetworkMonitor.  When a rule is triggered, it's emitted from ruleTriggered().
// Daemon then determines if the trigger has any effect (e.g. "connect" while
// already connected has no effect), then applies the action and updates
// DaemonState.
class Automation : public QObject
{
    Q_OBJECT

private:
    // Result of a rule test against a network.  This indicates whether the
    // rule matches at all, and if so, the specificity of the rule, as more
    // specific matches are preferred.
    //
    // These values are in ascending order so we can find the maximum
    // specificity when matching.
    enum RuleTest : int
    {
        NoMatch,
        // General network type match only
        GeneralMatch,
        // Wi-Fi SSID matches a specific rule criterion, this is the only
        // specific type of match currently
        WifiSsidMatch,
    };

public:
    // Indicates what caused a rule change
    enum class Trigger
    {
        RuleChange,
        NetworkChange,
    };
    Q_ENUM(Trigger);

private:
    // Test if a rule matches a specific network - returns a RuleTest value
    // indicating whether it matched and the specificity of the match.
    RuleTest ruleMatchesNetwork(const AutomationRule &rule,
                                const NetworkConnection &network) const;

    // Test whether a default IPv4 network matches the last default IPv4
    // network.  This determines whether this network causes a new rule trigger
    // or not.  This differs from NetworkConnection::operator==(); it only
    // considers fields that matter to Automation.
    bool networkMatchesLastDefIpv4(const NetworkConnection &newDefIpv4) const;

    // Match the last default IPv4 network (_pLastDefIpv4) to the current rules
    // in _rules - return the matching rule (or no rule).  The result points to
    // a rule in _rules.
    const AutomationRule *matchLastDefIpv4() const;

    // Update _pLastRule, result indicates whether it has changed
    bool updateLastRule(const AutomationRule *pNewRule);

public:
    // Set the current rules from the user's settings.  If this causes the
    // current rule to change (including a change to no-rule), the current rule
    // is emitted with a Trigger::RuleChange trigger.
    //
    // Note that when a trigger occurs due to a rules change, it's possible that
    // only the _action_ changed - the rule condition might not have changed.
    void setRules(std::vector<AutomationRule> rules);

    // Set the current networks - from NetworkMonitor::networksChanged().  If
    // this causes the current rule to change (including a change to no-rule),
    // the current rule is emitted with a Trigger::NetworkChange reason.
    //
    // Only the default IPv4 network is used from the networks specified, others
    // are ignored.
    //
    // Note that when a trigger occurs due to a network change, it is possible
    // for the same rule to trigger again with no change at all - this occurs
    // when switching directly from one network to another when both networks
    // match the same rule.
    void setNetworks(const std::vector<NetworkConnection> &newNetworks);

signals:
    // The current rule has changed.  The current rule could be 'none'; which
    // is indicated by an empty nullable_t.  'trigger' indicates why this
    // occurred; Daemon may choose not to apply actions for a RuleChange reason
    // (but still should update the current state).
    void ruleTriggered(const nullable_t<AutomationRule> &currentRule,
                       Trigger trigger);

private:
    // The current set of rules (specified by setRules())
    std::vector<AutomationRule> _rules;
    // The last default IPv4 network that we detected - or none
    nullable_t<NetworkConnection> _pLastDefIpv4;
    // The last rule that triggered - or none if no rule matches this network
    nullable_t<AutomationRule> _pLastRule;
};

#endif
