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
#line SOURCE_FILE("win_dnscachecontrol.cpp")

#include "win_dnscachecontrol.h"
#include "path.h"
#include "brand.h"
#include "daemon.h"
#include <Shlwapi.h>
#include <Psapi.h>
#include <VersionHelpers.h>
#include <array>

#pragma comment(lib, "Shlwapi.lib") // PathStripPathW()

namespace
{
    const std::wstring dnsCacheHklmKey{L"SYSTEM\\CurrentControlSet\\Services\\Dnscache"};
    const std::wstring imagePathValue{L"ImagePath"};
    const std::wstring typeValue{L"Type"};
    // The original Dnscache "image path" values (which is actually a command
    // line) that we expect to see on Win 10 or 8.1.  The trailing '-p' varies,
    // 8.1 and older 10 releases don't have it, but recent 10 does.
    //
    // We don't attempt to touch the service if it's set to anything else,
    // because it may mean the user has tampered with the service in a way that
    // we don't know how to handle.
    const std::array<std::wstring, 4> dnsCacheOrigImagePaths{
        L"%SystemRoot%\\system32\\svchost.exe -k NetworkService -p",
        L"%SystemRoot%\\System32\\svchost.exe -k NetworkService -p",
        L"%SystemRoot%\\system32\\svchost.exe -k NetworkService",
        L"%SystemRoot%\\System32\\svchost.exe -k NetworkService"
    };
    const DWORD dnsCacheOrigType = SERVICE_WIN32_SHARE_PROCESS;
    const DWORD dnsCacheStubType = SERVICE_WIN32_OWN_PROCESS;
    const std::wstring dnsCacheOrigBasename{L"svchost.exe"};

    const std::wstring dnsCacheStubBasename{BRAND_CODE L"-winsvcstub.exe"};

    const std::chrono::seconds serviceTransitionTimeout{10};

    // If we need to find and kill the NetworkService svchost, we have to match
    // the service command line against values from SCM - which have variables
    // expanded.  (See WinDnsCacheControl::killServiceIfNetworkService().)
    class ExpandedOrigPaths
    {
    public:
        ExpandedOrigPaths()
        {
            for(int i=0; i<_paths.size(); ++i)
            {
                _paths[i].resize(MAX_PATH+1); // Include trailing null char
                DWORD expandedSize = ::ExpandEnvironmentStringsW(dnsCacheOrigImagePaths[i].c_str(),
                                                                 _paths[i].data(),
                                                                 _paths[i].size());
                if(expandedSize > 0)
                    --expandedSize; // Remove trailing null char
                // The buffer should be long enough, but if in some extreme case it's
                // not, ExpandEnvironmentStringsW() would return the size actually
                // needed - don't extend the string without actual data there
                if(expandedSize < _paths[i].size())
                    _paths[i].resize(expandedSize);
            }
        }

    public:
        std::array<std::wstring, dnsCacheOrigImagePaths.size()> _paths;
    };
    const ExpandedOrigPaths expandedOrigPaths;
}

WinDnsCacheControl::WinDnsCacheControl()
    : _stubEnabled{false},
      // We can't get stop access to the Dnscache service; even as the
      // LOCAL SYSTEM user.  Only request the START and QUERY_INFORMATION
      // rights.
      _serviceState{L"Dnscache", SERVICE_START},
      _lastDnsCacheState{DnsCacheState::Down} // Service state isn't loaded yet
{
    _dnsCacheStubImagePath = (Path::BaseDir / BRAND_CODE "-winsvcstub.exe").str().toStdWString();

    LSTATUS openStatus = ::RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                                         dnsCacheHklmKey.c_str(), 0,
                                         KEY_QUERY_VALUE|KEY_SET_VALUE,
                                         _dnscacheRegKey.receive());
    if(!_dnscacheRegKey)
    {
        qWarning() << "Can't open Dnscache registry key -"
            << WinErrTracer{static_cast<DWORD>(openStatus)};
    }

    connect(&_serviceState, &WinServiceState::stateChanged, this,
            &WinDnsCacheControl::serviceStateChanged);

    _restartDelayTimer.setInterval(std::chrono::seconds(1));
    _restartDelayTimer.setSingleShot(true);
    connect(&_restartDelayTimer, &QTimer::timeout, this, &WinDnsCacheControl::startDnsCache);

    // If the system was left in an improper state (such as if the Dnscache
    // service was left pointing to the stub), we'll restore that when the
    // service state is loaded and serviceStateChanged() is called.
}

