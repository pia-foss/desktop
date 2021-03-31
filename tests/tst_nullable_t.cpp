// Copyright (c) 2021 Private Internet Access, Inc.
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

//OwnedInt is just a test object for verifying move semantics in nullable_t.
//Whenever an OwnedInt is moved, the moved-from OwnedInt is zeroed.
class OwnedInt
{
private:
    static int _lastDestroyedValue;

public:
    // Take the last value that was destroyed (to verify destruction in a few
    // tests)
    static int takeLastDestroyedValue()
    {
        int last = _lastDestroyedValue;
        _lastDestroyedValue = 0;
        return last;
    }

public:
    OwnedInt(int i = 0) : _i{i} {}
    OwnedInt(const OwnedInt &other) = default;
    OwnedInt(OwnedInt &&other) : OwnedInt{} {*this = std::move(other);}

    ~OwnedInt() {_lastDestroyedValue = _i;}

public:
    OwnedInt &operator=(const OwnedInt &other) = default;
    OwnedInt &operator=(OwnedInt &&other) {_i = other._i; other._i = 0; return *this;}

public:
    int value() const {return _i;}

private:
    int _i;
};

int OwnedInt::_lastDestroyedValue = 0;

// Functions for testing reference conversion operators of nullable_t
int ownedIntValue(OwnedInt &value)
{
    return value.value();
}
int ownedIntValue(const OwnedInt &value)
{
    return value.value();
}

using NullableInt = nullable_t<OwnedInt>;

// PostiveInt just throws if constructed with a value <= 0; used to test
// exception safety of nullable_t::emplace()
class PositiveInt
{
public:
    PositiveInt(int i)
        : _i{i}
    {
        if(_i <= 0)
            throw Error{HERE, Error::Code::Unknown};
    }
public:
    int value() const {return _i;}
private:
    int _i;
};

using NullablePositive = nullable_t<PositiveInt>;

class tst_nullable_t : public QObject
{
    Q_OBJECT

private slots:
    //Verify copy construction for nullable_t
    void copyConstruct()
    {
        //Copy an invalid nullable
        NullableInt srcInvalid;
        NullableInt destInvalid{srcInvalid};
        //Both should still be invalid
        QCOMPARE(srcInvalid.ptr(), nullptr);
        QCOMPARE(destInvalid.ptr(), nullptr);

        //Copy a valid nullable
        NullableInt srcValid{5};
        NullableInt destValid{srcValid};

        //Both should be valid and contain the same value
        QVERIFY(srcValid);
        QCOMPARE(srcValid.get().value(), 5);
        QVERIFY(destValid);
        QCOMPARE(destValid.get().value(), 5);
    }

    //Verify copy assignment to an invalid nullable_t
    void invalidCopyAssign()
    {
        NullableInt srcInvalid;
        NullableInt destInvalid;
        destInvalid = srcInvalid;
        //Both should still be invalid
        QCOMPARE(srcInvalid.ptr(), nullptr);
        QCOMPARE(destInvalid.ptr(), nullptr);

        //Copy a valid nullable
        NullableInt srcValid{5};
        NullableInt destValid;
        destValid = srcValid;

        //Both should be valid and contain the same value
        QVERIFY(srcValid);
        QCOMPARE(srcValid.get().value(), 5);
        QVERIFY(destValid);
        QCOMPARE(destValid.get().value(), 5);
    }

    //Verify copy assignment to a valid nullable_t
    void validCopyAssign()
    {
        NullableInt srcInvalid;
        NullableInt destInvalid{10};
        destInvalid = srcInvalid;
        //Both should now be invalid
        QCOMPARE(srcInvalid.ptr(), nullptr);
        QCOMPARE(destInvalid.ptr(), nullptr);

        //Copy a valid nullable
        NullableInt srcValid{20};
        NullableInt destValid{30};
        destValid = srcValid;

        //Both should be valid and contain the same value
        QVERIFY(srcValid);
        QCOMPARE(srcValid.get().value(), 20);
        QVERIFY(destValid);
        QCOMPARE(destValid.get().value(), 20);
    }

    //Verify move construction for nullable_t
    void moveConstruct()
    {
        //Move an invalid nullable
        NullableInt srcInvalid;
        NullableInt destInvalid{std::move(srcInvalid)};
        //Both should still be invalid
        QCOMPARE(srcInvalid.ptr(), nullptr);
        QCOMPARE(destInvalid.ptr(), nullptr);

        //Move a valid nullable
        NullableInt srcValid{5};
        NullableInt destValid{std::move(srcValid)};

        //srcValid should still be valid, but it was moved-from
        QVERIFY(srcValid);
        QCOMPARE(srcValid.get().value(), 0);
        //destValid should be valid and contain the moved value
        QVERIFY(destValid);
        QCOMPARE(destValid.get().value(), 5);
    }

