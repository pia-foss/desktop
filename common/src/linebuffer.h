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

#include "common.h"
#line HEADER_FILE("linebuffer.h")

#ifndef LINEBUFFER_H
#define LINEBUFFER_H

// Buffers output and emits completed lines.  Usually used to handle QProcess
// stdout/stderr streams.
// Any partial line that remains on destruction (a partial line that wasn't
// terminated with a line break) is ignored.  If the process is restarted,
// reset() can be used to reset the buffer.
class COMMON_EXPORT LineBuffer : public QObject
{
    Q_OBJECT

public:
    // Add data to the buffer; emits lineComplete() for completed lines.
    void append(const QByteArray &data);

    // Reset the buffer; returns the partial line left in the buffer (if there
    // was one)
    QByteArray reset();

signals:
    // A completed line was read
    void lineComplete(const QByteArray &line);

private:
    QByteArray _buffer;
};

#endif
