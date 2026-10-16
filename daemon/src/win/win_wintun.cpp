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

#include "win_wintun.h"
namespace WintunData
{
    const wchar_t *pOpenVPNName{L"PIA OpenVPN WinTUN Adapter"};
}

namespace WintunPools
{
    // WinTUN pool names used for OpenVPN connections.  Note that PIA ships
    // its own build of the WinTUN driver with PIA-specific IDs, so these
    // don't collide with other applications using WinTUN.
    //
    // (WireGuard does not use WinTUN - it uses the WireGuard-NT kernel
    // driver - so the "WireGuard" pool is unused.)
    const wchar_t *pWireGuardPool{L"WireGuard"};
    const wchar_t *pOpenVPNPool{L"OpenVPN"};
}

namespace
{
    void CALLBACK wintunLoggerCallback(WINTUN_LOGGER_LEVEL Level, 
        const DWORD64 Timestamp, const WCHAR *Message)
    {
        // The logger is already thread-safe, so we don't have to do any
        // additional serialization here.  (WinTUN can log from any thread.)
        switch(Level)
        {
            default:
            case WINTUN_LOG_INFO:
                qInfo() << "WinTUN:" << QString::fromWCharArray(Message);
                break;
            case WINTUN_LOG_WARN:
            case WINTUN_LOG_ERR:
                qWarning() << "WinTUN:" << QString::fromWCharArray(Message);
                break;
        }
    }
}


// Note that pia-wintun.dll is always 'pia-', not the current brand.  This
// is a driver, and drivers aren't supported for rebranding in our brand
// kit.
WintunModule::WintunModule()
    : _module{L"pia-wintun.dll"}
{
    qDebug() << "Loading WinTun module";
    loadProc(_pWTCreateAdapter, "WintunCreateAdapter");
    loadProc(_pWTOpenAdapter, "WintunOpenAdapter");
    loadProc(_pWTCloseAdapter, "WintunCloseAdapter");
    loadProc(_pWTGetAdapterLuid, "WintunGetAdapterLUID");
    loadProc(_pWTSetLogger, "WintunSetLogger");
    loadProc(_pWTDeleteDriver, "WintunDeleteDriver");
        
    // If anything failed, fail everything.  (In particular, this
    // ensures we don't create any WINTUN_ADAPTER_HANDLEs if we failed
    // to find WintunFreeAdapter().)
    if(!_pWTCreateAdapter || !_pWTOpenAdapter || !_pWTCloseAdapter ||
         !_pWTGetAdapterLuid || !_pWTSetLogger)
    {
        qWarning() << "Failed to load WinTUN entry points";
        _pWTCreateAdapter = nullptr;
        _pWTOpenAdapter = nullptr;
        _pWTCloseAdapter = nullptr;
        _pWTDeleteDriver = nullptr;
        _pWTGetAdapterLuid = nullptr;
        _pWTSetLogger = nullptr;
    }
    else
    {
        _pWTSetLogger(&wintunLoggerCallback);
        qDebug() << "WinTun loaded successfuly";
    }
}

std::shared_ptr<WintunAdapter> WintunModule::recreateAdapter(const WCHAR *Name) const
{
    return createAdapter(Name, L"PIA", nullptr);
}
