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

#include <kapps_core/src/retainshared.h>
#include <kapps_core/src/logger.h>
#include <QtTest>

namespace kapps::core
{

// A sample counted retainable object - each instance is counted to verify
// construction/destruction, and the object holds an integer value that can be
// copied/moved.
class RetainedValue : public RetainSharedFromThis<RetainedValue>
{
private:
    // The instance count is signed so double-destruction would be clearly
    // indicated (this may also cause a double-free which the heap may detect,
    // but neither is guaranteed)
    static int _instanceCount;

public:
    static int instanceCount() {return _instanceCount;}

public:
    RetainedValue(int value = 0) : _value{value} {++_instanceCount;}
    // Copy copies the stored value and counts another instance
    RetainedValue(const RetainedValue &other) : RetainedValue{} {*this = other;}
    RetainedValue &operator=(const RetainedValue &other)
    {
        _value = other._value;
        return *this;
    }
    // Move sets the moved-from value to -1 so it's clearly indicated, and it
    // counts another instance
    RetainedValue(RetainedValue &&other) : RetainedValue{} {*this = std::move(other);}
    RetainedValue &operator=(RetainedValue &&other)
    {
        _value = other._value;
        other._value = -1;
        return *this;
    }

    ~RetainedValue() {--_instanceCount;}

public:
    int value() const {return _value;}

private:
    int _value;
};

int RetainedValue::_instanceCount{};

class tst_retainshared : public QObject
{
    Q_OBJECT

private slots:
    // Test a simple retain/release on a shared object
    void testSingleRetain()
    {
        QCOMPARE(RetainedValue::instanceCount(), 0);
        auto p = std::make_shared<RetainedValue>();
        QCOMPARE(RetainedValue::instanceCount(), 1);
        auto r = p.get();   // Raw pointer
        r->retain();
        p.reset();
        // Not destroyed yet, we retained a reference
        QCOMPARE(RetainedValue::instanceCount(), 1);
        r->release();
        QCOMPARE(RetainedValue::instanceCount(), 0);
    }

    // Test retaining again after all API refs are dropped.  This is valid as
    // long as the application knows the object is still valid; i.e. it has not
    // mutated or destroyed the owner.
    void testReRetain()
    {
        QCOMPARE(RetainedValue::instanceCount(), 0);
        auto p = std::make_shared<RetainedValue>();
        QCOMPARE(RetainedValue::instanceCount(), 1);
        auto r = p.get();   // Raw pointer
        r->retain();
        r->release();
        // Not destroyed yet, owner still holds a shared_ptr
        QCOMPARE(RetainedValue::instanceCount(), 1);
        // We can retain again
        r->retain();
        // Destroy the owner
        p.reset();
        // Not destroyed yet, app holds a reference
        QCOMPARE(RetainedValue::instanceCount(), 1);
        // Release the app's reference too
        r->release();
        QCOMPARE(RetainedValue::instanceCount(), 0);
    }


    // Test copying the shared object
    void testCopy()
    {
        QCOMPARE(RetainedValue::instanceCount(), 0);
        auto p1 = std::make_shared<RetainedValue>(42);
        auto r1 = p1.get();
        r1->retain();
        p1.reset();
        QCOMPARE(RetainedValue::instanceCount(), 1);
        auto p2 = std::make_shared<RetainedValue>(*r1);
        QCOMPARE(RetainedValue::instanceCount(), 2);
        // Verify the copy and original
        QCOMPARE(r1->value(), 42);
        QCOMPARE(p2->value(), 42);
        auto r2 = p2.get();
        r2->retain();

        // The most likely failure mode here is that #2's retain state somehow
        // impacts #1 since #2 was copied from #1 (if it had errantly copied the
        // retain count, etc.).  Release #2 and verify that #1 is not impacted.
        // Keep holding p2 to verify that nothing is destroyed (we released p1).
        r2->release();
        QCOMPARE(RetainedValue::instanceCount(), 2);
        p2.reset();
        QCOMPARE(RetainedValue::instanceCount(), 1);
        r1->release();
        QCOMPARE(RetainedValue::instanceCount(), 0);
    }

