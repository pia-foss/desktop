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

#include "win_appmonitor.h"
#include "win_crypt.h"
#include <kapps_core/src/win/win_error.h>
#include <kapps_core/src/stringslice.h>
#include <Psapi.h>
#include <comutil.h>
#include <array>

#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "Wbemuuid.lib")

namespace kapps { namespace net {

// The threading model in WinAppMonitor is complex due to the different sources
// of notifications used.
// - Process creation notifications are received via WMI on a WMI thread.
// - Process terminate notifications are received on thread pool threads using
//   RegisterWaitForSingleObject() on each process handle.
//
//      .-----.                                              .-------------.
//      | WMI |                                              | Thread pool |
//      '-----'                                              '-------------'
//Indicate |   .--------------------.                    Handle     |
//         '-->| WAM::WbemEventSink |                    signaled   V
//             '--------------------'                     .--------------------.
//                      | Process           .------------>| WinAppTracker (2x) |
//                      V created           |Check        '--------------------'
//         .-----------------------.        |process      App IDs |   ^ Set
//         | WinSplitTunnelTracker |--------'             changed |   | rules
//         |                       |<-----------------------------'   |
//         '-----------------------'----------------------------------'
//         App IDs |   ^ Set
//         changed V   | rules
//              .---------------.
//              | WinAppTracker |
//              '---------------'
//                         ^ Set
//                         | rules
//                    .---------.
//                    | Product |
//                    '---------'

class SystemTimeTracer : public core::OStreamInsertable<SystemTimeTracer>
{
public:
    SystemTimeTracer(const SYSTEMTIME &tm) : _tm{tm} {}

    void trace(std::ostream &os) const
    {
        enum : std::size_t { DatePrintLen = 24 };
        std::array<char, 24> date{};
        // Day of week is ignored (overspecified)
        auto renderLen = std::snprintf(date.data(), date.size(),
                                       "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                                       _tm.wYear, _tm.wMonth, _tm.wDay,
                                       _tm.wHour, _tm.wMinute, _tm.wSecond,
                                       _tm.wMilliseconds);
        if(renderLen > 0 && renderLen < date.size())
            os << core::StringSlice{date.data(), static_cast<std::size_t>(renderLen)};
        else
            os << "<error: " << renderLen << '>';
    }

private:
    const SYSTEMTIME &_tm;
};

class FileTimeTracer : public core::OStreamInsertable<FileTimeTracer>
{
public:
    FileTimeTracer(const FILETIME &tm) : _tm{tm} {}

