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

#include "util.h"
#include "brand.h"
#include <cassert>

#include <cctype>
#include <cwctype>

Logger* Logger::_instance;

namespace
{
    const std::wstring longBrandLegacy{L"Private Internet Access"};
    const std::wstring longBrandTemplate{L"{{BRAND}}"};
    const std::wstring shortBrandLegacy{L"PIA"};
    const std::wstring shortBrandTemplate{L"{{BRAND_SHORT}}"};

    const std::wstring longBrand{L"" BRAND_NAME};
    const std::wstring shortBrand{L"" BRAND_SHORT_NAME};
}

void stringReplace(std::wstring &str, const std::wstring &from, const std::wstring &to)
{
    std::size_t searchPos = 0;
    while(true)
    {
        searchPos = str.find(from, searchPos);
        if(searchPos == std::wstring::npos)
            break;

        str.replace(searchPos, from.size(), to);
        searchPos += to.size();
    }
}

std::wstring loadString(UINT id)
{
    LPWSTR ptr;
    int len = LoadStringW(NULL, id, reinterpret_cast<LPWSTR>(&ptr), 0);
    if(len < 0)
        return {};
    std::wstring str{ptr, static_cast<std::size_t>(len)};

    // Apply brand replacements
    stringReplace(str, longBrandLegacy, longBrand);
    stringReplace(str, longBrandTemplate, longBrand);
    stringReplace(str, shortBrandLegacy, shortBrand);
    stringReplace(str, shortBrandTemplate, shortBrand);
    return str;
}

std::string utf8(LPCWSTR str, size_t len)
{
    std::string result;
    result.resize(WideCharToMultiByte(CP_UTF8, 0, str, (int)len, NULL, 0, NULL, NULL), 0);
    if (result.size())
        result.resize(WideCharToMultiByte(CP_UTF8, 0, str, (int)len, &result[0], (int)result.size(), NULL, NULL), 0);
    return result;
}

std::wstring utf16(LPCSTR str, size_t len)
{
    std::wstring result;
    result.resize(MultiByteToWideChar(CP_UTF8, 0, str, (int)len, NULL, 0), 0);
    if (result.size())
        result.resize(MultiByteToWideChar(CP_UTF8, 0, str, (int)len, &result[0], (int)result.size()), 0);
    return result;
}

template<typename Char>
static bool printf_check(const Char* fmt, size_t count)
{
    if (!fmt) return false;
    size_t index = 0;
    for (const Char* p = fmt; *p; p++)
    {
        if (*p == '%')
        {
            if (!*++p) return false;
            if (*p != '%')
            {
                if (++index > count) return false;
            }
        }
    }
    return true;
}

static bool format_check(LPCWSTR fmt, size_t count)
{
    if (!fmt) return false;
    for (const wchar_t* p = fmt; *p; p++)
    {
        if (*p == '%')
        {
            ++p;
            if (!*p || *p == '0') return false;
            if (*p < '1' || *p > '9') continue;
            int number = *p - '0';
            if (p[1] >= '0' && p[1] <= '9')
            {
                ++p;
                number = number * 10 + (*p - '0');
                if (p[1] >= '0' && p[1] <= '9') return false;
            }
            if (number > count) return false;
        }
    }
    return true;
}

std::string vstrprintf(LPCSTR fmt, va_list ap)
{
    std::string result;
    int len = _vscprintf(fmt, ap);
    result.resize(len + 1);
    result.resize(_vsnprintf(&result[0], result.size(), fmt, ap));
    return result;
}

std::wstring vwstrprintf(LPCWSTR fmt, va_list ap)
{
    std::wstring result;
    int len = _vscwprintf(fmt, ap);
    result.resize(len + 1);
    result.resize(_vsnwprintf(&result[0], result.size(), fmt, ap));
    return result;
}

std::string strprintf_impl(LPCSTR fmt, size_t count, ...)
{
    if (!printf_check(fmt, count))
    {
        LOG("Bad printf string: %s", fmt);
        return std::string();
    }
    va_list ap;
    va_start(ap, count);
    std::string result = vstrprintf(fmt, ap);
    va_end(ap);
    return result;
}

std::wstring wstrprintf_impl(LPCWSTR fmt, size_t count, ...)
{
    if (!printf_check(fmt, count))
    {
        LOG("Bad printf string: %s", fmt);
        return std::wstring();
    }
    va_list ap;
    va_start(ap, count);
    std::wstring result = vwstrprintf(fmt, ap);
    va_end(ap);
    return result;
}

