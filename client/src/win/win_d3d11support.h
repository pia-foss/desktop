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
#line HEADER_FILE("win/win_d3d11support.h")

#ifndef WIN_D3D11SUPPORT_H
#define WIN_D3D11SUPPORT_H

// Enumerate the D3D adapters on the system and select one that supports feature
// level 11.0.  Qt Quick's D3D11 backend requires an adapter supporting 11.0,
// but isn't smart enough to actually pick one - it uses the default feature
// level list, which will fall back as far as 9.1.
//
// Only the index is really needed to set up the Qt Quick backend, but this
// stores information about all the adapters observed so it can be traced (this
// detection has to occur before Logger is set up, so we have to trace later,
// see main.cpp).
class WinD3dAdapters
{
private:
    // Adapter information for later tracing - contains D3D types, so the
    // definition is not provided here.
    struct Adapter;

public:
    WinD3dAdapters();
    ~WinD3dAdapters();  // Out-of-line due to Adapter struct

public:
    // Returns the adapter index of the preferred hardware adapter if one is
    // found, or -1 otherwise (use WARP in that case).
    int getPreferredAdapter();

    // Trace information about the adapters that were observed
    void traceAdapters();

private:
    int _preferredAdapter;
    // Information about the adapters - stored to trace later with
    // traceAdapters().
    std::unique_ptr<std::vector<Adapter>> _pAdapters;   // Always valid
};

#endif
