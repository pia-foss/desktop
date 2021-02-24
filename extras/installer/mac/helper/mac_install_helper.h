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

#ifndef MAC_INSTALL_HELPER_H
#define MAC_INSTALL_HELPER_H

#include "brand.h"

// Constants for XPC communication used by install helper and client.
namespace MacInstall
{
    inline const char serviceIdentifier[] = BRAND_IDENTIFIER ".installhelper";

    // Specifies action to take - encoded as int64, value from Actions enum
    inline const char xpcKeyAction[] = "action";

    // Values for "action" key
    enum Action : int
    {
        Invalid = 0,    // XPC returns 0 if there is no value at the specified key
        Install,
        Uninstall,
    };

    // "Install" action - path to app bundle to install from.
    // (Usually a downloaded app bundle, but it could be in /Applications if a
    // reinstall is needed.)
    inline const char xpcKeyAppPath[] = "apppath";

    // "Install" action - name of OS user to use when migrating settings (if
    // applicable)
    inline const char xpcKeyInstallingUser[] = "installinguser";

    // Values for "result" key
    enum Result : int
    {
        Error = 0,   // Used if the key is missing
        Success,
    };

    // Specifies the result of a request - int64, from Result enum
    inline const char xpcKeyResult[] = "result";
}

#endif