std::pair<std::wstring, DWORD> WinDnsCacheControl::getDnscacheRegKeys()
{
    std::wstring imagePath;
    imagePath.resize(MAX_PATH+1);
    // Size is in bytes for RegGetValueW()
    DWORD imagePathSize = imagePath.size() * sizeof(wchar_t);
    LSTATUS getStatus = ::RegGetValueW(_dnscacheRegKey.get(), nullptr,
                                       imagePathValue.c_str(),
                                       RRF_RT_REG_EXPAND_SZ|RRF_NOEXPAND,
                                       nullptr, imagePath.data(),
                                       &imagePathSize);

    if(getStatus != ERROR_SUCCESS)
    {
        qWarning() << "Can't stub Dnscache; unable to get existing image path:"
            << WinErrTracer{static_cast<DWORD>(getStatus)};
        return {};
    }

    // Returned size is in bytes and includes the trailing null char
    imagePathSize /= sizeof(wchar_t);
    // If the size was 0 for some reason, leave the string empty; otherwise
    // subtract 1 for the trailing null char
    if(imagePathSize >= 1)
        --imagePathSize;
    imagePath.resize(imagePathSize);

    DWORD type{};
    DWORD typeSize = sizeof(type);
    getStatus = ::RegGetValueW(_dnscacheRegKey.get(), nullptr,
                               typeValue.c_str(), RRF_RT_REG_DWORD,
                               nullptr, &type, &typeSize);
    if(getStatus != ERROR_SUCCESS || typeSize != sizeof(type))
    {
        qWarning() << "Can't stub Dnscache; unable to get existing type:"
            << WinErrTracer{static_cast<DWORD>(getStatus)};
        return {};
    }

    return {std::move(imagePath), type};
}

bool WinDnsCacheControl::setDnscacheRegExpandSz(const std::wstring &valueName,
                                                const std::wstring &value)
{
    LSTATUS setStatus = ::RegSetValueExW(_dnscacheRegKey.get(),
                                         valueName.c_str(), 0, REG_EXPAND_SZ,
                                         reinterpret_cast<const BYTE*>(value.c_str()),
                                         // Size is in bytes and includes trailing null
                                         (value.size()+1) * sizeof(wchar_t));
    if(setStatus != ERROR_SUCCESS)
    {
        qWarning() << "Can't set Dnscache" << valueName << "to" << value << "-"
            << WinErrTracer{static_cast<DWORD>(setStatus)};
        return false;
    }
    return true;
}

bool WinDnsCacheControl::setDnscacheRegKeys(const std::wstring &imagePath, DWORD type,
                                            const std::wstring &rollbackImagePath)
{
    if(!setDnscacheRegExpandSz(imagePathValue, imagePath))
        return false;   // Traced by setDnscacheRegExpandSz()

    LSTATUS setStatus = ::RegSetValueExW(_dnscacheRegKey.get(), typeValue.c_str(),
                                         0, REG_DWORD,
                                         reinterpret_cast<BYTE*>(&type),
                                         sizeof(type));
    if(setStatus != ERROR_SUCCESS)
    {
        qWarning() << "Can't set Dnscache type to" << type
            << "after setting path to" << imagePath << "-"
            << WinErrTracer{static_cast<DWORD>(setStatus)};

        // Revert the image path, nothing we can do if this fails
        // If no rollback was given, then this was reverting to the original
        // value anyway
        if(!rollbackImagePath.empty())
            setDnscacheRegExpandSz(imagePathValue, rollbackImagePath);
        return false;
    }

    return true;
}

