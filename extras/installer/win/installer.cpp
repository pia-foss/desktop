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

#include "installer.h"
#include "brand.h"
#include "service_inl.h"
#include "tun_inl.h"
#include "safemode_inl.h"
#include "tasks.h"
#include "version.h"
#include "tasks/callout.h"
#include "tasks/file.h"
#include "tasks/function.h"
#include "tasks/launch.h"
#include "tasks/list.h"
#include "tasks/migrate.h"
#include "tasks/payload.h"
#include "tasks/process.h"
#include "tasks/registry.h"
#include "tasks/service.h"
#include "tasks/wgservice.h"
#include "tasks/shortcut.h"
#include "tasks/tap.h"
#include "tasks/uninstall.h"
#include "tasks/wintun.h"

#include <shlobj_core.h>
#include <shlwapi.h>

#pragma comment(lib, "Msi.lib")

#if defined(INSTALLER)
#define WINDOW_CLASS_NAME _T("PIAInstaller")
#elif defined(UNINSTALLER)
#define WINDOW_CLASS_NAME _T("PIAUninstaller")
#endif

#define WINDOW_WIDTH 700
#define WINDOW_HEIGHT 420
#define WINDOW_BACKGROUND RGB(0x32, 0x36, 0x42)
//#define BUTTON_BORDER_COLOR RGB(0x88, 0x90, 0x99)
#define BUTTON_BORDER_COLOR RGB(0x5c, 0x63, 0x70)

#define CAPTION_WHITE RGB(0xff, 0xff, 0xff)
#define CAPTION_GRAY RGB(0x88, 0x90, 0x99)

#define PROGRESS_FOREGROUND RGB(0x5d, 0xdf, 0x5a)
#define PROGRESS_BACKGROUND RGB(0x19, 0x1b, 0x21)

#define SPINNER_TICK 250
#define SPINNER_SIZE 5
#define SPINNER_SPACING 4
#define SPINNER_ACTIVE RGB(0x19, 0x1b, 0x21)
#define SPINNER_INACTIVE RGB(0x2b, 0x2e, 0x39)

#define MINIMIZE_BUTTON_WIDTH 56
#define CLOSE_BUTTON_WIDTH 67
#define CLOSE_BUTTON_HEIGHT 66

#define OK_BUTTON_ID 0x8801
#define OK_BUTTON_WIDTH 130
#define OK_BUTTON_HEIGHT 40
#define OK_BUTTON_X (WINDOW_WIDTH - 30 - OK_BUTTON_WIDTH)
#define OK_BUTTON_Y (WINDOW_HEIGHT - 30 - OK_BUTTON_HEIGHT)
#ifdef INSTALLER
#define OK_BUTTON_STR IDS_OK_BUTTON_INSTALL
#else
#define OK_BUTTON_STR IDS_OK_BUTTON_UNINSTALL
#endif

#define FONT_NAME _T("Roboto")

Installer* g_installer = nullptr;

#define LockedScope(mutex) AutoLock guard_##mutex { mutex }; (void)guard_##mutex

Installer::Installer()
{
    g_installer = this;

    InitializeCriticalSection(&_drawMutex);
    InitializeCriticalSection(&_stateMutex);
    _stateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (!g_silent) loadResources();
    createWindow();
    if (!g_silent) ShowWindow(_hWnd, SW_SHOWNORMAL);
}

Installer::~Installer()
{
    CloseHandle(_workerThread);

    CloseHandle(_stateEvent);
    DeleteCriticalSection(&_stateMutex);
    DeleteCriticalSection(&_drawMutex);

    if (_taskbarList)
    {
        _taskbarList->Release();
        _taskbarList = nullptr;
    }

    UnregisterClass((LPCTSTR)_windowClassAtom, g_instance);

    g_installer = nullptr;
}

// Read a WORD resource.
// On success, sets value to the value of the resource and returns true.
// On failure, does not modify value and returns false.
bool readWordResource(int resId, WORD &value)
{
    // Find the resource.
    // There's no cleanup needed for any of these handles since they refer to
    // resources in the application module.
    HRSRC hRes = FindResourceW(nullptr, MAKEINTRESOURCE(resId),
                                 MAKEINTRESOURCE(RT_RCDATA));
    if(!hRes)
    {
        LOG("Can't find WORD resource %d (err %d)", resId, GetLastError());
        return false;
    }

    // Sanity check that the resource contains a WORD
    // We call GetLastError() if this fails, but we can't tell for sure whether
    // SizeofResource() actually failed or if the resource legitimately is 0
    // bytes long, so clear any residual error that could be sitting around.
    SetLastError(0);
    DWORD resSize = SizeofResource(nullptr, hRes);
    if(resSize != sizeof(WORD))
    {
        LOG("WORD resource has unexpected size %d (%d)", resSize,
            GetLastError());
        return false;
    }

    HGLOBAL resMem = LoadResource(nullptr, hRes);
    if(!resMem)
    {
        LOG("Can't load WORD resource %d (err %d)", resId, GetLastError());
        return false;
    }

    // Get the address of the resource data
    LPVOID pResData = LockResource(resMem);
    if(!pResData)
    {
        LOG("Can't get address of WORD resource %d (err %d)", resId, GetLastError());
        return false;
    }

    // Read a WORD from the resource
    value = *reinterpret_cast<const WORD *>(pResData);
    return true;
}

// Windows only guarantees that the created font will have the characters
// specified by the character set that we choose here.  Any other characters
// could be missing.  Since the embedded Roboto font appears to have only
// Latin, Greek, and Cyrillic, it won't work for other scripts, like CJK
// scripts.
//
// GDI isn't able to fall back to other fonts on a per-character basis like
// Qt.  We might be able to do this with higher-level APIs like Uniscribe,
// but it looks like that will require a pretty significant overhaul of the
// drawing code here.
//
// So, we use a resource to identify the character set needed for the
// strings in this localization.  Using the locale's default character set
// wouldn't be correct for this - if we fall back to a language other than
// the current locale, it might use a different character set.  This way, we
// always use a character set that matches our localization.
//
// If a character accidentally slips through in a translation that isn't
// present in the character set we request for that locale, it might not
// display at all!
DWORD loadLocaleCharset()
{
    WORD charset = ANSI_CHARSET;
    if(!readWordResource(IDR_UICHARSET, charset))
    {
        LOG("Failed to load character set resource");
        // Already set charset to ANSI_CHARSET by default
    }

    return charset;
}