    //Verify move assignment to an invalid nullable_t
    void invalidMoveAssign()
    {
        NullableInt srcInvalid;
        NullableInt destInvalid;
        destInvalid = std::move(srcInvalid);
        //Both should still be invalid
        QCOMPARE(srcInvalid.ptr(), nullptr);
        QCOMPARE(destInvalid.ptr(), nullptr);

        //Move a valid nullable
        NullableInt srcValid{5};
        NullableInt destValid;
        destValid = std::move(srcValid);

        //srcValid should still be valid, but it was moved-from
        QVERIFY(srcValid);
        QCOMPARE(srcValid.get().value(), 0);
        //destValid should be valid and contain the moved value
        QVERIFY(destValid);
        QCOMPARE(destValid.get().value(), 5);
    }

    //Verify move assignment to a valid nullable_t
    void validMoveAssign()
    {
        NullableInt srcInvalid;
        NullableInt destInvalid{10};
        destInvalid = std::move(srcInvalid);
        //Both should now be invalid
        QCOMPARE(srcInvalid.ptr(), nullptr);
        QCOMPARE(destInvalid.ptr(), nullptr);

        //Move a valid nullable
        NullableInt srcValid{20};
        NullableInt destValid{30};
        destValid = std::move(srcValid);

        //srcValid should still be valid, but it was moved-from
        QVERIFY(srcValid);
        QCOMPARE(srcValid.get().value(), 0);
        //destValid should be valid and contain the moved value
        QVERIFY(destValid);
        QCOMPARE(destValid.get().value(), 20);
    }

    //Verify some basic operators
    void operators()
    {
        NullableInt invalid;
        NullableInt valid{42};
        const NullableInt constInvalid;
        const NullableInt constValid{42};

        // Test bool conversions (explicitly grab a bool from invalidFlag so we
        // can compare it to 'false' with QCOMPARE - QVERIFY(!...) would test
        // operator!(), not operator bool())
        bool invalidFlag{invalid};
        bool validFlag{valid};
        bool constInvalidFlag{constInvalid};
        bool constValidFlag{constValid};
        QCOMPARE(invalidFlag, false);
        QCOMPARE(validFlag, true);
        QCOMPARE(constInvalidFlag, false);
        QCOMPARE(constValidFlag, true);

        // Test operator!().  (It should return false for valid, so negate it
        // again for QVERIFY().)
        QVERIFY(!invalid);
        QVERIFY(!!valid);
        QVERIFY(!constInvalid);
        QVERIFY(!!constValid);

        // Test conversions
        QCOMPARE(ownedIntValue(*valid), 42);
        QCOMPARE(ownedIntValue(*constValid), 42);

        // Test dereferencing
        QCOMPARE((*valid).value(), 42);
        QCOMPARE((*constValid).value(), 42);
        QCOMPARE(valid->value(), 42);
        QCOMPARE(constValid->value(), 42);

        // Integral nullable_t should comparable to integer types (the bool
        // conversion should not interfere)
        nullable_t<int> validInt{64};
        QCOMPARE(validInt, 64);
    }

    // Test constructing a value in-place
    void verifyEmplace()
    {
        NullableInt value;
        value.emplace(71);
        QVERIFY(!!value);
        QVERIFY(value.get().value() == 71);

        // Emplace over an existing value
        value.emplace(56);
        QVERIFY(!!value);
        QVERIFY(value.get().value() == 56);
        // Prior value must have been destroyed
        QVERIFY(OwnedInt::takeLastDestroyedValue() == 71);
    }

    // Test emplace in the presence of exceptions
    void verifyConstructException()
    {
        NullablePositive value;
        value.emplace(17);
        QVERIFY(!!value);
        QVERIFY(value.get().value() == 17);

        // Try to emplace over it, but the new value throws
        QVERIFY_EXCEPTION_THROWN(value.emplace(-1), Error);
        QVERIFY(!value);    // Now empty due to throw in emplace()

        // Emplacing over an empty nullable should also remain empty
        NullablePositive empty;
        QVERIFY_EXCEPTION_THROWN(empty.emplace(-2), Error);
        QVERIFY(!empty);    // Still empty

        // Try to assign over the value, but the new value throws.
        // nullable_t doesn't provide a "forwarding" operator=(), so the
        // exception is thrown before its operator=() is called, preserving its
        // value.
        value.emplace(1997);
        QVERIFY(value.get().value() == 1997);
        QVERIFY_EXCEPTION_THROWN(value = -2006, Error);
        QVERIFY(!!value);   // Still holds prior value
        QVERIFY(value.get().value() == 1997);
    }
};

QTEST_GUILESS_MAIN(tst_nullable_t)
#include TEST_MOC
