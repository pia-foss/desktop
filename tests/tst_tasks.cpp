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

#include "async.h"

class tst_tasks : public QObject
{
    Q_OBJECT

    static int add(int x) { return x + 1; }
    int memberAdd(int x) { return add(x); }
    static int handle(const Error& error) { qWarning().noquote() << qEnumToString(error.code()); return 0; }
    int memberHandle(const Error& error) { return handle(error); }
    static QString printable(const Error& error, int x) { return error ? QStringLiteral("error") : QString::number(x); }
    QString memberPrintable(const Error& error, int x) { return printable(error, x); }

    void waitForTasksToDie()
    {
        QTRY_VERIFY(BaseTask::getTaskCount() == 0);
    }

    Error::Code err(int i) { return static_cast<Error::Code>(i); }

    enum AggregateValueType { ByIndex, InOrder };
    template<typename Output, typename Input>
    void testAggregate(Async<Output> (*fn)(const QVector<Async<Input>>& list), AggregateValueType type, int inputCount, const QVector<Input>& actions, BaseTask::State state, const Output& result, Error::Code errorCode)
    {
        QVector<Async<Input>> inputs;
        inputs.reserve(inputCount);
        for (int i = 0; i < inputCount; i++)
            inputs.append(Async<Input>::create());
        auto output = fn(inputs);
        int index = 0;
        for (int action : actions)
        {
            if (action > 0)
                inputs[action - 1]->resolve(type == InOrder ? ++index : action);
            else if (action < 0)
                inputs[-action - 1]->reject(Error(HERE, static_cast<Error::Code>(type == InOrder ? -++index : action)));
        }
        QCOMPARE(output->state(), state);
        QCOMPARE(output->error().code(), errorCode);
        if (state == BaseTask::Resolved)
            QCOMPARE(output->result(), result);
        else if (state == BaseTask::Pending)
            output.abandon();
    }

private slots:
    void init()
    {
        BaseTask::resetTaskIndex();
        QCOMPARE(BaseTask::getTaskCount(), 0);
    }
    void cleanup()
    {
        QCOMPARE(BaseTask::getTaskCount(), 0);
        BaseTask::rejectAllTasks(true);
    }

