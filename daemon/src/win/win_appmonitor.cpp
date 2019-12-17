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
#line SOURCE_FILE("win/win_appmonitor.cpp")

#include "win_appmonitor.h"
#include "win_appmanifest.h"
#include "win_crypt.h"
#include "win/win_winrtloader.h"
#include <QMutex>
#include <QMutexLocker>
#include <Psapi.h>
#include <comutil.h>
#include <array>

#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "Wbemuuid.lib")

class SystemTimeTracer : public DebugTraceable<SystemTimeTracer>
{
public:
    SystemTimeTracer(const SYSTEMTIME &tm) : _tm{tm} {}

    void trace(QDebug &dbg) const
    {
        // Day of week is ignored (overspecified)
        dbg << QString::asprintf("%04d-%02d-%02d %02d:%02d:%02d.%03d",
                                 _tm.wYear, _tm.wMonth, _tm.wDay,
                                 _tm.wHour, _tm.wMinute, _tm.wSecond,
                                 _tm.wMilliseconds);
    }

private:
    const SYSTEMTIME &_tm;
};

class FileTimeTracer : public DebugTraceable<FileTimeTracer>
{
public:
    FileTimeTracer(const FILETIME &tm) : _tm{tm} {}

    void trace(QDebug &dbg) const
    {
        SYSTEMTIME sysTime;
        if(!::FileTimeToSystemTime(&_tm, &sysTime))
            dbg << "<invalid time" << _tm.dwHighDateTime << _tm.dwLowDateTime << ">";
        else
        {
            dbg << SystemTimeTracer{sysTime};
            // SystemTimeTracer loses the 100-nanosecond precision of
            // FILETIME.  This is used by WinAppMonitor for times with only
            // millisecond precision, but trace this part if it would somehow
            // be set.
            ULARGE_INTEGER timeLi;
            timeLi.HighPart = _tm.dwHighDateTime;
            timeLi.LowPart = _tm.dwLowDateTime;
            auto hundredNsec = timeLi.QuadPart % 10;
            if(hundredNsec)
            {
                QDebugStateSaver saver{dbg};
                dbg.nospace() << "(+" << hundredNsec << "00ns)";
            }
        }
    }

private:
    const FILETIME &_tm;
};

std::set<const AppIdKey*, PtrValueLess> WinAppTracker::getAppIds() const
{
    std::set<const AppIdKey*, PtrValueLess> ids;
    for(const auto &excludedApp : _excludedApps)
        ids.insert(&excludedApp.first);
    for(const auto &proc : _procData)
        ids.insert(&proc.second._procAppId);
    return ids;
}