    void trace(std::ostream &os) const
    {
        SYSTEMTIME sysTime;
        if(!::FileTimeToSystemTime(&_tm, &sysTime))
        {
            os << "<invalid time " << _tm.dwHighDateTime << ' '
                << _tm.dwLowDateTime << ">";
        }
        else
        {
            os << SystemTimeTracer{sysTime};
            // SystemTimeTracer loses the 100-nanosecond precision of
            // FILETIME.  This is used by WinAppMonitor for times with only
            // millisecond precision, but trace this part if it would somehow
            // be set.
            ULARGE_INTEGER timeLi;
            timeLi.HighPart = _tm.dwHighDateTime;
            timeLi.LowPart = _tm.dwLowDateTime;
            auto hundredNsec = timeLi.QuadPart % 10;
            if(hundredNsec)
                os << "(+" << hundredNsec << "00ns)";
        }
    }

private:
    const FILETIME &_tm;
};

WinAppTracker::WinAppTracker(SplitType type, core::WorkThread &cleanupThread)
    : _type{type}, _cleanupThread{cleanupThread}
{
}

AppIdSet WinAppTracker::getAppIds() const
{
    std::lock_guard<std::mutex> lock{_mutex};
    AppIdSet ids;
    for(const auto &excludedApp : _apps)
        ids.insert(excludedApp.first);
    for(const auto &proc : _procData)
        ids.insert(proc.second._pProcAppId);
    return ids;
}

bool WinAppTracker::setSplitTunnelRules(const std::unordered_set<std::wstring> &executables)
{
    std::unique_lock<std::mutex> lock{_mutex};

    ExcludedApps_t addedApps;
    for(const auto &exe : executables)
    {
        std::shared_ptr<AppIdKey> pAppId{new AppIdKey{exe}};
        if(!*pAppId)
            continue;
        auto emplaceResult = addedApps.emplace(std::move(pAppId), ExcludedAppData{});
        if(emplaceResult.second)
            emplaceResult.first->second._targetPath = exe;
    }

    for(auto itExistingApp = _apps.begin(); itExistingApp != _apps.end(); )
    {
        // Is the app still excluded?
        auto itAddedApp = addedApps.find(itExistingApp->first);
        if(itAddedApp == addedApps.end())
        {
            // No - remove it.  Remove all processes first
            for(const auto &pid : itExistingApp->second._runningProcesses)
                _procData.erase(pid);
            itExistingApp = _apps.erase(itExistingApp);
        }
        else
        {
            // Yes - it's not being added, just keep it.
            addedApps.erase(itAddedApp);
            ++itExistingApp;
        }
    }

    // Add new apps
    while(!addedApps.empty())
    {
        // Move node from addedApps to _excludedApps (AppIdKey is move only,
        // also preserve _targetPath).
        auto appNode = addedApps.extract(addedApps.begin());
        auto insertResult = _apps.insert(std::move(appNode));
        // It's possible that the insert could fail if the same app ID was found
        // more than once from the split tunnel rules, and it was already in the
        // existing app IDs.  Ignore it in this case since it's a duplicate.
        if(insertResult.inserted)
        {
            // Read the signer names since we're adding a new app
            insertResult.position->second._signerNames = winGetExecutableSigners(insertResult.position->second._targetPath);
            // At this point, we could conceivably scan for existing processes
            // that match this app (and their descendants, etc.).  However, it
            // won't help for most "launcher" apps - the launcher is gone by
            // this point, we can't match the descendants.  It might help for
            // apps with long-lived "helpers" but those are much less common,
            // and we already have a caveat that apps may need to be restarted
            // after adding them anyway.
        }
    }

    // It's possible the set of excluded app IDs might not have changed, but it
    // usually does, it's not worth attempting to figure this out here (the
    // firewall implementation will compare the new app IDs to the current
    // rules).
    dumpNoLock();

    // It's only possible to match a child process if we have at least one
    // excluded app with at least one signer name.  If we have no apps (or they
    // are all unsigned), there's no need to monitor for child processes.
    bool needMonitor{false};
    for(const auto &app : _apps)
    {
        if(!app.second._signerNames.empty())
            needMonitor = true;
    }

    if(!needMonitor)
    {
        KAPPS_CORE_INFO() << "No signed apps in" << _type
            << "list, do not need to monitor processes";
    }

    // Unlock before notifying.  It's OK if more changes occur before the
    // recipient observes the new app IDs.
    lock.unlock();
    appIdsChanged();

    return needMonitor;
}

void WinAppTracker::checkMatchingProcess(WinHandle &procHandle,
                                         std::shared_ptr<AppIdKey> &pAppId,
                                         Pid_t pid)
{
    std::lock_guard<std::mutex> lock{_mutex};
    // If we already have this process, there's nothing to do.  Just take the
    // process handle to indicate that this was ours.
    // This can happen if a process is created just as we start to scan for a
    // new app rule; we might observe the process just before receiving the
    // "create" event.
    if(_procData.count(pid))
    {
        KAPPS_CORE_INFO() << "Already have" << _type << "PID" << pid
            << "-" << *pAppId << "- nothing to do";
        procHandle = {};
        pAppId = {};
        return;
    }

    // Check if it's a matching app itself.  Do this before checking if it's a
    // descendant - it's possible it could be both if one excluded app launches
    // another.
    auto itMatchingApp = _apps.find(pAppId);
    if(itMatchingApp != _apps.end())
    {
        // It matches one of our apps - take the process handle and app ID; this
        // indicates that the other trackers don't need to be checked.  (Do this
        // even if we do not actually add this process entry.)
        WinHandle takenProcHandle;
        std::shared_ptr<AppIdKey> pTakenAppId;
        procHandle.swap(takenProcHandle);
        pAppId.swap(pTakenAppId);

        // Add it only if there are signer names for this app - if there aren't,
        // we'd never match any descendants, skip it.
        if(!itMatchingApp->second._signerNames.empty())
        {
            KAPPS_CORE_INFO() << "PID" << pid << "is" << _type << "app"
                << itMatchingApp->first;
            addSplitProcess(itMatchingApp, std::move(takenProcHandle), pid,
                               std::move(pTakenAppId));
            dumpNoLock();
            // This does not cause excluded app IDs to change (it's an explicit app
            // ID we already knew about)
        }
        // Otherwise, there's no need to add it, since it can't match any
        // descendants, but we still took the process handle and app ID so we
        // won't check any other trackers or descendants of other rules in this
        // tracker.
    }
}

void WinAppTracker::checkMatchingChild(WinHandle &procHandle,
                                       std::shared_ptr<AppIdKey> &pAppId,
                                       const std::wstring &imgPath, Pid_t pid, Pid_t parentPid)
{
    // Since we're just checking for a child at this point, take a new lock on
    // _mutex
    checkMatchingChildImpl(std::unique_lock<std::mutex>{_mutex}, procHandle,
                           pAppId, imgPath, pid, parentPid);
}

void WinAppTracker::checkNewParent(WinHandle &procHandle,
                                   std::shared_ptr<AppIdKey> &pAppId,
                                   const std::wstring &imgPath, Pid_t pid,
                                   WinHandle &parentHandle,
                                   std::shared_ptr<AppIdKey> &pParentAppId,
                                   Pid_t parentPid)
{
    std::unique_lock<std::mutex> lock{_mutex};
    // If the parent process isn't known, check if it is an excluded app itself.
    // This sometimes happens if the process starts and exits within the 1-ms
    // polling interval of WMI - we're never notified about it at all.  It could
    // also happen if events are delivered out of order.
    //
    // We don't check if it's a potential descendant, the only way to get an
    // arbitrary process's parent in Windows is to try to take a toolhelp
    // snapshot and search for this process, which would not scale well.
    //
    // This means that apps whose "launcher" processes are very short-lived
    // should work, but if an intermediate process is very short-lived we might
    // not be able to identify their descendants.
    auto itParentApp = _apps.find(pParentAppId);
    // If the parent is an excluded app, it's ours - take parentHandle and
    // parentAppId to indicate this to the caller, and add it if it has any
    // signer names.
    if(itParentApp != _apps.end())
    {
        WinHandle takenProcHandle;
        std::shared_ptr<AppIdKey> pTakenAppId;
        parentHandle.swap(takenProcHandle);
        pParentAppId.swap(pTakenAppId);

        KAPPS_CORE_INFO() << "Parent" << parentPid << "matches" << _type
            << "rule for" << *pTakenAppId;
        if(!itParentApp->second._signerNames.empty())
        {
            KAPPS_CORE_INFO() << "Parent" << parentPid << "was not known but is"
                << _type << "- add it to" << *pTakenAppId;
            // The parent wasn't known and is a matching app.  Add it, and then
            // check the new child process.  (The child process may or may not
            // match; it might not have a matching signature.)
            //
            // It's possible this process has exited; if that's the case then
            // we'll immediately be notified and remove it after we finish
            // processing notifications.
            addSplitProcess(itParentApp, std::move(takenProcHandle), parentPid,
                            std::move(pTakenAppId));
            dumpNoLock();

            // Added a new parent, so check if this child matches that parent.
            // Transfer our lock into checkMatchingChildIMpl(); we can't unlock in
            // between since that could allow the parent to be removed if it has
            // already become signaled.
            checkMatchingChildImpl(std::move(lock), procHandle, pAppId, imgPath,
                                   pid, parentPid);
        }
        // Otherwise, we took the parent handle and app ID since this was our
        // parent (no need to check if it matches any other tracker), but we
        // don't need to check the child since the parent had no signer names
        // (it can't possibly match).
    }
}

void WinAppTracker::dump() const
{
    std::lock_guard<std::mutex> lock{_mutex};
    dumpNoLock();
}

void WinAppTracker::dumpNoLock() const
{
    std::size_t processes{0};
    KAPPS_CORE_INFO() << "===" << _type << "app tracker dump ===";
    KAPPS_CORE_INFO() << _apps.size() << "apps";
    for(auto itExclApp = _apps.begin(); itExclApp != _apps.end(); ++itExclApp)
    {
        const auto &app = *itExclApp;

        processes += app.second._runningProcesses.size();
        KAPPS_CORE_INFO().nospace() << "[" << app.second._runningProcesses.size() << "] "
            << app.first;
        for(const auto &pid : app.second._runningProcesses)
        {
            auto itProcData = _procData.find(pid);
            if(itProcData == _procData.end())
            {
                KAPPS_CORE_WARNING() << " -" << pid << "**MISSING**";
                continue;
            }

            KAPPS_CORE_INFO() << " -" << pid
                << core::tracePointer(itProcData->second._pProcAppId);

            if(itProcData->second._excludedAppPos != itExclApp)
            {
                KAPPS_CORE_WARNING() << "  ^ **MISMATCH**" << itProcData->second._excludedAppPos->first;
            }
        }
    }

    KAPPS_CORE_INFO() << "Total" << processes << "processes";
    if(processes != _procData.size())
    {
        KAPPS_CORE_WARNING() << "**MISMATCH** Expected" << processes << "- have"
            << _procData.size();
    }
}

void WinAppTracker::addSplitProcess(ExcludedApps_t::iterator itMatchingApp,
                                    WinHandle procHandle, Pid_t pid,
                                    std::shared_ptr<AppIdKey> pAppId)
{
    assert(itMatchingApp != _apps.end()); // Ensured by caller
    assert(_procData.count(pid) == 0);    // Ensured by caller
    assert(itMatchingApp->second._runningProcesses.count(pid) == 0);    // Class invariant

    itMatchingApp->second._runningProcesses.emplace(pid);
    ProcessData &data = _procData[pid];
    data._procHandle = std::move(procHandle);
    data._pWaiter.reset(new core::WinSingleWait{data._procHandle.get(),
        [this](HANDLE procHandle){onProcessExited(procHandle);}, true});
    data._pProcAppId = std::move(pAppId);
    data._excludedAppPos = itMatchingApp;
}


void WinAppTracker::checkMatchingChildImpl(std::unique_lock<std::mutex> lock,
                                           WinHandle &procHandle,
                                           std::shared_ptr<AppIdKey> &pAppId,
                                           const std::wstring &imgPath, Pid_t pid, Pid_t parentPid)
{
    auto itMatchingApp = isAppDescendant(parentPid, imgPath);
    if(itMatchingApp != _apps.end())
    {
        KAPPS_CORE_INFO() << "PID" << pid << "(parent" << parentPid
            << ") is descendant of excluded app" << itMatchingApp->first;
        WinHandle takenProcHandle;
        std::shared_ptr<AppIdKey> pTakenAppId;
        // As above, ensure the incoming handle and app ID are cleared
        takenProcHandle.swap(procHandle);
        pTakenAppId.swap(pAppId);
        addSplitProcess(itMatchingApp, std::move(takenProcHandle), pid,
                           std::move(pTakenAppId));
        dumpNoLock();

        // App IDs have most likely changed.  It's possible they didn't if the new
        // app ID was already known, but it's not worth the tracking to try to
        // determine that here, let the firewall implementation handle it.
        lock.unlock();
        appIdsChanged();
    }

    // Note that lock may or may not be unlocked at this point.
}

auto WinAppTracker::isAppDescendant(Pid_t parentPid, const std::wstring &imgPath)
    -> ExcludedApps_t::iterator
{
    // Note that the parent PID given does not in general refer to the actual
    // process that created this one - the PID could have been reused if the
    // parent has already exited.
    //
    // However, if the PID is found in _procData, then we _know_ it hasn't been
    // reused, because we have kept a handle open to that process since it was
    // first observed.
    auto itProcData = _procData.find(parentPid);
    if(itProcData == _procData.end())
        return _apps.end(); // Not a descendant of anything we care about

    // The process was started (potentially indirectly) by an excluded app.
    // However, we only consider it a valid descendant if shares at least one
    // signer name with the excluded app.
    //
    // This correctly differentiates most "launcher" and "helper" apps from
    // unrelated apps launching each other, such as web browsers opening
    // downloaded programs, etc.
    //
    // This might not be perfect, but it's better to err on the side of caution;
    // a user can always manually add an executable that we fail to detect, but
    // right now there's no way to prevent us from excluding something that we
    // do detect as a descendant.

    // If the excluded app has no signatures, never match any descendants for it
    // (skips checking the signatures on the new child).
    if(itProcData->second._excludedAppPos->second._signerNames.empty())
        return _apps.end();

    std::set<std::wstring> signerNames{winGetExecutableSigners(imgPath)};

    for(const auto &expectedSignerName : itProcData->second._excludedAppPos->second._signerNames)
    {
        // If a signer name matches, this is a valid descendant.
        if(signerNames.count(expectedSignerName) > 0)
            return itProcData->second._excludedAppPos;
    }

    // No signer name matched; not a valid descendant.
    return _apps.end();
}

void WinAppTracker::onProcessExited(HANDLE procHandle)
{
    std::unique_lock<std::mutex> lock{_mutex};
    Pid_t pid = ::GetProcessId(procHandle);

    // If this PID isn't known, there's nothing to do.  This shouldn't happen
    // though, because the PID can't be reused until we close our process
    // handle, and we keep that handle open until the WinSingleWait is
    // destroyed, or until the process exit is processed here.
    auto itProcData = _procData.find(pid);
    if(itProcData == _procData.end())
    {
        KAPPS_CORE_INFO() << "Already removed PID" << pid;
        return;
    }

    KAPPS_CORE_INFO() << "PID" << pid << "exited -" << _type << "app"
        << core::tracePointer(itProcData->second._pProcAppId) << "- group"
        << itProcData->second._excludedAppPos->first;

    // Remove everything about this process.  We don't remember extra app IDs
    // that we have found once the process exits.  In principle, we could try to
    // remember them so there's no exclusion race if the helper process starts
    // again, but there are a number of trade-offs here:
    //
    // - There's no way to reliably know the extra app IDs the first time an app
    //   is added.  If we try to remember them here, the behavior would change
    //   between the first app launch and subsequent launches.  Consistent
    //   behavior is preferred even if it is imprecise.
    //
    // - This is more robust if an app is excluded unexpectedly.  For example,
    //   if an excluded app occasionally runs "tracert.exe", it'll exclude it
    //   while it's being run by that app (probably desirable), but then it goes
    //   back to normal as soon as it exits (it won't affect a tracert from the
    //   shell, etc.).  The only potential problem is if tracert.exe is run in
    //   both excluded and non-excluded contexts simultaneously.
    //
    // - This approach minimizes the possibility of leaking state if there is an
    //   error in the state tracking of WinAppTracker.

    // Remove the PID from the excluded app group.
    assert(itProcData->second._excludedAppPos != _apps.end());    // Class invariant
    itProcData->second._excludedAppPos->second._runningProcesses.erase(pid);

    // We can't destroy the WinSingleWait in _pWaiter on this thread; it can't
    // be destroyed during its own callback.  Queue it over to the cleanup
    // thread so it is destroyed there later.
    //
    // We don't actually have to do any explicit processing on these work items,
    // Any's destructor will destroy the underlying value when they're pulled
    // from the queue.
    _cleanupThread.enqueue(std::move(itProcData->second._pWaiter));

    // unique_ptr's move constructor has a stronger-than-normal postcondition
    // that the moved-from object is always empty.
    assert(!itProcData->second._pWaiter);

    // Remove the process data.  Note that this closes procHandle, which is a
    // copy of the handle owned by the process data.
    _procData.erase(itProcData);

    // App IDs have most likely changed.  It's possible they didn't if there is
    // still a PID tracked with this same app ID, but again, let the firewall
    // handle that.
    dumpNoLock();
    lock.unlock();
    appIdsChanged();
}

WinSplitTunnelTracker::WinSplitTunnelTracker()
    : _cleanupThread{[](core::Any){}}, // Just used to destroy objects, see WinAppTracker::onProcessExited
      _vpnOnly{WinAppTracker::SplitType::VpnOnly, _cleanupThread},
      _excluded{WinAppTracker::SplitType::Excluded, _cleanupThread}
{
    _vpnOnly.appIdsChanged = [this]{appIdsChanged();};
    _excluded.appIdsChanged = [this]{appIdsChanged();};
}

std::wstring WinSplitTunnelTracker::getProcImagePath(const WinHandle &procHandle) const
{
    if(!procHandle)
    {
        KAPPS_CORE_WARNING() << "Can't get image path for null process handle";
        return {};
    }

    // First try QueryFullProcessImageNameW().  This returns a Win32 path but
    // tends not to work if the process has exited.
    std::wstring procImage;
    procImage.resize(2048);
    DWORD len = procImage.size();
    if(::QueryFullProcessImageNameW(procHandle.get(), 0, &procImage[0], &len))
    {
        procImage.resize(len);
        return procImage;
    }

    KAPPS_CORE_INFO() << "Couldn't get Win32 executable path for PID"
        << ::GetProcessId(procHandle.get())
        << core::WinErrTracer{::GetLastError()};

    // Try ::GetProcessImageFileNameW().  This usually still works for processes
    // that have exited, but it returns an NT path - it's possible to get this
    // to pass this into Win32 APIs, but we use it as a last resort to minimize
    // risk of corner cases.
    len = ::GetProcessImageFileNameW(procHandle.get(), procImage.data(),
                                            procImage.size());
    // 0 indicates failure; procImage.size() indicates possible truncation
    if(len == 0 || len == procImage.size())
    {
        KAPPS_CORE_WARNING() << "Couldn't get NT executable path for PID"
            << ::GetProcessId(procHandle.get())
            << core::WinErrTracer{::GetLastError()};
        return {};
    }

    // We need a Win32 path to pass it through the API to get an app ID - just
    // prefix the NT path with "\\?\GLOBALROOT" so it'll be passed through to
    // the kernel.
    procImage.resize(len);
    procImage.insert(0, L"\\\\?\\GLOBALROOT");
    return procImage;
}

std::shared_ptr<AppIdKey> WinSplitTunnelTracker::getProcAppId(const WinHandle &procHandle) const
{
    std::wstring imgPath{getProcImagePath(procHandle)};
    if(imgPath.empty())
        return {};

    std::shared_ptr<AppIdKey> pAppId{new AppIdKey{imgPath}};
    if(!*pAppId)
        return {};  // Return empty pointer rather than pointer-to-empty AppIdKey
    return pAppId;
}

AppIdSet WinSplitTunnelTracker::getExcludedAppIds() const
{
    return _excluded.getAppIds();
}

AppIdSet WinSplitTunnelTracker::getVpnOnlyAppIds() const
{
    return _vpnOnly.getAppIds();
}

bool WinSplitTunnelTracker::setSplitTunnelRules(const std::unordered_set<std::wstring> &excludedExes,
                                                const std::unordered_set<std::wstring> &vpnOnlyExes)
{
    bool vpnOnlyRules = _vpnOnly.setSplitTunnelRules(vpnOnlyExes);
    bool exclusionRules = _excluded.setSplitTunnelRules(excludedExes);
    return vpnOnlyRules || exclusionRules;
}

void WinSplitTunnelTracker::processCreated(WinHandle procHandle, Pid_t pid,
                                           WinHandle parentHandle, Pid_t parentPid)
{
    std::wstring imgPath{getProcImagePath(procHandle)};
    std::shared_ptr<AppIdKey> pAppId;
    if(!imgPath.empty())
    {
        pAppId.reset(new AppIdKey{imgPath});
    }
    // Can't do anything if we couldn't get this process's app ID.
    if(!pAppId || !*pAppId)
    {
        KAPPS_CORE_WARNING() << "Couldn't get app ID for PID" << pid << "(from parent"
            << parentPid << ")";
        return;
    }

    // First, check if the process is a direct match to any tracker's rules.
    // Check direct matches for all trackers before checking descendants, in
    // case a process matching one rule invokes another matching process of a
    // different rule type.
    //
    // The app trackers take the process handle if it's matched, so we're done
    // as soon as the process handle is taken.  (The WinAppTracker traces for
    // this.)
    _vpnOnly.checkMatchingProcess(procHandle, pAppId, pid);
    if(!procHandle)
        return;
    _excluded.checkMatchingProcess(procHandle, pAppId, pid);
    if(!procHandle)
        return;

    // Check if the process is a descendant of a known parent process.  If one
    // of the trackers matches it this way, then we don't need to find the
    // parent's AppIdKey.
    _vpnOnly.checkMatchingChild(procHandle, pAppId, imgPath, pid, parentPid);
    if(!procHandle)
        return;
    _excluded.checkMatchingChild(procHandle, pAppId, imgPath, pid, parentPid);
    if(!procHandle)
        return;

    // As discussed in checkNewParent(), it's possible to observe a new child
    // from WMI without having observed the creation of the parent process.
    // Since nothing has matched yet, find the AppIdKey for the parent process,
    // and check if the parent matches any rule.  (If it does, the matching
    // WinAppTracker also checks if the new child should be considered a
    // descendant.)
    auto pParentAppId{getProcAppId(parentHandle)};
    if(!pParentAppId || !*pParentAppId)
    {
        KAPPS_CORE_WARNING() << "Couldn't get app ID for parent" << parentPid
            << "of new process" << pid << "-" << imgPath;
        return;
    }
    _vpnOnly.checkNewParent(procHandle, pAppId, imgPath, pid, parentHandle,
                            pParentAppId, parentPid);
    if(!procHandle || !parentHandle)
        return;
    _excluded.checkNewParent(procHandle, pAppId, imgPath, pid, parentHandle,
                             pParentAppId, parentPid);
}

void WinSplitTunnelTracker::dump() const
{
    _vpnOnly.dump();
    _excluded.dump();
}

// This WBEM event sink is used to implement WinAppMonitor.
class WinAppMonitor::WbemEventSink : public IWbemObjectSink
{
public:
    WbemEventSink(WinAppMonitor &monitor, WinComPtr<ISWbemDateTime> pWbemDateTime)
        : _refs{0}, _pWbemDateTime{std::move(pWbemDateTime)}, _pMonitor{&monitor}
    {
        assert(_pWbemDateTime);   // Ensured by caller
    }
    // Not copiable due to ref count
    WbemEventSink(const WbemEventSink &) = delete;
    WbemEventSink &operator=(const WbemEventSink &) = delete;
    // Destructor just sanity-checks reference count
    virtual ~WbemEventSink() {assert(_refs == 0);}

public:
    // IUnknown
    virtual ULONG STDMETHODCALLTYPE AddRef() override;
    virtual ULONG STDMETHODCALLTYPE Release() override;
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override;

