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
#line HEADER_FILE("output.h")

#ifndef OUTPUT_H
#define OUTPUT_H

#include "util.h"
#include <QTextStream>

// Indent output produced by OutputLine().
class COMMON_EXPORT OutputIndent
{
private:
    static int _totalIndentation;

public:
    static int getTotalIndentation() {return _totalIndentation;}

public:
    OutputIndent(int amount);
    ~OutputIndent();

private:
    OutputIndent(const OutputIndent &) = delete;
    OutputIndent &operator=(const OutputIndent &) = delete;

private:
    int _amount;
};

// Object used to write one line of output to stdout or stderr.  Has a few
// features of QDebug:
// - inserts spaces between streamed values
// - terminates line when destroyed
class COMMON_EXPORT OutputLine
{
public:
    OutputLine(QIODevice *dev);
    OutputLine(OutputLine &&other);
    ~OutputLine();

private:
    OutputLine(const OutputLine &) = delete;
    OutputLine &operator=(const OutputLine &) = delete;

public:
    template<class Value>
    void put(Value &&value);

private:
    bool _spaceBeforeNext;
    nullable_t<QTextStream> _stream;
};

template<class Value>
void OutputLine::put(Value &&value)
{
    // Ignore if moved-from
    if(_stream && _stream->device())
    {
        if(_spaceBeforeNext)
            *_stream << ' ';
        *_stream << std::forward<Value>(value);
        _spaceBeforeNext = true;
    }
}

template<class Value>
OutputLine &operator<<(OutputLine &output, Value &&value)
{
    output.put(std::forward<Value>(value));
    return output;
}

// Trace to an rvalue OutputLine
template<class Value>
OutputLine &operator<<(OutputLine &&output, Value &&value)
{
    return output << std::forward<Value>(value);
}

// Write a line to stdout/stderr - use insertion operators to print values. For
// example:
//     outln() << "standard output" << 42;
//     outln(); // blank line
//     const char *msg = "an error occurred";
//     errln() << "standard error -" << msg;
OutputLine COMMON_EXPORT outln();
OutputLine COMMON_EXPORT errln();

#endif
