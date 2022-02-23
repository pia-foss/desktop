// Copyright (c) 2022 Private Internet Access, Inc.
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

typedef bool* HeapPtr;
static QSet<HeapPtr> g_heap;
static uintptr_t g_allocCount;
static uintptr_t g_freeCount;

class tst_raii : public QObject
{
    Q_OBJECT

private:
    static void heapReset() { g_heap.clear(); g_allocCount = 0; g_freeCount = 0; }
    static int heapAllocCount() { return g_allocCount; }
    static int heapFreeCount() { return g_freeCount; }
    static int heapLiveCount() { return g_heap.size(); }
    static bool heapEmpty() { return g_heap.empty(); }
    static bool heapCheckLiveCount(unsigned int expectedCount) { return expectedCount == g_allocCount - g_freeCount && expectedCount == (unsigned)g_heap.size(); }

    static HeapPtr heapAlloc() { HeapPtr p = reinterpret_cast<HeapPtr>(++g_allocCount); g_heap.insert(p); return p; }
    static void heapFree(HeapPtr p) { ++g_freeCount; if (!p) throw "attempted to free null pointer"; if (!g_heap.contains(p)) throw "attempted to free already freed handle"; g_heap.remove(p); }

private slots:
    void init()
    {
        heapReset();
    }
    void cleanup()
    {
        heapReset();
    }

    void raiiSimpleStatic()
    {
        {
            RAII(HeapPtr, heapFree) ptr(heapAlloc());
            HeapPtr raw = ptr; (void)raw;
            QVERIFY(heapCheckLiveCount(1));
        }
        QVERIFY(heapCheckLiveCount(0));
    }
    void raiiSentinel()
    {
        bool called = false;
        {
            RAII_SENTINEL({ called = true; });
            QVERIFY(!called);
        }
        QVERIFY(called);
    }
};

QTEST_APPLESS_MAIN(tst_raii)
#include TEST_MOC
