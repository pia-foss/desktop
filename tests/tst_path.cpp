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

#include "common.h"
#include <QtTest>

#include "path.h"
#include "brand.h"

#include <QDir>



class tst_path : public QObject
{
    Q_OBJECT

private slots:
    void testNativePaths()
    {
    #ifdef Q_OS_WIN
        Path winDir = Path("C:\\") / "Program Files" / "Private Internet Access";
        QCOMPARE(QDir::toNativeSeparators(winDir), "C:\\Program Files\\Private Internet Access");
        QCOMPARE(QDir::toNativeSeparators(winDir / "../.."), "C:");
        QCOMPARE(QDir::toNativeSeparators(winDir / "../../.."), "C:");
    #else
        Path posixDir = Path("/usr/share/privateinternetaccess");
        QCOMPARE(posixDir / "var", QStringLiteral("/usr/share/privateinternetaccess/var"));
        QCOMPARE(posixDir / "..", QStringLiteral("/usr/share"));
        QCOMPARE(posixDir / ".." / "..", QStringLiteral("/usr"));
        QCOMPARE(posixDir / "../..", QStringLiteral("/usr"));
        QCOMPARE(posixDir / "../../..", QStringLiteral("/"));
        QCOMPARE(posixDir / "../../../..", QStringLiteral("/"));
    #endif
    }

    void testDefaultPaths () {
        Path::initializePreApp();
        Path::initializePostApp();
        QVERIFY(Path::ExecutableDir.str().contains(Path::BaseDir));
#ifdef Q_OS_MAC
        QVERIFY(Path::DaemonDataDir.str().contains(BRAND_IDENTIFIER));
        QVERIFY(Path::DaemonSettingsDir.str().contains(BRAND_IDENTIFIER));
#endif
#ifdef Q_OS_LINUX
        QVERIFY(Path::DaemonDataDir.str().contains(BRAND_CODE));
#endif
    }

    void testCreatePaths () {
        QTemporaryDir tempDir;
        tempDir.setAutoRemove(true);

        Path tmpPath{tempDir.path()};

        QVERIFY(!QFile::exists(tempDir.filePath("test1")));
        (tmpPath / QStringLiteral("test1")).touch();
        QVERIFY(QFile::exists(tempDir.filePath("test1")));


        QVERIFY(!QFile::exists(tempDir.filePath("dir1/dir2")));
        (tmpPath / "dir1" / "dir2").mkpath();
        QVERIFY(QFile::exists(tempDir.filePath("dir1/dir2")));

        tempDir.remove();
    }

};

QTEST_GUILESS_MAIN(tst_path)
#include TEST_MOC
