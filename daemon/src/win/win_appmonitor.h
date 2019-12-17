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
#line HEADER_FILE("win/win_appmonitor.h")

#ifndef WIN_APPMONITOR_H
#define WIN_APPMONITOR_H

#include "settings.h"
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

public:
    // Get all the current known excluded app IDs - both the app IDs from rules
    // and any that have been detected as descendants.
    // The returned set is used by WinDaemon to compare against its current set
    // of app rules.  The AppIdKeys are owned by WinAppMonitor and remain valid
    // only as long as it isn't mutated.
    std::set<const AppIdKey*, PtrValueLess> getAppIds() const;

    // Set the current split tunnel rules.  Returns true if we need to monitor
    // for process creation (returns false if there is no way processCreated()
    // would ever match a process, which allows WinAppMonitor to shut down the
    // monitor when not needed).
    bool setSplitTunnelRules(const QVector<SplitTunnelRule> &rules);
    // A process has been created.
    void processCreated(WinHandle procHandle, Pid_t pid,
                        WinHandle parentHandle, Pid_t parentPid);

    // Dump the excluded app data to debug logs
    void dump() const;

private:
    void addExcludedProcess(ExcludedApps_t::iterator itExcludedApp,
                            WinHandle procHandle, Pid_t pid, AppIdKey appId);

    QString getProcImagePath(const WinHandle &procHandle) const;
    // Get the app ID for a process (empty if it can't be retrieved).
    AppIdKey getProcAppId(const WinHandle &procHandle) const;
    // Check if a new process is a descendant of an excluded app - provide the
    // parent process ID and the child process's image file path.
    //
    // Returns the position in _excludedApps if it is, or the end() iterator if
    // it isn't.
    ExcludedApps_t::iterator isAppDescendant(Pid_t parentPid, const QString &imgPath);
    // A process has exited - connected to the QWinEventNotifiers.
    void onProcessExited(HANDLE procHandle);

signals:
    // The set of excluded app IDs has changed.
    void excludedAppIdsChanged();

private:
    // These are the current "exclude" app IDs, along with all PIDs currently
    // associated with those app IDs.
    ExcludedApps_t _excludedApps;
    // This map contains data about the excluded processes, and also provides a
    // way to look up processes by PID alone.
    std::unordered_map<Pid_t, ProcessData> _procData;
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
    std::set<const AppIdKey*, PtrValueLess> getAppIds() const {return _tracker.getAppIds();}

    // Set the current split tunnel rules
    void setSplitTunnelRules(const QVector<SplitTunnelRule> &rules);

    // Dump the diagnostics debug logs
    void dump() const;

signals:
    // The current set of excluded app IDs has changed.  (Get the current list
    // with getExcludedAppIds().)
    void excludedAppIdsChanged();

private:
    WinAppTracker _tracker;
    // IWbemServices is loaded at startup, this is always valid if we were able
    // to connect to WMI.
    WinComPtr<IWbemServices> _pSvcs;
    // The sink and stub are created only when notifications are active.
    WinComPtr<WbemEventSink> _pSink;
    WinComPtr<IWbemObjectSink> _pSinkStubSink;
};

#endif
