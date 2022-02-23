// Copyright (c) 2022 Private Internet Access, Inc.
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
#line HEADER_FILE("win/win_appmonitor.h")

#ifndef WIN_APPMONITOR_H
#define WIN_APPMONITOR_H

#include "settings/splittunnel.h"
#include "win_firewall.h"
#include "win/win_com.h"
#include <QWinEventNotifier>
#include <WbemIdl.h>
#include <set>
#include <unordered_set>
#include <unordered_map>

struct PtrValueLess
{
    template<class Ptr_t>
    bool operator()(const Ptr_t &first, const Ptr_t &second) const
    {
        return *first < *second;
    }
};

// WinAppTracker is part of the implementation of WinAppMonitor.  It keeps track
// of the current set of excluded apps, and it is notified when processes are
// created/destroyed.
//
// This mainly manages container logistics for WinAppMonitor.  We need to be
// able to find processes by either exclued app ID or PID, like a bimap.  This
// is constructed here using a set and a map.
//
// We don't keep track of a full "process tree" - we just keep track of the
// descendants of a specified app ID as a group.  The consequence of this is
// that adding/removing a new app that has been launched as a child of another
// app won't necessarily be able to move around existing descendants of the new
// app.  We have a blanket warning that "apps may need to be restarted" when
// changing app settings though, so this is acceptable.
class WinAppTracker : public QObject
{
    Q_OBJECT

public:
    using Pid_t = DWORD;

    enum class SplitType
    {
        Excluded,
        VpnOnly
    };
    Q_ENUM(SplitType);

private:
    // This container is used to hold the current "exclude" app IDs, along with
    // all PIDs currently associated with those app IDs, and the signing subject
    // names found in the excluded app.
    //
    // This is an ordered map so iterators to unmodified elements remain valid
    // through erase/emplace (we keep iterators in _procData).
    struct ExcludedAppData
    {
        // Executable path for this app; link target for a link or the same as
        // the rule path if it's a manually browsed executable.
        std::wstring _targetPath;
        std::set<std::wstring> _signerNames;
        std::unordered_set<Pid_t> _runningProcesses;
    };
    using ExcludedApps_t = std::map<AppIdKey, ExcludedAppData>;
    struct ProcessData
    {
        // Handle to the process - ensures that the PID isn't reused until we
        // clean up our data following process exit.
        WinHandle _procHandle;
        // Notifier to signal us when the process exits
        std::unique_ptr<QWinEventNotifier> _pNotifier;
        // The process's app ID, if one could be found
        AppIdKey _procAppId;
        // The process's location in _excludedApps - used to remove this
        // process and to find the excluded app ID associated with this process.
        ExcludedApps_t::iterator _excludedAppPos;
    };

    // This map contains data about the excluded processes, and also provides a
    // way to look up processes by PID alone.
    using ProcDataMap = std::unordered_map<Pid_t, ProcessData>;

public:
    explicit WinAppTracker(SplitType type);

public:
    // Get all the current known app IDs for this rule type.
    std::set<const AppIdKey *, PtrValueLess> getAppIds() const;

    // Set the current split tunnel rules.  Returns true if we need to monitor
    // for process creation (returns false if there is no way processCreated()
    // would ever match a process, which allows WinAppMonitor to shut down the
    // monitor when not needed).
    bool setSplitTunnelRules(const QVector<SplitTunnelRule> &rules);

    // Check if a newly created process matches one of our app rules (directly,
    // not as a descendant).  If it does, this takes the process handle and app
    // ID - there's no need to check any other trackers in that case.
    void checkMatchingProcess(WinHandle &procHandle, AppIdKey &appId, Pid_t pid);

    // Check if a newly created process is a descendant of one of the other
    // processes we are tracking.  Like checkMatchingProcess(), this takes the
    // process handle and app ID if it matches.
    void checkMatchingChild(WinHandle &procHandle, AppIdKey &appId,
                            const QString &imgPath, Pid_t pid, Pid_t parentPid);

    // Check if a newly created process's parent matches one of our rules, and
    // if so, check if the child should now be tracked as a descendant.
    // - If the parent matches, this takes the parent handle and app ID.
    // - If the child also matches, this takes the child process handle and app
    //   ID
    // In either case, there's no need to continue checking other trackers if
    // any handle was taken, since the parent matched this tracker.
    void checkNewParent(WinHandle &procHandle, AppIdKey &appId,
                        const QString &imgPath, Pid_t pid,
                        WinHandle &parentHandle, AppIdKey &parentAppId,
                        Pid_t parentPid);

