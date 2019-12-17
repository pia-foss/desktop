// Copyright (c) 2019 London Trust Media Incorporated
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
#line SOURCE_FILE("tst_connection.cpp")

#include "integtestcase.h"
#include "cliharness.h"
#include "settings.h"
#include "tunnelcheckstatus.h"

// Connection tests
//
// These tests verify most of the basic Connection settings.  Most of them just
// run through each possible setting value and do a connection test with that
// value.
//
// Some more complex settings, like Proxy, are tested separately.
class TestConnection : public IntegTestCase
{
    Q_OBJECT

private:
    // Reset settings for a connection test
    void resetSettings()
    {
        CliHarness::resetSettings();
        // Disable "Try Alternate Settings" since we are checking specific
        // configurations
        CliHarness::applySetting(QStringLiteral("automaticTransport"), false);
    }

    // Check connectivity by expecting a request to be routed either on-VPN or
    // off-VPN.  Initial failures are allowed, but an incorrectly routed request
    // fails the test.
    // The result indicates success or failure, but testConnectivity() asserts
    // this internally, so tests only need to check this if they need to abort
    // on failure.
    bool testConnectivity(TunnelCheckStatus::Status expectedStatus)
    {
        QElapsedTimer elapsed;
        elapsed.start();

        TunnelCheckStatus tunnelStatus;
        // Ignore result of qWaitFor, will be observed as Unknown status if it
        // times out
        (void)QTest::qWaitFor([&]() -> bool {return tunnelStatus.status() != TunnelCheckStatus::Status::Unknown;}, 10000);
        COMPARE_CONTINUE(tunnelStatus.status(), expectedStatus);
        if(tunnelStatus.status() != expectedStatus)
        {
            qWarning() << "Expected request to be" << traceEnum(expectedStatus)
                << "- got" << traceEnum(tunnelStatus.status());
            return false;
        }

        qInfo() << "Verified routing" << traceEnum(expectedStatus) << "after"
            << traceMsec(elapsed.elapsed());
        return true;
    }

    // Run a connection test - just connect, wait for the connection to
    // complete, then disconnect and wait for that to complete.
    // The test function typically has set up the desired connection settings
    // before executing this test.
    void runConnectionTest()
    {
        CliHarness::connectVpn();
        CliHarness::waitFor(QStringLiteral("connectionstate"), QStringLiteral("Connected"));
        // Verify that we're really connected by checking the client/status API
        // We should never get a result routed outside the VPN, this causes the
        // test to fail.  Failures are initially OK though.
        testConnectivity(TunnelCheckStatus::Status::OnVPN);
        CliHarness::disconnectAndWait();
        testConnectivity(TunnelCheckStatus::Status::OffVPN);
    }

    // Test connecting with all of the options for a specific setting, except
    // the default (covered by the "default settings" test)
    void testSettingChoices(const QString &settingName,
                            const QString &defaultValue,
                            const QStringList &settingChoices)
    {
        for(const auto &value : settingChoices)
        {
            if(value == defaultValue)
                continue;   // Skip the default

            qInfo() << "Testing" << settingName << "-" << value;
            CliHarness::applySetting(settingName, value);
            runConnectionTest();
        }
    }

    virtual void integInit() override
    {
        IntegTestCase::integInit();
        // Ensure the client is disconnected
        QCOMPARE(CliHarness::get(QStringLiteral("connectionstate")), QStringLiteral("Disconnected"));
        // Reset to default settings for connection tests
        resetSettings();
    }

private slots:
    void testSimpleConnection()
    {
        runConnectionTest();
    }

    // Test all choices for each Connection setting
    void testProtocolChoices()
    {
        testSettingChoices(QStringLiteral("protocol"), DaemonSettings::default_protocol(),
                           DaemonSettings::choices_protocol());
    }
    void testRemotePorts()
    {
        // The port lists are updated dynamically from the region list, but for
        // now this just uses the defaults (which are currently the same)
        DaemonData defaultData;

        // Test TCP ports
        CliHarness::applySetting(QStringLiteral("protocol"), QStringLiteral("tcp"));
        // The port lists don't include "0" ("default"), this is covered by
        // testProtocolChoices() since it's the default setting
        for(const auto &port : defaultData.tcpPorts())
        {
            CliHarness::applySetting(QStringLiteral("remotePortTCP"), static_cast<int>(port));
            runConnectionTest();
        }

        // Test UDP ports
        CliHarness::applySetting(QStringLiteral("protocol"), QStringLiteral("udp"));
        for(const auto &port : defaultData.udpPorts())
        {
            CliHarness::applySetting(QStringLiteral("remotePortUDP"), static_cast<int>(port));
            runConnectionTest();
        }
    }
    void testSmallPackets()
    {
        // Although "use small packets" is displayed as a boolean setting, it's
        // actually a configurable MTU value.  "0" is the default, test the
        // default "enabled" value.
        qInfo() << "Testing \"Use Small Packets\"";
        CliHarness::applySetting(QStringLiteral("mtu"), 1250);
        runConnectionTest();
    }
    void testCipherChoices()
    {
        testSettingChoices(QStringLiteral("cipher"), DaemonSettings::default_cipher(),
                           DaemonSettings::choices_cipher());
    }
    void testAuthChoices()
    {
        // Auth choices only matter when using a CBC cipher
        CliHarness::applySetting(QStringLiteral("cipher"), QStringLiteral("AES-128-CBC"));
        testSettingChoices(QStringLiteral("auth"), DaemonSettings::default_auth(),
                           DaemonSettings::choices_auth());
    }
    void testServerCertChoices()    // "Handshake" setting
    {
        testSettingChoices(QStringLiteral("serverCertificate"), DaemonSettings::default_serverCertificate(),
                           DaemonSettings::choices_serverCertificate());
    }
};

namespace
{
    IntegTestCaseDef<TestConnection> _def;
}

#include "tst_connection.moc"
