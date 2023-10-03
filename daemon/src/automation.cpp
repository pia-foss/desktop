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

#include "automation.h"
auto Automation::ruleMatchesNetwork(const AutomationRule &rule,
                                    const NetworkConnection &network) const
    -> RuleTest
{
    // If the default connection is Wi-Fi, but the interface is not yet known to
    // be associated, it cannot match any rule.
    // This occurs as a transient state when connecting - all platform backends
    // get routing and Wi-Fi information separately, and we may not know that
    // the interface is connected by the time the routes appear.  Since we do
    // not know whether the interface is encrypted or what SSID it's connected
    // to, no rule can match.
    if(network.medium() == NetworkConnection::Medium::WiFi &&
        !network.wifiAssociated())
    {
        return RuleTest::NoMatch;
    }

    // Check the rule type
    if(rule.condition().ruleType() == QStringLiteral("openWifi"))
    {
        if(network.medium() == NetworkConnection::Medium::WiFi &&
           !network.wifiEncrypted())
        {
            return RuleTest::GeneralMatch;
        }
        return RuleTest::NoMatch;
    }
    else if(rule.condition().ruleType() == QStringLiteral("protectedWifi"))
    {
        if(network.medium() == NetworkConnection::Medium::WiFi &&
           network.wifiEncrypted())
        {
            return RuleTest::GeneralMatch;
        }
        return RuleTest::NoMatch;
    }
    else if(rule.condition().ruleType() == QStringLiteral("wired"))
    {
        if(network.medium() == NetworkConnection::Medium::Wired)
            return RuleTest::GeneralMatch;
        return RuleTest::NoMatch;
    }
    else if(rule.condition().ruleType() == QStringLiteral("ssid"))
    {
        // Look for the specified SSID.  If no SSID is set (somehow), the rule
        // is broken, don't match this.  (Shouldn't happen normally but could
        // occur if settings were manipulated manually, via CLI, etc.)
        if(rule.condition().ssid() != QStringLiteral("") &&
           network.wifiSsid() == rule.condition().ssid())
        {
            return RuleTest::WifiSsidMatch;
        }
        return RuleTest::NoMatch;
    }
    else
    {
        // Otherwise, this rule type is not known (possibly a new rule type
        // added by a future release) - ignore it.
        return RuleTest::NoMatch;
    }
}

bool Automation::networkMatchesLastDefIpv4(const NetworkConnection &newDefIpv4) const
{
    if(!_pLastDefIpv4)
        return true;    // No last nework, and a network is present now

    // Test fields that indicate this is a different network connection or that
    // would cause it to match rules differently.  Ignore fields that just
    // indicate the configuration for this connection (gateways, IP addresses,
    // and defaultIpv6() since Automation only checks IPv4 networks).

    // Interface - if changed, indicates that this is a new connection, although
    // possibly to an equivalent network.  For example, if two wired connections
    // are active simultaneously, then the default connection is disconnected,
    // a different connection becomes the default, and it might only differ in
    // interface (since we ignore IP addresses).
    return newDefIpv4.networkInterface() == _pLastDefIpv4->networkInterface() &&
        // Medium and WiFi parameters affect rule matching.
        newDefIpv4.medium() == _pLastDefIpv4->medium() &&
        newDefIpv4.wifiAssociated() == _pLastDefIpv4->wifiAssociated() &&
        newDefIpv4.wifiEncrypted() == _pLastDefIpv4->wifiEncrypted() &&
        newDefIpv4.wifiSsid() == _pLastDefIpv4->wifiSsid();
    // Ignore the following:
    // - defaultIpv4 - although this is checked to find the default connection,
    //   it's always true by this point since we only care about the default.
    // - defaultIpv6 - only IPv4 is examined by Automation
    // - gatewayIpv4/gatewayIpv6/addressesIpv4/addressesIpv6 - these don't
    //   affect rule choice and just indicate configuration on the interface.
    //   In some cases these can change without reconnecting, don't trigger a
    //   rule in that case.
}