    // Dump the excluded app data to debug logs
    void dump() const;

private:
    void addSplitProcess(ExcludedApps_t::iterator itMatchingApp,
                         WinHandle procHandle, Pid_t pid, AppIdKey appId);

    // Check if a new process is a descendant of an excluded app - provide the
    // parent process ID and the child process's image file path.
    //
    // Returns the position in _apps if it is, or the end() iterator if it
    // isn't.
    ExcludedApps_t::iterator isAppDescendant(Pid_t parentPid, const QString &imgPath);
    // A process has exited - connected to the QWinEventNotifiers.
    void onProcessExited(HANDLE procHandle);

signals:
    // The set of app IDs has changed.
    void appIdsChanged();

private:
    SplitType _type;
    // These are the current app IDs for this rule type, along with all PIDs
    // currently associated with those app IDs.
    ExcludedApps_t _apps;
    std::unordered_map<Pid_t, ProcessData> _procData;
};

// WinSplitTunnelTracker managers WinAppTracker objects for each type of split
// tunnel rule.  It receives notifications from WinAppMonitor and propagates
// them to each tracker.
class WinSplitTunnelTracker : public QObject
{
    Q_OBJECT

public:
    using Pid_t = WinAppTracker::Pid_t;

public:
    WinSplitTunnelTracker();

private:
    QString getProcImagePath(const WinHandle &procHandle) const;
    // Get the app ID for a process (empty if it can't be retrieved).
    AppIdKey getProcAppId(const WinHandle &procHandle) const;

public:
    // Get all the current known excluded app IDs - both the app IDs from rules
    // and any that have been detected as descendants.
    // The returned set is used by WinDaemon to compare against its current set
    // of app rules.  The AppIdKeys are owned by WinAppMonitor and remain valid
    // only as long as it isn't mutated.
    std::set<const AppIdKey*, PtrValueLess> getExcludedAppIds() const;
    std::set<const AppIdKey*, PtrValueLess> getVpnOnlyAppIds() const;

    // Set the current split tunnel rules.  Returns true if we need to monitor
    // for process creation (if any rule type returned true).
    bool setSplitTunnelRules(const QVector<SplitTunnelRule> &rules);

    // A process has been created.
    void processCreated(WinHandle procHandle, Pid_t pid,
                        WinHandle parentHandle, Pid_t parentPid);

    void dump() const;

signals:
    // The set of app IDs has changed.
    void appIdsChanged();

private:
    // It is possible (though unlikely) that we could end up identifying the
    // same executable path as both a VpnOnly app and an Excluded app (such as
    // if the shortcut and exe path both appeared as rules with different
    // types).  The VpnOnly tracker has the first chance for any given process
    // to ensure that type takes precedence.
    WinAppTracker _vpnOnly;
    WinAppTracker _excluded;
};

// WinAppMonitor monitors for child app IDs of app IDs that are excluded from
// the VPN using split tunnel.  The child app IDs identified are then also
// excluded from the VPN.
//
// This is necessary because a fair number of apps on Windows (especially
// Electron apps) invoke a "launcher" or "updater" from the Start Menu shortcut,
// which in turn invokes the actual application.  There's no reliable way to
// determine what the "actual" application is going to be ahead of time, instead
// we watch what the app does at runtime.
class WinAppMonitor : public QObject
{
    Q_OBJECT

private:
    class WbemEventSink;

public:
    WinAppMonitor();
    ~WinAppMonitor();

private:
    void activate();
    void deactivate();

public:
    // Get the current excluded app IDs; see WinAppTracker.
    std::set<const AppIdKey*, PtrValueLess> getExcludedAppIds() const {return _tracker.getExcludedAppIds();}
    std::set<const AppIdKey*, PtrValueLess> getVpnOnlyAppIds() const {return _tracker.getVpnOnlyAppIds();}

    // Set the current split tunnel rules
    void setSplitTunnelRules(const QVector<SplitTunnelRule> &rules);

    // Dump the diagnostics debug logs
    void dump() const;

signals:
    // The current set of excluded app IDs has changed.  (Get the current list
    // with getExcludedAppIds().)
    void appIdsChanged();

private:
    WinSplitTunnelTracker _tracker;
    // IWbemServices is loaded at startup, this is always valid if we were able
    // to connect to WMI.
    WinComPtr<IWbemServices> _pSvcs;
    // The sink and stub are created only when notifications are active.
    WinComPtr<WbemEventSink> _pSink;
    WinComPtr<IWbemObjectSink> _pSinkStubSink;
};

#endif
