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

#ifndef UTIL_H
#define UTIL_H
#pragma once

#include "common.h"

// A lot of this file concerns string handling and converting between ASCII/UTF8
// and UTF16, and many of the functions/macros rely on the defined lifetime of
// C++ temporaries to handle temporary converted strings. For example, the 'UTF8'
// macro returns a temporary string pointer that only lives until the next
// sequence point (e.g. ; or && or || or ,) which is enough for it to be usable
// as a function argument.
//
// If strings need to be saved by the function, the function should always take
// a string object instead of a pointer.
//

// Localization functions
// Load a localized string resource as a std::wstring.
std::wstring loadString(UINT id);

// Helper type for arguments that just care about a string pointer (mind the lifetimes!)
struct utf8ptr
{
    LPCSTR ptr;
    utf8ptr(LPCSTR ptr) : ptr(ptr) {}
    utf8ptr(const std::string& ptr) : ptr(ptr.c_str()) {}
    operator LPCSTR() const { return ptr; }
    bool empty() const { return !ptr || !*ptr; }
    size_t length() const { return strlen(ptr); }
};
struct utf16ptr
{
    LPCWSTR ptr;
    utf16ptr(LPCWSTR ptr) : ptr(ptr) {}
    utf16ptr(const std::wstring& ptr) : ptr(ptr.c_str()) {}
    operator LPCWSTR() const { return ptr; }
    bool empty() const { return !ptr || !*ptr; }
    size_t length() const { return wcslen(ptr); }
};

static inline LPCSTR ptr(const std::string& str) { return str.c_str(); }
static inline LPCSTR ptr(LPCSTR str) { return str; }
static inline LPCWSTR ptr(const std::wstring& str) { return str.c_str(); }
static inline LPCWSTR ptr(LPCWSTR str) { return str; }

static inline LPCSTR utf8(LPCSTR str) { return str; }
std::string utf8(LPCWSTR str, size_t len);
static inline std::string utf8(LPCWSTR str) { return utf8(str, wcslen(str)); }
static inline const std::string& utf8(const std::string& str) { return str; }
static inline std::string utf8(const std::wstring& str) { return utf8(str.c_str(), str.size()); }
#define UTF8(str) ptr(utf8(str))

static inline LPCWSTR utf16(LPCWSTR str) { return str; }
std::wstring utf16(LPCSTR str, size_t len);
static inline std::wstring utf16(LPCSTR str) { return utf16(str, strlen(str)); }
static inline std::wstring utf16(const std::string& str) { return utf16(str.c_str(), str.size()); }
static inline const std::wstring& utf16(const std::wstring& str) { return str; }
#define UTF16(str) ptr(utf16(str))


// Some useful functions are defined, with some convenient extra features:
//
// - strprintf/wstrprintf: Standard printf-style functions that return a
//   std::string or std::wstring, but that can also take any string as an
//   argument (use %s as the specifier; string arguments will be converted
//   on the fly to the compatible type).
//
// - format: Similar to printf but uses FormatMessage and ordered %1..%n
//   format specifiers. Returns std::wstring, mainly for localized strings.
//

// Step 1 acceptance/conversion of printf arguments for UTF8
template<typename T> static inline std::enable_if_t<std::is_arithmetic<T>::value || std::is_enum<T>::value, T> utf8_printf_arg(T arg) { return arg; }
static inline LPCSTR utf8_printf_arg(utf8ptr arg) { return arg; }
static inline std::string utf8_printf_arg(utf16ptr arg) { return utf8(arg); }

// Step 1 acceptance/conversion of printf arguments for UTF16
template<typename T> static inline std::enable_if_t<std::is_arithmetic<T>::value || std::is_enum<T>::value, T> utf16_printf_arg(T arg) { return arg; }
static inline LPCWSTR utf16_printf_arg(utf16ptr arg) { return arg; }
static inline std::wstring utf16_printf_arg(utf8ptr arg) { return utf16(arg); }

// Step 2 extraction of value to put on stack for vararg function
template<typename T> static inline std::enable_if_t<std::is_arithmetic<T>::value || std::is_enum<T>::value, T> printf_value(T arg) { return arg; }
static inline LPCSTR printf_value(utf8ptr arg) { return arg; }
static inline LPCWSTR printf_value(utf16ptr arg) { return arg; }