    void staticResolve()
    {
        auto voidTask = Async<void>::resolve();
        QVERIFY(voidTask->isFinished());
        QVERIFY(voidTask->isResolved());
        QVERIFY(!voidTask->isRejected());
        auto intTask = Async<int>::resolve(5);
        QVERIFY(intTask->isResolved());
        QCOMPARE(intTask->result(), 5);
        auto stringTask = Async<QString>::resolve("test");
        QVERIFY(stringTask->isResolved());
        QCOMPARE(stringTask->result(), "test");
        auto ptr = QSharedPointer<int>::create(6);
        auto ptrTask = Async<QSharedPointer<int>>::resolve(ptr);
        QVERIFY(ptrTask->isResolved());
        QCOMPARE(ptrTask->result(), ptr);
    }
    void staticReject()
    {
        auto voidTask = Async<void>::reject(UnknownError(HERE));
        QVERIFY(voidTask->isFinished());
        QVERIFY(voidTask->isRejected());
        QVERIFY(!voidTask->isResolved());
        QCOMPARE(voidTask->error().code(), Error::Unknown);
    }
    void chainThenFunctionPointer()
    {
        auto sourceTask = Async<int>::create();
        auto thenTask = sourceTask->then(add);
        QVERIFY(thenTask->isPending());
        sourceTask->resolve(5);
        QVERIFY(thenTask->isResolved());
        QCOMPARE(thenTask->result(), 6);
    }
    void chainThenMemberFunctionPointer()
    {
        auto sourceTask = Async<int>::create();
        auto thenTask = sourceTask->then(this, &tst_tasks::memberAdd);
        QVERIFY(thenTask->isPending());
        sourceTask->resolve(5);
        QVERIFY(thenTask->isResolved());
        QCOMPARE(thenTask->result(), 6);
    }
    void chainThenLambda()
    {
        auto sourceTask = Async<int>::create();
        auto thenTask = sourceTask->then([](int x) { return x + 1; });
        QVERIFY(thenTask->isPending());
        sourceTask->resolve(5);
        QVERIFY(thenTask->isResolved());
        QCOMPARE(thenTask->result(), 6);
    }
    void chainThenAlreadyResolved()
    {
        auto task = Async<int>::resolve(5)->then(add);
        QVERIFY(task->isResolved());
        QCOMPARE(task->result(), 6);
    }
    void chainThenReturnTask()
    {
        auto innerTask = Async<int>::create();
        auto outerTask = Async<int>::create();
        auto resultTask = outerTask->then([=](int x) {
            return x ? AutoAsync::resolve(x) : innerTask;
        });
        QVERIFY(!resultTask->isFinished());
        outerTask->resolve(0);
        QVERIFY(!resultTask->isFinished());
        innerTask->resolve(5);
        QVERIFY(outerTask->isResolved());
        QCOMPARE(outerTask->result(), 0);
        QVERIFY(resultTask->isResolved());
        QCOMPARE(resultTask->result(), 5);
    }
    void chainExceptFunctionPointer()
    {
        auto sourceTask = Async<int>::create();
        auto exceptTask = sourceTask->except(handle);
        QVERIFY(exceptTask->isPending());
        QTest::ignoreMessage(QtWarningMsg, "TaskRejected");
        sourceTask->reject();
        QVERIFY(sourceTask->isRejected());
        QCOMPARE(sourceTask->error().code(), Error::TaskRejected);
        QVERIFY(exceptTask->isResolved());
    }
    void chainExceptMemberFunctionPointer()
    {
        auto sourceTask = Async<int>::create();
        auto exceptTask = sourceTask->except(handle);
        QVERIFY(exceptTask->isPending());
        QTest::ignoreMessage(QtWarningMsg, "TaskRejected");
        sourceTask->reject();
        QVERIFY(sourceTask->isRejected());
        QCOMPARE(sourceTask->error().code(), Error::TaskRejected);
        QVERIFY(exceptTask->isResolved());
    }
    void chainExceptLambda()
    {
        Error::Code caught = Error::Success;
        auto sourceTask = Async<int>::create();
        auto exceptTask = sourceTask->except([&](const Error& error) { caught = error.code(); return 0; });
        QVERIFY(exceptTask->isPending());
        sourceTask->reject();
        QVERIFY(sourceTask->isRejected());
        QCOMPARE(sourceTask->error().code(), Error::TaskRejected);
        QVERIFY(exceptTask->isResolved());
        QCOMPARE(caught, Error::TaskRejected);
    }
    void chainExceptAlreadyRejected()
    {
        QTest::ignoreMessage(QtWarningMsg, "TaskRejected");
        auto task = Async<int>::reject()->except(handle);
        QVERIFY(task->isResolved());
    }
    void chainExceptReturnTask()
    {
        auto innerTask = Async<int>::create();
        auto outerTask = Async<int>::create();
        auto resultTask = outerTask->except([=](const Error&) {
            return innerTask;
        });
        QVERIFY(!resultTask->isFinished());
        outerTask->reject();
        QVERIFY(!resultTask->isFinished());
        innerTask->resolve(5);
        QVERIFY(outerTask->isRejected());
        QVERIFY(resultTask->isResolved());
        QCOMPARE(resultTask->result(), 5);
    }
    void chainNextFunctionPointer()
    {
        auto sourceTask = Async<int>::create();
        auto doneTask = sourceTask->next(printable);
        QVERIFY(sourceTask->isPending());
        sourceTask->resolve(5);
        QVERIFY(sourceTask->isResolved());
        QCOMPARE(sourceTask->result(), 5);
        QVERIFY(doneTask->isResolved());
        QCOMPARE(doneTask->result(), "5");
    }
    void chainNextMemberFunctionPointer()
    {
        auto sourceTask = Async<int>::create();
        auto doneTask = sourceTask->next(this, &tst_tasks::memberPrintable);
        QVERIFY(sourceTask->isPending());
        sourceTask->resolve(5);
        QVERIFY(sourceTask->isResolved());
        QCOMPARE(sourceTask->result(), 5);
        QVERIFY(doneTask->isResolved());
        QCOMPARE(doneTask->result(), "5");
    }
    void chainNextLambda()
    {
        auto sourceTask = Async<int>::create();
        auto doneTask = sourceTask->next([](const Error& error, int x) { return error ? QStringLiteral("error") : QString::number(x); });
        QVERIFY(sourceTask->isPending());
        sourceTask->resolve(5);
        QVERIFY(sourceTask->isResolved());
        QCOMPARE(sourceTask->result(), 5);
        QVERIFY(doneTask->isResolved());
        QCOMPARE(doneTask->result(), "5");
    }
    void chainNextAlreadyResolved()
    {
        auto task = Async<int>::resolve(5)->next(printable);
        QVERIFY(task->isResolved());
        QCOMPARE(task->result(), "5");
    }
    void chainNextAlreadyRejected()
    {
        auto task = Async<int>::reject()->next(printable);
        QVERIFY(task->isResolved());
        QCOMPARE(task->result(), "error");
    }
    void chainNextReturnTask()
    {
        auto innerTask = Async<int>::create();
        auto outerTask = Async<int>::create();
        auto resultTask = outerTask->next([=](const Error&, int x) {
            return x ? AutoAsync::resolve(x) : innerTask;
        });
        QVERIFY(!resultTask->isFinished());
        outerTask->resolve(0);
        QVERIFY(!resultTask->isFinished());
        innerTask->resolve(5);
        QVERIFY(outerTask->isResolved());
        QCOMPARE(outerTask->result(), 0);
        QVERIFY(resultTask->isResolved());
        QCOMPARE(resultTask->result(), 5);
    }
    void throwInThen()
    {
        auto task = Async<int>::resolve(5)->then([](int) { throw Error(HERE, Error::TaskRejected); });
        QVERIFY(task->isRejected());
        QCOMPARE(task->error().code(), Error::TaskRejected);
    }
    void errorWhileRunning()
    {
        auto task = Async<int>::create();
        QCOMPARE(task->error().code(), Error::TaskStillPending);
        task->resolve(5);
        QCOMPARE(task->error().code(), Error::Success);
    }
    void resultTypeDeduction()
    {
        auto charTask = Async<char>::create();
        auto shortTask = charTask->then([](auto&& x) { return static_cast<short>(x + 1); });
        auto intTask = shortTask->then([](auto&& x) { return static_cast<int>(x + 1); });
        QVERIFY((std::is_same<decltype(shortTask)::ResultType, short>::value));
        QVERIFY((std::is_same<decltype(intTask)::ResultType, int>::value));
        charTask->resolve(1);
        QVERIFY(shortTask->isResolved());
        QCOMPARE(shortTask->result(), 2);
        QVERIFY(intTask->isResolved());
        QCOMPARE(intTask->result(), 3);
    }
    void abandonChain()
    {
        // Create a chain of three tasks, and release first the parent task
        // and then the last task. The parent task should stay alive until
        // the last task is abandoned.
        auto sourceTask = Async<int>::create();
        auto thenTask = sourceTask->then(add)->then(add);
        QVERIFY(thenTask->isPending());
        QCOMPARE(BaseTask::getTaskCount(), 3);
        sourceTask.reset(); // should not trigger warnings even without abandon()
        QVERIFY(thenTask->isPending());
        QCOMPARE(BaseTask::getTaskCount(), 3);
        thenTask.abandon();
        QCOMPARE(BaseTask::getTaskCount(), 0);
    }
    void abandonChainGradually()
    {
        // Create a chain of five tasks, holding references to the original,
        // middle, and last tasks, then release them in reverse order. Tasks
        // should get destroyed/abandoned in the order 5&4, 3&2 and lastly 1.
        auto sourceTask = Async<int>::create();
        auto midTask = sourceTask->then(add)->then(add);
        auto thenTask = midTask->then(add)->then(add);
        QVERIFY(thenTask->isPending());
        QCOMPARE(BaseTask::getTaskCount(), 5);
        thenTask.abandon();
        QCOMPARE(BaseTask::getTaskCount(), 3);
        midTask.reset(); // should not trigger warnings even without abandon()
        QCOMPARE(BaseTask::getTaskCount(), 1);
        sourceTask.reset(); // should not trigger warnings even without abandon()
        QCOMPARE(BaseTask::getTaskCount(), 0);
    }
    void abandonInlineChain()
    {
        // Create a chain of five tasks, holding a reference only to the last
        // task, then release it. The entire chain should be destroyed.
        auto thenTask = Async<int>::create()->then(add)->then(add)->then(add)->then(add);
        QVERIFY(thenTask->isPending());
        QCOMPARE(BaseTask::getTaskCount(), 5);
        thenTask.abandon();
        QCOMPARE(BaseTask::getTaskCount(), 0);
    }
    void autoUpCast()
    {
        Async<QObject*> baseTask = Async<tst_tasks*>::resolve(this);
        QVERIFY(baseTask->isResolved());
        QCOMPARE(baseTask->result(), this);
        Async<double> doubleTask = Async<int>::resolve(5);
        QVERIFY(doubleTask->isResolved());
        QCOMPARE(doubleTask->result(), 5.0);
        Async<int> intTask = Async<char>::resolve(5);
        QVERIFY(intTask->isResolved());
        QCOMPARE(intTask->result(), 5);
    }
    void race_data()
    {
        QTest::addColumn<int>("taskCount");
        QTest::addColumn<QVector<int>>("actions");
        QTest::addColumn<BaseTask::State>("state");
        QTest::addColumn<int>("result");
        QTest::addColumn<Error::Code>("errorCode");

        typedef QVector<int> List;

        QTest::newRow("resolve 1") << 3 << List { 1 } << BaseTask::Resolved << 1 << Error::Success;
        QTest::newRow("resolve 2") << 3 << List { 2 } << BaseTask::Resolved << 2 << Error::Success;
        QTest::newRow("resolve 3") << 3 << List { 3 } << BaseTask::Resolved << 3 << Error::Success;
        QTest::newRow("reject 1") << 3 << List { -1 } << BaseTask::Rejected << 0 << err(-1);
        QTest::newRow("reject 2") << 3 << List { -2 } << BaseTask::Rejected << 0 << err(-2);
        QTest::newRow("reject 3") << 3 << List { -3 } << BaseTask::Rejected << 0 << err(-3);
        QTest::newRow("resolve multiple 1") << 3 << List { 1, 2, 3 } << BaseTask::Resolved << 1 << Error::Success;
        QTest::newRow("resolve multiple 2") << 3 << List { 2, 3, 1 } << BaseTask::Resolved << 2 << Error::Success;
        QTest::newRow("resolve multiple 3") << 3 << List { 3, 2, 1 } << BaseTask::Resolved << 3 << Error::Success;
        QTest::newRow("reject multiple 1") << 3 << List { -1, -2, -3 } << BaseTask::Rejected << 0 << err(-1);
        QTest::newRow("reject multiple 2") << 3 << List { -2, -3, -1 } << BaseTask::Rejected << 0 << err(-2);
        QTest::newRow("reject multiple 3") << 3 << List { -3, -2, -1 } << BaseTask::Rejected << 0 << err(-3);
        QTest::newRow("resolve+reject 1") << 3 << List { 1, -2 } << BaseTask::Resolved << 1 << Error::Success;
        QTest::newRow("resolve+reject 2") << 3 << List { 2, -3 } << BaseTask::Resolved << 2 << Error::Success;
        QTest::newRow("resolve+reject 3") << 3 << List { 3, -2 } << BaseTask::Resolved << 3 << Error::Success;
        QTest::newRow("reject+resolve 1") << 3 << List { -1, 2 } << BaseTask::Rejected << 0 << err(-1);
        QTest::newRow("reject+resolve 2") << 3 << List { -2, 3 } << BaseTask::Rejected << 0 << err(-2);
        QTest::newRow("reject+resolve 3") << 3 << List { -3, 2 } << BaseTask::Rejected << 0 << err(-3);
        QTest::newRow("single resolve") << 1 << List { 1 } << BaseTask::Resolved << 1 << Error::Success;
        QTest::newRow("single reject") << 1 << List { -1 } << BaseTask::Rejected << 0 << err(-1);
        QTest::newRow("empty") << 0 << List {} << BaseTask::Pending << 0 << Error::TaskStillPending;
    }
    void race()
    {
        QFETCH(int, taskCount);
        QFETCH(QVector<int>, actions);
        QFETCH(BaseTask::State, state);
        QFETCH(int, result);
        QFETCH(Error::Code, errorCode);

        testAggregate(Async<int>::raceIterable, ByIndex, taskCount, actions, state, result, errorCode);
    }
    void voidRaceWithTypedTasks()
    {
        auto voidRace = Async<void>::race(Async<int>::create(), Async<float>::create());
        voidRace.abandon();
        QVERIFY((std::is_void<decltype(voidRace)::ResultType>::value));
    }
    void autoRace()
    {
        auto doubleRace = AutoAsync::race(Async<int>::create(), Async<bool>::create(), Async<float>::create(), Async<double>::create());
        doubleRace.abandon();
        QVERIFY((std::is_same<decltype(doubleRace)::ResultType, double>::value));
        auto voidRace = AutoAsync::race(Async<int>::create(), Async<void>::create(), Async<double>::create());
        voidRace.abandon();
        QVERIFY((std::is_void<decltype(voidRace)::ResultType>::value));
        auto incompatibleRace = AutoAsync::race(Async<int>::create(), Async<QString>::create());
        incompatibleRace.abandon();
        QVERIFY((std::is_void<decltype(incompatibleRace)::ResultType>::value));
    }
    void all_data()
    {
        QTest::addColumn<int>("taskCount");
        QTest::addColumn<QVector<int>>("actions");
        QTest::addColumn<BaseTask::State>("state");
        QTest::addColumn<QVector<int>>("result");
        QTest::addColumn<Error::Code>("errorCode");

        typedef QVector<int> List;

        QTest::newRow("resolve 1 2 3") << 3 << List { 1, 2, 3 } << BaseTask::Resolved << List { 1, 2, 3 } << Error::Success;
        QTest::newRow("resolve 2 3 1") << 3 << List { 2, 3, 1 } << BaseTask::Resolved << List { 3, 1, 2 } << Error::Success;
        QTest::newRow("resolve 3 2 1") << 3 << List { 3, 2, 1 } << BaseTask::Resolved << List { 3, 2, 1 } << Error::Success;
        QTest::newRow("resolve only 1") << 3 << List { 1 } << BaseTask::Pending << List {} << Error::TaskStillPending;
        QTest::newRow("resolve only 2") << 3 << List { 2 } << BaseTask::Pending << List {} << Error::TaskStillPending;
        QTest::newRow("resolve only 3") << 3 << List { 3 } << BaseTask::Pending << List {} << Error::TaskStillPending;
        QTest::newRow("reject only 1") << 3 << List { -1 } << BaseTask::Rejected << List {} << err(-1);
        QTest::newRow("reject only 2") << 3 << List { -2 } << BaseTask::Rejected << List {} << err(-1);
        QTest::newRow("reject only 3") << 3 << List { -3 } << BaseTask::Rejected << List {} << err(-1);
        QTest::newRow("reject last 1") << 3 << List { 2, 3, -1 } << BaseTask::Rejected << List {} << err(-3);
        QTest::newRow("reject last 2") << 3 << List { 3, 1, -2 } << BaseTask::Rejected << List {} << err(-3);
        QTest::newRow("reject last 3") << 3 << List { 1, 2, -3 } << BaseTask::Rejected << List {} << err(-3);
        QTest::newRow("reject middle") << 3 << List { 1, -2, 3 } << BaseTask::Rejected << List {} << err(-2);
        QTest::newRow("empty") << 0 << List {} << BaseTask::Resolved << List {} << Error::Success;
    }
    void all()
    {
        QFETCH(int, taskCount);
        QFETCH(QVector<int>, actions);
        QFETCH(BaseTask::State, state);
        QFETCH(QVector<int>, result);
        QFETCH(Error::Code, errorCode);

        testAggregate(Async<int>::allIterable, InOrder, taskCount, actions, state, result, errorCode);
    }
    void voidAllWithTypedTasks()
    {
        auto voidAll = Async<void>::all(Async<int>::create(), Async<float>::create());
        voidAll.abandon();
        QVERIFY((std::is_void<decltype(voidAll)::ResultType>::value));
    }
    void autoAll()
    {
        auto doubleAll = AutoAsync::all(Async<int>::create(), Async<bool>::create(), Async<float>::create(), Async<double>::create());
        doubleAll.abandon();
        QVERIFY((std::is_same<decltype(doubleAll)::ResultType, QVector<double>>::value));
        auto voidAll = AutoAsync::all(Async<int>::create(), Async<void>::create(), Async<double>::create());
        voidAll.abandon();
        QVERIFY((std::is_void<decltype(voidAll)::ResultType>::value));
        auto incompatibleAll = AutoAsync::all(Async<int>::create(), Async<QString>::create());
        incompatibleAll.abandon();
        QVERIFY((std::is_void<decltype(incompatibleAll)::ResultType>::value));
    }
    void childKeepsParentAlive()
    {
        int resolved = 0;
        auto sourceTask = Async<int>::create();
        auto thenTask = sourceTask->then([&](int x) {
            QCOMPARE(BaseTask::getTaskCount(), 2);
            resolved = x; // note; can't return int due to QCOMPARE above
        }, Qt::QueuedConnection);
        sourceTask->resolve(5);
        sourceTask.reset();
        QTRY_VERIFY(thenTask->isFinished());
        QVERIFY(thenTask->isResolved());
        QCOMPARE(resolved, 5);
    }
    void chainWithDeadRecipient()
    {
        auto root = Async<int>::create();
        {
            QObject dummy;
            root->notify(&dummy, [](int) {
                QFAIL("should not run");
            });
        }
        root->resolve(5);
    }
    void chainThenWithDeadRecipient()
    {
        auto root = Async<int>::create();
        Async<void> result;
        {
            QObject dummy;
            result = root->then(&dummy, [](int) {
                QFAIL("should not run");
            });
        }
        root->resolve(5);
        QVERIFY(result->isRejected());
        QCOMPARE(result->error().code(), Error::TaskRecipientDestroyed);
    }
    void chainQueued()
    {
        auto root = Async<int>::create();
        auto result = root->then(add, Qt::QueuedConnection);
        root->resolve(5);
        QVERIFY(!result->isFinished());
        QTRY_VERIFY(result->isFinished());
        QVERIFY(result->isResolved());
        QCOMPARE(result->result(), 6);
    }
};

QTEST_GUILESS_MAIN(tst_tasks)
#include TEST_MOC
