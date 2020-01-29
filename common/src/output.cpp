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
#line SOURCE_FILE("output.cpp")

#include "output.h"
#include <QFile>

int OutputIndent::_totalIndentation = 0;

OutputIndent::OutputIndent(int amount)
    : _amount{amount}
{
    _totalIndentation += _amount;
}

OutputIndent::~OutputIndent()
{
    _totalIndentation -= _amount;
}

namespace
{
    // QIODevice decorator that applies the indentation level controlled by
    // OutputIndent to each line
    class IndentedIODevice : public QIODevice
    {
    public:
        IndentedIODevice(FILE *stream, OpenMode openMode)
            : _startLine{true}
        {
            _dev.open(stream, openMode);
            QIODevice::open(openMode);
        }
        virtual qint64 readData(char *data, qint64 maxSize) override
        {
            return _dev.read(data, maxSize);
        }
        virtual qint64 readLineData(char *data, qint64 maxSize) override
        {
            return _dev.readLine(data, maxSize);
        }
        virtual qint64 writeData(const char *data, qint64 maxSize) override;

    private:
        bool _startLine;
        QFile _dev;
    };

    qint64 IndentedIODevice::writeData(const char *data, qint64 maxSize)
    {
        const auto &indent = OutputIndent::getTotalIndentation();
        const char indentChar = ' ';

        const char *end = data + maxSize;
        while(data != end)
        {
            // If we're at the beginning of a line, write the indentation
            if(_startLine)
            {
                _startLine = false;
                for(int i=0; i<indent; ++i)
                {
                    if(_dev.write(&indentChar, 1) == -1)
                        return -1;
                }
            }

            // Write up to the end of data or a line terminator
            auto lineEnd = std::find(data, end, '\n');
            if(_dev.write(data, lineEnd - data) == -1)
                return -1;

            // If a line break was found, write it and flag the start of the next line
            if(lineEnd != end)
            {
                _dev.write(lineEnd, 1); // Newline char
                // Flush the output buffer after each line, important for
                // real-time logging
                _dev.flush();
                _startLine = true;
                ++lineEnd;  // Wrote this char
            }
            // Advance the current position past the written data
            data = lineEnd;
        }

        return maxSize;
    }

    IndentedIODevice _indentedOut{stdout, QIODevice::OpenModeFlag::WriteOnly};
    IndentedIODevice _indentedErr{stderr, QIODevice::OpenModeFlag::WriteOnly};
}

OutputLine::OutputLine(QIODevice *dev)
    : _spaceBeforeNext{false}
{
    _stream.emplace(dev);
}

OutputLine::OutputLine(OutputLine &&other)
    : _spaceBeforeNext{other._spaceBeforeNext}
{
    if(other._stream)
    {
        QIODevice *pTakenDevice = other._stream->device();
        other._stream.clear();
        _stream.emplace(pTakenDevice);
    }
}

OutputLine::~OutputLine()
{
    // Terminate line (unless OutputLine was moved-from)
    if(_stream && _stream->device())
        *_stream << '\n';
}

OutputLine outln()
{
    return OutputLine{&_indentedOut};
}

OutputLine errln()
{
    return OutputLine{&_indentedErr};
}
