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

#include <kapps_net/src/mac/flow_tracker.h>
#include <QtTest>

template<class K, class V>
using ConstrainedHash = kapps::net::ConstrainedHash<K, V>;

class tst_constrainedhash : public QObject
{
    Q_OBJECT

private slots:
    void testInsert()
    {
        {
            // No overflow
            ConstrainedHash<std::string, int> ch{2};
            ch.insert({"foo", 1});
            ch.insert({"bar", 2});

            QCOMPARE(ch.at("foo"), 1);
            QCOMPARE(ch.at("bar"), 2);
        }

        {
            // With overflow
            ConstrainedHash<std::string, int> ch{2};
            ch.insert({"foo", 1});
            ch.insert({"bar", 2});
            ch.insert({"baz", 3});

            QCOMPARE(ch.at("bar"), 2);
            QCOMPARE(ch.at("baz"), 3);
            QCOMPARE(ch.contains("foo"), false);
            QCOMPARE(ch.size(), 2);
        }

        {
            // Total replacement of all elements
            ConstrainedHash<std::string, int> ch{2};
            ch.insert({"foo", 1});
            ch.insert({"bar", 2});

            // This will replace all elements above with those below
            ch.insert({"alpha", 1});
            ch.insert({"beta", 2});

            QCOMPARE(ch.size(), 2);
            QCOMPARE(ch.at("alpha"), 1);
            QCOMPARE(ch.at("beta"), 2);
        }
    }

    void testContains()
    {
        {
            // No overflow
            ConstrainedHash<std::string, int> ch{2};
            ch.insert({"foo", 1});
            ch.insert({"bar", 2});

            QCOMPARE(ch.contains("foo"), true);
            QCOMPARE(ch.contains("bar"), true);
        }

        {
            // With overflow
            ConstrainedHash<std::string, int> ch{2};
            ch.insert({"foo", 1});
            ch.insert({"bar", 2});
            ch.insert({"baz", 3});

            QCOMPARE(ch.contains("bar"), true);
            QCOMPARE(ch.contains("baz"), true);
            QCOMPARE(ch.contains("foo"), false);
            QCOMPARE(ch.size(), 2);
        }

        {
            // Total replacement of all elements
            ConstrainedHash<std::string, int> ch{2};
            ch.insert({"foo", 1});
            ch.insert({"bar", 2});

            // This will replace all elements above with those below
            ch.insert({"alpha", 1});
            ch.insert({"beta", 2});

            QCOMPARE(ch.size(), 2);
            QCOMPARE(ch.contains("alpha"), true);
            QCOMPARE(ch.contains("beta"), true);
            QCOMPARE(ch.contains("foo"), false);
            QCOMPARE(ch.contains("bar"), false);
        }
    }

    void testAt()
    {

        {
            // Returns the correct value
            ConstrainedHash<std::string, int> ch{2};
            ch.insert({"foo", 1});
            QCOMPARE(ch.at("foo"), 1);
        }

        {
            // Throws if the key isn't present
            ConstrainedHash<std::string, int> ch{2};
            ch.insert({"foo", 1});
            QVERIFY_EXCEPTION_THROWN(ch.at("bar"), std::out_of_range);
        }

        {
            // Returns a reference to the value
            ConstrainedHash<std::string, int> ch{2};
            ch.insert({"foo", 1});
            ch.at("foo") = 20;

            QCOMPARE(ch.at("foo"), 20);
        }
    }
};

QTEST_APPLESS_MAIN(tst_constrainedhash)
#include TEST_MOC
