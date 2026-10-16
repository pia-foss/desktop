// Copyright (c) 2026 Private Internet Access, Inc.
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

#pragma once
#include "../firewallparams.h"
#include "../originalnetworkscan.h"
#include <kapps_net/net.h>
#include <kapps_core/core.h>
#include <kapps_core/src/coresignal.h>
#include <kapps_core/src/util.h>
#include <kapps_net/src/firewall.h>
#include <string.h>
#include <atomic>
#include <mutex>
#include <chrono>
#include <kapps_core/src/posix/pollthread.h>

namespace kapps::net {
class KAPPS_NET_EXPORT TransparentProxy
{
    // Config required by the proxy
    struct KAPPS_NET_EXPORT Config : core::OStreamInsertable<Config>
    {
        Config() = default;
        Config(const net::FirewallParams& params);
        bool operator==(const Config &rhs) const;
        bool operator!=(const Config &rhs) const { return !(operator==(rhs)); }
        void trace(std::ostream &os) const;

        std::vector<std::string> bypassApps;
        std::vector<std::string> vpnOnlyApps;
        std::string bindInterface;
        bool isConnected{false};
        bool vpnHasDefaultRoute{false};

        // This field isn't used by the transparent proxy, it's
        // only used by us to create a bound route to the tunnel.
        // The reason we have this field at all is sometimes the tunnel
        // ip we're given is stale (i.e when doing a 'reconnect to apply settings')
        // it will sometimes have the previous value of the tunnel ip rather than
        // the newest tunnel ip. So we track it so that when we get a new update() call
        // we know that the tunnel ip has changed to the updated one, so we can reset a bound route
        // for the correct ip (the previous bound route creation should fail as the ip will be invalid)
        std::string tunnelIp;

        bool transparentProxyLogEnabled;
    };

    // Proxy state.
    // Consider the concept of sync as "making the split tunnel extension
    // match the requested config", which can require the extension to start
    // or to update its internal config.
    enum class State: uint8_t
    {
        Synchronized,
        Synchronizing,
        Stopping,
        Stopped,
        Updating
    };

public:
    TransparentProxy(const net::FirewallParams &params, const net::FirewallConfig &firewallConfig);
    ~TransparentProxy();

public:
    // Send an update request to the system extension with new params.
    void update(const net::FirewallParams &params);

private:
    void sync(const Config &config);
    void attemptSync(const std::vector<std::string> &syncArgs);
    void stop();
    void activateSystemExtension() const;
    // Wait until the system extension is activated and allowed by the user.
    // It will try to activate the extension if it was deactivated somehow,
    // but the most likely use case is waiting for the user to manually allow.
    // Returns true if the extension is activated when it finishes.
    bool ensureActivateSystemExtension();
    // Wait until the proxy is allowed, this is the second (and final) step of the activation
    // process.
    bool ensureAllowedProxy(const std::vector<std::string> &syncArgs);
    int proxyCliRun(const std::vector<std::string> &parameters,
                    std::chrono::milliseconds timeout = std::chrono::milliseconds{-1}) const;
    void setState(State newState);
    State getState() const;
    std::string checkSystemExtensionStatus() const;
    std::string checkProxyConnectivity() const;
    void updateVpnBoundRouteIfNecessary();

public:
    kapps::core::Signal<bool> activationFailed;

private:
    Config _config;
    std::string _transparentProxyCliExecutable;
    std::string _transparentProxyLogFile;
    State _state{State::Stopped};
    mutable std::mutex _mutex;
    core::nullable_t<core::PollThread> _pSyncThread;
    bool _vpnBoundRouteExists;
    // Read and written only by the current sync thread, but that is a
    // succession of threads across sync() calls - atomic rather than relying on
    // emplace() joining the predecessor to order the handover.
    std::atomic<bool> _sysExtIsActivated{false};
    // True from the moment a sync is queued until both of its approval waits
    // (system extension, then proxy configuration) finish.  Set by sync() on
    // the caller's thread and cleared on the sync thread; sync() uses it to
    // skip a sync that would otherwise join - and block on - those waits.
    std::atomic<bool> _waitingForUserApproval{false};
};
}
