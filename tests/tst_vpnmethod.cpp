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
#include <QtTest>

#include "daemon/src/vpnmethod.h"


// All we can currently test on VPNMethod is the state transition code.
// We implement a dummy class below to define the pure virtual functions and introduce progressState()
// to wrap the private advanceState() method
class TestVPNMethod : public VPNMethod
{
public:
    using VPNMethod::VPNMethod;

    virtual void run(const ConnectionConfig&,
                     const Server&,
                     const Transport&,
                     const QHostAddress&,
                     const QHostAddress&,
                     quint16) override {}

    virtual void shutdown() override {}
    virtual std::shared_ptr<NetworkAdapter> getNetworkAdapter() const override {return _networkAdapter;}

    void progressState(State newState) { advanceState(newState); }

private:
    virtual void networkChanged() override {}

private:
    std::shared_ptr<NetworkAdapter> _networkAdapter;
};


class tst_vpnmethod : public QObject
{
    Q_OBJECT

private slots:

    void testInitialState()
    {
        TestVPNMethod vpnMethod{nullptr, {}};
        QVERIFY(vpnMethod.state() == VPNMethod::State::Created);
    }

    void testCanAdvanceStateForward()
    {
        TestVPNMethod vpnMethod{nullptr, {}};
        vpnMethod.progressState(VPNMethod::State::Connecting);
        QVERIFY(vpnMethod.state() == VPNMethod::State::Connecting);
    }

    void testCannotRegressState()
    {
        TestVPNMethod vpnMethod{nullptr, {}};
        vpnMethod.progressState(VPNMethod::State::Connecting);

        QTest::ignoreMessage(QtCriticalMsg, "Attempted to revert from state Connecting to earlier state Created");

        // We cannot go from Connecting to Created
        vpnMethod.progressState(VPNMethod::State::Created);

        QVERIFY(vpnMethod.state() == VPNMethod::State::Connecting);
    }

    void testNoStateChange()
    {
        TestVPNMethod vpnMethod{nullptr, {}};
        vpnMethod.progressState(VPNMethod::State::Created);
        QVERIFY(vpnMethod.state() == VPNMethod::State::Created);
    }
};

QTEST_GUILESS_MAIN(tst_vpnmethod)
#include TEST_MOC
