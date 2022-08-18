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
#include <daemon/src/networkmonitor.h>
#include <QtTest>

class tst_networkmonitor : public QObject
{
    Q_OBJECT

private:
    // Create a NetworkConnection and parse SSID data.  Returns an empty QString
    // if the SSID is invalid or not representable as text, or the parsed SSID
    // otherwise.
    static QString parseSsid(const char *data, std::size_t len)
    {
        NetworkConnection test;
        test.parseWifiSsid(data, len);
        return test.wifiSsid();
    }

    // Parse an SSID and deduce the array length from a string literal
    template<std::size_t len>
    static QString parseSsid(const char (&data)[len])
    {
        // Subtact 1 from len to ignore the null terminator added by the
        // compiler
        return parseSsid(data, len-1);
    }

private slots:
    // Trivial tests - just verify a few basic ASCII SSIDs
    void testParseSsidsAscii()
    {
        // Max length - 32 bytes
        QCOMPARE(parseSsid("PrivateInternetAccess Unit Tests"),
                 "PrivateInternetAccess Unit Tests");

        // Min length - 1 byte
        QCOMPARE(parseSsid("A"), "A");

        // Control characters (other than null).  We do accept these, although
        // the UI will need to make sure they render reasonably.
        QCOMPARE(parseSsid("Put it on my tab\t\n"), "Put it on my tab\t\n");
        QCOMPARE(parseSsid("Ring the bell\a\n"), "Ring the bell\a\n");
    }

    // Test valid UTF-8 encodings
    // The input data and expected output look similar here, but they're
    // encoded as UTF-8 and UTF-16 respectively by the compiler (u8 vs. u)
    void testParseSsidsUtf8()
    {
        // PIA "rotated" - if decoded as Latin-1 incorrectly, results in garbage
        QCOMPARE(parseSsid(u8"\u2c6fI\ua4d2"),
                 QStringLiteral(u"\u2c6fI\ua4d2"));

        // "SPÓÓKÝ" in UTF-8 (this can also be encoded in Latin-1)
        QCOMPARE(parseSsid("SP\xc3\x93\xc3\x93K\xc3\x9d"),
                 QStringLiteral(u"SP\u00d3\u00d3K\u00dd"));
        QCOMPARE(parseSsid(u8"SP\u00d3\u00d3K\u00dd"),
                 QStringLiteral(u"SP\u00d3\u00d3K\u00dd"));

        // Banana emoji - non-BMP
        QCOMPARE(parseSsid(u8"Banana \U0001F34C"),
                 QStringLiteral(u"Banana \U0001F34C"));

        // A family emoji - tests non-BMP characters and zero-width joiners
        QCOMPARE(parseSsid(u8"a family: \U0001F468\u200d\U0001F469\u200d\U0001F467"),
                 QStringLiteral(u"a family: \U0001F468\u200d\U0001F469\u200d\U0001F467"));
    }

    // Test valid Latin-1
    // Not much to see here, just make sure we fail over to Latin-1 if the input
    // is not valid UTF-8
    void testParseSsidsLatin1()
    {
        // "SPÓÓKÝ" in Latin-1 (this can also be encoded in UTF-8)
        QCOMPARE(parseSsid("SP\xd3\xd3K\xdd"),
                 QStringLiteral(u"SP\u00d3\u00d3K\u00dd"));
    }

    // Test invalid UTF-8 encodings
    // Note that some invalid UTF-8 values might incidentally be valid Latin-1,
    // so in some cases it's expected to fail over to Latin-1 and get nonsense
    // output.
    void testParseSsidsUtf8Invalid()
    {
        // The "rotated PIA" string above, with the last code point truncated.
        // Although this isn't valid Latin-1 either (0x93 is not a valid Latin-1
        // code point), Qt's Latin-1 codec doesn't care, so we fall back to
        // Latin-1 for this.
        QCOMPARE(parseSsid("\xe2\xb1\xafI\xea\x93"),
                 QStringLiteral(u"\u00E2\u00B1\u00AFI\u00EA\u0093"));

        // Overlong encoding of a null character - we rely on the codec to
        // reject this to prevent embedded null characters in UTF-8 strings.
        // Here too, this ends up falling back to Latin-1, which is reasonable.
        QCOMPARE(parseSsid("A null\xC0\x80in 16 bits"),
                 QStringLiteral(u"A null\u00C0\u0080in 16 bits"));

        // UTF-8 encoding of a UTF-16 surrogate pair ("CESU-8", surrogate pairs
        // aren't valid code points to encode in UTF-8)
        // Banana (U+1F34C) in UTF-16 is D83C DF4C
        // D83C -> ED A0 BC
        // DF4C -> ED BD CC
        // This also falls back to Latin-1.
        QCOMPARE(parseSsid("Banana \xed\xa0\xbc\xed\xbd\xcc"),
                 QStringLiteral(u"Banana \u00ED\u00A0\u00BC\u00ED\u00BD\u00CC"));
    }

    void testParseSsidsLatin1Invalid()
    {
        // The only thing that's invalid in Latin-1 are embedded null
        // characters - we explicitly check for zero bytes in the input.  Qt's
        // Latin-1 codec accepts unassigned Latin-1 code points, which is fine.
        QCOMPARE(parseSsid("Some\x00nulls\x00"), QString{});
    }

    void testParseSsidsInvalid()
    {
        // Zero length - not valid.  Explicitly check for the "invalid" trace,
        // since we wouldn't be able to distinguish a "successful" empty result
        // from an "invalid" empty result.
        NetworkConnection zeroLenTest;
        QTest::ignoreMessage(QtMsgType::QtWarningMsg, R"(Wi-Fi SSID can't be represented as text ( 0 bytes): "")");
        zeroLenTest.parseWifiSsid("", 0);

        // Too long - 32 bytes is the max
        QCOMPARE(parseSsid("This text has length thirty-three"), QString{});

        // Too long - >32 bytes of input, even though it would decode to <32
        // code points
        // "SPÓÓKÝSPÓÓKÝSPÓÓKÝSPÓÓKÝSPÓÓKÝ" - 30 code points, 45 UTF-8 code units
        QCOMPARE(parseSsid(u8"SP\u00d3\u00d3K\u00ddSP\u00d3\u00d3K\u00ddSP\u00d3\u00d3K\u00ddSP\u00d3\u00d3K\u00ddSP\u00d3\u00d3K\u00dd"),
                 QString{});
    }
};

QTEST_GUILESS_MAIN(tst_networkmonitor)
#include TEST_MOC
