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

#include <common/src/common.h>
#include <common/src/builtin/util.h>
#include <QtTest>

using Any = kapps::core::Any;

class tst_any : public QObject
{
    Q_OBJECT

private slots:
    void testEmpty()
    {
        QVERIFY(Any{}.empty());
        QVERIFY(!Any{42}.empty());
        QVERIFY(!Any{"hello"}.empty());

        Any a;
        QVERIFY(a.empty());
        a = 42;
        QVERIFY(!a.empty());
        a = {};
        QVERIFY(a.empty());
        a = "hello";
        QVERIFY(!a.empty());

        Any b;
        a = "hello";
        QVERIFY(!a.empty());
        a = std::move(b);
        QVERIFY(a.empty());
        b = {}; // b is in unspecified state after move
        QVERIFY(b.empty());
    }

    void testContainsType()
    {
        Any i{42};
        QVERIFY(i.containsType<int>());
        QVERIFY(!i.containsType<unsigned>());
        QVERIFY(i.containsType<const int &>());
        Any u{42u};
        QVERIFY(u.containsType<unsigned>());
        QVERIFY(!u.containsType<int>());
        QVERIFY(u.containsType<const unsigned &>());

        Any s{std::string{"str object"}};
        QVERIFY(s.containsType<std::string>());
        QVERIFY(s.containsType<const std::string &>());
        QVERIFY(!s.containsType<const char *>());
        // Not a StringSlice even though it's implicitly convertible
        QVERIFY(!s.containsType<kapps::core::StringSlice>());

        Any cp{"hello"};
        QVERIFY(cp.containsType<const char *>());
        QVERIFY(!cp.containsType<char *>());
        QVERIFY(!cp.containsType<const char>());
        // Const reference to const char * - analogous to const int & test above
        QVERIFY(cp.containsType<char const * const &>());

        // Char array decays to char *
        char charArray[]{"goodbye"};
        Any ca{charArray};
        QVERIFY(ca.containsType<char *>());
        // Const reference to char *
        QVERIFY(ca.containsType<char * const &>());

        class DummyDerivative : public std::vector<int>
        {
        public:
            using std::vector<int>::vector;
        };
        Any dd{DummyDerivative{}};
        QVERIFY(dd.containsType<DummyDerivative>());
        // Not a vector even though it's a base class
        // (Although it might be beneficial to allow this, it's not allowed
        // currently).
        QVERIFY(!dd.containsType<std::vector<int>>());
    }

    void testHandle()
    {
        // Index of called handler(s), used for visitor tests
        std::vector<int> called{};

        Any a;
        // Nothing matches an empty
        a.handle<int>([&](int){called.push_back(1);})
            .handle<bool>([&](bool){called.push_back(2);});
        QCOMPARE(called, std::vector<int>{});

        a = 1985;
        a.handle<char>([&](char){called.push_back(1);})
            .handle<unsigned>([&](unsigned){called.push_back(2);})
            .handle<int>([&](int){called.push_back(3);});
        QCOMPARE(called, std::vector<int>{3});
        called.clear();

        // handle<>() must match type exactly, conversions not allowed
        a = std::int8_t{};  // test implicit std::int8_t -> int conversion
        a.handle<int>([&](int){called.push_back(1);})
            .handle<std::int8_t>([&](std::int8_t){called.push_back(2);});
        QCOMPARE(called, std::vector<int>{2});
        called.clear();

        // test shared_ptr -> bool user-defined conversion
        a = std::shared_ptr<int>{new int{2015}};
        a.handle<bool>([&](bool){called.push_back(1);})
            .handle<std::shared_ptr<int>>([&](const std::shared_ptr<int>&){called.push_back(2);});
        QCOMPARE(called, std::vector<int>{2});
        called.clear();

        // Multiple matching visitors are all called in the proper order
        a = 1969;
        a.handle<int>([&](int){called.push_back(1);})
            .handle<bool>([&](bool){called.push_back(2);})
            .handle<int>([&](int){called.push_back(3);})
            .handle<char>([&](char){called.push_back(4);})
            .handle<int>([&](int){called.push_back(5);});
        QCOMPARE(called, (std::vector<int>{1, 3, 5}));
        called.clear();

        // Visitors receive correct value
        a = 6502;
        a.handle<int>([&](int i){called.push_back(i);})
            .handle<bool>([&](bool){called.push_back(0);})
            .handle<int>([&](int &i){called.push_back(i);})
            .handle<int>([&](const int &i){called.push_back(i);});
        QCOMPARE(called, (std::vector<int>{6502, 6502, 6502}));
        called.clear();

        // Visitors taking mutable references can modify the value
        a = 1024;
        a.handle<int>([&](int &i){called.push_back(i); i *= 2;})
            .handle<int>([&](int &i){called.push_back(i); i*= 2;})
            .handle<int>([&](int i){called.push_back(i);});
        QCOMPARE(called, (std::vector<int>{1024, 2048, 4096}));
        called.clear();
    }
};

QTEST_GUILESS_MAIN(tst_any)
#include TEST_MOC
