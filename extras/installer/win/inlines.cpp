// Copyright (c) 2021 Private Internet Access, Inc.
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
#include "util.h"

// Some limited functionality is shared between the Windows installer and
// daemon, which is implemented in these inline files.
//
// The daemon and installer use different logging facilities and have few
// dependencies in common (the daemon uses Qt heavily, while the installer
// cannot use it at all).
//
// These inline implementations should generally only use WinAPI and standard
// libraries.  Logging is implemtented by defining TAP_LOG, etc. to LOG in the
// installer, and qInfo() in the daemon.
//
// In particular, for logging:
// - Do not use Microsoft-specific %S.  For wide-character strings; use %ls.
// - Logging std::[w]string must use c_str() to pass the string pointer.  Do not
//   pass the [w]string directly (this works in the installer but not in the
//   daemon).

#define TAP_LOG LOG
#include "tap.inl"

#define SERVICE_LOG LOG
#include "service.inl"

#define TUN_LOG LOG
#include "tun.inl"

#define SAFEMODE_LOG LOG
#include "safemode.inl"