    // IWbemObjectSink
    virtual HRESULT STDMETHODCALLTYPE Indicate(LONG lObjectCount,
                                               IWbemClassObject **apObjArray) override;
    virtual HRESULT STDMETHODCALLTYPE SetStatus(LONG lFlags, HRESULT hResult,
                                                BSTR strParam,
                                                IWbemClassObject *pObjParam) override;

    // Disconnect WinAppMonitor because it's being destroyed
    void disconnect();

private:
    void handleEventObject(IWbemClassObject *pObj);
    DWORD readPidProp(IWbemClassObject &obj, const wchar_t *pPropName);
    void readNewProcess(IWbemClassObject &obj);

private:
    // IUnknown ref count - atomic
    LONG _refs;
    // Date/time converter - used only from notification callback
    WinComPtr<ISWbemDateTime> _pWbemDateTime;
    // Monitor where changes are sent - protected with a mutex, cleared when
    // monitor tells us to disconnect
    mutable std::mutex _monitorMutex;
    WinAppMonitor *_pMonitor;
};

ULONG WinAppMonitor::WbemEventSink::AddRef()
{
    return ::InterlockedIncrement(&_refs);
}

ULONG WinAppMonitor::WbemEventSink::Release()
{
    LONG refs = ::InterlockedDecrement(&_refs);
    if(refs == 0)
        delete this;
    return refs;
}

HRESULT WinAppMonitor::WbemEventSink::QueryInterface(REFIID riid, void **ppv)
{
    if(riid == IID_IUnknown)
    {
        IUnknown *pThisItf{this};
        *ppv = pThisItf;
        AddRef();
        return WBEM_S_NO_ERROR;
    }
    if(riid == IID_IWbemObjectSink)
    {
        IWbemObjectSink *pThisItf{this};
        *ppv = pThisItf;
        AddRef();
        return WBEM_S_NO_ERROR;
    }
    return E_NOINTERFACE;
}

HRESULT WinAppMonitor::WbemEventSink::Indicate(long lObjectCount,
                                               IWbemClassObject **apObjArray)
{
    std::lock_guard<std::mutex> lock{_monitorMutex};
    if(!_pMonitor)
        return WBEM_S_NO_ERROR; // Ignore notifications, disconnected

    KAPPS_CORE_INFO() << "Notifications:" << lObjectCount;
    IWbemClassObject **ppObj = apObjArray;
    IWbemClassObject **ppObjEnd = apObjArray + lObjectCount;
    while(ppObj != ppObjEnd)
    {
        handleEventObject(*ppObj);
        ++ppObj;
    }

    return WBEM_S_NO_ERROR;
}

HRESULT WinAppMonitor::WbemEventSink::SetStatus(LONG lFlags, HRESULT hResult,
                                                BSTR strParam,
                                                IWbemClassObject *pObjParam)
{
    switch(lFlags)
    {
        case WBEM_STATUS_COMPLETE:
            KAPPS_CORE_INFO() << "Event sink call complete -" << hResult;
            break;
        case WBEM_STATUS_PROGRESS:
            KAPPS_CORE_INFO() << "Event sink call progress -" << hResult;
            break;
        default:
            KAPPS_CORE_INFO() << "Event sink call unexpected status" << lFlags << "-"
                << hResult;
            break;
    }
    return WBEM_S_NO_ERROR;
}

void WinAppMonitor::WbemEventSink::disconnect()
{
    std::lock_guard<std::mutex> lock{_monitorMutex};
    _pMonitor = nullptr;
}

void WinAppMonitor::WbemEventSink::handleEventObject(IWbemClassObject *pObj)
{
    // Get the TargetInstance property - the process that was created
    WinComVariant targetVar;
    HRESULT targetErr = pObj->Get(L"TargetInstance", 0, targetVar.receive(),
                                  nullptr, nullptr);
    if(FAILED(targetErr))
    {
        KAPPS_CORE_WARNING() << "Failed to read target from event -" << targetErr;
        return;
    }

    HRESULT convErr = ::VariantChangeType(&targetVar.get(), &targetVar.get(), 0,
                                          VT_UNKNOWN);
    if(FAILED(convErr) || !targetVar.get().punkVal)
    {
        KAPPS_CORE_WARNING() << "Failed to convert target to IUnknown -" << convErr;
        return;
    }

    WinComPtr<IUnknown> pTgtUnk{targetVar.get().punkVal};
    pTgtUnk->AddRef();

    auto pTgtObj = pTgtUnk.queryInterface<IWbemClassObject>(IID_IWbemClassObject);
    if(!pTgtObj)
    {
        KAPPS_CORE_WARNING() << "Failed to get object interface from target";
        return;
    }

    readNewProcess(*pTgtObj);
}

DWORD WinAppMonitor::WbemEventSink::readPidProp(IWbemClassObject &obj,
                                                const wchar_t *pPropName)
{
    // Get the property
    WinComVariant pidVar;
    HRESULT pidErr = obj.Get(pPropName, 0, pidVar.receive(), nullptr, nullptr);
    if(FAILED(pidErr))
    {
        KAPPS_CORE_WARNING() << "Failed to read" << core::WStringSlice{pPropName}
            << "from new process -" << pidErr;
        return 0;
    }

    HRESULT pidConvErr = ::VariantChangeType(&pidVar.get(), &pidVar.get(), 0,
                                             VT_UI4);
    if(FAILED(pidConvErr))
    {
        KAPPS_CORE_WARNING() << "Failed to convert" << core::WStringSlice{pPropName}
            << "to VT_UI4 -" << pidConvErr;
        return 0;
    }

    return V_UI4(&pidVar.get());
}

void WinAppMonitor::WbemEventSink::readNewProcess(IWbemClassObject &obj)
{
    assert(_pWbemDateTime);   // Class invariant

    // Get the process ID and parent process ID
    DWORD pid = readPidProp(obj, L"ProcessId");
    DWORD ppid = readPidProp(obj, L"ParentProcessId");

    if(!pid || !ppid)
        return; // Traced by readPidProp()

    KAPPS_CORE_INFO() << "Parent" << ppid << "->" << pid;

    // Get the creation time
    WinComVariant createVar;
    HRESULT createErr = obj.Get(L"CreationDate", 0, createVar.receive(),
                                nullptr, nullptr);
    if(FAILED(createErr))
    {
        KAPPS_CORE_WARNING() << "Failed to read creation date from process" << pid
            << "-" << createErr;
        return;
    }
    HRESULT createConvErr = ::VariantChangeType(&createVar.get(), &createVar.get(),
                                                0, VT_BSTR);
    if(FAILED(createConvErr))
    {
        KAPPS_CORE_WARNING() << "Failed to convert creation date of process" << pid
            << "to VT_BSTR -" << createConvErr << "- type is"
            << V_VT(&createVar.get());
        return;
    }
    // Creation times are (bizarrely) encoded as strings
    HRESULT setTimeErr = _pWbemDateTime->put_Value(V_BSTR(&createVar.get()));
    if(FAILED(setTimeErr))
    {
        KAPPS_CORE_WARNING() << "Failed to parse creation date of process" << pid
            << "-" << setTimeErr << "- value is"
            << core::WStringSlice{V_BSTR(&createVar.get())};
        return;
    }
    BSTR createFileTimeBstrPtr{nullptr};
    HRESULT getTimeErr = _pWbemDateTime->GetFileTime(false, &createFileTimeBstrPtr);
    // Own the BSTR
    _bstr_t createFileTimeBstr{createFileTimeBstrPtr, false};
    createFileTimeBstrPtr = nullptr;
    if(FAILED(getTimeErr))
    {
        KAPPS_CORE_WARNING() << "Failed to get creation date of process" << pid
            << "-" << getTimeErr << "- value is"
            << core::WStringSlice{V_BSTR(&createVar.get())};
        return;
    }

    // Parse the new string, which is now a stringified FILETIME
    ULARGE_INTEGER timeLi;
    timeLi.QuadPart = static_cast<std::uint64_t>(_wtoi64(createFileTimeBstr));
    // This time typically only has millisecond precision, drop the
    // 100-nanosecond part anyway to be sure it's consistent with the test below
    timeLi.QuadPart -= (timeLi.QuadPart % 10);
    FILETIME createFileTime;
    createFileTime.dwLowDateTime = timeLi.LowPart;
    createFileTime.dwHighDateTime = timeLi.HighPart;

    // Open the process
    WinHandle procHandle{::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid)};
    if(!procHandle)
    {
        KAPPS_CORE_WARNING() << "Unable to open process" << pid;
        return;
    }

