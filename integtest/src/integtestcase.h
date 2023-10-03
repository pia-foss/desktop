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
#line HEADER_FILE("integtestcase.h")

#ifndef INTEGTESTCASE_H
#define INTEGTESTCASE_H

#include <common/src/output.h>
#include <QObject>
#include <QTest>
#include <deque>

// IntegTestCase is used as the test case framework for integration tests.
//
// Integration tests are built with QTest, but with multiple "test cases"
// (objects containing "test functions" in Qt parlance).
//
// Normally, QTest prefers to only have one "test case", per executable.  This
// isn't practical for integration tests; copying and running all the
// executables would be cumbersome.
//
// IntegTestCase and IntegTestCaseDef are designed to support multiple test
// cases in one executable.
// - IntegTestCase implements global execution/failure counts and a final test
//   summary.
// - IntegTestCaseDef identifies the test cases and provides a way to execute
//   them all.
//
// Test cases are defined as an object with the test functions as slots, just
// like any other QTest test case.  The only differences are that the object
// is derived from IntegTestCase, and an IntegTestCaseDef is created to identify
// it:
//     class TestSomething : public IntegTestCase
//     {
//         Q_OBJECT
//
//     private:
//         // If you need init/cleanup/initTestCase/cleanupTestCase, override
//         // the IntegTestCase::integ*() methods instead
//         // *Don't* put these in slots, or Qt will invoke them as test
//         // functions too.
//         virtual void integInit() override;
//
//     private slots:
//         ...test case functions...
//     };
//
//     namespace
//     {
//         IntegTestCaseDef<TestSomething> _def;
//     }
//
// This approach has some limitations but they are reasonable for integration
// tests.  Some of these could be improved in the future.
// - Tests cannot implement init/cleanup/initTestCase/cleanupTestCase due to
//   the way the test failure counts are implemented.  Instead, override the
//   IntegTestCase::integ*() methods.
// - QTest log files won't work, we have to use the stdout logging.
// - In integCleanup(), you _must_ use VERIFY_CONTINUE/COMPARE_CONTINUE to check
//   cleanup that could fail.  Using QVERIFY/QCOMPARE won't work, because Qt
//   would not have observed the failure by the time IntegTestCase checks it.
//   (Allowing cleanup to fail is probably a bad idea anyway.)

// Base test case implementation with IntegTestCase - implements global success/failure
// count tracking and execute().
class IntegTestCase : public QObject
{
    Q_OBJECT

private:
    static unsigned _totalPassed;
    static unsigned _totalFailed;
    // Used to implement VERIFY_CONTINUE and COMPARE_CONTINUE
    static bool _continueAssertionFailed;

public:
    // Signal a failure that does not end a test case.  cleanup() will fail the
    // test due to this if it has not already failed.
    // Used to implement VERIFY_CONTINUE and COMPARE_CONTINUE
    static void continueAssertionFailed() { _continueAssertionFailed = true; }
    static void printTotals();
    static unsigned returnTotalFailed();

    static void verifyImpl(bool success, const char *expr, const char *file, int line);
    template<class Actual, class Expected>
    static void compareContinueImpl(const Actual &actual, const Expected &expected,
                                    const char *actualExpr, const char *expectedExpr,
                                    const char *file, int line)
    {
        if(!static_cast<bool>((actual) == (expected)))
        {
            const char *actualVal = QTest::toString(actual);
            const char *expectedVal = QTest::toString(expected);

            outln() << "FAIL!  :" << QTest::currentTestFunction() << "Compared values are not the same";
            outln() << "   Actual (" << actualExpr << "): " << actualVal;
            outln() << "   Expected (" << expectedExpr << "): " << expectedVal;
            outln() << "   Loc: [" << file << "(" << line << ")]";
            continueAssertionFailed();
        }
    }

private:
    void countTestCase();

private slots:
    void init();
    void cleanup();
    void initTestCase();
    void cleanupTestCase();

public:
    virtual void integInit() {}
    virtual void integCleanup() {}
    virtual void integInitTestCase() {}
    virtual void integCleanupTestCase() {}
};


// The QVERIFY() / QCOMPARE() macros end the test case with 'return;'; so
// they can't be factored out of test cases.  (Using it from a void function
// would only skip the rest of that function, using it from a non-void function
// does not compile.)
//
// These macros perform a test assertion without aborting if the comparison
// fails.  This is reasonable in many tests that can continue running the rest
// of the test even if an assertion fails.
//
// It'd be nice if we could abort a test function with exception semantics, but
// Qt stops running all tests if a test throws an exception.
//
// These are implemented by tracking continuable failures in IntegTestCase
// manually.  We can't use trickery to call QVERIFY() or QCOMPARE() in this
// context, because QTest will count failures incorrectly if more than one
// assertion fails.  This isn't perfect - it doesn't produce the same failure
// output that QVERIFY would - but factoring out all QVERIFYs to the top-level
// test function would lose a lot of specific information, and involve a lot of
// bool's that invite mistakes.
#define VERIFY_CONTINUE(statement) \
    ::IntegTestCase::verifyImpl(static_cast<bool>(statement), #statement, __FILE__, __LINE__)
#define COMPARE_CONTINUE(actual, expected) \
    ::IntegTestCase::compareContinueImpl((actual), (expected), #actual, #expected, __FILE__, __LINE__)

// Meta-object used to identify all defined integration test types, so they can
// be run with execute()
class IntegTestCaseDefBase
{
private:
    // All test case definitions container, in static method to ensure correct
    // initialization order
    static std::deque<IntegTestCaseDefBase*> &allTestCases();

public:
    // Execute all test cases
    static unsigned executeAll(int argc, char **argv);

public:
    IntegTestCaseDefBase();
    ~IntegTestCaseDefBase();
    IntegTestCaseDefBase(const IntegTestCaseDefBase &) = delete;
    IntegTestCaseDefBase &operator=(const IntegTestCaseDefBase &) = delete;

public:
    // Get the name of this test case
    virtual const char *name() const = 0;
    // Execute this test case with QTest::qExec()
    virtual int execute(int argc, char **argv) const = 0;
};

template<class TestCase>
class IntegTestCaseDef : public IntegTestCaseDefBase
{
public:
    virtual const char *name() const override { return TestCase::staticMetaObject.className(); }
    virtual int execute(int argc, char **argv) const override
    {
        TestCase test{};
        return QTest::qExec(&test, argc, argv);
    }
};

#endif
