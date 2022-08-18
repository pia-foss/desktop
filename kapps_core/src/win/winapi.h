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

#pragma once
// Avoid including Windows and Winsock headers directly from other headers -
// they're sensitive to the exact order in which they're referenced, and they're
// sensitive to the NO* macros here.  Instead, include win/winapi.h to get these
// headers in the correct order.
//
// By default, kapps-core uses a relatively lean set of APIs suitable for our
// non-GUI components.  If you need GUI APIs too, define KAPPS_CORE_FULL_WINAPI
// before including this file or any other kapps-core headers (this can be
// specified on the compiler command line via build configuration).
//
// Unfortunately there's no better way to do this because we can't fully prevent
// leakage of the WinAPI headers from kapps-core headers, and Windows.h can't be
// included a second time to get additional APIs - that's just the way it is
// with Microsoft's single behemoth header :shrug:

// Avoid min(a,b) and max(a,b) macros, even with KAPPS_CORE_FULL_WINAPI
#define NOMINMAX

#ifndef KAPPS_CORE_FULL_WINAPI

// Reduce the amount of extra stuff defined by Windows.h
#define WIN32_LEAN_AND_MEAN

#define NOGDICAPMASKS      // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES  // VK_*
//#define NOWINMESSAGES      // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES        // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS       // SM_*
#define NOMENUS            // MF_*
//#define NOICONS            // IDI_*
#define NOKEYSTATES        // MK_*
#define NOSYSCOMMANDS      // SC_*
#define NORASTEROPS        // Binary and Tertiary raster ops
#define NOSHOWWINDOW       // SW_*
#define OEMRESOURCE        // OEM Resource values
#define NOATOM             // Atom Manager routines
#define NOCLIPBOARD        // Clipboard routines
#define NOCOLOR            // Screen colors
//#define NOCTLMGR           // Control and Dialog routines      // Needed by setupapi.h
#define NODRAWTEXT         // DrawText() and DT_*
#define NOGDI              // All GDI defines and routines
//#define NOKERNEL           // All KERNEL defines and routines
//#define NOUSER             // All USER defines and routines
//#define NONLS              // All NLS defines and routines
#define NOMB               // MB_* and MessageBox()
#define NOMEMMGR           // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE         // typedef METAFILEPICT
//#define NOMSG              // typedef MSG and associated routines
#define NOOPENFILE         // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL           // SB_* and scrolling routines
//#define NOSERVICE          // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND            // Sound driver routines
#define NOTEXTMETRIC       // typedef TEXTMETRIC and associated routines
#define NOWH               // SetWindowsHook and WH_*
//#define NOWINOFFSETS       // GWL_*, GCL_*, associated routines
#define NOCOMM             // COMM driver routines
#define NOKANJI            // Kanji support stuff.
#define NOHELP             // Help engine interface.
#define NOPROFILER         // Profiler interface.
#define NODEFERWINDOWPOS   // DeferWindowPos routines
#define NOMCX              // Modem Configuration Extensions

#endif

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <objbase.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <fwpvi.h>
#include <fwpmu.h>

#include <VersionHelpers.h>