    // Check if it's really the right process - the PID could have been reused
    FILETIME actualCreateTime, ignored1, ignored2, ignored3;
    if(!::GetProcessTimes(procHandle.get(), &actualCreateTime, &ignored1,
                          &ignored2, &ignored3))
    {
        KAPPS_CORE_WARNING() << "Unable to get creation time of" << pid;
        return;
    }

    // Drop the 100-nanosecond precision down to millisecond precision for
    // consistency with the WMI time
    timeLi.HighPart = actualCreateTime.dwHighDateTime;
    timeLi.LowPart = actualCreateTime.dwLowDateTime;
    timeLi.QuadPart -= (timeLi.QuadPart % 10);
    FILETIME approxCreateTime;
    approxCreateTime.dwHighDateTime = timeLi.HighPart;
    approxCreateTime.dwLowDateTime = timeLi.LowPart;

    if(approxCreateTime.dwLowDateTime != createFileTime.dwLowDateTime ||
        approxCreateTime.dwHighDateTime != createFileTime.dwHighDateTime)
    {
        KAPPS_CORE_WARNING() << "Ignoring PID" << pid
            << "- PID was reused.  Expected creation time"
            << FileTimeTracer{createFileTime} << "- got"
            << FileTimeTracer{approxCreateTime};
    }

