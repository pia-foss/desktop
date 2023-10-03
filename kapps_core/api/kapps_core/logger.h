// Copyright (c) 2023 Private Internet Access, Inc.
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

#ifndef KAPPS_CORE_API_LOGGER_H
#define KAPPS_CORE_API_LOGGER_H

#include "core.h"
#include "stringslice.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

// **********
// * Logger *
// **********
//
// The logger is used through Desktop modules to collect debug log messages and
// send them to a product-controlled logging sink.  The public C-linkage API
// can capture log messages and control whether logging is enabled, but it
// cannot write to the logger.
//
//

#define KAPPS_CORE_LOG_MESSAGE_LEVEL_FATAL 0
#define KAPPS_CORE_LOG_MESSAGE_LEVEL_ERROR 1
#define KAPPS_CORE_LOG_MESSAGE_LEVEL_WARNING 2
#define KAPPS_CORE_LOG_MESSAGE_LEVEL_INFO 3
#define KAPPS_CORE_LOG_MESSAGE_LEVEL_DEBUG 4

// All of the information provided with a log message, such as module/category/
// file/line, etc.
//
// All strings are UTF-8.  It is possible that improper code unit sequences
// could occur when forwarding output from child processes, etc., so the log
// callback should be able to consume improper UTF-8 messages (ideally without
// discarding them).
typedef struct KACLogMessage
{
    const KACStringSlice module;
    const KACStringSlice category;
    int level;  // KAPPS_CORE_LOG_MESSAGE_LEVEL_*
    const KACStringSlice file;
    int line;
    // The message is a null-terminated UTF-8 string.
    const char *pMessage;
} KACLogMessage;

// Callbacks for the logger module

// The callback implemented by the application to receive log messages.
//
// This callback can be called on any thread, including APC threads on
// Windows.  Calls from the logger are serialized (by the logger mutex).  The
// log callback itself MUST NOT create any log messages; behavior if it does is
// undefined.  (The log callback could write diagnostics directly to is own
// output though.)
typedef void (*KACLogCallbackWrite)(
    void *pContext, // Context for user, specified when installing callback
    const KACLogMessage *pMessage
);

// Structure defining the log callback function as well as a context pointer, if
// one is needed.
typedef struct KACLogCallback
{
    void *pContext; // User context pointer, forwarded to callbacks
    KACLogCallbackWrite pWriteFn;
} KACLogCallback;

// Initialize the logger with a callback.  This must be called before any traces
// can be written; traces before this are discarded.
//
// The KACLogCallback struct is copied.  Calls to the log callback are
// serialized with changes to the callback from this function; if the callback
// is changed then the old one will no longer be called from any thread.
KAPPS_CORE_EXPORT void KACLogInit(KACLogCallback *pCallback);

// Enable or disable logging.  Logging is initially disabled.  This can be
// called before installing a log callback; traces will start to be written
// once the callback is installed.
KAPPS_CORE_EXPORT void KACEnableLogging(int enable);
// Check whether logging is enabled.
KAPPS_CORE_EXPORT int KACLoggingEnabled();

#ifdef __cplusplus
}
#endif

#endif