const AutomationRule *Automation::matchLastDefIpv4() const
{
    if(!_pLastDefIpv4)
        return nullptr; // No network currently, no rules can match

    RuleTest currentResult{RuleTest::NoMatch};
    const AutomationRule *pCurrentMatch{};

    // Find the most specific rule - if no rules match, we'll return the
    // nullptr from above
    for(const auto &rule : _rules)
    {
        RuleTest nextResult{ruleMatchesNetwork(rule, *_pLastDefIpv4)};
        if(nextResult > currentResult)
        {
            pCurrentMatch = &rule;
            currentResult = nextResult;
        }
    }

    return pCurrentMatch;
}

bool Automation::updateLastRule(const AutomationRule *pNewRule)
{
    if(!pNewRule && !_pLastRule)
    {
        qInfo() << "Still no matching rule, no change";
        return false;
    }
    else if(!_pLastRule) // pNewRule is set, now have a match
    {
        qInfo() << "Now matched rule" << *pNewRule;
        _pLastRule.emplace(*pNewRule);
    }
    else if(!pNewRule) // _pLastRule is set, no longer have a match
    {
        qInfo() << "No longer have any matching rule";
        _pLastRule.clear();
    }
    // Otherwise, both are set, see if the actual rule changed (even if just the
    // action changed)
    else if(*pNewRule != *_pLastRule)
    {
        qInfo() << "Matching rule changed from" << *_pLastRule << "to"
            << *pNewRule;
        *_pLastRule = *pNewRule;
    }
    else
    {
        qInfo() << "Matching rule" << _pLastRule << "is the same, no change";
        return false;
    }

    return true;
}

void Automation::setRules(std::vector<AutomationRule> rules)
{
    qInfo() << "Changing automation rules from" << _rules.size() << "rules to"
        << rules.size() << "rules";

    _rules = std::move(rules);

    const AutomationRule *pNewRule = matchLastDefIpv4();
    if(updateLastRule(pNewRule))
        emit ruleTriggered(_pLastRule, Trigger::RuleChange);
}

void Automation::setNetworks(const std::vector<NetworkConnection> &newNetworks)
{
    // Find the default IPv4 network (if there is one)
    auto itDefIpv4 = std::find_if(newNetworks.begin(), newNetworks.end(),
        [](const NetworkConnection &conn){return conn.defaultIpv4();});

    const NetworkConnection *pNewDefIpv4{};
    if(itDefIpv4 != newNetworks.end())
        pNewDefIpv4 = &*itDefIpv4;

    if(!pNewDefIpv4 && !_pLastDefIpv4)
    {
        qInfo() << "Still no IPv4 network, no change";
        return; // Don't need to re-check rules
    }
    else if(!_pLastDefIpv4) // pNewDefIpv4 is set, now have a network
    {
        qInfo() << "Now connected to" << *pNewDefIpv4;
        _pLastDefIpv4.emplace(*pNewDefIpv4);
        // Recheck rules below
    }
    else if(!pNewDefIpv4)   // _pLastDefIpv4 is set, no longer have a network
    {
        qInfo() << "No longer connected to a network";
        _pLastDefIpv4.clear();
        // Recheck rules below
    }
    // Otherwise, both are set, see if the network changed
    else if(!networkMatchesLastDefIpv4(*pNewDefIpv4))
    {
        qInfo() << "Network changed from" << *_pLastDefIpv4 << "to"
            << *pNewDefIpv4;
        *_pLastDefIpv4 = *pNewDefIpv4;
        // Recheck rules below
    }
    else if(*pNewDefIpv4 != *_pLastDefIpv4)
    {
        qInfo() << "Network changed in configuration only from"
            << *_pLastDefIpv4 << "to" << *pNewDefIpv4;
        // Doesn't indicate a new connection.  Update our stored network
        // (although this only affects tracing since the differing parameters
        // aren't relevant to Automation), but don't reapply rules.
        *_pLastDefIpv4 = *pNewDefIpv4;
        return;
    }
    else
    {
        qInfo() << "Current network" << *pNewDefIpv4 << "is the same, no change";
        return; // Don't need to re-check rules
    }

    // Detect and apply the current rule - trigger even if the rule didn't
    // change, since the network changed
    const AutomationRule *pNewRule = matchLastDefIpv4();
    if(!updateLastRule(pNewRule))
    {
        // Just trace that the rule didn't change, but that we're emitting a
        // trigger anyway
        qInfo() << "Emitting trigger even though rule didn't change due to network change";
    }
    emit ruleTriggered(_pLastRule, Trigger::NetworkChange);
}