    // Open the parent process
    WinHandle parentHandle{::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, ppid)};
    if(!parentHandle)
    {
        KAPPS_CORE_WARNING() << "Unable to open parent process" << ppid << "-"
            << kapps::core::WinErrTracer{::GetLastError()};
        return;
    }
    // Check if this is really the right parent process, in case the PID was
    // reused
    FILETIME parentCreateTime;
    if(!::GetProcessTimes(parentHandle.get(), &parentCreateTime, &ignored1,
                          &ignored2, &ignored3))
    {
        KAPPS_CORE_WARNING() << "Unable to get creation time of" << ppid << "(parent of"
            << pid << ") -" << kapps::core::WinErrTracer{::GetLastError()};
        return;
    }

    // If the parent process is newer, the PID was reused.  (If they're the
    // same, we're unsure but we assume it's the correct one.)
    if(::CompareFileTime(&parentCreateTime, &actualCreateTime) > 0)
    {
        KAPPS_CORE_WARNING() << "Ignoring PID" << pid
            << "- parent PID" << ppid << "was reused.  Child was created at"
            << FileTimeTracer{actualCreateTime} << "- parent reported"
            << FileTimeTracer{parentCreateTime};
        // There's no reason to check the child if the parent PID was reused.
        // - If this had been a child of a process that we're excluding, we
        //   would still have a HANDLE open to the process and the PID wouldn't
        //   have been reused.
        // - If the parent was actually an excluded process that we didn't know
        //   about, there's no way to figure that out now since the process is
        //   gone.
        return;
    }

