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

#include "common/src/semversion.h"
#include <QtTest>

class tst_semversion : public QObject
{
    Q_OBJECT

private slots:
    // Test that PrereleaseTag::fieldToInt() handles numeric and non-numeric
    // fields correctly
    void numericFields()
    {
        // Valid integers return integer value
        QCOMPARE(PrereleaseTag::fieldToInt(u"0"), 0);
        QCOMPARE(PrereleaseTag::fieldToInt(u"002"), 2);
        QCOMPARE(PrereleaseTag::fieldToInt(u"10007"), 10007);
        QCOMPARE(PrereleaseTag::fieldToInt(u"2112"), 2112);
        QCOMPARE(PrereleaseTag::fieldToInt(u"8675309"), 8675309);

        // Invalid values return PrereleaseTag::NonNumeric
        QCOMPARE(PrereleaseTag::fieldToInt(u""), PrereleaseTag::NonNumeric);
        QCOMPARE(PrereleaseTag::fieldToInt(u"alpha"), PrereleaseTag::NonNumeric);
        QCOMPARE(PrereleaseTag::fieldToInt(u"-1000"), PrereleaseTag::NonNumeric);
        QCOMPARE(PrereleaseTag::fieldToInt(u"100utf"), PrereleaseTag::NonNumeric);
        QCOMPARE(PrereleaseTag::fieldToInt(u"  55"), PrereleaseTag::NonNumeric);
        QCOMPARE(PrereleaseTag::fieldToInt(u"55  "), PrereleaseTag::NonNumeric);
        // Integer part would overflow, should still be detected as non-numeric
        QCOMPARE(PrereleaseTag::fieldToInt(u"99999999999999999999alpha"), PrereleaseTag::NonNumeric);

        // Values that would be valid, but can't be represented as integers on
        // this platform, should throw
        // Test value from tag above
        QVERIFY_EXCEPTION_THROWN(PrereleaseTag::fieldToInt(u"99999999999999999999"), Error);
        // Overflow in the multiply-by-10 step (next power of 10 above 2^63)
        QVERIFY_EXCEPTION_THROWN(PrereleaseTag::fieldToInt(u"10000000000000000000"), Error);
        // Overflow in the add-digit step (2^63)
        QVERIFY_EXCEPTION_THROWN(PrereleaseTag::fieldToInt(u"9223372036854775808"), Error);

        if(sizeof(int) == 4)
        {
            QVERIFY_EXCEPTION_THROWN(PrereleaseTag::fieldToInt(u"10000000000"), Error);
            QVERIFY_EXCEPTION_THROWN(PrereleaseTag::fieldToInt(u"2147483648"), Error);
        }
    }

    // Test comparisons of prerelease tags
    void prereleaseTags()
    {
        PrereleaseTag zero{u"0"}, two{u"2"}, ten{u"10"};
        PrereleaseTag oten{u"010"}, tenf{u"10f"};
        PrereleaseTag alpha{u"alpha"}, ALPHA{u"ALPHA"};
        PrereleaseTag beta{u"beta"}, BETA{u"BETA"};

        QVERIFY(zero == zero);
        QVERIFY(oten == oten);
        QVERIFY(beta == beta);

        QVERIFY(zero < two);
        QVERIFY(two < ten); // Verifies numeric, not lexicographical, ordering
        QVERIFY(ten == oten);

        QVERIFY(two < tenf); // Numerics lower than non-numerics
        QVERIFY(tenf > two);

        QVERIFY(ALPHA < alpha); // ASCII-betical case-sensitive
        QVERIFY(BETA < beta);
        QVERIFY(alpha < beta);
        QVERIFY(alpha > BETA);
        QVERIFY(ALPHA < beta);
        QVERIFY(ALPHA < BETA);
    }

    void invalidVersions()
    {
        QVERIFY_EXCEPTION_THROWN(SemVersion{u""}, Error);
        QVERIFY_EXCEPTION_THROWN(SemVersion{u"."}, Error);
        // Invalid patch, minor, major (empties)
        QVERIFY_EXCEPTION_THROWN(SemVersion{u"0.0."}, Error);
        QVERIFY_EXCEPTION_THROWN(SemVersion{u"0.."}, Error);
        QVERIFY_EXCEPTION_THROWN(SemVersion{u".."}, Error);
        // Garbage after version
        QVERIFY_EXCEPTION_THROWN(SemVersion{u"0.0.0bogus"}, Error);
        QVERIFY_EXCEPTION_THROWN(SemVersion{u"0.0.0??"}, Error);
        QVERIFY_EXCEPTION_THROWN(SemVersion{u"0.0.0.0"}, Error);
        // Empty prerelease tags
        QVERIFY_EXCEPTION_THROWN(SemVersion{u"0.0.0-alpha."}, Error);
        QVERIFY_EXCEPTION_THROWN(SemVersion{u"0.0.0-alpha..beta"}, Error);
    }

