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

#include "linebuffer.h"

namespace samples
{
    const QByteArray oneNewline{"hello\n"};
    const QByteArray multipleNewlines{"hello\nworld\n"};
    const QByteArray noNewlines{"goodbye"};
}

class tst_linebuffer : public QObject
{
    Q_OBJECT

private slots:

    // lineComplete signal is only emitted if text in buffer ends in a newline
    void testAppendOneNewline()
    {
        LineBuffer buf;
        QSignalSpy spy{&buf, &LineBuffer::lineComplete};
        buf.append(samples::oneNewline);

        QCOMPARE(spy.count(), 1);

        QList<QVariant> arguments = spy.takeFirst();
        auto line = arguments.at(0).toByteArray();

        QCOMPARE(arguments.at(0).type(), QVariant::ByteArray);
        QCOMPARE(line, QByteArray{"hello"}); // trailing newline is removed
    }

    // signals are emitted for each newline in buffer
    void testAppendMultipleNewlines()
    {
        LineBuffer buf;
        QSignalSpy spy{&buf, &LineBuffer::lineComplete};
        buf.append(samples::multipleNewlines);

        // There are two lines, so the signal was emitted twice
        QCOMPARE(spy.count(), 2);

        // First line
        QList<QVariant> arguments1 = spy.takeFirst();
        auto line1 = arguments1.at(0).toByteArray();
        QCOMPARE(line1, QByteArray{"hello"});

        // Second line
        QList<QVariant> arguments2 = spy.takeFirst();
        auto line2 = arguments2.at(0).toByteArray();
        QCOMPARE(line2, QByteArray{"world"});
    }

    // buffered text has no newline, so no signal is emitted
    void testAppendNoNewline()
    {
        LineBuffer buf;
        QSignalSpy spy{&buf, &LineBuffer::lineComplete};
        buf.append(samples::noNewlines);

        // No lines
        QCOMPARE(spy.count(), 0);
    }

    void testAppendNoNewlineFollowedByNewline()
    {
        LineBuffer buf;
        QSignalSpy spy{&buf, &LineBuffer::lineComplete};
        buf.append(samples::noNewlines);
        QCOMPARE(spy.count(), 0);

        // Adding a subsequent newline causes buffered text to be emitted
        buf.append(QByteArray{"\n"});
        QCOMPARE(spy.count(), 1);

        QList<QVariant> arguments = spy.takeFirst();
        auto line = arguments.at(0).toByteArray();

        QCOMPARE(arguments.at(0).type(), QVariant::ByteArray);
        QCOMPARE(line, QByteArray{"goodbye"});
    }

    void testReset()
    {
        LineBuffer buf;
        QSignalSpy spy{&buf, &LineBuffer::lineComplete};
        buf.append(samples::noNewlines);

        // Clear out existing text in buffer
        buf.reset();

        // Append new text, this time with a newline
        buf.append(QByteArray{"sunshine\n"});
        QCOMPARE(spy.count(), 1);

        QList<QVariant> arguments = spy.takeFirst();
        auto line = arguments.at(0).toByteArray();

        QCOMPARE(arguments.at(0).type(), QVariant::ByteArray);

        // No trace of text previous to reset() exists in bufffer
        QCOMPARE(line, QByteArray{"sunshine"});
    }

};

QTEST_GUILESS_MAIN(tst_linebuffer)
#include TEST_MOC
