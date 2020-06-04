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

#include "daemon/src/daemon.h"

// Helper class to record method call information
struct MethodCall
{
    QString methodName;
    QString subnetArg;
    QString gatewayIpArg;
    QString interfaceNameArg;

    bool operator==(const MethodCall &other) const
    {
        return methodName == other.methodName &&
            subnetArg == other.subnetArg &&
            gatewayIpArg == other.gatewayIpArg &&
            interfaceNameArg == other.interfaceNameArg;
    }

    QString toString() const
    {
        return QStringLiteral("%1(%2, %3, %4)")
            .arg(methodName, subnetArg, gatewayIpArg, interfaceNameArg);
    }

    bool operator!=(const MethodCall &other) const {return !(*this == other);}
};

// Helper class to manage method call information
class MethodRecorder
{
public:
    void record(MethodCall methodCall)
    {
        _methodCalls.push_back(std::move(methodCall));
    }

    void reset() {_methodCalls.clear(); }

    const std::vector<MethodCall>&  methodCalls() const {return _methodCalls;}
private:
   std::vector<MethodCall> _methodCalls;
};


// Our RouteManager stub we use to verify SubnetBypass behavior
class StubRouteManager : public RouteManager
{

// This is the public interface for RouteManager
public:
    virtual void addRoute(const QString &subnet, const QString &gatewayIp, const QString &interfaceName, uint32_t metric=0) const override
    {
        _methodRecorder.record({"addRoute",subnet, gatewayIp, interfaceName});
    }

    virtual void removeRoute(const QString &subnet, const QString &gatewayIp, const QString &interfaceName) const override
    {
        _methodRecorder.record({"removeRoute", subnet, gatewayIp, interfaceName});
    }

// Public interface to expose our test helper
public:
    MethodRecorder &methodRecorder() {return _methodRecorder;}
private:
    mutable MethodRecorder _methodRecorder;
};

namespace
{
    OriginalNetworkScan validNetScan{"192.168.1.1", "eth0", "192.168.1.43", "2001:db8::123"};
}

FirewallParams validFirewallParams()
{
    FirewallParams params{};
    params.enableSplitTunnel = true;
    params.hasConnected = true;
    params.defaultRoute = true;
    params.netScan = validNetScan;
    params.bypassIpv4Subnets = QSet<QString>{"192.168.0.0/16"};

    return params;
}

auto createRouteManager(MethodRecorder* &outPtr) -> std::unique_ptr<StubRouteManager>
{
    auto routeManager{std::make_unique<StubRouteManager>()};

    // Save a pointer to it so we can verify its behaviour
    outPtr = &routeManager->methodRecorder();

    return routeManager;
}

// Verify the method was called (when it was called is not important - consequence of using a QSet to store subnets)
void verifyMethodCall(const QString &methodName, const QString &subnet, const std::vector<MethodCall> &methodCalls, const FirewallParams &params)
{
    MethodCall methodCall{methodName, subnet, params.netScan.gatewayIp(), params.netScan.interfaceName()};

    // Note we have to use std::find here and search over all methodCalls
    // because the subnets are stored as a QSet which does not have a defined order.
    // As a result we can't expect the first subnet in the set to be in the first method call, and so on
    auto result = std::find(methodCalls.begin(), methodCalls.end(), methodCall);

    QVERIFY(result != methodCalls.end());
}

// Verify the method was called at a certain point, i.e was it the first method invoked?
void verifyMethodCalledInOrder(uint order, const QString &methodName, const QString &subnet, const std::vector<MethodCall> &methodCalls, const FirewallParams &params)
{
    MethodCall methodCall{methodName, subnet, params.netScan.gatewayIp(), params.netScan.interfaceName()};
    QVERIFY(methodCall == methodCalls.at(order));
}


class tst_subnetbypass : public QObject
{
    Q_OBJECT

private slots:

    void testValidParams()
    {
        // Test object that allows us to verify the correct methods were invoked
        // with the correct arguments and in the correct order
        MethodRecorder *recorder{nullptr};

        // We have valid params and subnet so RouteManager
        // should be enabled and add a route
        {
            FirewallParams params{validFirewallParams()};
            SubnetBypass bypass{createRouteManager(recorder)};
            bypass.updateRoutes(params);

            QVERIFY(recorder->methodCalls().size() == 1);
            verifyMethodCall("addRoute", "192.168.0.0/16", recorder->methodCalls(), params);
        }

        // Disabled when params.enableSplitTunnel==false
        {
            FirewallParams params{validFirewallParams()};
            params.enableSplitTunnel = false;
            SubnetBypass bypass{createRouteManager(recorder)};
            bypass.updateRoutes(params);

            QVERIFY(recorder->methodCalls().size() == 0);
        }

        // Disabled when params.defaultRoute==false
        {
            FirewallParams params{validFirewallParams()};
            params.defaultRoute = false;
            SubnetBypass bypass{createRouteManager(recorder)};
            bypass.updateRoutes(params);

            QVERIFY(recorder->methodCalls().size() == 0);
        }

        // Disabled when params.hasConnected==false
        {
            FirewallParams params{validFirewallParams()};
            params.defaultRoute = false;
            SubnetBypass bypass{createRouteManager(recorder)};
            bypass.updateRoutes(params);

            QVERIFY(recorder->methodCalls().size() == 0);
        }

        // Disabled when params.netScan.ipv4Valid()==false
        {
            FirewallParams params{validFirewallParams()};
            // Empty interfaceName results in invalid netscan
            params.netScan.interfaceName("");
            SubnetBypass bypass{createRouteManager(recorder)};
            bypass.updateRoutes(params);

            QVERIFY(recorder->methodCalls().size() == 0);
        }
    }

