#include <string>
#include <Userenv.h>
#include "util_inl.h"

#pragma comment(lib, "Userenv.lib")

// Determine the system temp directory (e.g. C:\Windows\Temp).
//
// There's no Win32 API to do this.  The system directory is present as the TEMP
// environment variable in the system block, but users typically also have a
// TEMP variable indicating the user's temp directory.
//
// This uses CreateEnvironmentBlock() in order to get system variables only.
std::wstring getSystemTempPath()
{
    std::wstring path;
    const wchar_t* env;
    if (CreateEnvironmentBlock((LPVOID*)&env, NULL, FALSE))
    {
        for (const wchar_t* p = env; *p; p += wcslen(p) + 1)
        {
            if (!wcsnicmp(p, L"TEMP=", 5))
            {
                path.assign(p + 5);
                while (!path.empty() && path.back() == '\\')
                    path.pop_back();
                break;
            }
        }
        DestroyEnvironmentBlock((LPVOID)env);
    }
    return path;
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
