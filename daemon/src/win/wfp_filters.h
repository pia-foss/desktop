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
#line HEADER_FILE("wfp_filters.h")

#ifndef WFP_FILTERS_H
#define WFP_FILTERS_H

#include <QDomDocument>

// Data associated with a WFP filter
struct WfpFilter
{
    int weight;          // Precedence of filter (larger number is higher precedence)
    QString ipVersion;   // Ipv4 or Ipv6
    QString action;      // PERMIT or BLOCK
    QString condition;   // e.g  <remote_ip equals v4AddrMask: 127.0.0.1, 255.255.255.255>
};

// Converts WFP (Windows Filtering Platform) XML into a human-readable form.
// This allows us to introspect on our network filters: killswitch, LAN access, and so on.
//
// See an example of the XML at the bottom of this file.
class WfpFilters
{
    CLASS_LOGGING_CATEGORY("WfpFilters");

public:
    explicit WfpFilters(const QByteArray &xml) : _xml{xml}
    {
    }

    // Generate human-readable output for the PIA WFP filters
    QByteArray render();

private:
    // Process all the PIA WFP XML filters, storing them in _wfpFilters
    void parseXml();

    // Process XML for the condition values of a filter condition
    QString processConditionValue(const QDomElement &cv) const;

    // Process XML for filter conditions
    QString processCondition(const QDomNode &node) const;

    // Truncate a WFP string depending on its content
    QString maybeTruncate(QString text) const;

    // Easily navigate nested xml nodes
    QString dig(const QDomNode &node, const QStringList &elementNames) const;

    // Wrapper around wfpNameMap
    QString tr(const QString &wfpName) const;

private:
    const QByteArray _xml;
    QVector<WfpFilter> _wfpFilters;
};

/*
Example XML element that the class processes:

<item>
    <filterKey>{6e624b39-d146-4243-a43c-e61642c39f87}</filterKey>
    <displayData>
        <name>Private Internet Access Firewall</name>
        <description>Implements privacy filtering features of Private Internet Access.</description>
    </displayData>
    <flags numItems="1">
        <item>FWPM_FILTER_FLAG_PERSISTENT</item>
    </flags>
    <providerKey>{08de3850-a416-4c47-b3ad-657c5ef140fb}</providerKey>
    <providerData/>
    <layerKey>FWPM_LAYER_ALE_AUTH_CONNECT_V4</layerKey>
    <subLayerKey>{f31e288d-de5a-4522-9458-de14ebd0a3f8}</subLayerKey>
    <weight>
        <type>FWP_UINT8</type>
        <uint8>15</uint8>
    </weight>
    <filterCondition numItems="1">
        <item>
            <fieldKey>FWPM_CONDITION_IP_REMOTE_ADDRESS</fieldKey>
            <matchType>FWP_MATCH_EQUAL</matchType>
            <conditionValue>
                <type>FWP_V4_ADDR_MASK</type>
                <v4AddrMask>
                    <addr>127.0.0.1</addr>
                    <mask>255.255.255.255</mask>
                </v4AddrMask>
            </conditionValue>
        </item>
    </filterCondition>
    <action>
        <type>FWP_ACTION_PERMIT</type>
        <filterType/>
    </action>
    <rawContext>0</rawContext>
    <reserved/>
    <filterId>117325</filterId>
    <effectiveWeight>
        <type>FWP_UINT64</type>
        <uint64>17582052945254416384</uint64>
    </effectiveWeight>
</item>


The above will be converted to the more readable:

[Ipv4] [Weight: 15] PERMIT, condition: <remote_ip equals v4AddrMask: 127.0.0.1, 255.255.255.255>
*/

#endif
