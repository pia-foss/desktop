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

#include "transparent_proxy.h"
#include <kapps_core/src/logger.h>
#include <kapps_core/src/newexec.h>

namespace kapps::net {

namespace
{
    // The number of seconds  to try to sync the proxy before giving up
    const std::chrono::seconds syncProxyTimeout{60};
    // The number of seconds  to try to allow the system extension before giving up
    // This may take longer than the proxy if the user doesn't know where the
    // allow button is.
    const std::chrono::seconds systemExtensionAllowTimeout{60*10};

    // The wait interval between each attempt
    const std::chrono::seconds syncProxyAttemptInterval{1};
    const std::chrono::seconds systemExtensionAllowInterval{1};

    // Minimal wrapper class to enforce type and arg-order safety
    struct AppList
    {
        const std::vector<std::string> &apps;
    };

    // Given an app path (e.g /Applications/Firefox.app) return
    // the bundle id (e.g org.mozilla.firefox) or the original path
    // if the bundle id cannot be found.
    auto getAppDescriptor(const std::string &path) -> std::string
    {
        std::string bundleId{core::Exec::cmdWithOutput("mdls", {"-raw", "-name", "kMDItemCFBundleIdentifier", path})};

       // First try the bundle id,  failing that, fall back to the app path.
        // App paths can also be used as descriptors, but they must be a
        // complete path to the executable binary, not just to the .app
        // A "descriptor" is how the split tunnel identifies an app - either an
        // app id or a complete path to a binary.
        return (bundleId.empty() || bundleId == "(null)") ? path : bundleId;
    }

    // Add apps to an arg list - we use an AppList as a sort of
    // named parameter and to enforce type-safety and prevent mixup
    // of the arg orders (since two args are std::vector<std::string>)
    auto addAppsToArgs(std::string appOption, AppList appList,
        std::vector<std::string> &syncArgs) -> void
    {
        if(appList.apps.size() == 0)
            return;

        for(const auto &path: appList.apps)
        {
            syncArgs.emplace_back(appOption);
            syncArgs.emplace_back(getAppDescriptor(path));
        }
    }

    // The number of seconds passed since startTime
    auto secondsSince(const std::chrono::time_point<std::chrono::steady_clock>& startTime)
        -> int64_t
    {
        auto endTime = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    }