// Like the character set, the mirror flag is stored in the resource, so we use
// the correct value for the resources that we load, which may be different from
// the desktop's mirror state.  (If the user is using an RTL language that we
// do not support, we'll default to en_US, which should not be mirrored.)
bool loadUiMirror()
{
    WORD mirrored = FALSE;
    if(!readWordResource(IDR_UIMIRROR, mirrored))
    {
        LOG("Failed to load UI mirror resource");
        // Already set mirrored to FALSE by default
    }

    return mirrored;
}

void Installer::loadResources()
{
    _uiMirror = loadUiMirror();
    _appIcon.set((HICON)LoadImage(g_instance, MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
    _appIconSmall.set((HICON)LoadImage(g_instance, MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    _backgroundBrush.set(CreateSolidBrush(WINDOW_BACKGROUND));
    _progressForegroundBrush.set(CreateSolidBrush(PROGRESS_FOREGROUND));
    _progressBackgroundBrush.set(CreateSolidBrush(PROGRESS_BACKGROUND));
    _spinnerActiveBrush.set(CreateSolidBrush(SPINNER_ACTIVE));
    _spinnerInactiveBrush.set(CreateSolidBrush(SPINNER_INACTIVE));
    _buttonBorderPen.set(CreatePen(PS_SOLID, 1, BUTTON_BORDER_COLOR));
    _logoBitmap.set(LoadBitmap(g_instance, MAKEINTRESOURCE(IDB_LOGO)));
    _minimizeBitmap.set(LoadBitmap(g_instance, MAKEINTRESOURCE(IDB_MINIMIZE)));
    _closeBitmap.set(LoadBitmap(g_instance, MAKEINTRESOURCE(IDB_CLOSE)));

    HRSRC font = FindResource(g_instance, MAKEINTRESOURCE(IDF_FONT), RT_FONT);
    HGLOBAL fontResource = LoadResource(g_instance, font);
    LPVOID fontData = LockResource(fontResource);
    DWORD fontSize = SizeofResource(g_instance, font);
    DWORD fontCount;
    AddFontMemResourceEx(fontData, fontSize, 0, &fontCount);

    DWORD localeCharset = loadLocaleCharset();
    _mainFont.set(CreateFont(-14, 0, 0, 0, FW_REGULAR, FALSE, FALSE, FALSE, localeCharset, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, VARIABLE_PITCH, FONT_NAME));
}

void Installer::createWindow()
{
    WNDCLASSEX wcex {0};
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_DROPSHADOW;
    wcex.lpfnWndProc = staticWindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(Installer*);
    wcex.hInstance = g_instance;
    wcex.hIcon = _appIcon;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = _backgroundBrush;
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = WINDOW_CLASS_NAME;
    wcex.hIconSm = NULL;
    _windowClassAtom = RegisterClassEx(&wcex);
    if (!_windowClassAtom)
        LOG("Unable to create window class (%d)", GetLastError());

    RECT desktopRect;
    SystemParametersInfo(SPI_GETWORKAREA, NULL, &desktopRect, 0);

    _taskbarButtonCreatedMsg = RegisterWindowMessageW(L"TaskbarButtonCreated");

    DWORD exStyle = WS_EX_APPWINDOW | WS_EX_CONTROLPARENT;
    // If the UI is mirrored use WS_EX_LAYOUTRTL to flip the layout.
    if(_uiMirror)
        exStyle |= WS_EX_LAYOUTRTL;

    // The _hWnd variable is actually set inside WM_NCCREATE,
    // but we set it here too for clarity.
    _hWnd = CreateWindowEx(
                exStyle,
                WINDOW_CLASS_NAME,
                loadString(IDS_WINDOW_TITLE).c_str(),
                WS_POPUPWINDOW | WS_MINIMIZEBOX /*| WS_CAPTION*/,
                desktopRect.left + (desktopRect.right - desktopRect.left - WINDOW_WIDTH) / 2,
                desktopRect.top + (desktopRect.bottom - desktopRect.top - WINDOW_HEIGHT) / 2,
                WINDOW_WIDTH,
                WINDOW_HEIGHT,
                NULL,
                NULL,
                g_instance,
                reinterpret_cast<LPVOID>(this));
    if (!_hWnd)
        LOG("Unable to create window (%d)", GetLastError());

    _hWndButton = CreateWindow(
                L"BUTTON",
                L"",
                WS_TABSTOP | WS_CHILD | BS_OWNERDRAW,
                OK_BUTTON_X,
                OK_BUTTON_Y,
                OK_BUTTON_WIDTH,
                OK_BUTTON_HEIGHT,
                _hWnd,
                (HMENU)OK_BUTTON_ID,
                g_instance,
                NULL);
}

int Installer::run()
{
    if (!_hWnd)
    {
        MessageBox(NULL, L"Unable to create window", L"Critical error", MB_ICONERROR | MB_OK);
        return 1;
    }

    _workerThread = CreateThread(NULL, 0, &staticWorkerThreadMain, reinterpret_cast<LPVOID>(this), 0, &_workerThreadId);
    if (!_workerThread)
    {
        LOG("Unable to create worker thread (%d)", GetLastError());
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    WaitForSingleObject(_workerThread, INFINITE);
    DWORD threadResult;
    if (GetExitCodeThread(_workerThread, &threadResult))
    {
        LOG("Worker thread exited with code %d", threadResult);
    }
    else
    {
        LOG("Failed to get worker thread exit code (%d)", GetLastError());
        threadResult = 1;
    }

    return (int)threadResult;
}

Installer::State Installer::getState()
{
    LockedScope(_stateMutex);
    return _state;
}

Installer::Result Installer::getResult()
{
    LockedScope(_stateMutex);
    return _result;
}

Installer::State Installer::setState(Installer::State state, Installer::Result result)
{
    EnterCriticalSection(&_stateMutex);
    return setStateAndReleaseMutex(state, result);
}

Installer::State Installer::setStateAndReleaseMutex(Installer::State state, Installer::Result result)
{
    bool notify = false;
    if (result > _result)
    {
        _result = result;
        notify = true;
    }
    if (state > _state)
    {
        LOG("Setting state to %d", state);
        _state = state;
        notify = true;
        switch (state)
        {
        case Preparing: setCaption(IDS_CAPTION_PREPARING); break;
        case ReadyToInstall: setCaption(IDS_CAPTION_READYTOINSTALL); break;
        case Installing: setCaption(IDS_CAPTION_INSTALLING); break;
        case Aborting: setCaption(IDS_CAPTION_ABORTING); break;
        case Committing:
#ifdef INSTALLER
            setCaption(IDS_CAPTION_FINISHINGUP);
#else
            setCaption(IDS_CAPTION_CLEANINGUP);
#endif
            break;
        case RollingBack: setCaption(IDS_CAPTION_ROLLINGBACK); break;
        case Done: setCaption(_result == Success ? IDS_CAPTION_FINISHED : IDS_CAPTION_ABORTED); break;
        default: break;
        }
    }
    else
    {
        state = _state;
    }
    LeaveCriticalSection(&_stateMutex);
    if (notify)
    {
        if (GetCurrentThreadId() != _workerThreadId)
            SetEvent(_stateEvent);
        if (state < Exiting)
            PostMessage(_hWnd, WM_USER, 0, 0);
    }
    return state;
}

void Installer::setProgress(double progress, double timeRemaining)
{
    LockedScope(_drawMutex);
    bool notify = progress != _progress;
    _progress = progress;
    _timeRemaining = timeRemaining;
    if (notify)
        PostMessage(_hWnd, WM_USER, 0, 0);
}

void Installer::updateTaskbarProgress()
{
    if (!_taskbarList)
        return;
    else if (_progress < 0.0)
        _taskbarList->SetProgressState(_hWnd, TBPF_NOPROGRESS);
    else if (_state == RollingBack || (_state > Installing && _result == Success))
        _taskbarList->SetProgressState(_hWnd, TBPF_INDETERMINATE);
    else
    {
        TBPFLAG state;
        switch (_currentMessageBoxType & MB_ICONMASK)
        {
        case MB_ICONERROR:
            state = TBPF_ERROR;
            break;
        case MB_ICONWARNING:
            state = TBPF_PAUSED;
            break;
        default:
            state = TBPF_NORMAL;
            break;
        }
        _taskbarList->SetProgressState(_hWnd, state);
        _taskbarList->SetProgressValue(_hWnd, static_cast<ULONGLONG>(_progress * 65536.0), 65536);
    }
}

void Installer::setCaption(UIString caption)
{
    LockedScope(_drawMutex);
    if (_caption == caption) return;
    _caption = std::move(caption);
    if (_state < Exiting)
        InvalidateRect(_hWnd, NULL, FALSE);
}

void Installer::setError(UIString description)
{
    LOG("ERROR: %ls", description.str());
    LockedScope(_drawMutex);
    _result = Error;
    _error = std::move(description);
    if (_state < Exiting)
        InvalidateRect(_hWnd, NULL, FALSE);
}

void Installer::warnCorruptInstallation()
{
#ifdef UNINSTALLER

#endif
}

static int g_defaultButtons[][3] {
    { IDOK },
    { IDOK, IDCANCEL },
    { IDABORT, IDRETRY, IDIGNORE },
    { IDYES, IDNO, IDCANCEL },
    { IDYES, IDNO },
    { IDRETRY, IDCANCEL },
    { IDCANCEL, IDTRYAGAIN, IDCONTINUE },
};

int Installer::messageBox(UIString text, UIString caption, UIString msgSuffix, UINT type, int silentResult)
{
    if (g_silent)
    {
        return silentResult ? silentResult : g_defaultButtons[type & MB_TYPEMASK][(type & MB_DEFMASK) >> 8];
    }
    else
    {
        auto textStr = text.str();
        if(msgSuffix)
        {
            textStr += L"\n\n";
            textStr += msgSuffix.str();
        }
        if (_taskbarList)
        {
            LockedScope(_stateMutex);
            _currentMessageBoxType = type;
            updateTaskbarProgress();
        }
        int result = MessageBox(_hWnd, textStr.c_str(), caption.str().c_str(), type);
        if (_taskbarList)
        {
            LockedScope(_stateMutex);
            _currentMessageBoxType = 0;
            updateTaskbarProgress();
        }
        return result;
    }
}

void Installer::checkAbort()
{
    LockedScope(_stateMutex);
    if (_state == Aborting)
        throw InstallerAbort();
}

Installer::State Installer::waitForNewState(Installer::State minimumState)
{
    for (;;)
    {
        EnterCriticalSection(&_stateMutex);
        State currentState = _state;
        LeaveCriticalSection(&_stateMutex);
        if (currentState >= minimumState)
            return currentState;
        WaitForSingleObject(_stateEvent, INFINITE);
    }
}

void Installer::install()
{
    setState(Installing);
}

void Installer::abort()
{
    setState(Aborting);
}

void Installer::close()
{
    DestroyWindow(_hWnd);
    PostQuitMessage(0);
}


DWORD Installer::staticWorkerThreadMain(LPVOID self)
{
    if (HRESULT err = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))
        LOG("CoInitializeEx failed (%d)", err);

    return reinterpret_cast<Installer*>(self)->workerThreadMain();
}

// The main worker thread for both the installation and uninstallation process
DWORD Installer::workerThreadMain()
{
    DWORD exitCode = 0;

    TaskList tasks;
    tasks.setListener(this);

    try
    {
        setState(Preparing);

    #ifdef INSTALLER
        // Check the minimum required Windows version
        if (!IsWindows8OrGreater())
            InstallerError::abort(IDS_MB_REQUIRESWIN8);
    #endif

    #ifndef _WIN64
        // 32-bit builds shouldn't be installed on 64-bit systems (although we
        // could easily bundle both driver types in 32-bit builds, users really
        // ought to install the right version for their OS).
        BOOL isWow64 = FALSE;
        if (IsWow64Process(GetCurrentProcess(), &isWow64) && isWow64)
            InstallerError::abort(IDS_MB_32BITON64BIT);
    #endif

        // The installer/uninstaller requires at least Safe Mode with
        // Networking.  The TAP adapter can't be installed in safe mode.
        if(getBootMode() == BootMode::SafeMode)
        {
            InstallerError::abort(IDS_MB_REQUIRESNETWORKING);
        }

    #ifdef INSTALLER
        auto alphaPath = getShellFolder(CSIDL_PROGRAM_FILES) + L"\\piaX";
        auto alphaDaemonDataPath = getShellFolder(CSIDL_COMMON_APPDATA) + L"\\piaX";
        auto alphaClientDataPath = getShellFolder(CSIDL_LOCAL_APPDATA) + L"\\piaX";
    #endif

        g_startMenuPath = getShellFolder(CSIDL_COMMON_PROGRAMS);

        // Determine path to installed executables
        g_clientPath = g_installPath + L"\\" BRAND_CODE "-client.exe";
        g_servicePath = g_installPath + L"\\" BRAND_CODE "-service.exe";
        g_wgServicePath = g_installPath + L"\\" BRAND_CODE "-wgservice.exe";

        g_daemonDataPath = g_installPath + L"\\data";
        g_oldDaemonDataPath = getShellFolder(CSIDL_COMMON_APPDATA) + L"\\" PIA_PRODUCT_NAME;
        g_clientDataPath = getShellFolder(CSIDL_LOCAL_APPDATA) + L"\\" PIA_PRODUCT_NAME;

        // Configure MSI UI and logging.  This is global state in the MSI
        // library, so it applies to all tasks.
        initMsiLib(g_userTempPath.c_str());

        bool clientExists = !!PathFileExists(g_clientPath.c_str());
        bool serviceExists = !!PathFileExists(g_servicePath.c_str());
        bool uninstallDataExists = !!PathFileExists(getUninstallDataPath(g_installPath).c_str());

    #ifdef UNINSTALLER
        if (!uninstallDataExists)
        {
            if (messageBox(IDS_MB_CORRUPTINSTALLATION, IDS_MB_CAP_CORRUPTINSTALLATION, 0, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON1) != IDYES)
                throw InstallerAbort();
        }
    #endif

    #ifdef INSTALLER
        #if BRAND_MIGRATES_LEGACY
            // Detect any old installation (old desktop or alpha), migrate
            // settings from it and (offer to) uninstall it.
            tasks.addNew<MigrateTask>();
        #endif

        // Create the target directory
        tasks.addNew<CreateDirectoryTask>(g_installPath, true);
    #endif

        // Create the rollback directory
        tasks.addNew<CreateRollbackDirectoryTask>();

        // Kill any running clients
        tasks.addNew<KillExistingClientsTask>();

        // Stop the existing daemon service; restart it if we roll back
        tasks.addNew<StopExistingServiceTask>(g_daemonServiceParams.pName, true);

        // Stopping the daemon should also stop the Wireguard service, but stop
        // it explicitly for robustness.  Don't restart it if we roll back.
        tasks.addNew<StopExistingServiceTask>(g_wireguardServiceParams.pName, false);

        // Uninstall the existing service.  Execution is a no-op in installer,
        // but rollback is needed even in installer - it restores the old
        // services if they were replaced by install tasks (using the restored
        // daemon's "install" command).
        tasks.addNew<UninstallExistingServiceTask>();

    #ifdef UNINSTALLER
        // Uninstall the callout driver
        tasks.addNew<UninstallCalloutDriverTask>();

        // Uninstall the wireguard service
        tasks.addNew<UninstallWgServiceTask>();

        // Uninstall the WinTUN driver
        tasks.addNew<UninstallWintunTask>();
    #endif

        // Uninstall existing TAP driver (noop in installation, may perfom
        // rollback during aborted installation)
        tasks.addNew<UninstallTapDriverTask>();

    #ifdef INSTALLER
        // Remove any existing installation (listed in uninstall.dat)
        if (uninstallDataExists)
        {
            tasks.addNew<CaptionTask>(IDS_CAPTION_BACKINGUPFILES);
            tasks.addNew<ExecuteUninstallDataTask>(g_installPath);
        }

        // Remove any piaX alpha install (files and shortcuts only)
        if (PathFileExistsW(alphaPath.c_str()))
        {
            tasks.addNew<CaptionTask>(IDS_CAPTION_REMOVINGPREVIOUSVERSION);
            tasks.addNew<FunctionTask>([=] {
                setMigrationTextFile(L"account.json", readTextFile(alphaDaemonDataPath + L"\\account.json"));
                setMigrationTextFile(L"settings.json", readTextFile(alphaDaemonDataPath + L"\\settings.json"));
            });
            tasks.addNew<ExecuteUninstallDataTask>(alphaPath);
            tasks.addNew<RemoveDirectoryTask>(alphaPath, true);
            tasks.addNew<RemoveDirectoryTask>(alphaDaemonDataPath, true);
            tasks.addNew<RemoveDirectoryTask>(alphaClientDataPath, true);
        }

        // Copy new files
        tasks.addNew<PayloadTask>(g_installPath);

        // Add piavpn: URI handler
        tasks.addNew<WriteUrlHandlerRegistryTask>(g_clientPath);

        // Create the data directory (even if we don't have any settings to
        // write; the MSI install flag is written here)
        tasks.addNew<CreateDirectoryTask>(g_daemonDataPath, true);

        // Plant any remembered account/settings
        tasks.addNew<WriteSettingsTask>(g_daemonDataPath);

        // Install TAP driver
        tasks.addNew<InstallTapDriverTask>();

        // Install WinTUN driver
        tasks.addNew<InstallWintunTask>();

        // Update callout driver if installed
        tasks.addNew<UpdateCalloutDriverTask>();

        // Install Wireguard service
        tasks.addNew<InstallWgServiceTask>();

        // Install service
        tasks.addNew<InstallServiceTask>();

        // Start service
        tasks.addNew<StartInstalledServiceTask>();

        // Bulk of the work done; finish up
        tasks.addNew<CaptionTask>(IDS_CAPTION_FINISHINGUP);

        // Add shortcuts
        // Translation note - the product name is not translated
        tasks.addNew<AddShortcutTask>(L"" PIA_PRODUCT_NAME, g_clientPath);

        // Write uninstall data
        tasks.addNew<WriteUninstallDataTask>(g_installPath);

        // Add uninstall entry
        tasks.addNew<WriteUninstallRegistryTask>();
    #endif

    #ifdef UNINSTALLER
        // Remove any existing installation (listed in uninstall.dat)
        if (uninstallDataExists)
        {
            tasks.addNew<CaptionTask>(IDS_CAPTION_REMOVINGFILES);
            tasks.addNew<ExecuteUninstallDataTask>(g_installPath);
        }
        else
        {
            tasks.addNew<RemoveDirectoryTask>(g_installPath, true);
        }

        // Delete daemon data
        tasks.addNew<RemoveDirectoryTask>(g_daemonDataPath, true);

        // Delete client data (for current user)
        tasks.addNew<RemoveDirectoryTask>(g_clientDataPath, true);
    #endif

        // Remove old daemon data path (if it exists)
        if (PathFileExistsW(g_oldDaemonDataPath.c_str()))
        {
            // This is allowed to fail, since we might be executing out of it;
            // if so, the directory will be deleted in a future upgrade.
            tasks.addNew([] { deleteEntireDirectory(g_oldDaemonDataPath); });
        }

        // Last chance to abort the installation; beyond this point no tasks
        // should fail as these tasks can't be rolled back.
        tasks.addNew([this] {
            EnterCriticalSection(&_stateMutex);
            if (_state == Aborting)
            {
                LeaveCriticalSection(&_stateMutex);
                throw InstallerAbort();
            }
            setStateAndReleaseMutex(Committing, Success);
        });

        // Delete rollback data
        tasks.addNew([]{ deleteRollbackDirectory(); }, 1);

    #ifdef UNINSTALLER
        // Remove uninstall entry
        tasks.addNew<RemoveUninstallRegistryTask>();

        // Remove run entries (start on login)
        tasks.addNew<RemoveRunRegistryTask>();

        // Delete the install directory if it's empty
        tasks.addNew([this] {
            if (!RemoveDirectoryW(g_installPath.c_str()) &&
                    messageBox(IDS_MB_FILESREMAINING, IDS_MB_CAP_FILESREMAINING, 0, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON1) == IDYES)
                deleteEntireDirectory(g_installPath);
        }, 0.1);

        // Wipe the TAP driver from the driver store if possible
        tasks.addNew<CleanupTapDriverTask>();
    #endif

    #ifdef INSTALLER
        // Launch the client, or show a message asking to reboot if in safe mode
        tasks.addNew<LaunchClientTask>();
    #endif

        tasks.prepare();

        setState(ReadyToInstall);

        if (waitForNewState(Installing) >= Aborting)
        {
            setState(Done, Aborted);
            return 2;
        }

        tasks.execute();

        // Last chance to abort the installation
        EnterCriticalSection(&_stateMutex);
        if (_state == Aborting)
        {
            LeaveCriticalSection(&_stateMutex);
            throw InstallerAbort();
        }
        setStateAndReleaseMutex(Done, Success);

        return exitCode;
    }
    catch (const InstallerError& error)
    {
        setError(error.description());
        exitCode = 1;
    }
    catch (const InstallerAbort&)
    {
        exitCode = 2;
    }

    if (tasks.needsRollback())
    {
        setState(RollingBack);
        setCaption(IDS_CAPTION_ROLLINGBACK);

        try
        {
            tasks.rollback();
        }
        catch (...) {}
    }

    setState(Done, exitCode == 2 ? Aborted : Error);

    return exitCode;
}

LRESULT CALLBACK Installer::staticWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Installer* self;
    if (msg == WM_NCCREATE)
    {
        self = reinterpret_cast<Installer*>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams);
        self->_hWnd = hWnd;
        SetWindowLongPtrW(hWnd, 0, reinterpret_cast<LONG_PTR>(self));
        if (self->_taskbarButtonCreatedMsg)
            ChangeWindowMessageFilterEx(hWnd, self->_taskbarButtonCreatedMsg, MSGFLT_ALLOW, nullptr);
    }
    else
    {
        self = reinterpret_cast<Installer*>(GetWindowLongPtrW(hWnd, 0));
    }
    return self->windowProc(msg, wParam, lParam);
}

struct LPARAM_POINTS : public POINT
{
    LPARAM_POINTS(LPARAM lParam) { auto pt = MAKEPOINTS(lParam); x = pt.x; y = pt.y; }
};

#ifdef _DEBUG
#include <unordered_set>
#endif

LRESULT Installer::windowProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
#ifdef _DEBUG
    static std::unordered_set<UINT> blockedMessages;
    if (!blockedMessages.count(msg))
        OutputDebugStringA(strprintf("hWnd %p wParam %p lParam %p msg %s\n", (DWORD_PTR)_hWnd, wParam, lParam, getMessageName(msg).c_str()).c_str());
#define LOG_ONLY_ONCE blockedMessages.insert(msg)
#else
#define LOG_ONLY_ONCE ((void)0)
#endif

    // This convoluted set of macros gives us a convenient syntax to handle
    // messages while casting wParam/lParam to typed+named parameters.
    #define EXPR_AS(expr, ...) for (__VA_ARGS__ = any_cast(expr);;)
    #define WPARAM_AS(...) EXPR_AS(wParam, __VA_ARGS__)
    #define WPARAM_HIWORD_AS(...) EXPR_AS(HIWORD(wParam), __VA_ARGS__)
    #define WPARAM_LOWORD_AS(...) EXPR_AS(LOWORD(wParam), __VA_ARGS__)
    #define LPARAM_AS(...) EXPR_AS(lParam, __VA_ARGS__)
    #define LPARAM_HIWORD_AS(...) EXPR_AS(HIWORD(lParam), __VA_ARGS__)
    #define LPARAM_LOWORD_AS(...) EXPR_AS(LOWORD(lParam), __VA_ARGS__)
    #define HANDLE_MSG(msg, ...) \
        case msg: \
            if (int _handleCount = 0) { exit_##msg: break; } else \
            __VA_ARGS__ \
            for (;;) \
            if (_handleCount++) goto exit_##msg; else

    if (_taskbarButtonCreatedMsg && msg == _taskbarButtonCreatedMsg)
    {
        if (HRESULT err = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_ITaskbarList3, reinterpret_cast<void**>(&_taskbarList)))
            LOG("CoCreateInstance(CLSID_TaskbarList) failed (%d)", err);
        else if (err = _taskbarList->HrInit())
        {
            LOG("ITaskbarList::HrInit failed (%d)", err);
            _taskbarList->Release();
            _taskbarList = nullptr;
        }
    }

    switch (msg)
    {
        HANDLE_MSG(WM_CREATE) {
            //SetWindowPos(_hWnd, NULL, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, SWP_NOMOVE | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
        HANDLE_MSG(WM_GETICON) {
            LOG_ONLY_ONCE;
            return (LRESULT)(HICON)(wParam == ICON_BIG ? _appIcon : _appIconSmall);
        }
        HANDLE_MSG(WM_SETCURSOR, WPARAM_AS(HWND hover)) {
            LOG_ONLY_ONCE;
            /*
            int hit = LOWORD(lParam);
            int msg = HIWORD(lParam);
            if (hit == HTMINBUTTON || hit == HTCLOSE)
            {
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return TRUE;
            }
            */
        }
        HANDLE_MSG(WM_ERASEBKGND) {
            LOG_ONLY_ONCE;
            return TRUE;
        }
        HANDLE_MSG(WM_NCACTIVATE, WPARAM_AS(BOOL active)) {
            return TRUE;
        }
        HANDLE_MSG(WM_NCCALCSIZE) {
            // This conveniently equates to using the entire window as the client area
            return 0;
        }
        HANDLE_MSG(WM_PAINT) {
            LOG_ONLY_ONCE;
            doPaint();
            return 0;
        }
        HANDLE_MSG(WM_CTLCOLORBTN, WPARAM_AS(HDC hDC) LPARAM_AS(HWND hWnd)) {
            LOG_ONLY_ONCE;
            if (hWnd == _hWndButton)
                return (LRESULT)(HBRUSH)_backgroundBrush;
        }
        HANDLE_MSG(WM_DRAWITEM, WPARAM_AS(WORD id) LPARAM_AS(LPDRAWITEMSTRUCT dis)) {
            LOG_ONLY_ONCE;
            switch (dis->CtlType)
            {
            case ODT_BUTTON:
                int inset = dis->itemState & ODS_SELECTED ? 1 : 0;
                //SelectObject(dis->hDC, _backgroundBrush);
                SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
                SelectObject(dis->hDC, _buttonBorderPen);
                Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
                WCHAR text[100];
                GetWindowTextW(dis->hwndItem, text, 100);
                SetBkColor(dis->hDC, WINDOW_BACKGROUND);
                drawText(dis->hDC, { dis->rcItem.left + 2 + inset, dis->rcItem.top + 2 + inset, dis->rcItem.right - 2 + inset, dis->rcItem.bottom - 2 + inset }, CAPTION_WHITE, DT_CENTER | DT_VCENTER, text);
                return TRUE;
            }
        }
        HANDLE_MSG(WM_COMMAND, WPARAM_HIWORD_AS(WORD code) WPARAM_LOWORD_AS(WORD identifier) LPARAM_AS(HWND control)) {
            if (identifier == OK_BUTTON_ID)
            {
                switch (getState())
                {
                case ReadyToInstall: setState(Installing); break;
                case Done: setState(Exiting); close(); break;
                }
                return 0;
            }
        }
        HANDLE_MSG(WM_NCHITTEST, LPARAM_AS(LPARAM_POINTS pt)) {
            LOG_ONLY_ONCE;
            RECT rect; POINTS p;
            GetWindowRect(_hWnd, &rect);
            int x = pt.x - rect.left;
            int y = pt.y - rect.top;
            if (_uiMirror)
                x = (rect.right - rect.left) - x;
            if (y >= 0 && y < CLOSE_BUTTON_HEIGHT)
            {
                if (x >= WINDOW_WIDTH) {}
                else if (x >= WINDOW_WIDTH - CLOSE_BUTTON_WIDTH) return HTCLOSE;
                else if (x >= WINDOW_WIDTH - CLOSE_BUTTON_WIDTH - MINIMIZE_BUTTON_WIDTH) return HTMINBUTTON;
            }
            return HTCAPTION;
        }
        HANDLE_MSG(WM_NCLBUTTONDOWN, WPARAM_AS(int hit) LPARAM_AS(LPARAM_POINTS pt)) {
            if (hit == HTMINBUTTON || hit == HTCLOSE)
            {
                _capturedNonClientButton = hit;
                _hoveredNonClientButton = true;
                SetCapture(_hWnd);
                RECT rect = getNonClientButtonRect(_capturedNonClientButton);
                InvalidateRect(_hWnd, &rect, FALSE);
                return 0;
            }
        }
        HANDLE_MSG(WM_NCLBUTTONUP, WPARAM_AS(int hit) LPARAM_AS(LPARAM_POINTS pt)) {
        }
        HANDLE_MSG(WM_NCMOUSEMOVE, WPARAM_AS(int hit) LPARAM_AS(LPARAM_POINTS pt)) {
            LOG_ONLY_ONCE;
        }
        HANDLE_MSG(WM_NCRBUTTONDOWN, WPARAM_AS(int hit) LPARAM_AS(LPARAM_POINTS pt)) {
            // Use undocumented WM_POPUPSYSTEMMENU (0x313) to show the system menu
            //if (hit == HTCAPTION) SendMessage(_hWnd, WM_POPUPSYSTEMMENU, 0, lParam);
        }
        HANDLE_MSG(WM_NCRBUTTONUP, WPARAM_AS(int hit) LPARAM_AS(LPARAM_POINTS pt)) {
            // Use undocumented WM_POPUPSYSTEMMENU (0x313) to show the system menu
            if (hit == HTCAPTION) SendMessage(_hWnd, WM_POPUPSYSTEMMENU, 0, lParam);
        }
        HANDLE_MSG(WM_NCPAINT) {
            LOG_ONLY_ONCE;
            return 0;
        }
        HANDLE_MSG(WM_LBUTTONDOWN, WPARAM_AS(WPARAM keys) LPARAM_AS(LPARAM_POINTS pt)) {
        }
        HANDLE_MSG(WM_MOUSEMOVE, WPARAM_AS(WPARAM keys) LPARAM_AS(LPARAM_POINTS pt)) {
            if (_capturedNonClientButton)
            {
                RECT rect = getNonClientButtonRect(_capturedNonClientButton);
                bool hover = !!PtInRect(&rect, pt);
                if (hover != _hoveredNonClientButton)
                {
                    _hoveredNonClientButton = hover;
                    InvalidateRect(_hWnd, &rect, TRUE);
                }
            }
        }
        HANDLE_MSG(WM_LBUTTONUP, WPARAM_AS(WPARAM keys) LPARAM_AS(LPARAM_POINTS pt)) {
            if (_capturedNonClientButton)
            {
                RECT capturedRect = getNonClientButtonRect(_capturedNonClientButton);
                ReleaseCapture();
                int button = std::exchange(_capturedNonClientButton, HTNOWHERE);
                if (PtInRect(&capturedRect, pt))
                {
                    SendMessage(_hWnd, WM_SYSCOMMAND, (button == HTMINBUTTON) ? SC_MINIMIZE : SC_CLOSE, 0);
                }
                return 0;
            }
        }
        HANDLE_MSG(WM_CONTEXTMENU, WPARAM_AS(HWND clicked) LPARAM_AS(LPARAM_POINTS pt)) {
            // Use undocumented WM_POPUPSYSTEMMENU (0x313) to show the system menu
            //if (clicked == _hWnd) SendMessage(_hWnd, WM_POPUPSYSTEMMENU, 0, lParam);
        }
        HANDLE_MSG(WM_USER) {
            // State change notification
            InvalidateRect(_hWnd, NULL, FALSE);
            State state = getState();
            if (g_passive)
            {
                switch (state)
                {
                case ReadyToInstall:
                    setState(Installing);
                    break;
                case Done:
                    if (g_silent)
                    {
                        setState(Exiting);
                        close();
                    }
                    else
                    {
                        SetTimer(_hWnd, 0, 1500, NULL);
                    }
                    break;
                }
            }
            else
            {
                switch (state)
                {
                case ReadyToInstall: SetWindowTextW(_hWndButton, loadString(OK_BUTTON_STR).c_str()); ShowWindow(_hWndButton, SW_SHOW); break;
                case Done: SetWindowTextW(_hWndButton, loadString(IDS_OK_BUTTON_FINISH).c_str()); ShowWindow(_hWndButton, SW_SHOW); break;
                default: ShowWindow(_hWndButton, SW_HIDE); break;
                }
            }
            updateTaskbarProgress();
            return 0;
        }
        HANDLE_MSG(WM_TIMER, WPARAM_AS(UINT id)) {
            LOG_ONLY_ONCE;
            if (id == 0)
            {
                setState(Exiting);
                close();
            }
            else if (id == 1 && _state < Exiting)
            {
                InvalidateRect(_hWnd, NULL, FALSE);
            }
            return 0;
        }
        HANDLE_MSG(WM_CLOSE) {
            EnterCriticalSection(&_stateMutex);
            switch (State state = getState())
            {
            case Installing:
                if (IDYES == messageBox(IDS_MB_ABORTINSTALLATION, IDS_MB_CAP_ABORTINSTALLATION, 0, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON1))
                    setStateAndReleaseMutex(Aborting);
                else
                    LeaveCriticalSection(&_stateMutex);
                InvalidateRect(_hWnd, NULL, FALSE);
                return 0;
            case Initializing:
            case Preparing:
            case Aborting:
            case Committing:
            case RollingBack:
                LeaveCriticalSection(&_stateMutex);
                return 0;
            case ReadyToInstall:
            case Done:
            case Exiting:
                setStateAndReleaseMutex(Exiting);
                close();
                return 0;
            }
        }
        case WM_MOVE:
        case WM_MOVING:
        case WM_WINDOWPOSCHANGING:
        case WM_WINDOWPOSCHANGED:
            LOG_ONLY_ONCE;
            break;
        default: {
        #ifdef _DEBUG
            //OutputDebugStringA(strprintf("Message %s wParam 0x%04x lParam 0x%08x\n", getMessageName(msg).c_str(), wParam, lParam).c_str());
        #endif
        }
    }
    return DefWindowProc(_hWnd, msg, wParam, lParam);

#undef HANDLE_MSG
#undef EXPR_AS
#undef WPARAM_AS
#undef WPARAM_HIWORD_AS
#undef WPARAM_LOWORD_AS
#undef LPARAM_AS
#undef LPARAM_HIWORD_AS
#undef LPARAM_LOWORD_AS
}

RECT Installer::getNonClientButtonRect(int button)
{
    RECT rect = { WINDOW_WIDTH - CLOSE_BUTTON_WIDTH - MINIMIZE_BUTTON_WIDTH, 0, WINDOW_WIDTH, CLOSE_BUTTON_HEIGHT };
    if (button == HTMINBUTTON) rect.right -= CLOSE_BUTTON_WIDTH;
    else if (button == HTCLOSE) rect.left += MINIMIZE_BUTTON_WIDTH;
    return rect;
}

void Installer::doPaint()
{
    PAINTSTRUCT ps;
    BITMAP bitmap;
    HDC realDC = BeginPaint(_hWnd, &ps);
    HDC dc = CreateCompatibleDC(realDC);
    HBITMAP buffer = CreateCompatibleBitmap(realDC, WINDOW_WIDTH, WINDOW_HEIGHT);
    SelectObject(dc, buffer);
    IntersectClipRect(dc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom);
    HDC mem = CreateCompatibleDC(dc);

    FillRect(dc, &ps.rcPaint, _backgroundBrush);

    SelectObject(mem, _logoBitmap);
    GetObject(_logoBitmap, sizeof(bitmap), &bitmap);
    BitBlt(dc, (WINDOW_WIDTH - bitmap.bmWidth) / 2, 100, bitmap.bmWidth, bitmap.bmHeight, mem, 0, 0, SRCCOPY);

    int inset;

    inset = _capturedNonClientButton == HTMINBUTTON && _hoveredNonClientButton ? 1 : 0;
    SelectObject(mem, _minimizeBitmap);
    GetObject(_minimizeBitmap, sizeof(bitmap), &bitmap);
    BitBlt(dc, WINDOW_WIDTH - CLOSE_BUTTON_WIDTH - MINIMIZE_BUTTON_WIDTH + 20 + inset, 30 + inset, bitmap.bmWidth, bitmap.bmHeight, mem, 0, 0, SRCCOPY);

    inset = _capturedNonClientButton == HTCLOSE && _hoveredNonClientButton ? 1 : 0;
    SelectObject(mem, _closeBitmap);
    GetObject(_closeBitmap, sizeof(bitmap), &bitmap);
    BitBlt(dc, WINDOW_WIDTH - CLOSE_BUTTON_WIDTH + 19 + inset, 30 + inset, bitmap.bmWidth, bitmap.bmHeight, mem, 0, 0, SRCCOPY);

    std::wstring caption;
    double progress;
    double timeRemaining;
    {
        LockedScope(_drawMutex);
        caption = _caption.str();
        progress = _progress;
        timeRemaining = _timeRemaining;
    }
    if (progress > 1.0)
        progress = 1.0;
    if (progress < 0.0 || timeRemaining == 0.0)
    {
        _spinnerTickCount = 0;
        KillTimer(_hWnd, 1);
    }
    else if (!_spinnerTickCount && ((progress > 0.0 && progress < 0.0) || timeRemaining > 0.0))
    {
        _spinnerTickCount = GetTickCount();
        // Paranoia: if we accidentally got a zero tick count, subtract one
        // (overflowing backwards) so we get a valid non-zero reference point.
        if (_spinnerTickCount == 0)
            _spinnerTickCount--;
        SetTimer(_hWnd, 1, SPINNER_TICK, NULL);
    }

    if (!caption.empty())
    {
        drawText(dc, { 50, 199, WINDOW_WIDTH - 50, 234 }, CAPTION_WHITE, DT_CENTER, caption.c_str());
    }

    if (progress >= 0.0)
    {
        RECT rcBack = { 50, 246, WINDOW_WIDTH - 50, 248 };
        RECT rcFore = rcBack;
        rcFore.right = (LONG)(rcFore.left + (rcFore.right - rcFore.left) * progress);
        rcBack.left = rcFore.right + 1;
        if (rcFore.left < rcFore.right) FillRect(dc, &rcFore, _progressForegroundBrush);
        if (rcBack.left < rcBack.right) FillRect(dc, &rcBack, _progressBackgroundBrush);
    }

    if (_spinnerTickCount)
    {
        DWORD now = GetTickCount();
        DWORD stage = ((now - _spinnerTickCount) / SPINNER_TICK) % 4;
        const DWORD width = 2 * SPINNER_SIZE + 1 * SPINNER_SPACING;
        const DWORD x = WINDOW_WIDTH / 2 - width / 2;
        const DWORD y = 280;
        for (DWORD i = 0; i < 4; i++)
        {
            RECT rcBlock;
            rcBlock.left = x + (i >= 2 ? (1 - i % 2) : (i % 2)) * (SPINNER_SIZE + SPINNER_SPACING);
            rcBlock.top = y + (i / 2) * (SPINNER_SIZE + SPINNER_SPACING);
            rcBlock.right = rcBlock.left + SPINNER_SIZE;
            rcBlock.bottom = rcBlock.top + SPINNER_SIZE;
            FillRect(dc, &rcBlock, i == stage ? _spinnerActiveBrush : _spinnerInactiveBrush);
        }
    }

    BitBlt(realDC, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, dc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
    DeleteDC(dc);
    DeleteObject(buffer);
    DeleteDC(mem);
    EndPaint(_hWnd, &ps);
}

void Installer::drawText(HDC dc, const RECT& rect, COLORREF color, DWORD flags, utf16ptr text, int spacing)
{
    HGDIOBJ old = SelectObject(dc, _mainFont);

    DWORD fontLangInfo = GetFontLanguageInfo(dc);
    DWORD gcpFlags = 0;
    if(fontLangInfo & GCP_GLYPHSHAPE)
    {
        // We must pass GCP_GLYPHSHAPE if the font supports it for this
        // language, it is required for legibility in Arabic.
        gcpFlags |= GCP_GLYPHSHAPE;
        // Spacing renders shaped/ligated languages illegible.
        spacing = 0;
    }

    int dx[200];
    WCHAR glyphs[200];
    GCP_RESULTS gcpResults = {0};
    gcpResults.lStructSize = sizeof(gcpResults);
    gcpResults.lpDx = dx;
    gcpResults.lpGlyphs = glyphs;
    gcpResults.nGlyphs = sizeof(glyphs);
    auto tab = wcschr(text, '\t');

    // Zero the first entry in glyphs.  This value has special significance to
    // GetCharacterPlacement() for shaped/ligated languages.
    glyphs[0] = 0;
    DWORD wh = GetCharacterPlacement(dc, text, wcslen(text), rect.right - rect.left, &gcpResults, GCP_USEKERNING|gcpFlags);
    int w = LOWORD(wh), h = HIWORD(wh);

    int tabOffset = 0;
    if (tab)
    {
        dx[tab - text] += 10;
        w += 10;
    }
    for (int i = 0; i < gcpResults.nGlyphs; i++)
    {
        dx[i] += spacing;
        w += spacing;
        if (tab && i <= tab - text)
        {
            tabOffset += dx[i];
        }
    }

    int x = rect.left, y = rect.top;
    if (flags & DT_CENTER)
        x += (rect.right - rect.left - w) / 2;
    else if (flags & DT_RIGHT)
        x = rect.right - w;
    if (flags & DT_VCENTER)
        y += (rect.bottom - rect.top - h) / 2;
    else if (flags & DT_BOTTOM)
        y = rect.bottom - h;

    SetTextColor(dc, color);
    SetBkColor(dc, WINDOW_BACKGROUND);
    ExtTextOutW(dc, x, y, ETO_GLYPH_INDEX, &rect, glyphs, tab ? tab - text : gcpResults.nGlyphs, dx);
    if (tab)
    {
        SetTextColor(dc, CAPTION_GRAY);
        int lastMode = SetBkMode(dc, TRANSPARENT);
        ExtTextOutW(dc, x + tabOffset, y, ETO_GLYPH_INDEX, &rect, glyphs + (tab - text + 1), gcpResults.nGlyphs - (tab - text + 1), dx + (tab - text + 1));
        SetBkMode(dc, lastMode);
    }

    SelectObject(dc, old);
}

#undef LockedScope