bool WinAppTracker::setSplitTunnelRules(const QVector<SplitTunnelRule> &rules)
{
    // Try to load the link reader; this can fail.
    nullable_t<WinLinkReader> linkReader;
    try
    {
        linkReader.emplace();
    }
    catch(const Error &ex)
    {
        qWarning() << "Unable to resolve shell links -" << ex;
        // Eat error and continue
    }

    ExcludedApps_t addedApps;
    for(const auto &rule : rules)
    {
        // Ignore any future rule types
        if(rule.mode() != QStringLiteral("exclude"))
            continue;

        // UWP apps can have more than one target
        if(rule.path().startsWith(uwpPathPrefix))
        {
            auto installDirs = getWinRtLoader().adminGetInstallDirs(rule.path().mid(uwpPathPrefix.size()));
            // Inspect all of the install directories.  It's too late for us to
            // do anything if this fails, just grab all the executables we can
            // find.
            AppExecutables appExes{};
            for(const auto &dir : installDirs)
                inspectUwpAppManifest(dir, appExes);

            for(const auto &exe : appExes.executables)
            {
                // Definitely not a link for UWP apps - don't need a link reader
                // and don't need to capture the target path
                AppIdKey appId{nullptr, exe};
                if(appId.empty())
                    continue;
                auto emplaceResult = addedApps.emplace(std::move(appId), ExcludedAppData{});
                if(emplaceResult.second)
                    emplaceResult.first->second._targetPath = exe.toStdWString();
            }
        }
        else
        {
            std::wstring targetPath;
            // If the client gave us a link target, use that, otherwise use the app
            // path.  If an app path is a shortcut, we'll still try to resolve it,
            // but as described on SplitTunnelRule::linkTarget this may not work if
            // we are not in the correct user session.
            AppIdKey appId{linkReader.ptr(),
                           rule.linkTarget().isEmpty() ? rule.path() : rule.linkTarget(),
                           &targetPath};
            if(appId.empty())
                continue;   // Traced by AppIdKey
            auto emplaceResult = addedApps.emplace(std::move(appId), ExcludedAppData{});
            if(emplaceResult.second)
            {
                emplaceResult.first->second._targetPath = std::move(targetPath);
                // Don't load _signerNames yet, this could be expensive, do that if
                // this is actually a new app we don't already have.
            }
        }
    }

    for(auto itExistingApp = _excludedApps.begin(); itExistingApp != _excludedApps.end(); )
    {
        // Is the app still excluded?
        auto itAddedApp = addedApps.find(itExistingApp->first);
        if(itAddedApp == addedApps.end())
        {
            // No - remove it.  Remove all processes first
            for(const auto &pid : itExistingApp->second._runningProcesses)
                _procData.erase(pid);
            itExistingApp = _excludedApps.erase(itExistingApp);
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
        auto insertResult = _excludedApps.insert(std::move(appNode));
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
    dump();
    emit excludedAppIdsChanged();

    // It's only possible to match a child process if we have at least one
    // excluded app with at least one signer name.  If we have no apps (or they
    // are all unsigned), there's no need to monitor for child processes.
    for(const auto &excludedApp : _excludedApps)
    {
        if(!excludedApp.second._signerNames.empty())
            return true;
    }

    qInfo() << "No signed apps in exclusion list, do not need to monitor processes";
    return false;
}

void WinAppTracker::processCreated(WinHandle procHandle, Pid_t pid,
                                   WinHandle parentHandle, Pid_t parentPid)
{
    // If we are already excluding this process, there's nothing to do.  This
    // can happen if a process is created just as we start to scan for a new app
    // rule; we might observe the process just before receiving the "create"
    // event.
    if(_procData.count(pid))
    {
        qInfo() << "Already excluding PID" << pid << "(from parent"
            << parentPid << "), nothing to do";
        return;
    }

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
    if(_procData.count(parentPid) == 0)
    {
        AppIdKey parentAppId{getProcAppId(parentHandle)};
        auto itParentApp = _excludedApps.end();
        if(parentAppId)
            itParentApp = _excludedApps.find(parentAppId);
        // If the parent is an excluded app, and it has at least one signer
        // name, track it.  (Skip if there are no signer names, we would never
        // match any descendants, no need to track it.)
        if(itParentApp != _excludedApps.end() &&
            !itParentApp->second._signerNames.empty())
        {
            qInfo() << "Parent" << parentPid
                << "was not known but is excluded, add it to" << parentAppId;
            // The parent wasn't known and is an excluded app.  Add it and then
            // continue on below.  (We expect the child process to also match
            // as a descendant, but it's theoretically possible that it could
            // itself be a _different_ excluded app, so let the normal logic
            // handle that.
            //
            // It's possible this process has exited; if that's the case then
            // we'll immediately be notified and remove it after we finish
            // processing notifications.
            addExcludedProcess(itParentApp, std::move(parentHandle), parentPid,
                               std::move(parentAppId));
            dump();
        }
    }
    // Might have moved from parentHandle, can't use it after this.
    parentHandle = {};

    QString imgPath{getProcImagePath(procHandle)};
    AppIdKey appId;
    if(!imgPath.isEmpty())
    {
        // As in getProcAppId(), definitely not a link, no need to load a link
        // reader
        appId.reset(nullptr, imgPath);
    }
    // Can't do anything if we couldn't get this process's app ID.
    if(!appId)
    {
        qWarning() << "Couldn't get app ID for PID" << pid << "(from parent"
            << parentPid << ")";
        return;
    }

    // Check if it's an excluded app itself.  Do this before checking if it's a
    // descendant - it's possible it could be both if one excluded app launches
    // another.
    auto itExcludedApp = _excludedApps.find(appId);
    if(itExcludedApp != _excludedApps.end())
    {
        // Add it only if there are signer names for this app - if there aren't,
        // we'd never match any descendants, skip it.
        if(!itExcludedApp->second._signerNames.empty())
        {
            qInfo() << "PID" << pid << "is excluded app" << itExcludedApp->first;
            addExcludedProcess(itExcludedApp, std::move(procHandle), pid,
                               std::move(appId));
            dump();
            // This does not cause excluded app IDs to change (it's an explicit app
            // ID we already knew about)
        }
        // Otherwise, it's skipped, do _not_ check if it's a descendant of
        // another excluded app too.
    }
    // If it isn't, check if it's a descendant.
    else if((itExcludedApp = isAppDescendant(parentPid, imgPath)) != _excludedApps.end())
    {
        qInfo() << "PID" << pid << "(parent" << parentPid
            << ") is descendant of excluded app" << itExcludedApp->first;
        addExcludedProcess(itExcludedApp, std::move(procHandle), pid,
                           std::move(appId));
        dump();
        // App IDs have most likely changed.  It's possible they didn't if the new
        // app ID was already known, but it's not worth the tracking to try to
        // determine that here, let the firewall implementation handle it.
        emit excludedAppIdsChanged();
    }
}

void WinAppTracker::dump() const
{
    std::size_t processes{0};
    qInfo() << "===App tracker dump===";
    qInfo() << _excludedApps.size() << "excluded apps";
    for(auto itExclApp = _excludedApps.begin(); itExclApp != _excludedApps.end(); ++itExclApp)
    {
        const auto &app = *itExclApp;

        processes += app.second._runningProcesses.size();
        qInfo().nospace() << "[" << app.second._runningProcesses.size() << "] "
            << app.first;
        for(const auto &pid : app.second._runningProcesses)
        {
            auto itProcData = _procData.find(pid);
            if(itProcData == _procData.end())
            {
                qWarning() << " -" << pid << "**MISSING**";
                continue;
            }

            qInfo() << " -" << pid << itProcData->second._procAppId;
            if(itProcData->second._excludedAppPos != itExclApp)
            {
                qWarning() << "  ^ **MISMATCH**" << itProcData->second._excludedAppPos->first;
            }
        }
    }

    qInfo() << "Total" << processes << "processes";
    if(processes != _procData.size())
    {
        qWarning() << "**MISMATCH** Expected" << processes << "- have"
            << _procData.size();
    }
}

void WinAppTracker::addExcludedProcess(ExcludedApps_t::iterator itExcludedApp,
                                       WinHandle procHandle, Pid_t pid,
                                       AppIdKey appId)
{
    Q_ASSERT(itExcludedApp != _excludedApps.end()); // Ensured by caller
    Q_ASSERT(_procData.count(pid) == 0);    // Ensured by caller
    Q_ASSERT(itExcludedApp->second._runningProcesses.count(pid) == 0);    // Class invariant

    itExcludedApp->second._runningProcesses.emplace(pid);
    ProcessData &data = _procData[pid];
    data._procHandle = std::move(procHandle);
    data._pNotifier.reset(new QWinEventNotifier{data._procHandle.get()});
    data._procAppId = std::move(appId);
    data._excludedAppPos = itExcludedApp;

    connect(data._pNotifier.get(), &QWinEventNotifier::activated, this,
            &WinAppTracker::onProcessExited);
}

QString WinAppTracker::getProcImagePath(const WinHandle &procHandle) const
{
    // First try QueryFullProcessImageNameW().  This returns a Win32 path but
    // tends not to work if the process has exited.
    std::array<wchar_t, 2048> procImage{};
    DWORD len = procImage.size();
    if(::QueryFullProcessImageNameW(procHandle.get(), 0, procImage.data(),
                                    &len))
    {
        return QString::fromWCharArray(procImage.data(), static_cast<qsizetype>(len));
    }

    qInfo() << "Couldn't get Win32 executable path for PID"
        << ::GetProcessId(procHandle.get()) << SystemError{HERE};

    // Try ::GetProcessImageFileNameW().  This usually still works for processes
    // that have exited, but it returns an NT path - it's possible to get this
    // to pass this into Win32 APIs, but we use it as a last resort to minimize
    // risk of corner cases.
    len = ::GetProcessImageFileNameW(procHandle.get(), procImage.data(),
                                            procImage.size());
    // 0 indicates failure; procImage.size() indicates possible truncation
    if(len == 0 || len == procImage.size())
    {
        qWarning() << "Couldn't get NT executable path for PID"
            << ::GetProcessId(procHandle.get()) << SystemError{HERE};
        return {};
    }

    // We need a Win32 path to pass it through the API to get an app ID - just
    // prefix the NT path with "\\?\GLOBALROOT" so it'll be passed through to
    // the kernel.
    return QStringLiteral("\\\\?\\GLOBALROOT") + QString::fromWCharArray(procImage.data(), static_cast<qsizetype>(len));
}

AppIdKey WinAppTracker::getProcAppId(const WinHandle &procHandle) const
{
    QString imgPath{getProcImagePath(procHandle)};
    if(imgPath.isEmpty())
        return {};

    // It's definitely not a link, so no need to load a WinLinkReader here
    return AppIdKey{nullptr, imgPath};
}

auto WinAppTracker::isAppDescendant(Pid_t parentPid, const QString &imgPath)
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
        return _excludedApps.end(); // Not a descendant of anything we care about

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
        return _excludedApps.end();

    std::set<std::wstring> signerNames{winGetExecutableSigners(imgPath)};

    for(const auto &expectedSignerName : itProcData->second._excludedAppPos->second._signerNames)
    {
        // If a signer name matches, this is a valid descendant.
        if(signerNames.count(expectedSignerName) > 0)
            return itProcData->second._excludedAppPos;
    }

    // No signer name matched; not a valid descendant.
    return _excludedApps.end();
}

void WinAppTracker::onProcessExited(HANDLE procHandle)
{
    Pid_t pid = ::GetProcessId(procHandle);

    // If this PID isn't known, there's nothing to do.  It might be possible for
    // this to happen if a process exit races with an app removal, although this
    // depends on whether it's possible for a queued QWinEventNotifier signal to
    // be received after it has been destroyed.
    auto itProcData = _procData.find(pid);
    if(itProcData == _procData.end())
    {
        qInfo() << "Already removed PID" << pid;
        return;
    }

    qInfo() << "PID" << pid << "exited - app" << itProcData->second._procAppId
        << "- group" << itProcData->second._excludedAppPos->first;

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
    Q_ASSERT(itProcData->second._excludedAppPos != _excludedApps.end());    // Class invariant
    itProcData->second._excludedAppPos->second._runningProcesses.erase(pid);

    // Remove the process data.  Note that this closes procHandle, which is a
    // copy of the handle owned by the process data.
    _procData.erase(itProcData);

    // App IDs have most likely changed.  It's possible they didn't if there is
    // still a PID tracked with this same app ID, but again, let the firewall
    // handle that.
    dump();
    emit excludedAppIdsChanged();
}

// This WBEM event sink is used to implement WinAppMonitor.
class WinAppMonitor::WbemEventSink : public IWbemObjectSink
{
public:
    WbemEventSink(WinAppMonitor &monitor, WinComPtr<ISWbemDateTime> pWbemDateTime)
        : _refs{0}, _pWbemDateTime{std::move(pWbemDateTime)}, _pMonitor{&monitor}
    {
        Q_ASSERT(_pWbemDateTime);   // Ensured by caller
    }
    // Not copiable due to ref count
    WbemEventSink(const WbemEventSink &) = delete;
    WbemEventSink &operator=(const WbemEventSink &) = delete;
    // Destructor just sanity-checks reference count
    virtual ~WbemEventSink() {Q_ASSERT(_refs == 0);}

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
    QMutex _monitorMutex;
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
    QMutexLocker lock{&_monitorMutex};
    if(!_pMonitor)
        return WBEM_S_NO_ERROR; // Ignore notifications, disconnected

    qInfo() << "Notifications:" << lObjectCount;
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
            qInfo() << "Event sink call complete -" << hResult;
            break;
        case WBEM_STATUS_PROGRESS:
            qInfo() << "Event sink call progress -" << hResult;
            break;
        default:
            qInfo() << "Event sink call unexpected status" << lFlags << "-"
                << hResult;
            break;
    }
    return WBEM_S_NO_ERROR;
}