std::string strprintf_impl(LPCSTR fmt, size_t count, ...);
template<typename... Args> static inline std::string strprintf(utf8ptr fmt, Args&&... args) { return strprintf_impl(fmt, sizeof...(args), printf_value(utf8_printf_arg(std::forward<Args>(args)))...); }
#define UTF8PRINTF(fmt, ...) strprintf(fmt,##__VA_ARGS__)

std::wstring wstrprintf_impl(LPCWSTR fmt, size_t count, ...);
template<typename... Args> static inline std::wstring wstrprintf(utf16ptr fmt, Args&&... args) { return wstrprintf_impl(fmt, sizeof...(args), printf_value(utf16_printf_arg(std::forward<Args>(args)))...); }
#define UTF16PRINTF(fmt, ...) wstrprintf(L"" fmt,##__VA_ARGS__)

std::wstring format_impl(LPCWSTR fmt, size_t count, ...);
template<typename... Args> static inline std::wstring format(utf16ptr fmt, Args&&... args) { return format_impl(fmt, sizeof...(args), printf_value(utf16_printf_arg(std::forward<Args>(args)))...); }
#define FORMAT(fmt, ...) format(L"" fmt,##__VA_ARGS__)

std::string vstrprintf(LPCSTR fmt, va_list ap);
std::wstring vwstrprintf(LPCWSTR fmt, va_list ap);
std::wstring vformat(LPCWSTR fmt, va_list ap);

bool stringStartsWithCaseInsensitive(const std::wstring& str, const std::wstring& prefix);

class Logger
{
public:
    Logger();
    ~Logger();
    static Logger* instance() { return _instance; }
    void write(utf8ptr line);
private:
    static Logger* _instance;
    HANDLE _logFile;
    CRITICAL_SECTION _logMutex;
};
#define LOG(fmt, ...) Logger::instance()->write(UTF8PRINTF(fmt,##__VA_ARGS__))

std::string getMessageName(UINT msg);

void appendQuotedArgument(std::wstring& cmdline, utf16ptr arg, bool forceQuoting = false);

int runProgram(utf16ptr executable, const std::initializer_list<utf16ptr> args = {}, utf16ptr cwd = nullptr);
bool launchProgramAsDesktopUser(utf16ptr executable, const std::initializer_list<utf16ptr> args = {}, utf16ptr cwd = nullptr);

// UIString loads a string resource identified by resource ID.
// Up to one format parameter can be passed currently; if given the message is
// formatted with this parameter.
// Multiple format parameters aren't supported right now - these really need
// positional argument support to work correctly with translations anyway.
class UIString
{
public:
    // Create a 'null' UIString.
    UIString() : UIString{0} {}
    // Create a UIString referring to a string resource.  id can be 0, which
    // creates a 'null' UIString.
    UIString(UINT id) : _id{id}, _hasParam{false} {}
    // Create a UIString referring to a format string with one parameter.
    UIString(UINT id, std::wstring param)
        : _id{id}, _hasParam{true}, _param{std::move(param)}
    {}
    UIString(UINT id, utf16ptr param)
        : _id{id}, _hasParam{true}, _param{param}
    {}
    // Create a UIString referring to a format string with an integer parameter.
    // Note that the format string should still use '%s' since the parameters
    // are stringified by the constructor.
    // (TODO: use positional parameters for better localization, don't include
    // formats in localized strings)
    UIString(UINT id, int param);

public:
    // Get the message as a wstring.
    std::wstring str() const;

    // Test if the UIString is null
    bool is_null() const {return !_id;}
    // Test if the UIString is non-null
    explicit operator bool() const {return _id;}

    bool operator==(const UIString &other) const;

private:
    UINT _id;
    bool _hasParam;
    std::wstring _param;
};

int messageBox(UIString text, UIString caption, UIString msgSuffix, UINT type, int silentResult = 0);

void checkAbort();

std::string readTextFile(utf16ptr path);
bool writeTextFile(utf16ptr path, utf8ptr text, DWORD creationFlags = CREATE_ALWAYS);

#endif // UTIL_H
