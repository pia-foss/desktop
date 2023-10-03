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


#pragma once
#include <kapps_core/src/util.h>
#include <kapps_core/src/stringslice.h>
#include <kapps_net/net.h>
#include "../firewall.h" // For FirewallConfig
#include <unordered_map>
#include <string>

namespace kapps { namespace net {

// PFFirewall is the lowest-level wrapper over the BSD PF firewall as
// implemented in macOS.  It provides the primitive operations we need to
// enable/disable anchors, set the content of an anchor, etc.
//
// This is essentially a driver for PF and has no mutable state of its own.
// Thread-safety is required as this is used by both the main firewall logic and
// the macOS split tunnel implementation (from its own worker thread).
class KAPPS_NET_EXPORT PFFirewall
{
private:
    using MacroPairs = std::unordered_map<std::string, std::string>;
private:
    int execute(const std::string &command, bool ignoreErrors = false);
    bool isPFEnabled();
    void installRootAnchors();
    // Test if a specific type of root anchor is loaded and non-empty
    bool isRootAnchorLoaded(const std::string &modifier);
    bool areAllRootAnchorsLoaded();
    bool areAnyRootAnchorsLoaded();
    std::string getMacroArgs(const MacroPairs& macroPairs);

public:
    PFFirewall(const FirewallConfig &config)
    : _rootAnchor{config.brandInfo.identifier}
    , _config{config}
    {
        assert(!_rootAnchor.empty());
    }

public:
    void install();
    void uninstall();
    bool isInstalled();
    void enableAnchor(const kapps::core::StringSlice &anchor,
                      const kapps::core::StringSlice &modifier,
                      const MacroPairs &macroPairs);
    void disableAnchor(const kapps::core::StringSlice &anchor,
                       const kapps::core::StringSlice &modifier);
    void setAnchorEnabled(const kapps::core::StringSlice &anchor,
                          const kapps::core::StringSlice &modifier,
                          bool enable, const MacroPairs &macroPairs);
    void setAnchorTable(const kapps::core::StringSlice &anchor, bool enabled,
                        const kapps::core::StringSlice &table,
                        const std::vector<std::string> &items);

    // Manipulate anchors containing filter rules
    void setFilterEnabled(const kapps::core::StringSlice &anchor, bool enable,
                          const MacroPairs &macroPairs={});
    void setFilterWithRules(const kapps::core::StringSlice &anchor, bool enabled,
                            const std::vector<std::string> &rules);

    // Manipulate anchors containing translation rules
    void setTranslationEnabled(const kapps::core::StringSlice &anchor, bool enable,
                               const MacroPairs &macroPairs={});

    void ensureRootAnchorPriority();

    // Flush firewall state
    void flushState();

private:
    // There is no mutable state, which allows MacSplitTunnel to safely use the
    // same PFFirewall as MacFirewall from its worker thread.
    const std::string _rootAnchor;
    // Contains config information such as daemonDataDir, etc
    const FirewallConfig _config;
};

}}