void WinAppMonitor::WbemEventSink::disconnect()
{
    QMutexLocker lock{&_monitorMutex};
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
        qWarning() << "Failed to read target from event -" << targetErr;
        return;
    }

    HRESULT convErr = ::VariantChangeType(&targetVar.get(), &targetVar.get(), 0,
                                          VT_UNKNOWN);
    if(FAILED(convErr) || !targetVar.get().punkVal)
    {
        qWarning() << "Failed to convert target to IUnknown -" << convErr;
        return;
    }

    WinComPtr<IUnknown> pTgtUnk{targetVar.get().punkVal};
    pTgtUnk->AddRef();

    auto pTgtObj = pTgtUnk.queryInterface<IWbemClassObject>(IID_IWbemClassObject);
    if(!pTgtObj)
    {
        qWarning() << "Failed to get object interface from target";
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
        qWarning() << "Failed to read" << QStringView{pPropName}
            << "from new process -" << pidErr;
        return 0;
    }

    HRESULT pidConvErr = ::VariantChangeType(&pidVar.get(), &pidVar.get(), 0,
                                             VT_UI4);
    if(FAILED(pidConvErr))
    {
        qWarning() << "Failed to convert" << QStringView{pPropName}
            << "to VT_UI4 -" << pidConvErr;
        return 0;
    }

    return V_UI4(&pidVar.get());
}

