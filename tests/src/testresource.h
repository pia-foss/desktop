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

#include "common.h"
#line HEADER_FILE("testresource.h")

#ifndef TESTRESOURCE_H
#define TESTRESOURCE_H

namespace TestResource
{
    // Initialize test resources.  This ensures that the resources are linked
    // into this unit test (needed because they're compiled into a static
    // library) and initializes them for use with Qt.
    //
    // load() calls this automatically; this is only needed if the code under
    // test tries to load a resouce without using TestResource::load() (such as
    // for CA certificates).
    void init();

    // Read a resource into a QByteArray().  Also calls init() to ensure the
    // resources are loaded.  Pass a complete file path including the leading
    // colon, such as ":/ca/rsa_4096.crt".
    QByteArray load(const QString &path);
}

#endif
