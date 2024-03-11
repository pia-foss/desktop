// Copyright (c) 2024 Private Internet Access, Inc.
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

#ifndef SETTINGS_AUTOMATION_H
#define SETTINGS_AUTOMATION_H

#include "../common.h"
#include "../json.h"

// Automation rules
//
// Automation rules are composed of a "condition" and an "action".  The
// condition determines when the rule applies, and at that point the action is
// applied.
class COMMON_EXPORT AutomationRuleAction : public NativeJsonObject,
    public kapps::core::OStreamInsertable<AutomationRuleAction>
{
    Q_OBJECT
public:
    AutomationRuleAction() {}
    AutomationRuleAction(const AutomationRuleAction &other) {*this = other;}
    AutomationRuleAction &operator=(const AutomationRuleAction &other)
    {
        connection(other.connection());
        return *this;
    }

    bool operator==(const AutomationRuleAction &other) const
    {
        return connection() == other.connection();
    }
    bool operator!=(const AutomationRuleAction &other) const {return !(*this == other);}

public:
    void trace(std::ostream &os) const;

    // What to do with the VPN connection when this rule applies:
    // - "enable" - enable the VPN (connect)
    // - "disable" - disable the VPN (disconnect)
    JsonField(QString, connection, "enable", { "enable", "disable" })
};

// Conditions have both a "network type" and "SSID" to implement the different
// types of rules created from the UI.  Empty or 'null' for any criterion means
// any network matches.
//
// The UI does not allow setting more than one criterion on a given condition
// (only one criterion can be non-null).  The backend applies all non-null
// criteria; if more than one was non-null then they would all have to match to
// trigger a rule.
class COMMON_EXPORT AutomationRuleCondition : public NativeJsonObject,
    public kapps::core::OStreamInsertable<AutomationRuleCondition>
{
    Q_OBJECT

public:
    AutomationRuleCondition() {}
    AutomationRuleCondition(const AutomationRuleCondition &other) {*this = other;}
    AutomationRuleCondition &operator=(const AutomationRuleCondition &other)
    {
        ruleType(other.ruleType());
        ssid(other.ssid());
        return *this;
    }

    bool operator==(const AutomationRuleCondition &other) const
    {
        return ruleType() == other.ruleType() &&
            ssid() == other.ssid();
    }
    bool operator!=(const AutomationRuleCondition &other) const {return !(*this == other);}

public:
    void trace(std::ostream &os) const;

    // Rule type - determines what networks this condition matches
    // - "openWifi" - any unencrypted Wi-Fi network
    // - "protectedWifi" - any encrypted Wi-Fi network
    // - "wired" - any wired network
    // - "ssid" - Wi-Fi network with specified SSID (AutomationCondition::ssid())
    //
    // In some cases, the current network may be of a type that's not
    // supported (such as mobile data, Bluetooth or USB tethering to a phone,
    // etc.)  No network type is currently defined for these networks.
    JsonField(QString, ruleType, {}, { "openWifi", "protectedWifi", "wired", "ssid" })

    // Wireless SSID - if set, only wireless networks with the given SSID will
    // match.
    //
    // Only SSIDs that are printable UTF-8 or Latin-1 can be matched, other
    // SSIDs cannot be represented as text.  We do not distinguish between
    // UTF-8 and Latin-1 encodings of the same text.
    // See NetworkConnection::ssid().
    JsonField(QString, ssid, {})
};

class COMMON_EXPORT AutomationRule : public NativeJsonObject,
    public kapps::core::OStreamInsertable<AutomationRule>
{
    Q_OBJECT

public:
    AutomationRule() {}
    AutomationRule(const AutomationRule &other) {*this = other;}
    AutomationRule &operator=(const AutomationRule &other)
    {
        condition(other.condition());
        action(other.action());
        return *this;
    }

    bool operator==(const AutomationRule &other) const
    {
        return condition() == other.condition() &&
            action() == other.action();
    }
    bool operator!=(const AutomationRule &other) const {return !(*this == other);}

public:
    void trace(std::ostream &os) const;

    JsonObjectField(AutomationRuleCondition, condition, {})
    JsonObjectField(AutomationRuleAction, action, {})
};

#endif
