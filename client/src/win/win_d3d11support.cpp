// Copyright (c) 2020 Private Internet Access, Inc.
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
#line SOURCE_FILE("win/win_d3d11support.cpp")

#include "win_d3d11support.h"
#include "win/win_util.h"
#include "win/win_com.h"
#include "dxgi.h"
#include "d3d11.h"

#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "dxguid.lib")

struct WinD3dAdapters::Adapter
{
    int _index;
    QString _name;
    bool _isSoftware;
    HRESULT _featureLevelResult;
    D3D_FEATURE_LEVEL _featureLevel;
    bool _selected;
};

WinD3dAdapters::WinD3dAdapters()
    : _preferredAdapter{-1}, _pAdapters{new std::vector<Adapter>{}}
{
    // Qt has logic to try to use CreateDXGIFactory2() if available, but that
    // just adds an extra flag to choose whether to use the debug layer, which
    // we don't need.
    ProcAddress createDxgiFactory1Addr{QStringLiteral("dxgi.dll"),
                                       QByteArrayLiteral("CreateDXGIFactory1")};

    if(!createDxgiFactory1Addr.get())
    {
        qWarning() << "Could not load CreateDXGIFactory1(), assuming no D3D11 hardware support";
        return;
    }

    WinComPtr<IDXGIFactory1> pDxgiFactory;
    using CreateDXGIFactory1Ptr = HRESULT (WINAPI *)(REFIID, void**);
    auto pCreateDxgiFactory1 = reinterpret_cast<CreateDXGIFactory1Ptr>(createDxgiFactory1Addr.get());
    HRESULT factoryResult = pCreateDxgiFactory1(IID_IDXGIFactory1,
                                                pDxgiFactory.receiveV());
    if(FAILED(factoryResult) || !pDxgiFactory)
    {
        qWarning() << "Unable to create DXGI factory, assuming no D3D11 hardware support";
        return;
    }

    // Check all adapters for tracing; keep the first feature level 11 adapter.
    WinComPtr<IDXGIAdapter1> pAdapter;
    for(int i=0; pDxgiFactory->EnumAdapters1(i, pAdapter.receive()) == S_OK; ++i)
    {
        // Get some info about the device, for tracing
        DXGI_ADAPTER_DESC1 adapterDesc{};
        pAdapter->GetDesc1(&adapterDesc);
        // Get the feature level using D3D11CreateDevice().  No device
        // interfaces are actually created, since we don't give any interface
        // pointers to receive them.
        D3D_FEATURE_LEVEL featureLevel{};
        HRESULT deviceResult = D3D11CreateDevice(pAdapter.get(),
                                                 D3D_DRIVER_TYPE_UNKNOWN,
                                                 nullptr, 0, nullptr, 0,
                                                 D3D11_SDK_VERSION, nullptr,
                                                 &featureLevel, nullptr);
        _pAdapters->push_back({});
        auto &newAdapter = _pAdapters->back();
        newAdapter._index = i;
        newAdapter._name = QString::fromWCharArray(adapterDesc.Description);
        newAdapter._isSoftware = !!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE);
        newAdapter._featureLevelResult = deviceResult;
        if(SUCCEEDED(deviceResult))
        {
            newAdapter._featureLevel = featureLevel;
            // If it supports 11.0 and we haven't selected an adapter yet,
            // select this one.
            if(featureLevel >= D3D_FEATURE_LEVEL_11_0 &&
               !newAdapter._isSoftware && _preferredAdapter < 0)
            {
                newAdapter._selected = true;
                _preferredAdapter = i;
            }
        }
    }

    // Go ahead and trace now - it's harder to get this since it won't go into
    // the log, but on the off chance the client would crash due to D3D before
    // logging was initialized, we could still get this by running the client
    // manually.
    traceAdapters();
}

WinD3dAdapters::~WinD3dAdapters()
{
}

int WinD3dAdapters::getPreferredAdapter()
{
    return _preferredAdapter;
}

void WinD3dAdapters::traceAdapters()
{
    Q_ASSERT(_pAdapters);   // Class invariant
    qInfo() << "Found" << _pAdapters->size() << "D3D adapters, selected index"
        << _preferredAdapter;
    for(const auto &adapter : *_pAdapters)
    {
        qInfo() << "Adapter" << adapter._index << "-" << adapter._name;
        if(FAILED(adapter._featureLevelResult))
        {
            qInfo() << "  Unable to determine feature level - error"
                << adapter._featureLevelResult;
        }
        else
        {
            qInfo() << "  Supports feature level"
                << QString::number(adapter._featureLevel, 16);
        }
        if(adapter._isSoftware)
        {
            qInfo() << "  Software adapter";
        }
        if(adapter._selected)
        {
            qInfo() << "  * Selected this adapter";
        }
    }
}
