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

#include "tasks.h"
#include "installer.h"
#include "util.h"

bool g_rebootAfterInstall = false;
bool g_rebootBeforeInstall = false;

#ifdef INSTALLER
#define ERROR_MSG_SUFFIX_ABORT IDS_MB_SUFFIX_INSTALL_ABORT
#define ERROR_MSG_SUFFIX_RETRY IDS_MB_SUFFIX_INSTALL_RETRY
#define ERROR_MSG_SUFFIX_IGNORE IDS_MB_SUFFIX_INSTALL_IGNORE
#define ERROR_MSG_SUFFIX_RETRYIGNORE IDS_MB_SUFFIX_INSTALL_RETRYIGNORE
#else
#define ERROR_MSG_SUFFIX_ABORT IDS_MB_SUFFIX_UNINSTALL_ABORT
#define ERROR_MSG_SUFFIX_RETRY IDS_MB_SUFFIX_UNINSTALL_RETRY
#define ERROR_MSG_SUFFIX_IGNORE IDS_MB_SUFFIX_UNINSTALL_IGNORE
#define ERROR_MSG_SUFFIX_RETRYIGNORE IDS_MB_SUFFIX_UNINSTALL_RETRYIGNORE
#endif

ErrorType InstallerError::raise(ErrorType type, UIString str)
{
#ifdef UNINSTALLER
    type |= ShouldIgnore;
#endif

    LOG("ERROR: %ls", str.str());
    // Never display dialog boxes in silent mode
    if (g_silent || type & Silent)
    {
        if ((type & (Ignore | ShouldIgnore)) == (Ignore | ShouldIgnore))
            return Ignore;
        goto abort;
    }
    // Skip ignorable errors in passive mode
    if (g_passive && (type & (Ignore | ShouldIgnore)) == (Ignore | ShouldIgnore))
        return Ignore;

    UINT mbType;
    UINT msgSuffixId = 0;
    switch (type & (Ignore | Retry))
    {
    case Ignore | Retry:
        mbType = MB_ICONWARNING | MB_CANCELTRYCONTINUE | (type & ShouldIgnore ? MB_DEFBUTTON3 : MB_DEFBUTTON2);
        msgSuffixId = ERROR_MSG_SUFFIX_RETRYIGNORE;
        break;
    case Ignore:
        mbType = MB_ICONWARNING | MB_YESNO;
        msgSuffixId = ERROR_MSG_SUFFIX_IGNORE;
        break;
    case Retry:
        mbType = MB_ICONWARNING | MB_RETRYCANCEL;
        msgSuffixId = ERROR_MSG_SUFFIX_RETRY;
        break;
    default:
    case Abort:
        mbType = MB_ICONERROR | MB_OK;
        msgSuffixId = ERROR_MSG_SUFFIX_ABORT;
        break;
    }
    switch (messageBox(str, IDS_MB_CAP_ERROR, msgSuffixId, mbType))
    {
    default:
    case IDOK:
    case IDCANCEL:
    case IDABORT:
    case IDNO:
    abort:
        if (type & NoThrow)
            return Abort;
        else
            throw InstallerError(std::move(str));
    case IDRETRY:
    case IDTRYAGAIN:
        return Retry;
    case IDYES:
    case IDCONTINUE:
        return Ignore;
    }
}

void InstallerError::abort(UIString str)
{
    raise(Abort, std::move(str));
}

CaptionTask::CaptionTask(UIString caption)
    : _caption(std::move(caption))
{

}

void CaptionTask::execute()
{
    if (_caption)
        _listener->setCaption(_caption);
}
