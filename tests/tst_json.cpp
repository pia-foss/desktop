// Copyright (c) 2019 London Trust Media Incorporated
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
#include <QSignalSpy>

#include "json.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class TestSettings : public NativeJsonObject
{
    Q_OBJECT

private:
    static inline bool arrayValidatorFunc(const QJsonArray &a)
    {
        for (const auto& v : a)
        {
            if (!v.isDouble())
                return false;
        }
        return true;
    }

public:
    TestSettings() : NativeJsonObject(SaveUnknownProperties) {}

    JsonField(bool, boolField, false)
    JsonField(int, intField, 0)
    JsonField(double, doubleField, 0.0)
    JsonField(QString, stringField, QString())
    JsonField(QJsonArray, arrayField, {})
    JsonField(QJsonObject, objectField, {})
    JsonField(QString, validatedStringField, QStringLiteral("test"), { "a", "b", "c" })
    JsonField(QJsonArray, validatedArrayField, {}, &TestSettings::arrayValidatorFunc)
};

class tst_json : public QObject
{
    Q_OBJECT

private slots:
    void castActualTypes()
    {
        const bool boolValue = true;
        const int intValue = 1;
        const double doubleValue = 1.5;
        const QString stringValue = "test";
        const QJsonArray arrayValue = { 1, 2, 3 };
        const QJsonObject objectValue = { { "test", "test" } };

        QCOMPARE(json_cast<bool>(QJsonValue(boolValue)), boolValue);
        QCOMPARE(json_cast<int>(QJsonValue(intValue)), intValue);
        QCOMPARE(json_cast<double>(QJsonValue(doubleValue)), doubleValue);
        QCOMPARE(json_cast<double>(QJsonValue(intValue)), (double)intValue);
        QCOMPARE(json_cast<int>(QJsonValue((double)intValue)), intValue);
        QCOMPARE(json_cast<QString>(QJsonValue(stringValue)), stringValue);
        QCOMPARE(json_cast<QJsonArray>(QJsonValue(arrayValue)), arrayValue);
        QCOMPARE(json_cast<QJsonObject>(QJsonValue(objectValue)), objectValue);

        QCOMPARE(json_cast<QJsonValue>(boolValue), QJsonValue(boolValue));
        QCOMPARE(json_cast<QJsonValue>(intValue), QJsonValue(intValue));
        QCOMPARE(json_cast<QJsonValue>(doubleValue), QJsonValue(doubleValue));
        QCOMPARE(json_cast<QJsonValue>(stringValue), QJsonValue(stringValue));
        QCOMPARE(json_cast<QJsonValue>(arrayValue), QJsonValue(arrayValue));
        QCOMPARE(json_cast<QJsonValue>(objectValue), QJsonValue(objectValue));

        QVERIFY_EXCEPTION_THROWN(json_cast<bool>(QJsonValue(intValue)), json_cast_exception);
        QVERIFY_EXCEPTION_THROWN(json_cast<QJsonObject>(QJsonValue(arrayValue)), json_cast_exception);
        QVERIFY_EXCEPTION_THROWN(json_cast<QJsonArray>(QJsonValue(objectValue)), json_cast_exception);
        QVERIFY_EXCEPTION_THROWN(json_cast<int>(QJsonValue(1.5)), json_cast_exception);
    }
    void castCollections()
    {
        const QList<int> listValue { 1, 2, 3 };
        QCOMPARE(json_cast<QList<int>>(QJsonArray { 1, 2, 3 }), listValue);
        QVERIFY_EXCEPTION_THROWN(json_cast<QList<int>>(QJsonArray { 1, 2, "test" }), json_cast_exception);

        const QHash<QString, QString> hashValue { { "test", "test" } };
        QCOMPARE((json_cast<QHash<QString, QString>>(QJsonObject { { "test", "test" } })), hashValue);
        QVERIFY_EXCEPTION_THROWN((json_cast<QHash<QString, QString>>(QJsonObject { { "test", 1 } })), json_cast_exception);
    }
    void castCustomStructures()
    {
        typedef QList<QSharedPointer<TestSettings>> TestSettingsList;
        auto settings = json_cast<TestSettingsList>(QJsonArray {
                                                        QJsonObject { { "intField", 4 }, { "stringField", "test" } },
                                                        QJsonObject { { "boolField", true }, { "unknown", "test" } }
                                                    });
        QCOMPARE(settings.size(), 2);
        QCOMPARE(settings[0]->intField(), 4);
        QCOMPARE(settings[0]->stringField(), "test");
        QCOMPARE(settings[1]->boolField(), true);
        QCOMPARE(settings[1]->get("unknown"), "test");
    }

