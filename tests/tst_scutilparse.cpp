// Copyright (c) 2026 Private Internet Access, Inc.
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
#include <QtTest>
#include <QString>
#include "extras/openvpn/mac/scutil_parser.h"

class tst_scutilparse : public QObject
{
    Q_OBJECT

private slots:

    void testParserArrayWithBrokenIdentifier()
    {
        const QString scutilText = "<array> {\n"
                                   "  0 : Default: FC6F7E27-969A-4E96-AEBF-A75441964325 0\n"
                                   "}\n";

        auto parseResult = scutilParse(scutilText);
        auto array = parseResult.toArray();

        QCOMPARE(array.size(), 1);
        QCOMPARE(array[0].toString(),
                 QStringLiteral("Default: FC6F7E27-969A-4E96-AEBF-A75441964325 0"));
    }

    void testParserFlatDict()
    {
        const QString scutilText = "<dictionary> {\n  PrimaryInterface : en0\n  PrimaryService : BBCB371A-6C30-4A04-96B3-541D935CA495\n  Router : 192.168.1.1\n}\n";
        auto parseResult = scutilParse(scutilText);
        auto value = parseResult.toObject().value("PrimaryService").toString();
        QCOMPARE(value, "BBCB371A-6C30-4A04-96B3-541D935CA495");
    }

    void testParserNestedDict()
    {
        const QString scutilText = "<dictionary> {\n  Foo : Foovalue\n  Bar : <dictionary> {\n  nestedkey : nestedvalue\n }\n  Baz : bazvalue\n}\n";
        auto parseResult = scutilParse(scutilText);
        auto value = parseResult.toObject().value("Bar").toObject().value("nestedkey").toString();
        QCOMPARE(value, "nestedvalue");
    }

    void testParserFlatArray()
    {
        const QString scutilText = "<array> {\n  0 : zero\n  1 : one\n  2 : two\n}\n";
        auto parseResult = scutilParse(scutilText);
        auto array = parseResult.toArray();

        QCOMPARE(array[0].toString(), "zero");
        QCOMPARE(array[1].toString(), "one");
        QCOMPARE(array[2].toString(), "two");
    }

    void testParserNestedArray()
    {
        const QString scutilText = "<array> {\n  0 : zero\n  1 : <array> {\n 0 : nestedzero\n 1 : nestedone\n}\n  2 : two\n}\n";
        auto parseResult = scutilParse(scutilText);
        auto array = parseResult.toArray();

        QCOMPARE(array[0].toString(), "zero");
        QCOMPARE(array[1].toArray()[0], "nestedzero");
        QCOMPARE(array[2].toString(), "two");
    }

    void testParserMultiTokenScalarValue()
    {
        const QString scutilText = R"(<dictionary> {
  NetworkSignatureHash : <data> 0xf01d9930e439d7a10f5e5cb03b648e116859cde3
  NetworkSignature : IPv4.Router=192.168.254.254;IPv4.RouterHardwareAddress=b8:9f:cc:6a:4f:d8
}
)";

        auto parseResult = scutilParse(scutilText);
        auto object = parseResult.toObject();

        QCOMPARE(object.value("NetworkSignatureHash").toString(),
                 QStringLiteral("<data> 0xf01d9930e439d7a10f5e5cb03b648e116859cde3"));
        QCOMPARE(object.value("NetworkSignature").toString(),
                 QStringLiteral(
                     "IPv4.Router=192.168.254.254;IPv4.RouterHardwareAddress=b8:9f:cc:6a:4f:d8"));
    }

    void testParserMultiTokenScalarValueWithExtraSpaces()
    {
        const QString scutilText =
            "<dictionary> {\n"
            "  NetworkSignatureHash : <data>  0xf01d9930e439d7a10f5e5cb03b648e116859cde3  \n"
            "}\n";

        auto parseResult = scutilParse(scutilText);
        auto object = parseResult.toObject();

        QCOMPARE(object.value("NetworkSignatureHash").toString(),
                 QStringLiteral("<data> 0xf01d9930e439d7a10f5e5cb03b648e116859cde3"));
    }

    void testParserNoKeyError()
    {
        // Scutil returns "  No such key" when it can't find
        // a key for a given dict - in this case we treat this error text
        // as a token in the grammar and just return a Null QJson object.
        const QString scutilText = "  No such key\n";
        auto parseResult = scutilParse(scutilText);

        QCOMPARE(parseResult, QJsonValue::Null);
    }

    void testParserErrorResultsInUndefined()
    {
        // Incomplete dictionary definition should result in parse error
        const QString scutilText = "  <dictionary> {\n";
        auto parseResult = scutilParse(scutilText);

        QCOMPARE(parseResult, QJsonValue::Undefined);
    }

    void testEmptyDictionary()
    {
        const QString scutilText = "<dictionary> {\n }\n";
        auto parseResult = scutilParse(scutilText);

        QCOMPARE(parseResult, QJsonObject{});
    }

    void testEmptyArray()
    {
        const QString scutilText = "<array> {\n }\n";
        auto parseResult = scutilParse(scutilText);

        QCOMPARE(parseResult, QJsonArray{});
    }

    void testParserReturnsNullIfDictionaryContainsPIAEmpty()
    {
        // A key of PIAEmpty implies the dictionary is empty, this causes the
        // parser to throw out the dictionary and just return Null
        const QString scutilText = "<dictionary> {\n PIAEmpty : TRUE\n }\n";
        auto parseResult = scutilParse(scutilText);

        QCOMPARE(parseResult, QJsonValue::Null);
    }

    void testParserWhiteSpaceIsIgnored()
    {
        const QString scutilText = "<dictionary> {\n            alpha : a\n    beta : b\ngamma : g\n}\n";
        auto parseResult = scutilParse(scutilText);
        auto alphaValue = parseResult.toObject().value("alpha").toString();
        auto betaValue = parseResult.toObject().value("beta").toString();
        auto gammaValue = parseResult.toObject().value("gamma").toString();

        QCOMPARE(alphaValue, "a");
        QCOMPARE(betaValue, "b");
        QCOMPARE(gammaValue, "g");
    }
};

QTEST_GUILESS_MAIN(tst_scutilparse)
#include TEST_MOC
