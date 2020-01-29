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

#include "common.h"
#include <QtTest>

#include "daemon/src/win/wfp_filters.h"
#include "brand.h"

namespace
{
    const QByteArray validXml = QByteArrayLiteral(
R"(
<item>
    <filterKey>{6e624b39-d146-4243-a43c-e61642c39f87}</filterKey>
    <displayData>
        <name>Private Internet Access Firewall</name>
        <description>Implements privacy filtering features of Private Internet Access.</description>
    </displayData>
    <flags numItems="1">
        <item>FWPM_FILTER_FLAG_PERSISTENT</item>
    </flags>
    <providerKey>)" BRAND_WINDOWS_WFP_PROVIDER_GUID R"(</providerKey>
    <providerData/>
    <layerKey>FWPM_LAYER_ALE_AUTH_CONNECT_V4</layerKey>
    <subLayerKey>)" BRAND_WINDOWS_WFP_SUBLAYER_GUID R"(</subLayerKey>
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
</item>
)");

    const QByteArray invalidXml = QByteArrayLiteral(
R"(
<item>
    <filterKey>{6e624b39-d146-4243-a43c-e61642c39f87}</filterKey>
    <displayData>
        <name>Private Internet Access Firewall</name>
        <description>Implements privacy filtering features of Private Internet Access.</description>
    </displayData>
    <flags numItems="1">
        <item>FWPM_FILTER_FLAG_PERSISTENT</item>
    </flags>
    <providerKey>)" BRAND_WINDOWS_WFP_PROVIDER_GUID R"(</providerKey>
    <providerData/>
    <layerKey>FWPM_LAYER_ALE_AUTH_CONNECT_V4</layerKey>
    <subLayerKey>)" BRAND_WINDOWS_WFP_SUBLAYER_GUID R"(</subLayerKey>
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
</item>
)");

    const QString validParsedOutput = "Number of PIA filters: 1 - Highest priority at TOP\r\n"
                                      "[Ipv4] [Weight: 15] PERMIT condition: <remote_ip equals v4AddrMask: 127.0.0.1, 255.255.255.255>";
}

class tst_wfp_filters : public QObject
{
    Q_OBJECT

private slots:
    void testSuccessfulParse()
    {
        WfpFilters wfp(validXml);
        QCOMPARE(wfp.render(), validParsedOutput);
    }

    void testMissingFields()
    {
        QTest::ignoreMessage(QtSystemMsg, QRegularExpression{"No such element: action"});

        WfpFilters wfp(invalidXml);

        QRegularExpression re{"No such element: action"};
        QVERIFY(re.match(wfp.render()).hasMatch());
    }

    void testNoItems()
    {
        WfpFilters wfp("<item></item>");
        QCOMPARE(wfp.render(), "Number of PIA filters: 0 - Highest priority at TOP");
    }

    void testInvalidXml()
    {
        WfpFilters wfp("item>/item>");
        QCOMPARE(wfp.render(), "Number of PIA filters: 0 - Highest priority at TOP");
    }
};

QTEST_GUILESS_MAIN(tst_wfp_filters)
#include TEST_MOC