void WinAppMonitor::WbemEventSink::readNewProcess(IWbemClassObject &obj)
{
    Q_ASSERT(_pWbemDateTime);   // Class invariant

    // Get the process ID and parent process ID
    DWORD pid = readPidProp(obj, L"ProcessId");
    DWORD ppid = readPidProp(obj, L"ParentProcessId");

    if(!pid || !ppid)
        return; // Traced by readPidProp()

    qInfo() << "Parent" << ppid << "->" << pid;

    // Get the creation time
    WinComVariant createVar;
    HRESULT createErr = obj.Get(L"CreationDate", 0, createVar.receive(),
                                nullptr, nullptr);
    if(FAILED(createErr))
    {
        qWarning() << "Failed to read creation date from process" << pid
            << "-" << createErr;
        return;
    }
    HRESULT createConvErr = ::VariantChangeType(&createVar.get(), &createVar.get(),
                                                0, VT_BSTR);
    if(FAILED(createConvErr))
    {
        qWarning() << "Failed to convert creation date of process" << pid
            << "to VT_BSTR -" << createConvErr << "- type is"
            << V_VT(&createVar.get());
        return;
    }
    // Creation times are (bizarrely) encoded as strings
    HRESULT setTimeErr = _pWbemDateTime->put_Value(V_BSTR(&createVar.get()));
    if(FAILED(setTimeErr))
    {
        qWarning() << "Failed to parse creation date of process" << pid
            << "-" << setTimeErr << "- value is"
            << QStringView{V_BSTR(&createVar.get())};
        return;
    }
    BSTR createFileTimeBstrPtr{nullptr};
    HRESULT getTimeErr = _pWbemDateTime->GetFileTime(false, &createFileTimeBstrPtr);
    // Own the BSTR
    _bstr_t createFileTimeBstr{createFileTimeBstrPtr, false};
    createFileTimeBstrPtr = nullptr;
    if(FAILED(getTimeErr))
    {
        qWarning() << "Failed to get creation date of process" << pid
            << "-" << getTimeErr << "- value is"
            << QStringView{V_BSTR(&createVar.get())};
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
        qWarning() << "Unable to open process" << pid;
        return;
    }

    // Check if it's really the right process - the PID could have been reused
    FILETIME actualCreateTime, ignored1, ignored2, ignored3;
    if(!::GetProcessTimes(procHandle.get(), &actualCreateTime, &ignored1,
                          &ignored2, &ignored3))
    {
        qWarning() << "Unable to get creation time of" << pid;
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
        qWarning() << "Ignoring PID" << pid
            << "- PID was reused.  Expected creation time"
            << FileTimeTracer{createFileTime} << "- got"
            << FileTimeTracer{approxCreateTime};
    }

    // Open the parent process
    WinHandle parentHandle{::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, ppid)};
    // Check if this is really the right parent process, in case the PID was
    // reused
    FILETIME parentCreateTime;
    if(!::GetProcessTimes(parentHandle.get(), &parentCreateTime, &ignored1,
                          &ignored2, &ignored3))
    {
        qWarning() << "Unable to get creation time of" << ppid << "(parent of"
            << pid << ")";
        return;
    }

    // If the parent process is newer, the PID was reused.  (If they're the
    // same, we're unsure but we assume it's the correct one.)
    if(::CompareFileTime(&parentCreateTime, &actualCreateTime) > 0)
    {
        qWarning() << "Ignoring PID" << pid
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
    Q_ASSERT(_pMonitor);    // Checked by Indicate()
    _pMonitor->_tracker.processCreated(std::move(procHandle), pid,
                                       std::move(parentHandle), ppid);
}

