// Copyright (c) 2019 London Trust Media Incorporated
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

class tst_check : public QObject
{
    Q_OBJECT

private slots:
    void checkIfWithTrueCondition()
    { CHECK_IF(value == 1, 2); }
    void checkIfWithFalseCondition()
    { QVERIFY_EXCEPTION_THROWN(CHECK_IF(value == 1, 1), CheckError); }
    void checkIfTrueWithTrue()
    { QVERIFY_EXCEPTION_THROWN(CHECK_IF_TRUE(1 == 1), CheckError); }
    void checkIfTrueWithFalse()
    { CHECK_IF_TRUE(1 == 2); }
};

QTEST_APPLESS_MAIN(tst_check)
#include TEST_MOC
