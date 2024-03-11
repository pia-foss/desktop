// Copyright (c) 2024 Private Internet Access, Inc.
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
#include <kapps_core/src/workqueue.h>
#include <QtTest>

using WorkThread = kapps::core::WorkThread;

class tst_workthread : public QObject
{
    Q_OBJECT

private:
    void delay()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

private slots:
    void testWorkedItems()
    {
        // Completed "work items" will be put here by the worker thread.
        // This is owned by the worker thread, so we can't touch it until
        // WorkThread is destroyed
        std::vector<std::string> workedItems{};
        auto doWork = [&](kapps::core::Any item)
        {
            item.handle<std::string>([&](std::string &s)
            {
                qInfo() << "worked on:" << s;
                workedItems.push_back(std::move(s));
            });
        };

        {
            WorkThread worker{doWork};
            worker.enqueue(std::string{"frobulate burners"});
            worker.enqueue(std::string{"dismantle contraptions"});
        }

        QCOMPARE(workedItems, (std::vector<std::string>{"frobulate burners", "dismantle contraptions"}));
    }

    void testSyncInvoke()
    {
        std::vector<std::string> workedItems{};

        // Do some synchronous work on the work thread interspersed with work
        // on this thread, then verify the execution order.
        //
        // This is hard to test deterministically - even if the synchronization
        // didn't work correctly, it might still execute in the right order due
        // to chance.  Some sleeps are added to try to make this unlikely.

        {
            WorkThread worker([](kapps::core::Any){});

            worker.syncInvoke([&]{delay(); workedItems.push_back("red");});
            workedItems.push_back("orange");
            worker.syncInvoke([&]{delay(); workedItems.push_back("yellow");});
            workedItems.push_back("green");
        }

        QCOMPARE(workedItems, (std::vector<std::string>{"red", "orange", "yellow", "green"}));
    }

    void testQueueInvoke()
    {
        std::vector<std::string> workedItems{};

        // Queued (asynchronous) invocations are serialized with each other and
        // with synchronous invocations, but not with this thread.  Here, it's
        // OK to capture workedItems by reference, because we don't touch it on
        // this thread until after joining the work thread.
        {
            WorkThread worker([](kapps::core::Any){});

            // An initial delay helps ensure that all work items are still
            // properly executed before the destructor exits, even if we hit the
            // destructor before the thread executes the later items
            worker.queueInvoke([&]{delay(); workedItems.push_back("red");});
            worker.queueInvoke([&]{workedItems.push_back("orange");});
            worker.queueInvoke([&]{workedItems.push_back("yellow");});
            worker.queueInvoke([&]{workedItems.push_back("green");});
        }

        QCOMPARE(workedItems, (std::vector<std::string>{"red", "orange", "yellow", "green"}));
    }
};

QTEST_GUILESS_MAIN(tst_workthread)
#include TEST_MOC
