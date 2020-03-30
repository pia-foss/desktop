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
#line SOURCE_FILE("configwriter.cpp")

#include "configwriter.h"
#include <cstring>

ConfigWriter::LineEnding ConfigWriter::endl;

ConfigWriter::ConfigWriter(const QString &path)
    : _file{path}
{
    // Always construct _stream, so we can invalidate if opening the file
    // failed.  QTextStream doesn't do anything with the file until we try to
    // insert something.
    _stream.emplace(&_file);
    if(!_file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "Unable to open config file for writing:" << path;
        invalidate();
    }
}

template<class CharT>
bool ConfigWriter::isCharSafe(CharT c)
{
    return c != '\n' && c != '\r';
}

template<>
bool ConfigWriter::isCharSafe(QChar c)
{
    return c.unicode() != '\n' && c.unicode() != '\r';
}

template<class TextIteratorT>
bool ConfigWriter::isTextSafe(TextIteratorT begin, TextIteratorT end)
{
    while(begin != end)
    {
        if(!isCharSafe(*begin))
            return false;
        ++begin;
    }
    return true;
}

bool ConfigWriter::isValueSafe(const QString &value)
{
    return isTextSafe(value.begin(), value.end());
}

bool ConfigWriter::isValueSafe(QStringView value)
{
    return isTextSafe(value.begin(), value.end());
}

bool ConfigWriter::isValueSafe(QLatin1String value)
{
    return isTextSafe(value.begin(), value.end());
}

bool ConfigWriter::isValueSafe(const QStringRef &value)
{
    return isTextSafe(value.begin(), value.end());
}

bool ConfigWriter::isValueSafe(const QByteArray &value)
{
    return isTextSafe(value.begin(), value.end());
}

bool ConfigWriter::isValueSafe(const char *pValue)
{
    return pValue && isTextSafe(pValue, pValue + std::strlen(pValue));
}

bool ConfigWriter::isValueSafe(char v)
{
    return isCharSafe(v);
}

bool ConfigWriter::isValueSafe(QChar v)
{
    return isCharSafe(v);
}

bool ConfigWriter::valid() const
{
    return !_stream.isNull();
}

void ConfigWriter::invalidate()
{
    qWarning() << "Config file invalidated:" << _file.fileName();
    _stream.clear();
    if(_file.isOpen())
        _file.close();
    if(!_file.remove())
    {
        qWarning() << "Unable to remove invaldated config file:"
            << _file.fileName();
    }
}

void ConfigWriter::endLine()
{
    if(valid())
        *_stream << '\n';
}
