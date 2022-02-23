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

#include "common.h"
#line HEADER_FILE("clientlib.h")
#ifndef CLIENTLIB_H

#ifdef DYNAMIC_CLIENTLIB
    #ifdef _WIN32
        #ifdef BUILD_CLIENTLIB
            #define CLIENTLIB_EXPORT __declspec(dllexport)
        #else
            #define CLIENTLIB_EXPORT __declspec(dllimport)
        #endif
    #else
        #define CLIENTLIB_EXPORT __attribute__((visibility("default")))
    #endif
#else
    #define CLIENTLIB_EXPORT
#endif

// Initialize logging and run the client.
//
// Prints diagnostics if the application exits due to an unhandled exception.
// The app still exits, but this provides some output for supportability.
//
// Exceptions from initializing the logger are logged to stderr (since the
// logger itself couldn't be initialized). Any other exceptions from run() are
// handled by logging with qCritical().
CLIENTLIB_EXPORT int runClient(bool logToStdErr, int argc, char *argv[],
                               int (*run)(int argc, char *argv[]));

#endif