    // Test moving the shared object
    void testMove()
    {
        QCOMPARE(RetainedValue::instanceCount(), 0);
        auto p1 = std::make_shared<RetainedValue>(56);
        auto r1 = p1.get();
        r1->retain();
        p1.reset();
        QCOMPARE(RetainedValue::instanceCount(), 1);
        auto p2 = std::make_shared<RetainedValue>(std::move(*r1));
        QCOMPARE(RetainedValue::instanceCount(), 2);
        // Verify the copy and original
        QCOMPARE(r1->value(), -1);  // Moved-from
        QCOMPARE(p2->value(), 56);
        auto r2 = p2.get();
        r2->retain();

        // Like testCopy(), verify releasing #2 does not affect #1
        r2->release();
        QCOMPARE(RetainedValue::instanceCount(), 2);
        p2.reset();
        QCOMPARE(RetainedValue::instanceCount(), 1);
        r1->release();
        QCOMPARE(RetainedValue::instanceCount(), 0);
    }

    // Test copy-assignment
    void testCopyAssign()
    {
        QCOMPARE(RetainedValue::instanceCount(), 0);
        auto p1 = std::make_shared<RetainedValue>(9001);
        auto r1 = p1.get();
        r1->retain();
        p1.reset();
        QCOMPARE(RetainedValue::instanceCount(), 1);
        auto p2 = std::make_shared<RetainedValue>(6001);
        QCOMPARE(RetainedValue::instanceCount(), 2);
        *p2 = *r1;
        QCOMPARE(RetainedValue::instanceCount(), 2);
        // Verify the copy and original
        QCOMPARE(r1->value(), 9001);
        QCOMPARE(p2->value(), 9001);
        auto r2 = p2.get();
        r2->retain();

        // Like testCopy(), verify releasing #2 does not affect #1
        r2->release();
        QCOMPARE(RetainedValue::instanceCount(), 2);
        p2.reset();
        QCOMPARE(RetainedValue::instanceCount(), 1);
        r1->release();
        QCOMPARE(RetainedValue::instanceCount(), 0);
    }

    // Test move-assignment
    void testMoveAssign()
    {
        QCOMPARE(RetainedValue::instanceCount(), 0);
        auto p1 = std::make_shared<RetainedValue>(256);
        auto r1 = p1.get();
        r1->retain();
        p1.reset();
        QCOMPARE(RetainedValue::instanceCount(), 1);
        auto p2 = std::make_shared<RetainedValue>(252);
        QCOMPARE(RetainedValue::instanceCount(), 2);
        *p2 = std::move(*r1);
        QCOMPARE(RetainedValue::instanceCount(), 2);
        // Verify the copy and original
        QCOMPARE(r1->value(), -1);
        QCOMPARE(p2->value(), 256);
        auto r2 = p2.get();
        r2->retain();

        // Like testCopy(), verify releasing #2 does not affect #1
        r2->release();
        QCOMPARE(RetainedValue::instanceCount(), 2);
        p2.reset();
        QCOMPARE(RetainedValue::instanceCount(), 1);
        r1->release();
        QCOMPARE(RetainedValue::instanceCount(), 0);
    }

    // Test multiple references to the same object
    // The application can retain via the API even after internal refs are gone,
    // as long as the app holds at least one reference.
    void testMultipleRetain()
    {
        QCOMPARE(RetainedValue::instanceCount(), 0);
        auto p = std::make_shared<RetainedValue>();
        QCOMPARE(RetainedValue::instanceCount(), 1);
        auto r = p.get();   // Raw pointer
        r->retain();
        p.reset();
        // The app can still retain via the API after the internal shared_ptr is
        // gone, as long as at least one reference is held
        r->retain();    // 2x
        r->retain();    // 3x
        QCOMPARE(RetainedValue::instanceCount(), 1);
        // Release each and verify it's destroyed at the right time
        r->release();   // 2x
        QCOMPARE(RetainedValue::instanceCount(), 1);
        r->release();   // 1x
        QCOMPARE(RetainedValue::instanceCount(), 1);
        r->release();   // Destroyed
        QCOMPARE(RetainedValue::instanceCount(), 0);
    }