bool stringStartsWithCaseInsensitive(const std::wstring& str, const std::wstring& prefix)
{
    if (str.size() < prefix.size()) return false;
    return str.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), str.begin(), [](wchar_t a, wchar_t b) { return a == b || std::towupper(a) == std::towupper(b); });
}

Logger::Logger()
    : _logFile(INVALID_HANDLE_VALUE)
{
    InitializeCriticalSection(&_logMutex);

    if (!g_userTempPath.empty())
    {
        std::wstring path = g_userTempPath;
        if (path.back() != '\\')
            path.push_back('\\');
        path += L"" BRAND_CODE "-install.log";
        _logFile = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    _instance = this;
}

Logger::~Logger()
{
    _instance = nullptr;

    if (_logFile != INVALID_HANDLE_VALUE)
        CloseHandle(_logFile);

    DeleteCriticalSection(&_logMutex);
}

void Logger::write(utf8ptr line)
{
    EnterCriticalSection(&_logMutex);
#ifdef _DEBUG
    OutputDebugStringA(line);
    //OutputDebugStringA("\n");
#endif
    if (_logFile != INVALID_HANDLE_VALUE)
    {
        DWORD len = strlen(line), written;
        DWORD pos = SetFilePointer(_logFile, 0, NULL, FILE_END);
        LockFile(_logFile, pos, 0, len + 2, 0);
        SYSTEMTIME t;
        GetSystemTime(&t);
        char timestamp[32]; // 26 characters used
        sprintf(timestamp, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
        WriteFile(_logFile, timestamp, 26, &written, NULL);
        WriteFile(_logFile, line.ptr, len, &written, NULL);
        WriteFile(_logFile, "\r\n", 1, &written, NULL);
        UnlockFile(_logFile, pos, 0, len + 2, 0);
        FlushFileBuffers(_logFile);
    }
    LeaveCriticalSection(&_logMutex);
}

std::string getMessageName(UINT msg)
{
    const char* name = "<unknown>";
#ifdef _DEBUG
#define CASE_MSG(msg) case msg: name = #msg; break;
    switch(msg)
    {
    CASE_MSG(WM_NULL)
    CASE_MSG(WM_CREATE)
    CASE_MSG(WM_DESTROY)
    CASE_MSG(WM_MOVE)
    CASE_MSG(WM_SIZE)
    CASE_MSG(WM_ACTIVATE)
    CASE_MSG(WM_SETFOCUS)
    CASE_MSG(WM_KILLFOCUS)
    CASE_MSG(WM_ENABLE)
    CASE_MSG(WM_SETREDRAW)
    CASE_MSG(WM_SETTEXT)
    CASE_MSG(WM_GETTEXT)
    CASE_MSG(WM_GETTEXTLENGTH)
    CASE_MSG(WM_PAINT)
    CASE_MSG(WM_CLOSE)
    CASE_MSG(WM_QUERYENDSESSION)
    CASE_MSG(WM_QUIT)
    CASE_MSG(WM_QUERYOPEN)
    CASE_MSG(WM_ERASEBKGND)
    CASE_MSG(WM_SYSCOLORCHANGE)
    CASE_MSG(WM_ENDSESSION)
    CASE_MSG(WM_SHOWWINDOW)
    //CASE_MSG(WM_CTLCOLOR)
    CASE_MSG(WM_WININICHANGE)
    CASE_MSG(WM_DEVMODECHANGE)
    CASE_MSG(WM_ACTIVATEAPP)
    CASE_MSG(WM_FONTCHANGE)
    CASE_MSG(WM_TIMECHANGE)
    CASE_MSG(WM_CANCELMODE)
    CASE_MSG(WM_SETCURSOR)
    CASE_MSG(WM_MOUSEACTIVATE)
    CASE_MSG(WM_CHILDACTIVATE)
    CASE_MSG(WM_QUEUESYNC)
    CASE_MSG(WM_GETMINMAXINFO)
    CASE_MSG(WM_PAINTICON)
    CASE_MSG(WM_ICONERASEBKGND)
    CASE_MSG(WM_NEXTDLGCTL)
    CASE_MSG(WM_SPOOLERSTATUS)
    CASE_MSG(WM_DRAWITEM)
    CASE_MSG(WM_MEASUREITEM)
    CASE_MSG(WM_DELETEITEM)
    CASE_MSG(WM_VKEYTOITEM)
    CASE_MSG(WM_CHARTOITEM)
    CASE_MSG(WM_SETFONT)
    CASE_MSG(WM_GETFONT)
    CASE_MSG(WM_SETHOTKEY)
    CASE_MSG(WM_GETHOTKEY)
    CASE_MSG(WM_QUERYDRAGICON)
    CASE_MSG(WM_COMPAREITEM)
    CASE_MSG(WM_GETOBJECT)
    CASE_MSG(WM_COMPACTING)
    CASE_MSG(WM_COMMNOTIFY)
    CASE_MSG(WM_WINDOWPOSCHANGING)
    CASE_MSG(WM_WINDOWPOSCHANGED)
    CASE_MSG(WM_POWER)
    //CASE_MSG(WM_COPYGLOBALDATA)
    CASE_MSG(WM_COPYDATA)
    CASE_MSG(WM_CANCELJOURNAL)
    CASE_MSG(WM_NOTIFY)
    CASE_MSG(WM_INPUTLANGCHANGEREQUEST)
    CASE_MSG(WM_INPUTLANGCHANGE)
    CASE_MSG(WM_TCARD)
    CASE_MSG(WM_HELP)
    CASE_MSG(WM_USERCHANGED)
    CASE_MSG(WM_NOTIFYFORMAT)
    CASE_MSG(WM_CONTEXTMENU)
    CASE_MSG(WM_STYLECHANGING)
    CASE_MSG(WM_STYLECHANGED)
    CASE_MSG(WM_DISPLAYCHANGE)
    CASE_MSG(WM_GETICON)
    CASE_MSG(WM_SETICON)
    CASE_MSG(WM_NCCREATE)
    CASE_MSG(WM_NCDESTROY)
    CASE_MSG(WM_NCCALCSIZE)
    CASE_MSG(WM_NCHITTEST)
    CASE_MSG(WM_NCPAINT)
    CASE_MSG(WM_NCACTIVATE)
    CASE_MSG(WM_GETDLGCODE)
    CASE_MSG(WM_SYNCPAINT)
    CASE_MSG(WM_UAHDESTROYWINDOW)
    CASE_MSG(WM_UAHDRAWMENU)
    CASE_MSG(WM_UAHDRAWMENUITEM)
    CASE_MSG(WM_UAHINITMENU)
    CASE_MSG(WM_UAHMEASUREMENUITEM)
    CASE_MSG(WM_UAHNCPAINTMENUPOPUP)
    CASE_MSG(WM_NCMOUSEMOVE)
    CASE_MSG(WM_NCLBUTTONDOWN)
    CASE_MSG(WM_NCLBUTTONUP)
    CASE_MSG(WM_NCLBUTTONDBLCLK)
    CASE_MSG(WM_NCRBUTTONDOWN)
    CASE_MSG(WM_NCRBUTTONUP)
    CASE_MSG(WM_NCRBUTTONDBLCLK)
    CASE_MSG(WM_NCMBUTTONDOWN)
    CASE_MSG(WM_NCMBUTTONUP)
    CASE_MSG(WM_NCMBUTTONDBLCLK)
    CASE_MSG(WM_NCXBUTTONDOWN)
    CASE_MSG(WM_NCXBUTTONUP)
    CASE_MSG(WM_NCXBUTTONDBLCLK)
    CASE_MSG(WM_INPUT)
    CASE_MSG(WM_KEYDOWN)
    CASE_MSG(WM_KEYUP)
    CASE_MSG(WM_CHAR)
    CASE_MSG(WM_DEADCHAR)
    CASE_MSG(WM_SYSKEYDOWN)
    CASE_MSG(WM_SYSKEYUP)
    CASE_MSG(WM_SYSCHAR)
    CASE_MSG(WM_SYSDEADCHAR)
    CASE_MSG(WM_UNICHAR)
    //CASE_MSG(WM_WNT_CONVERTREQUESTEX)
    //CASE_MSG(WM_CONVERTREQUEST)
    //CASE_MSG(WM_CONVERTRESULT)
    //CASE_MSG(WM_INTERIM)
    CASE_MSG(WM_IME_STARTCOMPOSITION)
    CASE_MSG(WM_IME_ENDCOMPOSITION)
    CASE_MSG(WM_IME_COMPOSITION)
    CASE_MSG(WM_INITDIALOG)
    CASE_MSG(WM_COMMAND)
    CASE_MSG(WM_SYSCOMMAND)
    CASE_MSG(WM_TIMER)
    CASE_MSG(WM_HSCROLL)
    CASE_MSG(WM_VSCROLL)
    CASE_MSG(WM_INITMENU)
    CASE_MSG(WM_INITMENUPOPUP)
    //CASE_MSG(WM_SYSTIMER)
    CASE_MSG(WM_MENUSELECT)
    CASE_MSG(WM_MENUCHAR)
    CASE_MSG(WM_ENTERIDLE)
    CASE_MSG(WM_MENURBUTTONUP)
    CASE_MSG(WM_MENUDRAG)
    CASE_MSG(WM_MENUGETOBJECT)
    CASE_MSG(WM_UNINITMENUPOPUP)
    CASE_MSG(WM_MENUCOMMAND)
    CASE_MSG(WM_CHANGEUISTATE)
    CASE_MSG(WM_UPDATEUISTATE)
    CASE_MSG(WM_QUERYUISTATE)
    CASE_MSG(WM_CTLCOLORMSGBOX)
    CASE_MSG(WM_CTLCOLOREDIT)
    CASE_MSG(WM_CTLCOLORLISTBOX)
    CASE_MSG(WM_CTLCOLORBTN)
    CASE_MSG(WM_CTLCOLORDLG)
    CASE_MSG(WM_CTLCOLORSCROLLBAR)
    CASE_MSG(WM_CTLCOLORSTATIC)
    CASE_MSG(WM_MOUSEMOVE)
    CASE_MSG(WM_LBUTTONDOWN)
    CASE_MSG(WM_LBUTTONUP)
    CASE_MSG(WM_LBUTTONDBLCLK)
    CASE_MSG(WM_RBUTTONDOWN)
    CASE_MSG(WM_RBUTTONUP)
    CASE_MSG(WM_RBUTTONDBLCLK)
    CASE_MSG(WM_MBUTTONDOWN)
    CASE_MSG(WM_MBUTTONUP)
    CASE_MSG(WM_MBUTTONDBLCLK)
    CASE_MSG(WM_MOUSELAST)
    CASE_MSG(WM_MOUSEWHEEL)
    CASE_MSG(WM_XBUTTONDOWN)
    CASE_MSG(WM_XBUTTONUP)
    CASE_MSG(WM_XBUTTONDBLCLK)
    CASE_MSG(WM_PARENTNOTIFY)
    CASE_MSG(WM_ENTERMENULOOP)
    CASE_MSG(WM_EXITMENULOOP)
    CASE_MSG(WM_NEXTMENU)
    CASE_MSG(WM_SIZING)
    CASE_MSG(WM_CAPTURECHANGED)
    CASE_MSG(WM_MOVING)
    CASE_MSG(WM_POWERBROADCAST)
    CASE_MSG(WM_DEVICECHANGE)
    CASE_MSG(WM_MDICREATE)
    CASE_MSG(WM_MDIDESTROY)
    CASE_MSG(WM_MDIACTIVATE)
    CASE_MSG(WM_MDIRESTORE)
    CASE_MSG(WM_MDINEXT)
    CASE_MSG(WM_MDIMAXIMIZE)
    CASE_MSG(WM_MDITILE)
    CASE_MSG(WM_MDICASCADE)
    CASE_MSG(WM_MDIICONARRANGE)
    CASE_MSG(WM_MDIGETACTIVE)
    CASE_MSG(WM_MDISETMENU)
    CASE_MSG(WM_ENTERSIZEMOVE)
    CASE_MSG(WM_EXITSIZEMOVE)
    CASE_MSG(WM_DROPFILES)
    CASE_MSG(WM_MDIREFRESHMENU)
    //CASE_MSG(WM_IME_REPORT)
    CASE_MSG(WM_IME_SETCONTEXT)
    CASE_MSG(WM_IME_NOTIFY)
    CASE_MSG(WM_IME_CONTROL)
    CASE_MSG(WM_IME_COMPOSITIONFULL)
    CASE_MSG(WM_IME_SELECT)
    CASE_MSG(WM_IME_CHAR)
    CASE_MSG(WM_IME_REQUEST)
    CASE_MSG(WM_IME_KEYDOWN)
    CASE_MSG(WM_IME_KEYUP)
    CASE_MSG(WM_NCMOUSEHOVER)
    CASE_MSG(WM_MOUSEHOVER)
    CASE_MSG(WM_NCMOUSELEAVE)
    CASE_MSG(WM_MOUSELEAVE)
    CASE_MSG(WM_CUT)
    CASE_MSG(WM_COPY)
    CASE_MSG(WM_PASTE)
    CASE_MSG(WM_CLEAR)
    CASE_MSG(WM_UNDO)
    CASE_MSG(WM_RENDERFORMAT)
    CASE_MSG(WM_RENDERALLFORMATS)
    CASE_MSG(WM_DESTROYCLIPBOARD)
    CASE_MSG(WM_DRAWCLIPBOARD)
    CASE_MSG(WM_PAINTCLIPBOARD)
    CASE_MSG(WM_VSCROLLCLIPBOARD)
    CASE_MSG(WM_SIZECLIPBOARD)
    CASE_MSG(WM_ASKCBFORMATNAME)
    CASE_MSG(WM_CHANGECBCHAIN)
    CASE_MSG(WM_HSCROLLCLIPBOARD)
    CASE_MSG(WM_QUERYNEWPALETTE)
    CASE_MSG(WM_PALETTEISCHANGING)
    CASE_MSG(WM_PALETTECHANGED)
    CASE_MSG(WM_HOTKEY)
    CASE_MSG(WM_POPUPSYSTEMMENU)
    CASE_MSG(WM_PRINT)
    CASE_MSG(WM_PRINTCLIENT)
    CASE_MSG(WM_APPCOMMAND)
    //CASE_MSG(WM_HANDHELDFIRST)
    //CASE_MSG(WM_HANDHELDLAST)
    //CASE_MSG(WM_AFXFIRST)
    //CASE_MSG(WM_AFXLAST)
    //CASE_MSG(WM_PENWINFIRST)
    //CASE_MSG(WM_RCRESULT)
    //CASE_MSG(WM_HOOKRCRESULT)
    //CASE_MSG(WM_GLOBALRCCHANGE)
    //CASE_MSG(WM_PENMISCINFO)
    //CASE_MSG(WM_SKB)
    //CASE_MSG(WM_HEDITCTL)
    //CASE_MSG(WM_PENCTL)
    //CASE_MSG(WM_PENMISC)
    //CASE_MSG(WM_CTLINIT)
    //CASE_MSG(WM_PENEVENT)
    //CASE_MSG(WM_PENWINLAST)
    //CASE_MSG(WM_PSD_PAGESETUPDLG)
    CASE_MSG(WM_USER)
    //CASE_MSG(WM_CHOOSEFONT_GETLOGFONT)
    //CASE_MSG(WM_PSD_FULLPAGERECT)
    //CASE_MSG(WM_PSD_MINMARGINRECT)
    //CASE_MSG(WM_PSD_MARGINRECT)
    //CASE_MSG(WM_PSD_GREEKTEXTRECT)
    //CASE_MSG(WM_PSD_ENVSTAMPRECT)
    //CASE_MSG(WM_PSD_YAFULLPAGERECT)
    //CASE_MSG(WM_CAP_UNICODE_START)
    //CASE_MSG(WM_CHOOSEFONT_SETLOGFONT)
    //CASE_MSG(WM_CHOOSEFONT_SETFLAGS)
    //CASE_MSG(WM_CAP_SET_CALLBACK_STATUSW)
    //CASE_MSG(WM_CAP_DRIVER_GET_NAMEW)
    //CASE_MSG(WM_CAP_DRIVER_GET_VERSIONW)
    //CASE_MSG(WM_CAP_FILE_SET_CAPTURE_FILEW)
    //CASE_MSG(WM_CAP_FILE_GET_CAPTURE_FILEW)
    //CASE_MSG(WM_CAP_FILE_SAVEASW)
    //CASE_MSG(WM_CAP_FILE_SAVEDIBW)
    //CASE_MSG(WM_CAP_SET_MCI_DEVICEW)
    //CASE_MSG(WM_CAP_GET_MCI_DEVICEW)
    //CASE_MSG(WM_CAP_PAL_OPENW)
    //CASE_MSG(WM_CAP_PAL_SAVEW)
    //CASE_MSG(WM_CPL_LAUNCH)
    //CASE_MSG(WM_CPL_LAUNCHED)
    CASE_MSG(WM_APP)
    //CASE_MSG(WM_RASDIALEVENT)
    }
#undef CASE_MSG
#endif
    return strprintf("%s (%d)", name, msg);
}

void appendQuotedArgument(std::wstring& cmdline, utf16ptr arg, bool forceQuoting)
{
    if (!cmdline.empty())
        cmdline.push_back(' ');
    if (!forceQuoting && arg && *arg && !wcspbrk(arg, L" \t\n\v\""))
        cmdline.append(arg);
    else
    {
        cmdline.push_back('"');
        if (arg) for (const wchar_t* p = arg; ; p++)
        {
            unsigned backslashCount = 0;
            while (*p == '\\')
            {
                ++p;
                ++backslashCount;
            }
            if (!*p)
            {
                cmdline.append(backslashCount * 2, '\\');
                break;
            }
            else if (*p == '"')
            {
                cmdline.append(backslashCount * 2 + 1, '\\');
                cmdline.push_back(*p);
            }
            else
            {
                cmdline.append(backslashCount, '\\');
                cmdline.push_back(*p);
            }
        }
        cmdline.push_back('"');
    }
}

int runProgram(utf16ptr executable, const std::initializer_list<utf16ptr> args, utf16ptr cwd)
{
    std::wstring cmdline;
    appendQuotedArgument(cmdline, executable);
    for (const auto& arg : args)
        appendQuotedArgument(cmdline, arg);

    PROCESS_INFORMATION pi = {0};
    STARTUPINFO si = {0};
    si.cb = sizeof(si);

    if (CreateProcess(NULL, &cmdline[0], NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, NULL, cwd, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode;
        if (!GetExitCodeProcess(pi.hProcess, &exitCode)) exitCode = (DWORD)-1;

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        return (int)exitCode;
    }
    else
    {
        return -1;
    }
}


bool launchProgramAsDesktopUser(utf16ptr executable, const std::initializer_list<utf16ptr> args, utf16ptr cwd)
{
    bool result = false;

    // Launch the process unelevated by copying the token of the shell (desktop) window
    if (HWND shellWnd = GetShellWindow())
    {
        DWORD shellPID;
        DWORD shellTID = GetWindowThreadProcessId(shellWnd, &shellPID);
        if (HANDLE shellProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, shellPID))
        {
            HANDLE shellToken;
            if (OpenProcessToken(shellProcess, TOKEN_DUPLICATE, &shellToken))
            {
                HANDLE primaryToken;
                if (DuplicateTokenEx(shellToken, TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID, NULL, SecurityImpersonation, TokenPrimary, &primaryToken))
                {
                    std::wstring cmdline;
                    appendQuotedArgument(cmdline, executable);
                    for (const auto& arg : args)
                        appendQuotedArgument(cmdline, arg);

                    PROCESS_INFORMATION pi = {0};
                    STARTUPINFO si = {0};
                    si.cb = sizeof(si);

                    if (CreateProcessWithTokenW(primaryToken, 0, NULL, &cmdline[0], 0, NULL, cwd, &si, &pi))
                    {
                        result = true;
                    }

                    CloseHandle(primaryToken);
                }
                CloseHandle(shellToken);
            }
            CloseHandle(shellProcess);
        }
    }

    return result;
}

UIString::UIString(UINT id, int param)
    : _id{id}, _hasParam{true}, _param{wstrprintf(L"%d", param)}
{
}

std::wstring UIString::str() const
{
    if(!_id)
        return {};
    // These results could be cached if necessary
    if(!_hasParam)
        return loadString(_id);
    return wstrprintf(loadString(_id), _param);
}

bool UIString::operator==(const UIString &other) const
{
    return _id == other._id && _hasParam == other._hasParam && _param == other._param;
}

#include "installer.h"

int messageBox(UIString text, UIString caption, UIString msgSuffix, UINT type, int silentResult)
{
    if (g_installer)
        return g_installer->messageBox(std::move(text), std::move(caption), std::move(msgSuffix), type, silentResult);
    else
        return silentResult;
}

void checkAbort()
{
    if (g_installer)
        g_installer->checkAbort();
}

std::string readTextFile(utf16ptr path)
{
    std::string result;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER size;
        if (GetFileSizeEx(file, &size) && size.HighPart == 0)
        {
            result.resize(size.LowPart);
            DWORD read = 0;
            if (!ReadFile(file, &result[0], size.LowPart, &read, NULL) || read != size.LowPart)
            {
                result.resize(0);
            }
        }
        CloseHandle(file);
    }
    return result;
}

bool writeTextFile(utf16ptr path, utf8ptr text, DWORD creationFlags)
{
    bool result = false;
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, creationFlags, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        DWORD length = (DWORD)text.length(), written = 0;
        if (WriteFile(file, text, length, &written, NULL) && written == length)
        {
            result = true;
        }
        CloseHandle(file);
    }
    return result;
}