std::wstring WinDnsCacheControl::getDnscacheRunningBasename(DWORD pid,
                                                            WinHandle &dnscacheProcess)
{
    if(!pid)
    {
        qInfo() << "Dnscache service is not running";
        // Nothing else to do, it's not running
        return {};
    }

    dnscacheProcess = WinHandle{::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_TERMINATE,
                                              FALSE, pid)};
    if(!dnscacheProcess)
    {
        WinErrTracer err{::GetLastError()};
        qInfo() << "Can't determine state of Dnscache; unable to open process"
            << pid << "-" << err;
        return {};
    }

    // Get the process's image name to figure out what it is
    std::wstring procImage;
    procImage.resize(2048);
    // GetProcessImageFileNameW() returns an NT path but works more reliably
    // than QueryFullProcessImageNameW(); in this case we only need the file
    // name anyway.
    DWORD nameLen = ::GetProcessImageFileNameW(dnscacheProcess.get(), procImage.data(),
                                               procImage.size());
    if(nameLen == 0)
    {
        WinErrTracer err{::GetLastError()};
        qInfo() << "Can't determine state of Dnscache; can't get name of process"
            << pid << "-" << err;
        return {};
    }

    ::PathStripPathW(procImage.data());
    procImage.resize(::wcslen(procImage.data()));
    return procImage;
}

void WinDnsCacheControl::serviceStateChanged(WinServiceState::State newState,
                                             DWORD newPid)
{
    WinHandle dnscacheProcess;
    std::wstring procImage = getDnscacheRunningBasename(newPid, dnscacheProcess);

    if(procImage.empty())
    {
        qInfo() << "Dnscache does not appear to be running (pid" << newPid << ")";
        _lastDnsCacheState = DnsCacheState::Down;
    }
    else if(procImage == dnsCacheOrigBasename)
    {
        qInfo() << "Dnscache is running normally with PID" << newPid;
        _lastDnsCacheState = DnsCacheState::Normal;
    }
    else if(procImage == dnsCacheStubBasename)
    {
        qInfo() << "Dnscache is stubbed with PID" << newPid;
        _lastDnsCacheState = DnsCacheState::Stubbed;

        std::wstring origPath = g_daemon->data().winDnscacheOriginalPath().toStdWString();

        // If we don't know the original Dnscache path somehow (possibly the
        // very first attempt failed, and the registry was saved but not
        // DaemonData), then we can't restore it - never apply an empty
        // value.
        if(origPath.empty())
        {
            qWarning() << "Cannot restore Dnscache image path - original value is not known";
        }
        else
        {
            // Since the stub is now up, restore the original settings for
            // Dnscache.  We don't need to bother checking that the ImagePath still
            // points to the stub; if it's already been reverted somehow then this
            // is a no-op.
            if(!setDnscacheRegKeys(origPath, dnsCacheOrigType, {}))
            {
                qWarning() << "Failed to restore Dnscache settings";
            }
        }
    }
    else
    {
        qWarning() << "Dnscache has unexpected name"
            << QString::fromWCharArray(procImage.data()) << "in PID" << newPid
            << "- cannot determine state";
        _lastDnsCacheState = DnsCacheState::Down;
    }

    // If the normal Dnscache service has somehow started again, and we were
    // supposed to be running the stub, stub it again.
    if(_lastDnsCacheState == DnsCacheState::Normal && _stubEnabled)
    {
        qWarning() << "Dnscache is running normally when supposed to be stubbed; stubbing it now";
        stubDnsCache(dnscacheProcess, newPid);
    }
    // If the stub Dnscache service has started up, but we already decided to
    // go back to the normal state, kill the stub so the service will start
    // again.
    else if(_lastDnsCacheState == DnsCacheState::Stubbed && !_stubEnabled)
    {
        qInfo() << "Stub is active when normal state is preferred, restart Dnscache"
            << newPid << "now";
        restartDnsCache(dnscacheProcess, newPid);
    }
    // If Dnscache is not running at all, and the stub is not enabled, check if
    // our stub path was somehow set and revert it if so.  This is for
    // resiliency in the rare event that the daemon could be terminated (power
    // loss, etc.) after applying the stub path, and in case the stub is unable
    // to actually start - we need to be able to recover the next time the
    // daemon starts.
    else if(_lastDnsCacheState == DnsCacheState::Down && !_stubEnabled)
    {
        std::wstring origPath = g_daemon->data().winDnscacheOriginalPath().toStdWString();
        auto existingValues = getDnscacheRegKeys();
        // As in stubDnsCache(), type is not checked; if we applied the stub
        // path we need to revert it even if the type was not applied or was
        // reverted.  We would have checked that the type was expected before
        // applying the stub path, so we can revert it.
        if(!origPath.empty() && existingValues.first == _dnsCacheStubImagePath)
        {
            qWarning() << "Dnscache stub path found from prior run - reverting";
            setDnscacheRegKeys(origPath, dnsCacheOrigType, {});
        }
    }
}