    // We opened the process successfully, notify WinAppMonitor
    // (_monitorMutex is locked by Indicate())
    assert(_pMonitor);    // Checked by Indicate()
    _pMonitor->_tracker.processCreated(std::move(procHandle), pid,
                                       std::move(parentHandle), ppid);
}

WinAppMonitor::WinAppMonitor()
{
    _tracker.appIdsChanged = [this]{appIdsChanged();};

    // Create the WMI locator
    auto pLocator = WinComPtr<IWbemLocator>::createInprocInst(CLSID_WbemLocator, IID_IWbemLocator);
    if(!pLocator)
    {
        KAPPS_CORE_WARNING() << "Unable to create WMI locator";   // Error traced by WinComPtr
        return;
    }

    // Connect to WMI
    HRESULT connectErr = pLocator->ConnectServer(_bstr_t{L"ROOT\\CIMV2"},
                                                 nullptr, nullptr, nullptr,
                                                 WBEM_FLAG_CONNECT_USE_MAX_WAIT,
                                                 nullptr, nullptr,
                                                 _pSvcs.receive());
    if(FAILED(connectErr) || !_pSvcs)
    {
        KAPPS_CORE_WARNING() << "Unable to connect to WMI -" << connectErr;
        return;
    }

    HRESULT proxyErr = ::CoSetProxyBlanket(_pSvcs.get(),
                                           RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                                           nullptr, RPC_C_AUTHN_LEVEL_CALL,
                                           RPC_C_IMP_LEVEL_IMPERSONATE,
                                           nullptr, EOAC_NONE);
    if(FAILED(proxyErr))
    {
        KAPPS_CORE_WARNING() << "Unable to configure WMI proxy -" << connectErr;
        return;
    }
}

