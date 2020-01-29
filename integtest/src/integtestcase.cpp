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
#line SOURCE_FILE("integtestcase.cpp")

#include "integtestcase.h"
#include "output.h"

unsigned IntegTestCase::_totalPassed{0};
unsigned IntegTestCase::_totalFailed{0};
bool IntegTestCase::_continueAssertionFailed{false};

void IntegTestCase::printTotals()
{
    outln() << "TOTAL:" << _totalPassed << "passed," << _totalFailed
        << "failed";
}

void IntegTestCase::verifyImpl(bool success, const char *expr, const char *file, int line)
{
    if(!success)
    {
        outln() << "FAIL!  :" << QTest::currentTestFunction() << expr;
        outln() << "   Loc: [" << file << "(" << line << ")]";
        continueAssertionFailed();
    }
}

void IntegTestCase::countTestCase()
{
    // If Qt has already failed the test, do not call QFAIL again, even if the
    // test _also_ logged a continuable failure
    if(QTest::currentTestFailed())
    {
        // In case a continuable failure was also logged
        _continueAssertionFailed = false;
        outln() << "Test cleanup -" << QTest::currentTestFunction() << "- test failed";
        ++_totalFailed;
    }
    else if(_continueAssertionFailed)
    {
        _continueAssertionFailed = false;
        outln() << "Test cleanup -" << QTest::currentTestFunction() << "- test failed";
        ++_totalFailed;
        // This will return, can't do anything after this.
        QFAIL("Failed due to prior assertion failure");
    }
    else
    {
        outln() << "Test cleanup -" << QTest::currentTestFunction() << "- test passed";
        ++_totalPassed;
    }
}

void IntegTestCase::init()
{
    _continueAssertionFailed = false;
    // Allow test case to initialize
    integInit();
    // If the test has already failed (with QCOMPARE/QVERIFY), Qt does not call
    // cleanup, so count the test case now.
    // Also bail if the test logged a continueable failure, for consistency with
    // QCOMPARE/QVERIFY.
    if(_continueAssertionFailed || QTest::currentTestFailed())
    {
        qInfo() << "Test init -" << QTest::currentTestFunction()
            << "- already failed, will not perform cleanup";
        countTestCase();
    }
}

void IntegTestCase::cleanup()
{
    integCleanup();
    countTestCase();
}

void IntegTestCase::initTestCase()
{
    _continueAssertionFailed = false;
    integInitTestCase();
    // Oddly, QTest counts "initTestCase" and "cleanupTestCase" in the total
    // success/failure counts, as if they were "test functions".  Count them too
    // for consistency.
    countTestCase();
}

void IntegTestCase::cleanupTestCase()
{
    _continueAssertionFailed = false;
    integCleanupTestCase();
    countTestCase();
}

IntegTestCaseDefBase::IntegTestCaseDefBase()
{
    // Add this object to all test case definitions
    allTestCases().push_back(this);
}

IntegTestCaseDefBase::~IntegTestCaseDefBase()
{
    // We could remove the test case from allTestCases(), but there's not much
    // point, we just assume that test case execution occurs before static
    // destruction.
}

auto IntegTestCaseDefBase::allTestCases() -> std::deque<IntegTestCaseDefBase*>&
{
    static std::deque<IntegTestCaseDefBase*> _allTestCases;
    return _allTestCases;
}

void IntegTestCaseDefBase::executeAll(int argc, char **argv)
{
    // Sort all the test cases by name
    auto allTests = allTestCases();

    std::sort(allTests.begin(), allTests.end(), [](const auto &pFirst, const auto &pSecond)
    {
        // Class invariant - no nullptrs in this container
        Q_ASSERT(pFirst);
        Q_ASSERT(pSecond);
        return QLatin1String{pFirst->name()} < pSecond->name();
    });

    for(const auto &pTest : allTests)
    {
        Q_ASSERT(pTest);    // Class invariant - no nullptrs in container
        pTest->execute(argc, argv);
    }

    IntegTestCase::printTotals();
}
