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
#include <common/src/settings/daemonsettings.h>
#include <QtTest>

class tst_daemonsettings : public QObject
{
    Q_OBJECT

private slots:
    void testSplitTunnelActivationFailedDefaultsFalse()
    {
        DaemonSettings settings;
        QCOMPARE(settings.splitTunnelActivationFailed(), false);
    }

    void testSplitTunnelActivationFailedRoundTrip()
    {
        DaemonSettings settings;
        settings.splitTunnelActivationFailed(true);
        QCOMPARE(settings.splitTunnelActivationFailed(), true);

        QJsonObject json = settings.toJsonObject();
        QCOMPARE(json.value(QStringLiteral("splitTunnelActivationFailed")).toBool(), true);

        DaemonSettings restored;
        restored.readJsonObject(json);
        QCOMPARE(restored.splitTunnelActivationFailed(), true);
    }

    void testSplitTunnelActivationFailedClearsToFalse()
    {
        DaemonSettings settings;
        settings.splitTunnelActivationFailed(true);
        settings.splitTunnelActivationFailed(false);
        QCOMPARE(settings.splitTunnelActivationFailed(), false);
    }
};

QTEST_GUILESS_MAIN(tst_daemonsettings)
#include TEST_MOC
