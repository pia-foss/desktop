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

#include <common/src/common.h>
#line SOURCE_FILE("clientlib.cpp")

#include "clientlib.h"
#include <common/src/dtop.h>

KAPPS_CORE_LOG_MODULE(clientlib, "src/clientlib.cpp")

int runClient(bool logToStdErr, int argc, char *argv[], int (*run)(int argc, char *argv[]))
{
    // If we can't initialize the logger at all, we can only print that
    // exception to stderr.
    try
    {
        setUtf8LocaleCodec();
        Logger::initialize(logToStdErr);
    }
    catch(const Error &ex)
    {
        std::cerr << "Can't initialize logging: "
            << ex.errorString().toStdString() << " at " << ex.location()
            << std::endl;
        return -1;
    }
    catch(const std::exception &ex)
    {
        std::cerr << "Can't initialize logging: " << ex.what() << std::endl;
        return -1;
    }

    // Since logger was initialized, we can log any other exceptions using
    // qCritical().  If disk logging was enabled and initialized, these will go
    // to the log files, otherwise they will only go to stderr.
    try
    {
        // Initialize other kapps::core integration points
        initKApps();
        return run(argc, argv);
    }
    catch(const Error &ex)
    {
        qCritical() << "Exiting due to exception: " << ex << "at"
            << ex.location();
        return -2;
    }
    catch(const std::exception &ex)
    {
        qCritical() << "Exiting due to exception: " << ex.what();
        return -2;
    }
}
