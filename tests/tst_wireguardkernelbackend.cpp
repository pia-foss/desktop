// Copyright (c) 2026 Private Internet Access, Inc.
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
#include "daemon/src/linux/wireguardkernelbackend.h"
#include <QtTest>

class tst_wireguardkernelbackend : public QObject
{
    Q_OBJECT

private slots:
    // The kernel module cannot apply Amnezia obfuscation.  If obfuscation
    // parameters are ever routed to this backend, creating the interface has to
    // fail immediately - configuring vanilla WireGuard instead would only fail
    // later as a handshake timeout against a server expecting obfuscation.
    void testRejectsObfuscation()
    {
        WireguardKernelBackend backend;
        wg_device wgDev{};

        // The returned task is discarded explicitly - createInterface() throws
        // before producing one, and Async is marked Q_REQUIRED_RESULT
        QVERIFY_EXCEPTION_THROWN(
            (void)backend.createInterface(wgDev,
                                          std::optional<AmneziaObfuscation>{AmneziaObfuscation{}},
                                          qMakePair(QHostAddress{}, 0)),
            Error);
    }
};

QTEST_GUILESS_MAIN(tst_wireguardkernelbackend)
#include TEST_MOC