WinAppMonitor::~WinAppMonitor()
{
    deactivate();
}

void WinAppMonitor::activate()
{
    if(_pSink || _pSinkStubSink)
        return; // Already active, skip trace

    if(!_pSvcs)
    {
        KAPPS_CORE_WARNING() << "Can't activate monitor, couldn't connect to WMI";
        return;
    }

    KAPPS_CORE_INFO() << "Activating monitor";

    auto pAptmt = WinComPtr<IUnsecuredApartment>::createLocalInst(CLSID_UnsecuredApartment, IID_IUnsecuredApartment);
    if(!pAptmt)
    {
        KAPPS_CORE_WARNING() << "Unable to create apartment for WMI notifications";
        return;
    }

    // Create an SWbemDateTime object - we need this to convert the funky
    // datetime strings from WMI into a usable value.
    auto pWbemDateTime = WinComPtr<ISWbemDateTime>::createInprocInst(CLSID_SWbemDateTime, __uuidof(ISWbemDateTime));
    if(!pWbemDateTime)
    {
        // This hoses us because we can't verify process handles that we would
        // open.
        KAPPS_CORE_WARNING() << "Unable to create WbemDateTime converter";
        return;
    }

    WinComPtr<WbemEventSink> pNewSink{new WbemEventSink{*this, std::move(pWbemDateTime)}};
    pNewSink->AddRef();    // New object

    WinComPtr<IUnknown> pSinkStubUnk;
    HRESULT stubErr = pAptmt->CreateObjectStub(pNewSink.get(), pSinkStubUnk.receive());
    if(FAILED(stubErr) || !pSinkStubUnk)
    {
        KAPPS_CORE_WARNING() << "Unable to create WMI sink stub -" << stubErr;
        return;
    }

    auto pNewSinkStubSink = pSinkStubUnk.queryInterface<IWbemObjectSink>(IID_IWbemObjectSink);
    if(!pNewSinkStubSink)
    {
        KAPPS_CORE_WARNING() << "Stub failed to return sink interface";
        return;
    }

    // 'WITHIN 0.1' specifies the aggregation interval for these events.
    //
    // Although the doc discourages using intervals smaller than a few seconds,
    // a small interval is needed to correctly detect applications with
    // "launchers" like Opera.
    //
    // Opera's start menu shortcut points to 'launcher.exe', which starts up
    // 'opera.exe' (and probably checks for updates and such).  We have to
    // observe 'opera.exe' as a child of 'launcher.exe' for this to work.
    //
    // 'launcher.exe' is very short-lived though - some repeated measurements on
    // a Ryzen 7 VM show that it runs for ~0.5 seconds at the least, and
    // 'WITHIN 1' fails to observe it in time.  (We see the PPID, but the
    // process object is gone, so we can't figure out who the parent was.)
    // 'WITHIN 0.1' reliably detects it.
    //
    // We could add specific rules for Opera, but it's unlikely to be the only
    // app doing this.  If WITHIN 0.1 is not sufficient to detect some app, we
    // probably need to move this into kernel mode using
    // PsSetCreateProcessNotifyRoutineEx() in a driver.
    HRESULT queryErr = _pSvcs->ExecNotificationQueryAsync(
        _bstr_t{L"WQL"},
        _bstr_t{L"SELECT * FROM __InstanceCreationEvent WITHIN 0.1 WHERE "
                "TargetInstance ISA 'Win32_Process'"},
        WBEM_FLAG_SEND_STATUS,
        nullptr,
        pNewSinkStubSink);
    if(FAILED(queryErr))
    {
        KAPPS_CORE_WARNING() << "Unable to execute WMI query -" << queryErr;
        return;
    }

    KAPPS_CORE_INFO() << "Successfully activated monitor";
    _pSink = std::move(pNewSink);
    _pSinkStubSink = std::move(pNewSinkStubSink);
}

void WinAppMonitor::deactivate()
{
    if(!_pSinkStubSink && !_pSink)
        return; // Skip trace

    KAPPS_CORE_INFO() << "Deactivating monitor";
    if(_pSvcs && _pSinkStubSink)
        _pSvcs->CancelAsyncCall(_pSinkStubSink.get());
    // We don't know when the sink will actually be released, so we have to
    // disconnect it from WinAppMonitor.
    if(_pSink)
        _pSink->disconnect();
    _pSinkStubSink.reset();
    _pSink.reset();
}

void WinAppMonitor::setSplitTunnelRules(const std::unordered_set<std::wstring> &excludedExes,
                                        const std::unordered_set<std::wstring> &vpnOnlyExes)
{
    if(_tracker.setSplitTunnelRules(excludedExes, vpnOnlyExes))
    {
        activate();
    }
    else
    {
        deactivate();
    }
}

void WinAppMonitor::dump() const
{
    KAPPS_CORE_INFO() << "_pSvcs:" << !!_pSvcs << "- pSink:" << !!_pSink
        << "- pSinkStubSink:" << !!_pSinkStubSink;
    _tracker.dump();
}

}}