    std::string vectorToString(const std::vector<std::string> &vec)
    {
        std::string result{"["};

        for(size_t i = 0; i < vec.size(); ++i)
        {
            result += vec[i];
            if(i < vec.size() - 1)
            {
                result += ", ";
            }
        }

        result += "]";
        return result;
    }

}

TransparentProxy::Config::Config(const net::FirewallParams& params)
: bypassApps{params.excludeApps}
, vpnOnlyApps{params.vpnOnlyApps}
, bindInterface{}
// We use params.isConnected rather than params.hasConnected so that
// we detect when the connection goes down during a "reconnect to apply settings"
// otherwise we won't know that a new bound route needs to be created (as _vpnBoundRouteExists is
// only cleared on a disconnect)
, isConnected{params.isConnected}
, vpnHasDefaultRoute{!params.bypassDefaultApps}
, tunnelIp{params.tunnelDeviceLocalAddress}
, transparentProxyLogEnabled{params.transparentProxyLogEnabled}
{
    if(vpnHasDefaultRoute)
    {
        // Bind to the physical when VPN has default route
        bindInterface = params.netScan.interfaceName();
    }
    else
    {
        // Bind to the VPN when physical has the default route
        bindInterface = params.tunnelDeviceName;
    }

    // The bindInterface should only be empty when
    // disconnected from the VPN
    if(bindInterface.empty())
    {
        // Sentinel value indicating the bindInterface isn't set
        // This should only happen when disconnected from the VPN *and*
        // vpnHasDefaultRoute is false (i.e in inverse tunnel mode)
        bindInterface = "!";
    }
}

bool TransparentProxy::Config::operator==(const Config &rhs) const
{
    return bypassApps == rhs.bypassApps &&
        vpnOnlyApps == rhs.vpnOnlyApps &&
        bindInterface == rhs.bindInterface &&
        isConnected == rhs.isConnected &&
        vpnHasDefaultRoute == rhs.vpnHasDefaultRoute &&
        tunnelIp == rhs.tunnelIp &&
        transparentProxyLogEnabled == rhs.transparentProxyLogEnabled;
}

void TransparentProxy::Config::trace(std::ostream &os) const
{
    os << "Config{"
        << "isConnected: " << (isConnected ? "true" : "false") << ","
        << " vpnHasDefaultRoute: " << (vpnHasDefaultRoute ? "true" : "false") << ","
        << " bindInterface: " << bindInterface << ","
        << " bypassApps: "
        << vectorToString(bypassApps) << ","
        << " vpnOnlyApps: "
        << vectorToString(vpnOnlyApps) << ","
        << " tunnelIp: " << tunnelIp
        << "}";
}

// The first time this runs on a user's system it will pop up a window
// and require the user to go to settings -> privacy -> security -> and click allow.
void TransparentProxy::activateSystemExtension() const
{
    KAPPS_CORE_INFO() << "Activating the system extension";
    proxyCliRun({"sysext", "activate"});
}

bool TransparentProxy::ensureActivateSystemExtension()
{
    // Sysext is already activated - nothing to do.
    // Note: we do not deal with a case where the user deactivates the system
    // extension manually or through other out-of-band means. The assumption
    // we make is that once it's active, it will remain active for the life-time
    // of the daemon.
    if(_sysExtIsActivated)
      return true;

    std::string sysextState = checkSystemExtensionStatus();
    // If already activated, don't do anything. This is a very likely code path.
    if(sysextState == "bundled installed")
    {
        _sysExtIsActivated = true;
        return true;
    }

    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + systemExtensionAllowTimeout;

    // Keep track of our attempts
    size_t attemptCount{0};

    while(true)
    {
        if(std::chrono::steady_clock::now() >= endTime)
        {
            KAPPS_CORE_INFO() << "Timeout reached. System extension not activated!"
                << "Took" << secondsSince(startTime) << "seconds";
            return false;
        }

        if(sysextState == "bundled installed")
        {
            KAPPS_CORE_INFO() << "System extension activated after" << attemptCount
                << "attempts!" << "Took"  << secondsSince(startTime) << "seconds";

            _sysExtIsActivated = true;

            return true;
        }
        else if(sysextState == "bundled waiting_for_user")
        {
            KAPPS_CORE_INFO() << "Waiting for user to allow the system extension. Attempt" << attemptCount;
            // Nothing to do, just keep waiting for the user to allow the extension.
        }
        else
        {
            KAPPS_CORE_INFO() << "Activation requested again. Attempt" << attemptCount;
            // Request activation.
            // This should be a very rare case given we activate when we first enable split tunnel.
            activateSystemExtension();
        }
        ++attemptCount;

        // Pause between each successive attempt (so we can read state changes and possibly terminate the loop)
        std::this_thread::sleep_for(systemExtensionAllowInterval);
        sysextState = checkSystemExtensionStatus();
    }
}

TransparentProxy::TransparentProxy(const kapps::net::FirewallParams &params, const kapps::net::FirewallConfig &firewallConfig)
: _config{params}
, _transparentProxyCliExecutable{firewallConfig.transparentProxyCliExecutable}
, _transparentProxyLogFile{firewallConfig.transparentProxyLogFile}
, _state{State::Stopped}
, _vpnBoundRouteExists{false}
, _sysExtIsActivated{false}
{
    // Activate the extension - if the extension is already
    // activated this will just no-op. It will also update the extension with
    // no additional prompts if the bundled one is newer than the current one.
    activateSystemExtension();

    // Sync the proxy
    sync(_config);
}

auto TransparentProxy::getState() const -> State
{
    return _state;
}

void TransparentProxy::setState(State newState)
{
    _state = newState;
}

// The first time this runs on a user's system it will pop up a window
// and require the user to allow the network manager.
void TransparentProxy::sync(const Config &config)
{
    {
        // Protect the calls to getState() and setState
        std::lock_guard<std::mutex> guard{_mutex};

        if(getState() == State::Synchronizing || getState() == State::Synchronized)
        {
            KAPPS_CORE_INFO() << "Trying to sync proxy - but already synchronized (or synchronizing)!";
            return;
        }

        setState(State::Synchronizing);
    }

    KAPPS_CORE_INFO() << "Sending sync request to transparent proxy extension";
    std::vector<std::string> syncArgs;

    // Reserve space for our argument list (+6 for the subcommand names)
    size_t syncArgsize = config.bypassApps.size() + config.vpnOnlyApps.size() + 6;
    syncArgs.reserve(syncArgsize);

    syncArgs.emplace_back("proxy");
    syncArgs.emplace_back("sync");

    if(config.transparentProxyLogEnabled)
    {
        // Set log location
        syncArgs.emplace_back("--sys-ext-log-file");
        syncArgs.emplace_back(_transparentProxyLogFile);

        // Set log level to debug
        syncArgs.emplace_back("--sys-ext-log-level");
        syncArgs.emplace_back("debug");
    }
    else
    {
        // If Debug Logs are disabled, we do not set proxy logs location.
        // This will likely save the logs in /tmp/STProxy.log.
        // The logs will not be included in what is uploaded to CSI

        // Still set the log level to 'debug' so we get extensive info
        // even if it's saved to a tmp location
        syncArgs.emplace_back("--sys-ext-log-level");
        syncArgs.emplace_back("debug");
    }

    // Add bypass apps
    addAppsToArgs("--bypass-app", AppList{config.bypassApps}, syncArgs);
    // Add vpnOnly apps
    addAppsToArgs("--vpn-only-app", AppList{config.vpnOnlyApps}, syncArgs);

    // Apply interface
    syncArgs.emplace_back("--bind-interface");
    syncArgs.emplace_back(config.bindInterface);

    if(config.isConnected)
        syncArgs.emplace_back("--is-connected");

    if(config.vpnHasDefaultRoute)
        syncArgs.emplace_back("--route-vpn");

    // The 'emplace' here will also call the destructor on
    // any pre-existing thread (causing it to join()).
    _pSyncThread.emplace([](core::Any){});

    // Kick off our attempts to sync the proxy in a background thread
    // it keeps trying to sync it until it succeeds or it times out.
    _pSyncThread->queueInvoke([this, syncArgs]
    {
        // Make sure the system extension is active before synchronizing
        if(ensureActivateSystemExtension())
        {
            attemptSync(syncArgs);
        }
    });
}

// Sometimes the proxy doesn't sync immediately - so we keep trying
// to sync it until either we connect or a timeout is reached
// this code should only be run in a background thread (see _pSyncThread).
void TransparentProxy::attemptSync(const std::vector<std::string> &syncArgs)
{
    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + syncProxyTimeout;

    // Keep track of our attempts
    size_t attemptCount{0};

    while(true)
    {
        if(std::chrono::steady_clock::now() >= endTime)
        {
            KAPPS_CORE_INFO() << "Timeout reached. Could not sync transparent proxy!"
                << "Took" << secondsSince(startTime) << "seconds";
            break;
        }

        ++attemptCount;

        {
            // Protect the calls to getState() and setState()
            std::lock_guard<std::mutex> guard{_mutex};

            // Are we still trying to sync the proxy?
            // It may have been moved to Stopped already by
            // a call to stop() - so check it again.
            if(getState() != State::Synchronizing)
            {
                KAPPS_CORE_WARNING() << "State is no longer Synchronizing, giving up trying to synchronize transparent proxy.";
                break;
            }

            // Trigger the attempt
            proxyCliRun(syncArgs);

            // Read off the proxy connectivity - are we connected yet?
            const std::string currentProxyConnectivity = checkProxyConnectivity();
            KAPPS_CORE_INFO() << "Trying to synchronize transparent proxy, current state is:" << currentProxyConnectivity;

            // This loop attempts to sync the proxy process. The proxy's connectivity state
            // and our desired state for the proxy are distinct and should not be confused.
            // The proxy's connectivity state can be 'disconnected', 'disconnecting', 'connecting',
            // or 'connected'. Our desired state for the proxy is 'stopped', 'synchronizing', or 'synchronized'.
            //
            // When the proxy's connectivity state is anything other than 'connected', we continue
            // trying to sync the proxy. It's common for the proxy's state to be 'disconnecting'
            // during this process, as it may still be in the midst of shutting down from a previous
            // state. However, our own state is 'synchronizing', as we are actively trying to initiate
            // the proxy.
            //
            // The key point here is that we only care about the proxy reaching our target state,
            // which in this context is 'connected'. Until the proxy achieves this 'connected' state,
            // we persistently attempt to sync it. Once the proxy is 'connected', we update our own
            // state to 'synchronized', indicating that the proxy is now running as intended.
            if(currentProxyConnectivity == "connected")
            {
                // We finally reached the connected state,
                // so transition to synchronized
                setState(State::Synchronized);
                KAPPS_CORE_INFO() << "Transparent proxy synchronized after" << attemptCount
                    << "attempts!" << "Took"  << secondsSince(startTime) << "seconds";
                break;
            }
        }

        // Pause between each successive attempt (so we can read state changes and possibly terminate the loop)
        // do this outside the lock - to allow stop() to possibly change the state to Stopped
        std::this_thread::sleep_for(syncProxyAttemptInterval);
    }
}

void TransparentProxy::stop()
{
    {
        // Protect the calls to getState and setState
        std::lock_guard<std::mutex> guard{_mutex};

        if(getState() == State::Stopped)
        {
            KAPPS_CORE_INFO() << "stop() was called but transparent proxy is already stopped, nothing to do.";
            return;
        }

        KAPPS_CORE_INFO() << "Sending stop request to transparent proxy extension";
        KAPPS_CORE_INFO() << "Shutting down sync thread (if it's running)";

        // Cleanup the sync thread - the thread will see it's now in the State::Stopped
        // state and it will exit the loop. The thread will only keeps trying to sync the proxy
        // when it's in the 'State::Synchronizing' state.
        setState(State::Stopped);
    }
    // This will block until the thread finishes and join()s.
    // Ensure .clear() is called outside the loop - otherwise it may
    // deadlock as the thread could attempt to acquire the mutex while we
    // already hold it.
    _pSyncThread.clear();
    proxyCliRun({"proxy", "stop"});
}

TransparentProxy::~TransparentProxy()
{
    stop();
}

// This creates a bound route for the VPN tunnel if the split tunnel is
// running in inverse-mode.
// This allows traffic binding to the VPN to get routed correctly.
// Without it, we'd get "no route to host" errors for vpnOnly traffic
void TransparentProxy::updateVpnBoundRouteIfNecessary()
{
    // When the VPN goes down, the bound route
    // will automatically get deleted (as there's no active vpn interface anymore)
    if(!_config.isConnected)
    {
        _vpnBoundRouteExists = false;
        return;
    }

    // Only create the bound route to the VPN interface if:
    // * The VPN is connected (otherwise there is no VPN interface to bind to!)
    // * The VPN does NOT have the default route (otherwise there's no need to bind to it, as it's the default anyway)
    if(_config.isConnected && !_config.vpnHasDefaultRoute)
    {
        // Only bind if the VPN tunnel is really truly up (i.e has an ip address)
        if(!_config.tunnelIp.empty() && !_vpnBoundRouteExists)
        {
            KAPPS_CORE_INFO() << "Creating bound route for VPN interface" << _config.bindInterface;
            // Create a 'bound route,' which is a route tied to a specific interface and
            // used when a packet's source address matches the interface's IP.
            if(0 == core::Exec::cmd("route", {"add", "-net", "0.0.0.0",  _config.tunnelIp, "-ifscope", _config.bindInterface}))
            {
                // Remember we created the route so we don't try to create it again, and only set it on success
                // so we can keep trying to create a route if it failed the last time
                _vpnBoundRouteExists = true;
            }
        }
    }
}

void TransparentProxy::update(const FirewallParams &params)
{
    Config newConfig{params};

    if(_config != newConfig)
    {
        _config = newConfig;

        // Create the VPN bound route (if we have to)
        updateVpnBoundRouteIfNecessary();

        KAPPS_CORE_INFO() << "Transparent proxy config updated: " << _config;
        {
            // Protect the calls to getState and setState
            std::lock_guard<std::mutex> guard{_mutex};
            setState(State::Updating);
        }
        KAPPS_CORE_INFO() << "Sending sync request to update the config";
        sync(_config);
    }
    else
    {
        KAPPS_CORE_INFO() << "Split tunnel config did not change - nothing to update";
    }
}

std::string TransparentProxy::checkSystemExtensionStatus() const
{
    return core::Exec::cmdWithOutput(_transparentProxyCliExecutable, {"sysext", "status"});
}

std::string TransparentProxy::checkProxyConnectivity() const
{
    return core::Exec::cmdWithOutput(_transparentProxyCliExecutable, {"proxy", "status"});
}

int TransparentProxy::proxyCliRun(const std::vector<std::string> &parameters) const
{
    std::string traceCmd = _transparentProxyCliExecutable;
    for(const std::string &arg: parameters) traceCmd += (" " + arg);
    KAPPS_CORE_INFO() << traceCmd;

    return core::Exec::cmd(_transparentProxyCliExecutable, parameters);
}
}