     void testNoChanges()
    {
        MethodRecorder *recorder{nullptr};

        FirewallParams params{validFirewallParams()};
        params.bypassIpv4Subnets = QSet<QString> { "192.168.0.0/16" };

        SubnetBypass bypass{createRouteManager(recorder)};
        bypass.updateRoutes(params);

        QVERIFY(recorder->methodCalls().size() == 1);
        verifyMethodCall("addRoute", "192.168.0.0/16", recorder->methodCalls(), params);

        // No changes to params
        bypass.updateRoutes(params);

        // So no new methods are invoked (number of method calls remains the same)
        QVERIFY(recorder->methodCalls().size() == 1);
    }

    void testMultipleSubnets()
    {
        MethodRecorder *recorder{nullptr};

        FirewallParams params{validFirewallParams()};
        params.bypassIpv4Subnets = QSet<QString> { "192.168.1.0/24", "10.0.0.0/8", "1.1.1.1/32" };
        SubnetBypass bypass{createRouteManager(recorder)};
        bypass.updateRoutes(params);

        QVERIFY(recorder->methodCalls().size() == 3);
        verifyMethodCall("addRoute", "192.168.1.0/24", recorder->methodCalls(), params);
        verifyMethodCall("addRoute", "10.0.0.0/8", recorder->methodCalls(), params);
        verifyMethodCall("addRoute", "1.1.1.1/32", recorder->methodCalls(), params);
    }

    void testChangingSubnets()
    {
        MethodRecorder *recorder{nullptr};

        FirewallParams params{validFirewallParams()};
        params.bypassIpv4Subnets = QSet<QString> { "192.168.1.0/24"};

        SubnetBypass bypass{createRouteManager(recorder)};
        bypass.updateRoutes(params);

        QVERIFY(recorder->methodCalls().size() == 1);
        verifyMethodCall("addRoute", "192.168.1.0/24", recorder->methodCalls(), params);

        // Now let's change the subnet
        params.bypassIpv4Subnets = QSet<QString> {"10.0.0.0/8"};

        // Reset recorder first for convenience
        // This just erases previously stored method calls
        recorder->reset();

        bypass.updateRoutes(params);
        QVERIFY(recorder->methodCalls().size() == 2);
        verifyMethodCalledInOrder(0, "removeRoute", "192.168.1.0/24", recorder->methodCalls(), params);
        verifyMethodCalledInOrder(1, "addRoute", "10.0.0.0/8", recorder->methodCalls(), params);
    }

    void testDisabling()
    {
        MethodRecorder *recorder{nullptr};

        FirewallParams params{validFirewallParams()};
        params.bypassIpv4Subnets = QSet<QString> { "192.168.1.0/24"};

        SubnetBypass bypass{createRouteManager(recorder)};

        // Subnet routes should be created
        bypass.updateRoutes(params);

        // This will disable the subnet bypass on next call to updateRoutes
        params.enableSplitTunnel = false;
        bypass.updateRoutes(params);

        QVERIFY(recorder->methodCalls().size() == 2);
        verifyMethodCalledInOrder(0, "addRoute", "192.168.1.0/24", recorder->methodCalls(), params);
        verifyMethodCalledInOrder(1, "removeRoute", "192.168.1.0/24", recorder->methodCalls(), params);
    }

    void testDisableMultiple()
    {
        MethodRecorder *recorder{nullptr};

        FirewallParams params{validFirewallParams()};
        params.bypassIpv4Subnets = QSet<QString> { "192.168.1.0/24", "10.0.0.0/8" };

        SubnetBypass bypass{createRouteManager(recorder)};
        bypass.updateRoutes(params);

        // Setting this to false will disable SubnetBypass
        params.hasConnected = false;
        bypass.updateRoutes(params);

        QVERIFY(recorder->methodCalls().size() == 4);

        verifyMethodCall("addRoute", "192.168.1.0/24", recorder->methodCalls(), params);
        verifyMethodCall("addRoute", "10.0.0.0/8", recorder->methodCalls(), params);
        verifyMethodCall("removeRoute", "192.168.1.0/24", recorder->methodCalls(), params);
        verifyMethodCall("removeRoute", "10.0.0.0/8", recorder->methodCalls(), params);
    }

    void testChangedNetScan()
    {
        MethodRecorder *recorder{nullptr};

        FirewallParams params{validFirewallParams()};
        params.bypassIpv4Subnets = QSet<QString> { "192.168.1.0/24" };

        SubnetBypass bypass{createRouteManager(recorder)};

        // First let's create routes with the current netScan
        bypass.updateRoutes(params);

        // Store this as we need it to assert on the initial method calls
        const FirewallParams oldParams{params};
        // Now, let's change netScan (this should force SubnetByass to clear routes and recreate them)
        params.netScan.interfaceName("eth1");
        bypass.updateRoutes(params);

        QVERIFY(recorder->methodCalls().size() == 3);
        // Creation of the original route
        verifyMethodCalledInOrder(0, "addRoute", "192.168.1.0/24", recorder->methodCalls(), oldParams);
        qInfo() << recorder->methodCalls()[0].toString();

        // After a changed netScan the current route should be deleted and then recreated using the
        // Network information.
        verifyMethodCalledInOrder(1, "removeRoute", "192.168.1.0/24", recorder->methodCalls(), oldParams);
        verifyMethodCalledInOrder(2, "addRoute", "192.168.1.0/24", recorder->methodCalls(), params);
    }
 };

QTEST_GUILESS_MAIN(tst_subnetbypass)
#include TEST_MOC