void WinDnsCacheControl::stubDnsCache(const WinHandle &dnscacheProcess,
                                      DWORD dnscachePid)
{
    if(!_dnscacheRegKey)
    {
        qWarning() << "Can't stub Dnscache, couldn't open registry key";
        return;
    }

    // Check that the values for DnsCache are the values we expect.  If they
    // aren't, don't do anything; we wouldn't be able to recover the values if a
    // failure occurred.
    auto existingValues = getDnscacheRegKeys();

    // Just in case we failed to revert it for some reason.
    // Type is not checked - on the extreme off chance that we set the path but
    // not the type, we still need to revert the path.  We would have checked
    // the existing type before setting the path, so we can revert the type too.
    if(existingValues.first == _dnsCacheStubImagePath)
    {
        qWarning() << "Dnscache path was already set to stub path unexpectedly:"
            << existingValues.first << existingValues.second;
        // Continue to stop it and restore the path anyway
    }
    else if(std::find(dnsCacheOrigImagePaths.begin(), dnsCacheOrigImagePaths.end(),
            existingValues.first) != dnsCacheOrigImagePaths.end() &&
            existingValues.second == dnsCacheOrigType)
    {
        qInfo() << "Found original Dnscache service image path/type:"
            << existingValues.first << existingValues.second;
        // Save this value so we can restore it, it's also persisted to disk in
        // case we would fail to restore it somehow - the daemon re-checks at
        // startup.  This generally only changes with an OS upgrade, so if the
        // daemon was to crash (or power failed, etc.) right after stubbing the
        // service, we should have a detected value here that we can restore.
        g_daemon->data().winDnscacheOriginalPath(QString::fromStdWString(existingValues.first));
    }
    else
    {
        // We don't know what to do with this value.  Although we could still
        // save it and proceed, it likely means that the user or another program
        // has tampered with the service, leave it alone to avoid breaking
        // anything.
        qWarning() << "Can't stop Dnscache; original image path/type is not an expected value:"
            << existingValues.first << existingValues.second;
        return;
    }

    // Apply the stub image path
    if(!setDnscacheRegKeys(_dnsCacheStubImagePath, dnsCacheStubType, existingValues.first))
    {
        qWarning() << "Failed to set Dnscache stub settings, not able to stop Dnscache";
        return;
    }

    qInfo() << "Terminating Dnscache process" << dnscachePid
        << "to restart with stub";

    restartDnsCache(dnscacheProcess, dnscachePid);
}

