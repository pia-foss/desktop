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

#pragma once
#include <Windows.h>
#include <initializer_list>
#include <string>

// Miscellaneous functionality that is shared between the Windows installer and
// product.

// Determine the system temp directory (e.g. C:\Windows\Temp).
std::wstring getSystemTempPath();

// Helper type for arguments that just care about a string pointer (mind the
// lifetimes!)
struct utf16ptr
{
    LPCWSTR ptr;
    utf16ptr(LPCWSTR ptr) : ptr(ptr) {}
    utf16ptr(const std::wstring& ptr) : ptr(ptr.c_str()) {}
    operator LPCWSTR() const { return ptr; }
    bool empty() const { return !ptr || !*ptr; }
    size_t length() const { return wcslen(ptr); }
};

// Append an argument to a command line, quoting it if needed (Windows
// command-line quoting conventions).
void appendQuotedArgument(std::wstring& cmdline, utf16ptr arg, bool forceQuoting = false);

// Run a program with the given arguments and wait for it to exit.  Returns the
// process exit code, or -1 if the process could not be created (or its exit
// code could not be read).
int runProgram(utf16ptr executable, const std::initializer_list<utf16ptr> args = {}, utf16ptr cwd = nullptr);
