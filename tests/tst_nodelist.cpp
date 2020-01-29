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

class tst_nodelist : public QObject
{
    Q_OBJECT

    struct Item : Node<Item>
    {
    };
    typedef NodeList<Item> List;

    void verifyList(List* list, const std::initializer_list<Item*>& expected, bool& success)
    {
        Item* i = list->first();
        for (auto it = std::begin(expected), end = std::end(expected); it != end; ++it, (i = i->next()))
        {
            QCOMPARE(i, *it);
            QCOMPARE(i->list(), list);
            QVERIFY(list->contains(i));
        }
        QVERIFY(i == nullptr);
        i = list->last();
        for (auto it = std::rbegin(expected), end = std::rend(expected); it != end; ++it, (i = i->prev()))
        {
            QCOMPARE(i, *it);
            QCOMPARE(i->list(), list);
            QVERIFY(list->contains(i));
        }
        QVERIFY(i == nullptr);
        QCOMPARE(list->count(), std::end(expected) - std::begin(expected));
        success = true;
    }
    bool verifyList(List* actual, const std::initializer_list<Item*>& expected)
    {
        bool success = false;
        verifyList(actual, expected, success);
        return success;
    }
    void verifyDetached(const std::initializer_list<Item*>& expected, bool& success)
    {
        for (Item* i : expected)
        {
            QCOMPARE(i->prev(), nullptr);
            QCOMPARE(i->next(), nullptr);
            QCOMPARE(i->list(), nullptr);
        }
        success = true;
    }
    bool verifyDetached(const std::initializer_list<Item*>& expected)
    {
        bool success = false;
        verifyDetached(expected, success);
        return success;
    }

private slots:
    void insertFirst()
    {
        List list;
        Item a, b;
        a.insertFirst(&list);
        QVERIFY(verifyList(&list, { &a }));
        b.insertFirst(&list);
        QVERIFY(verifyList(&list, { &b, &a }));
    }
    void insertLast()
    {
        List list;
        Item a, b;
        a.insertLast(&list);
        QVERIFY(verifyList(&list, { &a }));
        b.insertLast(&list);
        QVERIFY(verifyList(&list, { &a, &b }));
    }
    void insertBefore()
    {
        List list;
        Item a, b, c;
        a.insertFirst(&list);
        b.insertBefore(&a);
        QVERIFY(verifyList(&list, { &b, &a }));
        c.insertBefore(&a);
        QVERIFY(verifyList(&list, { &b, &c, &a }));
    }
    void insertAfter()
    {
        List list;
        Item a, b, c;
        a.insertFirst(&list);
        b.insertAfter(&a);
        QVERIFY(verifyList(&list, { &a, &b }));
        c.insertAfter(&a);
        QVERIFY(verifyList(&list, { &a, &c, &b }));
    }
    void remove()
    {
        List list;
        Item a, b, c, d;
        a.insertLast(&list);
        b.insertLast(&list);
        c.insertLast(&list);
        d.insertLast(&list);
        b.remove();
        QVERIFY(verifyList(&list, { &a, &c, &d }));
        QVERIFY(verifyDetached({ &b }));
        a.remove();
        QVERIFY(verifyList(&list, { &c, &d }));
        QVERIFY(verifyDetached({ &a, &b }));
        d.remove();
        QVERIFY(verifyList(&list, { &c }));
        QVERIFY(verifyDetached({ &a, &b, &d }));
        c.remove();
        QVERIFY(verifyList(&list, {}));
        QVERIFY(verifyDetached({ &a, &b, &c, &d }));
    }
    void clear()
    {
        List list;
        Item a, b, c;
        a.insertLast(&list);
        b.insertLast(&list);
        c.insertLast(&list);
        list.clear();
        QVERIFY(verifyDetached({ &a, &b, &c }));
    }
    void reorder()
    {
        List list;
        Item a, b, c;
        a.insertLast(&list); // a
        b.insertLast(&list); // a b
        c.insertLast(&list); // a b c
        b.insertFirst(&list); // b a c
        QVERIFY(verifyList(&list, { &b, &a, &c }));
        c.insertBefore(&a); // b c a
        QVERIFY(verifyList(&list, { &b, &c, &a }));
        b.insertAfter(&c); // c b a
        QVERIFY(verifyList(&list, { &c, &b, &a }));
        c.insertLast(&list); // b a c
        QVERIFY(verifyList(&list, { &b, &a, &c }));
    }
    void moveBetweenLists()
    {
        List list1, list2;
        Item a, b, c;
        a.insertLast(&list1);
        b.insertLast(&list1);
        c.insertLast(&list1);
        b.insertLast(&list2);
        QVERIFY(verifyList(&list1, { &a, &c }));
        QVERIFY(verifyList(&list2, { &b }));
        a.insertFirst(&list2);
        QVERIFY(verifyList(&list1, { &c }));
        QVERIFY(verifyList(&list2, { &a, &b }));
        c.insertLast(&list2);
        QVERIFY(verifyList(&list1, {}));
        QVERIFY(verifyList(&list2, { &a, &b, &c }));
    }
};

QTEST_GUILESS_MAIN(tst_nodelist)
#include TEST_MOC