bool WinDnsCacheControl::killServiceIfNetworkService(ServiceHandle &scm,
                                                     std::vector<unsigned char> &configBuffer,
                                                     const ENUM_SERVICE_STATUS_PROCESSW &svcStatus)
{
    if(svcStatus.ServiceStatusProcess.dwCurrentState != SERVICE_RUNNING)
        return false;   // Don't care, need to find a running process

    QStringView svcName{svcStatus.lpServiceName};

    ServiceHandle svc{::OpenServiceW(scm, svcStatus.lpServiceName,
                                     SERVICE_QUERY_CONFIG)};
    if(!svc)
    {
        WinErrTracer error{::GetLastError()};
        qWarning() << "Unable to open service" << svcName << "-" << error;
        return false;
    }

    QUERY_SERVICE_CONFIGW *pSvcConfig = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(configBuffer.data());
    DWORD bufSizeNeeded{};
    if(!::QueryServiceConfigW(svc, pSvcConfig,
                              configBuffer.size(), &bufSizeNeeded))
    {
        WinErrTracer error{::GetLastError()};
        qWarning() << "Unable to get config of service" << svcName << "-" << error;
        return false;
    }

    auto isSharedNetworkSvc = [pSvcConfig](const std::wstring &val)
    {
        return val.compare(pSvcConfig->lpBinaryPathName) == 0;
    };
    if(std::find_if(expandedOrigPaths._paths.begin(), expandedOrigPaths._paths.end(),
                    isSharedNetworkSvc) == expandedOrigPaths._paths.end())
    {
        // Not the right service host
        return false;
    }

    DWORD pid = svcStatus.ServiceStatusProcess.dwProcessId;
    qInfo() << "Found running NetworkService service" << svcName << "with PID"
        << pid;

    WinHandle svcProcess{::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_TERMINATE,
                                       FALSE, pid)};
    if(!svcProcess)
    {
        WinErrTracer err{::GetLastError()};
        qInfo() << "Can't open process" << pid << "-" << err;
        return false;
    }

    qInfo() << "Terminating NetworkService service" << svcName << "PID"
        << pid << "to allow Dnscache to restart";
    if(!::TerminateProcess(svcProcess.get(), static_cast<UINT>(-1)))
    {
        WinErrTracer err{::GetLastError()};
        qInfo() << "Can't open process" << pid << "-" << err;
        return false;
    }

    // Found and terminated the process, we're done
    return true;
}

bool WinDnsCacheControl::killNetworkService()
{
    // To kill the NetworkService svchost instance, look for a running service
    // that's configured with the NetworkService image path used for Dnscache.
    ServiceHandle scm{::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE)};
    if(!scm)
    {
        WinErrTracer error{::GetLastError()};
        qWarning() << "Unable to open SCM to find NetworkService svchost -"
            << error;
        return false;
    }

    std::vector<unsigned char> statusBuffer;
    statusBuffer.resize(256*1024);  // Max size 256KB per doc
    std::vector<unsigned char> configBuffer;
    configBuffer.resize(8192);  // Max size 8K per doc for QueryServiceConfigW()
    DWORD bytesNeeded{}, servicesReturned{}, resumeHandle{};

    bool moreData{true};
    while(moreData)
    {
        bool enumResult = EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO,
                                                SERVICE_WIN32_SHARE_PROCESS,
                                                SERVICE_ACTIVE,
                                                statusBuffer.data(),
                                                statusBuffer.size(),
                                                &bytesNeeded, &servicesReturned,
                                                &resumeHandle, nullptr);
        WinErrTracer error{::GetLastError()};
        if(enumResult)
        {
            moreData = false;
        }
        else if(error.code() == ERROR_MORE_DATA)
        {
            moreData = true;
        }
        else
        {
            qWarning() << "Unable enumerate services -" << error;
            return false;
        }

        qInfo() << "Found" << servicesReturned << "active services";


        auto pSvcStatus = reinterpret_cast<const ENUM_SERVICE_STATUS_PROCESSW*>(statusBuffer.data());
        for(DWORD i=0; i<servicesReturned; ++i)
        {
            if(killServiceIfNetworkService(scm, configBuffer, pSvcStatus[i]))
                return true;
        }
    }

    qWarning() << "Did not find any running NetworkService to kill";
    return false;
}