WinAppMonitor::WinAppMonitor()
{
    connect(&_tracker, &WinAppTracker::excludedAppIdsChanged, this,
            &WinAppMonitor::excludedAppIdsChanged);

    // Create the WMI locator
    auto pLocator = WinComPtr<IWbemLocator>::createInprocInst(CLSID_WbemLocator, IID_IWbemLocator);
    if(!pLocator)
    {
        qWarning() << "Unable to create WMI locator";   // Error traced by WinComPtr
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
        qWarning() << "Unable to connect to WMI -" << connectErr;
        return;
    }

    HRESULT proxyErr = ::CoSetProxyBlanket(_pSvcs.get(),
                                           RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                                           nullptr, RPC_C_AUTHN_LEVEL_CALL,
                                           RPC_C_IMP_LEVEL_IMPERSONATE,
                                           nullptr, EOAC_NONE);
    if(FAILED(proxyErr))
    {
        qWarning() << "Unable to configure WMI proxy -" << connectErr;
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
        qWarning() << "Can't activate monitor, couldn't connect to WMI";
        return;
    }

    qInfo() << "Activating monitor";

    auto pAptmt = WinComPtr<IUnsecuredApartment>::createLocalInst(CLSID_UnsecuredApartment, IID_IUnsecuredApartment);
    if(!pAptmt)
    {
        qWarning() << "Unable to create apartment for WMI notifications";
        return;
    }

    // Create an SWbemDateTime object - we need this to convert the funky
    // datetime strings from WMI into a usable value.
    auto pWbemDateTime = WinComPtr<ISWbemDateTime>::createInprocInst(CLSID_SWbemDateTime, __uuidof(ISWbemDateTime));
    if(!pWbemDateTime)
    {
        // This hoses us because we can't verify process handles that we would
        // open.
        qWarning() << "Unable to create WbemDateTime converter";
        return;
    }

    WinComPtr<WbemEventSink> pNewSink{new WbemEventSink{*this, std::move(pWbemDateTime)}};
    pNewSink->AddRef();    // New object

    WinComPtr<IUnknown> pSinkStubUnk;
    HRESULT stubErr = pAptmt->CreateObjectStub(pNewSink.get(), pSinkStubUnk.receive());
    if(FAILED(stubErr) || !pSinkStubUnk)
    {
        qWarning() << "Unable to create WMI sink stub -" << stubErr;
        return;
    }

    auto pNewSinkStubSink = pSinkStubUnk.queryInterface<IWbemObjectSink>(IID_IWbemObjectSink);
    if(!pNewSinkStubSink)
    {
        qWarning() << "Stub failed to return sink interface";
        return;
    }

    HRESULT queryErr = _pSvcs->ExecNotificationQueryAsync(
        _bstr_t{L"WQL"},
        _bstr_t{L"SELECT * FROM __InstanceCreationEvent WITHIN 1 WHERE "
                "TargetInstance ISA 'Win32_Process'"},
        WBEM_FLAG_SEND_STATUS,
        nullptr,
        pNewSinkStubSink);
    if(FAILED(queryErr))
    {
        qWarning() << "Unable to execute WMI query -" << queryErr;
        return;
    }

    qInfo() << "Successfully activated monitor";
    _pSink = std::move(pNewSink);
    _pSinkStubSink = std::move(pNewSinkStubSink);
}

void WinAppMonitor::deactivate()
{
    if(!_pSinkStubSink && !_pSink)
        return; // Skip trace

    qInfo() << "Deactivating monitor";
    if(_pSvcs && _pSinkStubSink)
        _pSvcs->CancelAsyncCall(_pSinkStubSink.get());
    // We don't know when the sink will actually be released, so we have to
    // disconnect it from WinAppMonitor.
    if(_pSink)
        _pSink->disconnect();
    _pSinkStubSink.reset();
    _pSink.reset();
}

void WinAppMonitor::setSplitTunnelRules(const QVector<SplitTunnelRule> &rules)
{
    if(_tracker.setSplitTunnelRules(rules))
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
    qInfo() << "_pSvcs:" << !!_pSvcs << "- pSink:" << !!_pSink
        << "- pSinkStubSink:" << !!_pSinkStubSink;
    _tracker.dump();
}
