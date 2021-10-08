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

#include "integtestcase.h"
#include "cliharness.h"
#include "path.h"
#include "version.h"

int main(int argc, char **argv)
{
    Path::initializePreApp();
    QCoreApplication app{argc, argv};
    Path::initializePostApp();

    auto piactlVersion = CliHarness::getVersion();
    // Exact match required, including build tags - the integration test
    // artifact should come from the same build.
    if(piactlVersion != Version::semanticVersion())
    {
        outln() << "WARNING: Integration test version mismatch";
        outln() << "Test version:" << Version::semanticVersion();
        outln() << "Installed version:" << piactlVersion;
        if(qgetenv("PIA_INTEG_MISMATCH") == QByteArrayLiteral("1"))
        {
            outln() << "Continuing anyway, PIA_INTEG_MISMATCH=1 set";
        }
        else
        {
            outln() << "Aborting test.  (Run with PIA_INTEG_MISMATCH=1 to override.)";
            return 1;
        }
    }

    outln() << "Running tests for version:" << Version::semanticVersion();

    IntegTestCaseDefBase::executeAll(argc, argv);
    return 0;
}
