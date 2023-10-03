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
#include <QtTest>

#include <kapps_net/src/routemanager.h>
#include <kapps_net/src/subnetbypass.h>
#include "daemon/src/daemon.h"

namespace
{
    using SubnetBypass = kapps::net::SubnetBypass;
    using FirewallParams = kapps::net::FirewallParams;
}

// Helper class to record method call information
struct MethodCall
{
    std::string methodName;
    std::string subnetArg;
    std::string gatewayIpArg;
    std::string interfaceNameArg;

    bool operator==(const MethodCall &other) const
    {
        return methodName == other.methodName &&
            subnetArg == other.subnetArg &&
            gatewayIpArg == other.gatewayIpArg &&
            interfaceNameArg == other.interfaceNameArg;
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

    void reset() {_methodCalls.clear();}

    const std::vector<MethodCall>&  methodCalls() const {return _methodCalls;}

private:
   std::vector<MethodCall> _methodCalls;
};


// Our RouteManager stub we use to verify SubnetBypass behavior
class StubRouteManager : public RouteManager
{

// This is the public interface for RouteManager
public:
    virtual void addRoute4(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName, uint32_t metric=0) const override
    {
        _methodRecorder.record({"addRoute4",subnet, gatewayIp, interfaceName});
    }

    virtual void removeRoute4(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName) const override
    {
        _methodRecorder.record({"removeRoute4", subnet, gatewayIp, interfaceName});
    }

    virtual void addRoute6(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName, uint32_t metric=0) const override
    {
        _methodRecorder.record({"addRoute6",subnet, gatewayIp, interfaceName});
    }

    virtual void removeRoute6(const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName) const override
    {
        _methodRecorder.record({"removeRoute6", subnet, gatewayIp, interfaceName});
    }

// Public interface to expose our test helper
public:
    MethodRecorder &methodRecorder() {return _methodRecorder;}
private:
    mutable MethodRecorder _methodRecorder;
};

namespace
{
    OriginalNetworkScan validNetScan{"192.168.1.1", "eth0", "192.168.1.43", 24, 1500,
                                     "2001:db8::123", "2001::1", 1500};
}

FirewallParams validFirewallParams()
{
    FirewallParams params{};
    params.enableSplitTunnel = true;
    params.hasConnected = true;
    params.bypassDefaultApps = false;
    params.netScan = validNetScan;
    params.bypassIpv4Subnets =  std::set<std::string>{"192.168.0.0/16"};

    return params;
}

static auto createRouteManager(MethodRecorder* &outPtr) -> std::unique_ptr<StubRouteManager>
{
    auto routeManager{std::make_unique<StubRouteManager>()};

    // Save a pointer to it so we can verify its behaviour
    outPtr = &routeManager->methodRecorder();

    return routeManager;
}

// Verify the method was called (when it was called is not important - consequence of using a QSet to store subnets)
static void verifyMethodCall(const std::string &methodName, const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName, const std::vector<MethodCall> &methodCalls)
{
    MethodCall methodCall{methodName, subnet, gatewayIp, interfaceName};

    // Note we have to use std::find here and search over all methodCalls
    // because the subnets are stored as a QSet which does not have a defined order.
    // As a result we can't expect the first subnet in the set to be in the first method call, and so on
    auto result = std::find(methodCalls.begin(), methodCalls.end(), methodCall);

    QVERIFY(result != methodCalls.end());
}

// Verify the method was called at a certain point, i.e was it the first method invoked?
static void verifyMethodCalledInOrder(uint order, const std::string &methodName, const std::string &subnet, const std::string &gatewayIp, const std::string &interfaceName, const std::vector<MethodCall> &methodCalls)
{
    MethodCall methodCall{methodName, subnet, gatewayIp, interfaceName};
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
            params.bypassIpv6Subnets =  std::set<std::string>{"2001:beef:cafe::1/128"};
            SubnetBypass bypass{createRouteManager(recorder)};
            bypass.updateRoutes(params);

            QVERIFY(recorder->methodCalls().size() == 2);
            verifyMethodCall("addRoute4", "192.168.0.0/16", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
            verifyMethodCall("addRoute6", "2001:beef:cafe::1/128", params.netScan.gatewayIp6(), params.netScan.interfaceName(), recorder->methodCalls());
        }

        // Disabled when params.enableSplitTunnel==false
        {
            FirewallParams params{validFirewallParams()};
            params.enableSplitTunnel = false;
            SubnetBypass bypass{createRouteManager(recorder)};
            bypass.updateRoutes(params);

            QVERIFY(recorder->methodCalls().size() == 0);
        }

#ifndef Q_OS_MACOS
        // Disabled when params.bypassDefaultApps==true
        {
            FirewallParams params{validFirewallParams()};
            params.bypassDefaultApps = true;
            SubnetBypass bypass{createRouteManager(recorder)};
            bypass.updateRoutes(params);

            QVERIFY(recorder->methodCalls().size() == 0);
        }
#endif

#ifndef Q_OS_MACOS
        // Disabled when params.hasConnected==false
        {
            FirewallParams params{validFirewallParams()};
            params.hasConnected = false;
            SubnetBypass bypass{createRouteManager(recorder)};
            bypass.updateRoutes(params);

            QVERIFY(recorder->methodCalls().size() == 0);
        }
#endif

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
        params.bypassIpv4Subnets = std::set<std::string> { "192.168.0.0/16" };

        SubnetBypass bypass{createRouteManager(recorder)};
        bypass.updateRoutes(params);

        QVERIFY(recorder->methodCalls().size() == 1);
        verifyMethodCall("addRoute4", "192.168.0.0/16", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());

        // No changes to params
        bypass.updateRoutes(params);

        // So no new methods are invoked (number of method calls remains the same)
        QVERIFY(recorder->methodCalls().size() == 1);
    }

