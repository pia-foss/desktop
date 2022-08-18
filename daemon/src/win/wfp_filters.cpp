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
#line SOURCE_FILE("wfp_filters.cpp")

#include "wfp_filters.h"
#include "brand.h"

namespace
{
    // Our WFP sublayer key (taken from win/win_firewall.cpp)
    const QString subLayerKey = BRAND_WINDOWS_WFP_SUBLAYER_GUID;

    // Map WFP names to human-readable names
    const QHash<QString, QString> wfpNameMap {
        { "FWPM_LAYER_ALE_AUTH_CONNECT_V4", "Ipv4" },
        { "FWPM_LAYER_ALE_AUTH_CONNECT_V6", "Ipv6" },
        { "FWP_ACTION_BLOCK", "BLOCK" },
        { "FWP_ACTION_PERMIT", "PERMIT" },
        { "FWP_MATCH_EQUAL", "equals" },
        { "FWPM_CONDITION_IP_REMOTE_ADDRESS", "remote_ip" },
        { "FWPM_CONDITION_ALE_APP_ID", "app_id" },
        { "FWPM_CONDITION_IP_REMOTE_PORT", "remote_port" },
        { "FWPM_CONDITION_IP_LOCAL_PORT", "local_port" },
        { "FWPM_CONDITION_IP_LOCAL_INTERFACE", "local_interface" }
    };
}

// Render the processed filter information as one large string
QByteArray WfpFilters::render()
{
    QStringList result {};

    parseXml();

    result << QStringLiteral("Number of PIA filters: %1 - Highest priority at TOP").arg(_wfpFilters.count());

    std::sort(_wfpFilters.begin(), _wfpFilters.end(), [](const WfpFilter &a, const WfpFilter &b) {
        return a.weight > b.weight;
    });

    for (const auto &filter : _wfpFilters)
    {
        result << QStringLiteral("[%1] [Weight: %2] %3 condition: %4").arg(filter.ipVersion).arg(filter.weight, 2).arg(filter.action, -6).arg(filter.condition);
    }

    // Concatenate lines and convert from QString back to QByteArray
    return result.join("\r\n").toLatin1();
}

void WfpFilters::parseXml()
{
    QDomDocument doc;
    doc.setContent(_xml);
    QDomNodeList items = doc.elementsByTagName("item");

    for (int i = 0; i < items.size(); i++)
    {
        QDomNode node = items.item(i);
        QDomElement el = node.firstChildElement("subLayerKey");

        if (el.text() == subLayerKey)
        {
            int weight = dig(node, {"weight", "uint8"}).toInt();
            QString ipVersion = tr(dig(node, {"layerKey"}));
            QString action = tr(dig(node, {"action", "type"}));
            QString condition = processCondition(node);

            _wfpFilters.push_back({weight, ipVersion, action, condition});
        }
    }
}

QString WfpFilters::processConditionValue(const QDomElement &cv) const
{
    QDomNodeList children = cv.childNodes();
    QStringList result {};

    for (int i = 0; i < children.size(); i++)
    {
        QDomElement element = children.item(i).toElement();

        if (element.tagName() == "type")
            continue; // Not interested in type information
        else if (element.firstChild().isText())
            result << maybeTruncate(element.text());
        else
            result << QStringLiteral("%1: %2").arg(element.tagName()).arg(processConditionValue(element));
    }

    return result.join(", ");
}

QString WfpFilters::processCondition(const QDomNode &node) const
{
    QDomElement filterCondition { node.firstChildElement("filterCondition") };

    if (filterCondition.attribute("numItems").toInt() < 1)
    {
        // No conditions exist on this filter
        return "None";
    }
    else
    {
        QDomNodeList items = filterCondition.elementsByTagName("item");

        QStringList result {};
        for (int i = 0; i < items.size(); i++)
        {
            QString conditionString;
            auto item = items.item(i);
            auto conditionValue = item.firstChildElement("conditionValue");

            result << QStringLiteral("<%2 %1 %3>")
                .arg(tr(dig(item, { "matchType" }))).arg(tr(dig(item, { "fieldKey" })))
                .arg(processConditionValue(conditionValue));
        }
        return result.join(" ");
    }
}

QString WfpFilters::dig(const QDomNode &node, const QStringList &elementNames) const
{
    QDomElement element { node.toElement() };

    for (const QString &str : elementNames)
    {
        element = element.firstChildElement(str);
        if (element.isNull())
        {
            auto errorStr { QStringLiteral("No such element: %1").arg(str) };
            qError() << errorStr;
            return errorStr;
        }
    }

    return element.text();
}


QString WfpFilters::tr(const QString &wfpName) const
{
    const QString &simpleName { wfpNameMap[wfpName] };

    if (simpleName.isEmpty())
        return wfpName;
    else
        return simpleName;
}

QString WfpFilters::maybeTruncate(QString text) const
{
    // only truncate long strings if they're not file paths
    if (text.size() > 20 && !text.contains("\\"))
    {
        text.truncate(17);
        text.append("...");
    }
    return text;
}
