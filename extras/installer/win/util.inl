#include <string>
#include <Userenv.h>

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