    void testMultipleSubnets()
    {
        {
            MethodRecorder *recorder{nullptr};

            FirewallParams params{validFirewallParams()};
            params.bypassIpv4Subnets = std::set<std::string> { "192.168.1.0/24", "10.0.0.0/8", "1.1.1.1/32" };
            SubnetBypass bypass{createRouteManager(recorder)};
            bypass.updateRoutes(params);

            QVERIFY(recorder->methodCalls().size() == 3);
            verifyMethodCall("addRoute4", "192.168.1.0/24", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
            verifyMethodCall("addRoute4", "10.0.0.0/8", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
            verifyMethodCall("addRoute4", "1.1.1.1/32", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
        }

        {
            MethodRecorder *recorder{nullptr};

            FirewallParams params{validFirewallParams()};
            params.bypassIpv4Subnets = std::set<std::string> { "192.168.1.0/24", "10.0.0.0/8", "1.1.1.1/32" };
            params.bypassIpv6Subnets = std::set<std::string> { "2001:cafe::1/128", "2001:cafe::2/128" };
            SubnetBypass bypass{createRouteManager(recorder)};
            bypass.updateRoutes(params);

            QVERIFY(recorder->methodCalls().size() == 5);

            // Ipv4
            verifyMethodCall("addRoute4", "192.168.1.0/24", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
            verifyMethodCall("addRoute4", "10.0.0.0/8", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
            verifyMethodCall("addRoute4", "1.1.1.1/32", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());

            // Ipv6
            verifyMethodCall("addRoute6", "2001:cafe::1/128", params.netScan.gatewayIp6(), params.netScan.interfaceName(), recorder->methodCalls());
            verifyMethodCall("addRoute6", "2001:cafe::2/128", params.netScan.gatewayIp6(), params.netScan.interfaceName(), recorder->methodCalls());
        }

    }

    void testChangingSubnets()
    {
        // Ipv4
        {
            MethodRecorder *recorder{nullptr};

            FirewallParams params{validFirewallParams()};
            params.bypassIpv4Subnets = std::set<std::string>{"192.168.1.0/24"};

            SubnetBypass bypass{createRouteManager(recorder)};
            bypass.updateRoutes(params);

            QVERIFY(recorder->methodCalls().size() == 1);
            verifyMethodCall("addRoute4", "192.168.1.0/24", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());

            // Now let's change the subnet
            params.bypassIpv4Subnets = std::set<std::string>{"10.0.0.0/8"};

            // Reset recorder first for convenience
            // This just erases previously stored method calls
            recorder->reset();

            bypass.updateRoutes(params);
            QVERIFY(recorder->methodCalls().size() == 2);
            verifyMethodCalledInOrder(0, "removeRoute4", "192.168.1.0/24", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
            verifyMethodCalledInOrder(1, "addRoute4", "10.0.0.0/8", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
        }

        // Ipv6
        {
            MethodRecorder *recorder{nullptr};

            FirewallParams params{validFirewallParams()};
            params.bypassIpv4Subnets = {};
            params.bypassIpv6Subnets = std::set<std::string>{"2001:cafe::1/128"};

            SubnetBypass bypass{createRouteManager(recorder)};
            bypass.updateRoutes(params);

            QVERIFY(recorder->methodCalls().size() == 1);
            verifyMethodCall("addRoute6", "2001:cafe::1/128", params.netScan.gatewayIp6(), params.netScan.interfaceName(), recorder->methodCalls());

            // Now let's change the subnet
            params.bypassIpv6Subnets = std::set<std::string>{"2001:beef::1/128"};

            // Reset recorder first for convenience
            // This just erases previously stored method calls
            recorder->reset();

            bypass.updateRoutes(params);
            QVERIFY(recorder->methodCalls().size() == 2);
            verifyMethodCalledInOrder(0, "removeRoute6", "2001:cafe::1/128", params.netScan.gatewayIp6(), params.netScan.interfaceName(), recorder->methodCalls());
            verifyMethodCalledInOrder(1, "addRoute6", "2001:beef::1/128", params.netScan.gatewayIp6(), params.netScan.interfaceName(), recorder->methodCalls());
        }
    }

    void testDisabling()
    {
        MethodRecorder *recorder{nullptr};

        FirewallParams params{validFirewallParams()};
        params.bypassIpv4Subnets = std::set<std::string> { "192.168.1.0/24"};

        SubnetBypass bypass{createRouteManager(recorder)};

        // Subnet routes should be created
        bypass.updateRoutes(params);

        // This will disable the subnet bypass on next call to updateRoutes
        params.enableSplitTunnel = false;
        bypass.updateRoutes(params);

        QVERIFY(recorder->methodCalls().size() == 2);
        verifyMethodCalledInOrder(0, "addRoute4", "192.168.1.0/24", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
        verifyMethodCalledInOrder(1, "removeRoute4", "192.168.1.0/24", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
    }

    void testDisableMultiple()
    {
        MethodRecorder *recorder{nullptr};

        FirewallParams params{validFirewallParams()};
        params.bypassIpv4Subnets = std::set<std::string> { "192.168.1.0/24", "10.0.0.0/8" };
        params.bypassIpv6Subnets = std::set<std::string> { "2001:cafe::1/128", "2001:cafe::2/128" };

        SubnetBypass bypass{createRouteManager(recorder)};
        bypass.updateRoutes(params);

        // Setting this to false will disable SubnetBypass
        params.enableSplitTunnel = false;
        bypass.updateRoutes(params);

        QVERIFY(recorder->methodCalls().size() == 8);

        verifyMethodCall("addRoute4", "192.168.1.0/24", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
        verifyMethodCall("addRoute4", "10.0.0.0/8", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
        verifyMethodCall("removeRoute4", "192.168.1.0/24", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
        verifyMethodCall("removeRoute4", "10.0.0.0/8", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());

        verifyMethodCall("addRoute6", "2001:cafe::1/128", params.netScan.gatewayIp6(), params.netScan.interfaceName(), recorder->methodCalls());
        verifyMethodCall("addRoute6", "2001:cafe::2/128", params.netScan.gatewayIp6(), params.netScan.interfaceName(), recorder->methodCalls());
        verifyMethodCall("removeRoute6", "2001:cafe::1/128", params.netScan.gatewayIp6(), params.netScan.interfaceName(), recorder->methodCalls());
        verifyMethodCall("removeRoute6", "2001:cafe::2/128", params.netScan.gatewayIp6(), params.netScan.interfaceName(), recorder->methodCalls());
    }

    void testChangedNetScan()
    {
        MethodRecorder *recorder{nullptr};

        FirewallParams params{validFirewallParams()};
        params.bypassIpv4Subnets = std::set<std::string> { "192.168.1.0/24" };

        SubnetBypass bypass{createRouteManager(recorder)};

        // First let's create routes with the current netScan
        bypass.updateRoutes(params);

        // Store this as we need it to assert on the initial method calls
        const FirewallParams oldParams{params};
        // Now, let's change netScan (this should force SubnetBypass to clear routes and recreate them)
        params.netScan.interfaceName("eth1");
        bypass.updateRoutes(params);

        QVERIFY(recorder->methodCalls().size() == 3);
        // Creation of the original route
        verifyMethodCalledInOrder(0, "addRoute4", "192.168.1.0/24", oldParams.netScan.gatewayIp(), oldParams.netScan.interfaceName(), recorder->methodCalls());

        // After a changed netScan the current route should be deleted and then recreated using the
        // Network information.
        verifyMethodCalledInOrder(1, "removeRoute4", "192.168.1.0/24", oldParams.netScan.gatewayIp(), oldParams.netScan.interfaceName(), recorder->methodCalls());
        verifyMethodCalledInOrder(2, "addRoute4", "192.168.1.0/24", params.netScan.gatewayIp(), params.netScan.interfaceName(), recorder->methodCalls());
    }
 };

QTEST_GUILESS_MAIN(tst_subnetbypass)
#include TEST_MOC
