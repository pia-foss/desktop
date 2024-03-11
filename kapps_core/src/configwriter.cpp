// Copyright (c) 2024 Private Internet Access, Inc.
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

#include <kapps_core/core.h>
#include "logger.h"
#include "util.h"

#include "configwriter.h"
#include <cstring>

namespace kapps { namespace core {

ConfigWriter::LineEnding ConfigWriter::endl;

ConfigWriter::ConfigWriter(const std::string &path)
    :
#ifdef KAPPS_CORE_OS_WINDOWS
    // Use the filesystem::path constructor otherwise windows interprets the UTF-8 as the system code page,
    // resulting in incorrect behavior
    // We can only rely on support foor std::filesystem in windows currently :/
    _file{std::filesystem::u8path(path)}
#else
    _file{path}
#endif
    , _path{path}
{
    if(!_file.is_open())
    {
        KAPPS_CORE_WARNING() << "Unable to open config file for writing:" << _path;
        invalidate();
    }
}

template<class CharT>
bool ConfigWriter::isCharSafe(CharT c)
{
    return c != '\n' && c != '\r';
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

bool ConfigWriter::isValueSafe(const std::string &value)
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

void ConfigWriter::invalidate()
{
    KAPPS_CORE_WARNING() << "Config file invalidated:" << _path;
    if(_file.is_open())
        _file.close();

    if(removeFile(_path))
    {
        KAPPS_CORE_WARNING() << "Unable to remove invaldated config file:"
            << _path;
    }

    _valid = false;
}

void ConfigWriter::endLine()
{
    if(valid())
        _file << '\n';
}
}}
