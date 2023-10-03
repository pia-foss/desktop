// Copyright (c) 2023 Private Internet Access, Inc.
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

#pragma once
#include <kapps_core/src/logger.h>
#include <fstream>
#include <sstream>
#include <utility>

// ConfigWriter is used to write a line-delimited configuration file.  It
// ensures that option values don't unexpectedly contain line break characters.
// (Otherwise, user-controlled option values could be used to inject unexpected
// options into the config file, such as if a username was something like
// "p1234\ncheck_certificate=false".
//
// ConfigWriter internally uses a QTextStream and QFile to write the file.  If
// a line break occurs in an unexpected place, it deletes the file and flags
// itself as invalid.
//
// It also flags itself as invalid if the config file can't be opened, etc.  It
// is possible that the config file still exists in this case, so callers should
// check the invalid flag before attempting to use the config file, rather than
// just relying on it to be deleted.
//
// ConfigWriter only checks for errant line endings.  It doesn't know anything
// else about the format being used.  For the formats used in PIA (OpenVPN and
// WireGuard), this is sufficient to guard against option injection.

namespace kapps { namespace core {
class KAPPS_CORE_EXPORT ConfigWriter
{
public:
    // Marker for inserting a line ending.
    // ConfigWriter{...} << ConfigWriter::endl; is equivalent to endLine().
    static struct LineEnding {} endl;

public:
    // Create ConfigWriter with the path to the file.  This attempts to open the
    // file.  If it can't the writer becomes invalid.
    explicit ConfigWriter(const std::string &path);

private:
    // Check if a character is safe to write (not a line break character)
    template<class CharT>
    bool isCharSafe(CharT c);
    // Check if text indicated by a range is safe to write (does not contain any
    // line breaks).
    template<class TextIteratorT>
    bool isTextSafe(TextIteratorT begin, TextIteratorT end);;

    // Validators for types - these determine what can be written to
    // ConfigWriter.  Types to be written must have an isValueSafe()
    // implementation, and it must return true for a particular value to be
    // written.
    // Strings
    bool isValueSafe(const std::string &value);
    bool isValueSafe(const char *pValue);
    // Chars
    bool isValueSafe(char v);
    // Integers - these are always safe, they never cause a line break
    bool isValueSafe(int){return true;}
    bool isValueSafe(unsigned int){return true;}
    bool isValueSafe(long long){return true;}
    bool isValueSafe(unsigned long long ){return true;}

public:
    // Check if the writer is valid/invalid.  Once it becomes invalid, the file
    // is removed if possible, and all subsequent writes are ignored.
    //
    // The file could still exist if it was not possible to open it in the first
    // place, so callers should check the invalid flag before using the file.
    bool valid() const {return _valid;}
    bool invalid() const {return !valid();}

    // Call to invalidate the config file.  This is used if any written value
    // unexpectedly contains a line ending, but it can also be used by the
    // caller if the config file cannot be written for some other reason.
    void invalidate();

    // Write a complete line - a line ending will be added.
    // Equvalent to write(line); endLine();
    // "line" is often a string but can be any value that can be passed to
    // write().
    template<class ValueT>
    void writeLine(ValueT &&line){write(std::forward<ValueT>(line)); endLine();}

    // Write content to the current line.  Any value type can be written as long
    // as it has:
    //   - a QTextStream::operator<<() implementation
    //   - a ConfigWriter::isValueSafe() implementation
    template<class ValueT>
    void write(const ValueT &value)
    {
        if(invalid())
            return; // Already invalid
        if(!isValueSafe(value))
        {
            KAPPS_CORE_WARNING() << "Invalidate config file" << _path
                << "due to invalid value" << value;
            invalidate();
            return;
        }
        // Safe to write the value
        _file << value;
    }

    // Terminate the current line
    void endLine();

private:
    std::ofstream _file;
    std::string _path;
    bool _valid{true};
};

// Insertion operator to write values to ConfigWriter
template<class ValueT>
ConfigWriter &operator<<(ConfigWriter &w, ValueT &&v)
{
    w.write(std::forward<ValueT>(v));
    return w;
}

// Insertion operator for line ending
inline ConfigWriter &operator<<(ConfigWriter &w, ConfigWriter::LineEnding)
{
    w.endLine();
    return w;
}

}}