    // Test versions with no tags
    void versionsNoTags()
    {
        SemVersion v001{u"0.0.1"}, v005{u"0.0.5"};
        SemVersion v010{u"0.1.0"}, v055{u"0.5.5"};
        SemVersion v100{u"1.0.0"}, v105{u"1.0.5"};

        // Spelling all of these out is a little excessive, but refactoring it
        // into a list of versions and checking all permutations with loops
        // would produce pretty bad test output.  Since QVERIFY() is macro
        // based, all failures would print the same line number and expression,
        // it'd be impossible to tell which failed from the output.

        QVERIFY(v001 == v001);
        QVERIFY(v001 < v005);
        QVERIFY(v001 < v010);
        QVERIFY(v001 < v055);
        QVERIFY(v001 < v100);
        QVERIFY(v001 < v105);

        QVERIFY(v005 > v001);
        QVERIFY(v005 == v005);
        QVERIFY(v005 < v010);
        QVERIFY(v005 < v055);
        QVERIFY(v005 < v100);
        QVERIFY(v005 < v105);

        QVERIFY(v010 > v001);
        QVERIFY(v010 > v005);
        QVERIFY(v010 == v010);
        QVERIFY(v010 < v055);
        QVERIFY(v010 < v100);
        QVERIFY(v010 < v105);

        QVERIFY(v055 > v001);
        QVERIFY(v055 > v005);
        QVERIFY(v055 > v010);
        QVERIFY(v055 == v055);
        QVERIFY(v055 < v100);
        QVERIFY(v055 < v105);

        QVERIFY(v100 > v001);
        QVERIFY(v100 > v005);
        QVERIFY(v100 > v010);
        QVERIFY(v100 > v055);
        QVERIFY(v100 == v100);
        QVERIFY(v100 < v105);

        QVERIFY(v105 > v001);
        QVERIFY(v105 > v005);
        QVERIFY(v105 > v010);
        QVERIFY(v105 > v055);
        QVERIFY(v105 > v100);
        QVERIFY(v105 == v105);
    }

    // Test prerelease tags
    void versionPrereleaseTags()
    {
        SemVersion v050a{u"0.5.0-a"}, v050b{u"0.5.0-b"}, v050{u"0.5.0"};
        SemVersion v100a{u"1.0.0-a"}, v100{u"1.0.0"};

        QVERIFY(v050a == v050a);
        QVERIFY(v050a < v050b);
        QVERIFY(v050a < v050);
        QVERIFY(v050a < v100a);
        QVERIFY(v050a < v100);

        QVERIFY(v050b > v050a);
        QVERIFY(v050b == v050b);
        QVERIFY(v050b < v050);
        QVERIFY(v050b < v100a);
        QVERIFY(v050b < v100);

        QVERIFY(v050 > v050a);
        QVERIFY(v050 > v050b);
        QVERIFY(v050 == v050);
        QVERIFY(v050 < v100a);
        QVERIFY(v050 < v100);

        QVERIFY(v100a > v050a);
        QVERIFY(v100a > v050b);
        QVERIFY(v100a > v050);
        QVERIFY(v100a == v100a);
        QVERIFY(v100a < v100);

        QVERIFY(v100 > v050a);
        QVERIFY(v100 > v050b);
        QVERIFY(v100 > v050);
        QVERIFY(v100 > v100a);
        QVERIFY(v100 == v100);
    }

    // Verify construction from individual components
    void componentConstruction()
    {
        SemVersion val34{3, 4};
        SemVersion str34{u"3.4.0"};
        QVERIFY(val34 == str34);
        SemVersion val567{5, 6, 7};
        SemVersion str567{u"5.6.7"};
        QVERIFY(val567 == str567);

        SemVersion valPrerelease{8, 9, 10, {PrereleaseTag{u"semver"},
                                            PrereleaseTag{u"test"},
                                            PrereleaseTag{u"16"}}};
        SemVersion strPrerelease{u"8.9.10-semver.test.16"};
        QVERIFY(valPrerelease == strPrerelease);
    }

    // Test build tags
    void versionBuildTags()
    {
        SemVersion v100i{u"1.0.0+i"}, v100j{u"1.0.0+j"}, v100{u"1.0.0"};
        SemVersion v100ai{u"1.0.0-a+i"}, v100aj{u"1.0.0-a+j"}, v100a{u"1.0.0-a"};

        QVERIFY(v100i == v100j);
        QVERIFY(v100i == v100);
        QVERIFY(v100j == v100);

        QVERIFY(v100ai == v100aj);
        QVERIFY(v100ai == v100a);
        QVERIFY(v100aj == v100a);

        QVERIFY(v100i > v100ai);
        QVERIFY(v100i > v100a);
    }
};

QTEST_GUILESS_MAIN(tst_semversion)
#include TEST_MOC
