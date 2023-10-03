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

#include <common/src/common.h>
#line SOURCE_FILE("tst_regions.cpp")

#include "integtestcase.h"
#include "cliharness.h"
#include <common/src/settings/daemonsettings.h>
#include "tunnelcheckstatus.h"
#include <random>
#include <QDateTime>
#include <QTest>

// Region connection tests
//
// These tests verify standard connections on different regions.
//
class TestRegions : public IntegTestCase
{
    Q_OBJECT

private:
    // Set the region to the specified value and connect the vpn.
    // Returns the Ip address obtained for the region.
    QString connectToRegion(QString region)
    {
        CliHarness::set("region", region);
        CliHarness::connectVpn();
        CliHarness::waitFor(QStringLiteral("connectionstate"), QStringLiteral("Connected"));
        CliHarness::waitForNotEqual(QStringLiteral("vpnip"), QStringLiteral("Unknown"), std::chrono::milliseconds{10000});

        // Retrieve the ip we got for the region
        QString ipAddress = CliHarness::get("vpnip");

        CliHarness::disconnectAndWait();
        return ipAddress;
    }

    virtual void integInit() override
    {
        IntegTestCase::integInit();
        // Ensure the client is disconnected
        QCOMPARE(CliHarness::get(QStringLiteral("connectionstate")), QStringLiteral("Disconnected"));
        // Reset to default settings for connection tests
        CliHarness::resetSettings();
        _availableRegions = CliHarness::get("regions").split("\n").filter(QRegExp("^[a-z]+$"));
        // Remove the auto region as it could repeat an Ip address, failing the test.
        _availableRegions.removeOne("auto");
        QVERIFY(_availableRegions.size() >= MAX_REGIONS);

        // Shuffle the regions list so we try a different subset every time
        std::random_device rd;
        std::mt19937 twister(static_cast<unsigned int>(QDateTime::currentDateTime().toMSecsSinceEpoch()));
        std::shuffle(_availableRegions.begin(), _availableRegions.end(), twister);
        _availableRegions = _availableRegions.mid(0, MAX_REGIONS);
    }

    QStringList _availableRegions;
    const int MAX_REGIONS = 10;

private slots:
    void testRegions()
    {
        QStringList regionIps;
        for (const auto& region : _availableRegions) {
            QString regionIp = connectToRegion(region);
            QVERIFY(regionIp != "Unknown");
            bool isNewIp = !regionIps.contains(regionIp);
            QVERIFY(isNewIp);
            regionIps.append(regionIp);
            std::cout << "Region " << region << " OK\n";
        }
    }
};

namespace
{
    IntegTestCaseDef<TestRegions> _def;
}

#include "tst_regions.moc"
