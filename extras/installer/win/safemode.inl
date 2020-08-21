#ifndef SAFEMODE_INL
#define SAFEMODE_INL

#include "safemode_inl.h"
#include <windows.h>
#include <array>

BootMode getBootMode()
{
    int cleanboot = ::GetSystemMetrics(SM_CLEANBOOT);
    switch(cleanboot)
    {
    case 0:
        SAFEMODE_LOG("Boot mode: normal");
        return BootMode::Normal;
    case 1:
        SAFEMODE_LOG("Boot mode: safe mode");
        return BootMode::SafeMode;
    case 2:
        SAFEMODE_LOG("Boot mode: safe mode with networking");
        return BootMode::SafeModeWithNetworking;
    default:
        SAFEMODE_LOG("Boot mode: unknown (%d)", cleanboot);
        return BootMode::Normal;
    }
}

#endif
