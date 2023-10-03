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
#include <QtTest>

#include <common/src/exec.h>

class tst_exec : public QObject
{
    Q_OBJECT

private:

#ifdef Q_OS_UNIX
    QString _command{"ls"};
    QStringList _args{QStringLiteral("-a")};
#else
    QString _command{"where.exe"};
    QStringList _args{QStringLiteral("/Q"), QStringLiteral("where.exe")};
#endif

private slots:

    void testCmd()
    {
        // Success
        auto result1 = Exec::cmd(_command, _args, true);
        QVERIFY(result1 == 0);

        // Failure
        auto result2 = Exec::cmd(QStringLiteral("c0mmandDoesNotExist"), {}, true);
        QVERIFY(result2 != 0);
    }

    void testCmdWithErrorMessages()
    {
        // the -2 code is defined in waitForExitCode() see util.cpp
        QTest::ignoreMessage(QtWarningMsg, "(-2)\"c0mmandDoesNotExist\"");
        Exec::cmd(QStringLiteral("c0mmandDoesNotExist"), {}, false);
    }

    void testCmdWithRegex()
    {
// We need to use this wacky way of limiting tests to a specific
// platform. If we did the obvious (wrap all the related methods in a single #ifdef)
// then Qt MOC would not recognize those methods as tests, and would fail to execute them.
#ifdef Q_OS_UNIX
        Executor executor{CURRENT_CATEGORY};

        // ls -a will return . and .. so the Regex will match the "."
        auto regexMatch = executor.cmdWithRegex(_command, _args, QRegularExpression{R"(\.)"});
        QVERIFY(regexMatch.hasMatch());
#endif
    }

    void testBash()
    {
#ifdef Q_OS_UNIX
        // Success
        auto result1 = Exec::bash(QStringLiteral("ls -a | grep ."), true);
        QVERIFY(result1 == 0);

        // Failure
        // -a will return . and .. so grep -v . should always fail
        auto result2 = Exec::bash(QStringLiteral("ls -a | grep -v ."), true);
        QVERIFY(result2 != 0);
#endif
    }

    void testCmdWithOutput()
    {
#ifdef Q_OS_UNIX
        // Success
        auto output1 = Exec::cmdWithOutput(QStringLiteral("ls"), { QStringLiteral("/bin/bash") });
        QVERIFY(output1 == QStringLiteral("/bin/bash"));

        // Failure
        auto output2 = Exec::cmdWithOutput(QStringLiteral("c0mmandDoesNotExist"), {});
        QVERIFY(output2.isEmpty());
#endif
    }

    void testBashWithOutput()
    {
#ifdef Q_OS_UNIX
        // Success
        auto output1 = Exec::bashWithOutput(QStringLiteral("ls /bin/bash | grep bash"));
        QVERIFY(output1 == QStringLiteral("/bin/bash"));

        // Failure
        auto output2 = Exec::bashWithOutput(QStringLiteral("which bash | grep -v bash"));
        QVERIFY(output2.isEmpty());
#endif
    }
};

QTEST_GUILESS_MAIN(tst_exec)
#include TEST_MOC