void WinDnsCacheControl::startDnsCache()
{
    _restartDelayTimer.stop();

    _serviceState.startService()
        .timeout(serviceTransitionTimeout)
        ->notify(this, [this](const Error &err)
        {
            qInfo() << "Result of restarting Dnscache -" << err;
            // In some cases, when Dnscache is hosted by a shared
            // svchost with other services, attempting to restore the
            // normal service can fail with ERROR_INCOMPATIBLE_SERVICE_SID_TYPE.
            //
            // It is not clear why this exact error occurs, as the SID
            // types of the services were not changed, but reports
            // indicate that it can happen with other shared services
            // too and may be an internal bug in SCM.  On 8.1, it has
            // been observed to persist through a "shutdown" (which
            // actually hibernates/restores the kernel and SCM), but a
            // reboot fixes it.
            //
            // If this happens, we need to kill the existing
            // NetworkService svchost.exe so SCM can recover all the
            // services.
            //
            // Since this requires shared service hosts, it should only
            // happen on Win 10 <1703 or on >=1703 with <3.5 GiB of RAM
            if(err.code() == Error::Code::WinServiceIncompatibleSidType)
            {
                // Kill the NetworkService svchost instance.  If we do find it
                // and kill it, try to start again immediately.  Otherwise, wait
                // and try again.  This can fail if (for example) a service was
                // just starting that caused the conflict, and we're not able to
                // find it in the first attempt.
                if(killNetworkService())
                    startDnsCache();
                else
                    _restartDelayTimer.start();
            }
        });
}

void WinDnsCacheControl::restartDnsCache(const WinHandle &dnscacheProcess,
                                         DWORD dnscachePid)
{
    // Terminate the existing process - the system doesn't allow us to stop
    // Dnscache gracefully.
    //
    // On systems with < 3.5GiB of memory, Windows may use the same svchost
    // instance to host other services too, which unfortunately means those
    // services will also go down.  Generally this appears to be low impact
    // (those services aren't critical and will be restarted), and there is no
    // other way to do this.
    //
    // On systems with >= 3.5GiB of memory, Windows now splits up services
    // instead of sharing them:
    //   https://docs.microsoft.com/en-us/windows/application-management/svchost-service-refactoring
    ::TerminateProcess(dnscacheProcess.get(), static_cast<UINT>(-1));

    _restartDelayTimer.stop();

    // Wait for it to stop, and then try to start it immediately.
    //
    // If we're starting the stub, we don't really need the service to be up,
    // since it's just a stub, but this allows us to restore the normal
    // ImagePath as soon as possible (which is done in serviceStateChanged()
    // when we see that the stub is up).
    //
    // If we're starting the real service, we do need it to be up ASAP, since
    // it's needed to connect with WireGuard or the OpenVPN static (non-DHCP)
    // method.
    _serviceState.waitForStop()
        .timeout(serviceTransitionTimeout)
        ->notify(this, [this, dnscachePid](const Error &err)
        {
            qInfo() << "Result of waiting for Dnscache" << dnscachePid
                << "to stop after termination -" << err
                << "- restarting now";
            // Try to start the service.
            startDnsCache();
        });
}

void WinDnsCacheControl::disableDnsCache()
{
    if(_stubEnabled)
        return; // Nothing to do

    _stubEnabled = true;
    qInfo() << "Enabling Dnscache stub service to disable DNS cache";
    // Rerun the state change logic to sync up to the new _stubEnabled state.
    // This also re-detects _lastDnsCacheState, but that's fine.
    serviceStateChanged(_serviceState.lastState(), _serviceState.lastPid());
}

void WinDnsCacheControl::restoreDnsCache()
{
    if(!_stubEnabled)
        return; // Nothing to do

    _stubEnabled = false;
    qInfo() << "Restoring Dnscache service";
    // As in disable(), just rerun the state change logic.
    serviceStateChanged(_serviceState.lastState(), _serviceState.lastPid());
}
