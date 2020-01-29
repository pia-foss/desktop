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
#line SOURCE_FILE("linebuffer.cpp")

#include "linebuffer.h"

void LineBuffer::append(const QByteArray &data)
{
    _buffer += data;

    int lineStartPos = 0;
    int lineEndPos = _buffer.indexOf('\n', lineStartPos);
    while(lineEndPos >= 0)
    {
        int lineTrimmedEnd = lineEndPos;
        if(lineEndPos > lineStartPos && _buffer[lineEndPos-1] == '\r')
            --lineTrimmedEnd;

        QByteArray line = _buffer.mid(lineStartPos, lineTrimmedEnd - lineStartPos);
        emit lineComplete(line);

        lineStartPos = lineEndPos + 1;
        lineEndPos = _buffer.indexOf('\n', lineStartPos);
    }

    // lineStartPos is now the beginning of the last partial line in the buffer.
    // If the buffer content ended with '\n', it's the length of the buffer, so
    // this empties the buffer.
    _buffer = _buffer.mid(lineStartPos);
}

QByteArray LineBuffer::reset()
{
    return std::exchange(_buffer, QByteArray{});
}