    // Test stack objects.  We can't create std::shared_ptr<>s to these, and we
    // can't retain or release them, but they can still be manipulated.
    void testStackObjects()
    {
        QCOMPARE(RetainedValue::instanceCount(), 0);
        RetainedValue v1{111};
        QCOMPARE(RetainedValue::instanceCount(), 1);
        RetainedValue v2{222};
        QCOMPARE(RetainedValue::instanceCount(), 2);
        RetainedValue v3{333};
        QCOMPARE(RetainedValue::instanceCount(), 3);

        // Copy and move
        v1 = v2;
        QCOMPARE(v1.value(), 222);
        QCOMPARE(v2.value(), 222);
        v1 = std::move(v3);
        QCOMPARE(v1.value(), 333);
        QCOMPARE(v3.value(), -1);
        QCOMPARE(RetainedValue::instanceCount(), 3);

        // Copy construct and move construct
        RetainedValue v4{v1};
        QCOMPARE(RetainedValue::instanceCount(), 4);
        QCOMPARE(v4.value(), 333);
        QCOMPARE(v1.value(), 333);
        RetainedValue v5{std::move(v1)};
        QCOMPARE(RetainedValue::instanceCount(), 5);
        QCOMPARE(v5.value(), 333);
        QCOMPARE(v1.value(), -1);

        // We can copy and move to heap values too
        auto p6 = std::make_shared<RetainedValue>(v4);
        QCOMPARE(RetainedValue::instanceCount(), 6);
        QCOMPARE(p6->value(), 333);
        QCOMPARE(v4.value(), 333);
        auto p7 = std::make_shared<RetainedValue>(std::move(v5));
        QCOMPARE(RetainedValue::instanceCount(), 7);
        QCOMPARE(p7->value(), 333);
        QCOMPARE(v5.value(), -1);

        p7.reset();
        QCOMPARE(RetainedValue::instanceCount(), 6);
        p6.reset();
        QCOMPARE(RetainedValue::instanceCount(), 5);
    }

    // (C++17) Verify that an improper retain throws
    // This is only guaranteed in C++17.  In C++14, calling shared_from_this()
    // when no references exist is undefined behavior.
    //
    // Note that if this ever happens, it is still an error; this behavior
    // should not be relied upon.  In this case, it would likely be a library
    // error, as the library probably returned a non-retainable object from an
    // API.  An application might also cause this if it tried to retain an
    // object that no longer exists, but this would more likely result in memory
    // corruption since we can't necessarily diagnose it.
    void testImproperRetain()
    {
        // Retain is invalid when there is no shared_ptr owner, even if the
        // object is actually on the heap - this one is in a unique_ptr
        auto p = std::make_unique<RetainedValue>();
#if __cplusplus >= 201703L
        QVERIFY_EXCEPTION_THROWN(p->retain(), std::exception);
#endif
    }

    // Verify that an improper release throws.  This is implemented just by
    // checking the reference count, so it's guaranteed even in C++14.
    //
    // Like improper retains, this is also an error and should not be relied
    // upon.  In this case, it is most likely an application error; the
    // application is releasing a reference it does not own, or released too
    // many times, etc.
    void testImproperRelease()
    {
        // Like testImproperRelease(), this object is on the heap but not owned
        // by a shared_ptr - release() is still invalid
        auto p = std::make_unique<RetainedValue>();
        QVERIFY_EXCEPTION_THROWN(p->release(), std::exception);
    }
};

};

QTEST_GUILESS_MAIN(kapps::core::tst_retainshared)
#include TEST_MOC