    void boolFieldProperty()
    {
        TestSettings settings;
        settings.boolField(true);
        QCOMPARE(settings.boolField(), true);
        settings.boolField(false);
        QCOMPARE(settings.boolField(), false);
        QVERIFY(settings.get("boolField").isBool());
    }
    void intFieldProperty()
    {
        TestSettings settings;
        settings.intField(1);
        QCOMPARE(settings.intField(), 1);
        settings.intField(std::numeric_limits<int>::max());
        QCOMPARE(settings.intField(), std::numeric_limits<int>::max());
        settings.intField(std::numeric_limits<int>::min());
        QCOMPARE(settings.intField(), std::numeric_limits<int>::min());
        settings.intField(0);
        QCOMPARE(settings.intField(), 0);
        QVERIFY(settings.get("intField").isDouble());
    }
    void doubleFieldProperty()
    {
        TestSettings settings;
        settings.doubleField(1.0);
        QCOMPARE(settings.doubleField(), 1.0);
        settings.doubleField(std::numeric_limits<double>::max());
        QCOMPARE(settings.doubleField(), std::numeric_limits<double>::max());
        settings.doubleField(std::numeric_limits<double>::max());
        QCOMPARE(settings.doubleField(), std::numeric_limits<double>::max());
        settings.doubleField(0.0);
        QCOMPARE(settings.doubleField(), 0.0);
        QVERIFY(settings.get("doubleField").isDouble());
    }
    void stringFieldProperty()
    {
        TestSettings settings;
        settings.stringField("test");
        QCOMPARE(settings.stringField(), "test");
        QVERIFY(settings.get("stringField").isString());
    }
    void arrayFieldProperty()
    {
        TestSettings settings;
        settings.arrayField(QJsonArray { 1, 2, 3 });
        QCOMPARE(settings.arrayField(), (QJsonArray { 1, 2, 3 }));
        QVERIFY(settings.get("arrayField").isArray());
    }
    void objectFieldProperty()
    {
        TestSettings settings;
        settings.objectField(QJsonObject { { "test", "test" } });
        QCOMPARE(settings.objectField(), (QJsonObject { { "test", "test" } }));
        QVERIFY(settings.get("objectField").isObject());
    }
    void unknownProperty()
    {
        TestSettings settings;
        QVERIFY(settings.set("test", true));
        QCOMPARE(settings.get("test"), true);
        QVERIFY(settings.set("test", 0));
        QCOMPARE(settings.get("test"), 0);
        QVERIFY(settings.set("test", 0.0));
        QCOMPARE(settings.get("test"), 0.0);
        QVERIFY(settings.set("test", "test"));
        QCOMPARE(settings.get("test"), "test");
        QVERIFY(settings.set("test", (QJsonArray { 1, 2, 3 })));
        QCOMPARE(settings.get("test"), (QJsonArray { 1, 2, 3 }));
        QVERIFY(settings.set("test", (QJsonObject { { "test", "test" } })));
        QCOMPARE(settings.get("test"), (QJsonObject { { "test", "test" } }));
    }
    void getPropertyWithIndexOperator()
    {
        TestSettings settings;
        settings.boolField(true);
        QCOMPARE(settings["boolField"], true);
        QVERIFY(settings.set("test", "test"));
        QCOMPARE(settings["test"], "test");
        // TODO: Use compiler black magic to check that settings["test"] = "test"; does not compile
    }
    void assignProperties()
    {
        TestSettings settings;
        settings.assign({
                            { "boolField", true },
                            { "intField", 1 },
                            { "doubleField", 1.0 },
                            { "stringField", "test" },
                            { "arrayField", QJsonArray { 1, 2, 3 } },
                            { "objectField", QJsonObject { { "test", "test" } } },
                        });
        QVERIFY(settings.get("boolField").isBool());
        QVERIFY(settings.get("intField").isDouble());
        QVERIFY(settings.get("doubleField").isDouble());
        QVERIFY(settings.get("stringField").isString());
        QVERIFY(settings.get("arrayField").isArray());
        QVERIFY(settings.get("objectField").isObject());
        QCOMPARE(settings.boolField(), true);
        QCOMPARE(settings.intField(), 1);
        QCOMPARE(settings.doubleField(), 1.0);
        QCOMPARE(settings.stringField(), "test");
        QCOMPARE(settings.arrayField(), (QJsonArray { 1, 2, 3 }));
        QCOMPARE(settings.objectField(), (QJsonObject { { "test", "test" } }));
    }
    void fieldChangeListener()
    {
        TestSettings settings;
        QSignalSpy spyChanged(&settings, &TestSettings::boolFieldChanged);
        settings.boolField(true);
        QCOMPARE(spyChanged.count(), 1);
        settings.boolField(true);
        QCOMPARE(spyChanged.count(), 1);
        settings.intField(2); // should not change by touching other properties
        settings.set("test", "test");
        QCOMPARE(spyChanged.count(), 1);
        settings.reset();
        QCOMPARE(spyChanged.count(), 2);
    }
    void propertyChangeListener()
    {
        TestSettings settings;
        QSignalSpy spyChanged(&settings, &TestSettings::propertyChanged);
        settings.boolField(true);
        QCOMPARE(spyChanged.count(), 1);
        settings.boolField(true);
        QCOMPARE(spyChanged.count(), 1);
        settings.intField(2);
        QCOMPARE(spyChanged.count(), 2);
        settings.set("test", "test");
        QCOMPARE(spyChanged.count(), 3);
        settings.reset();
        QCOMPARE(spyChanged.count(), 6); // all three properties reset
    }
    void unknownPropertyChangeListener()
    {
        TestSettings settings;
        QSignalSpy spyChanged(&settings, &TestSettings::unknownPropertyChanged);
        settings.set("test", "test");
        QCOMPARE(spyChanged.count(), 1);
        settings.set("test", "test");
        QCOMPARE(spyChanged.count(), 1);
        settings.set("test2", "test");
        QCOMPARE(spyChanged.count(), 2);
        settings.boolField(true); // should not change by touching named fields
        QCOMPARE(spyChanged.count(), 2);
        settings.reset();
        QCOMPARE(spyChanged.count(), 4); // both unknown properties reset
    }
    void fieldTypeMismatch()
    {
        TestSettings settings;
        settings.set("intField", "test");
        QVERIFY(settings.error());
        settings.set("intField", 4);
        QVERIFY(!settings.error());
    }
    void fieldWithValidatorList()
    {
        TestSettings settings;
        settings.validatedStringField("d");
        QVERIFY(settings.error());
        settings.validatedStringField("a");
        QVERIFY(!settings.error());
    }
    void fieldWithValidatorFunction()
    {
        TestSettings settings;
        settings.validatedArrayField({ 1, 2, "3" });
        QVERIFY(settings.error());
        settings.validatedArrayField({ 1, 2, 3 });
        QVERIFY(!settings.error());
    }
};

QTEST_GUILESS_MAIN(tst_json)
#include TEST_MOC
