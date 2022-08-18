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

#include "logger.h"
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <cstdio>
#include <cstdarg>

namespace kapps { namespace core {

namespace
{
    bool isPathSep(char c)
    {
#ifdef KAPPS_CORE_OS_WINDOWS
        if(c == '\\')
            return true;
#endif
        return c == '/';
    }

    bool pathCharEqual(char a, char b)
    {
        if(isPathSep(a) && isPathSep(b))
            return true;
        return a == b;
    }
    bool pathCharLess(char a, char b)
    {
        if(isPathSep(a) && isPathSep(b))
            return false;   // Not less (path separators are equal even if different chars)
        return a < b;
    }

    // Get the part of a string between the last / (or '\\' on Windows) and the
    // first subsequent '.'.  Note that this is a simplified algorithm and does
    // not attempt to handle some corner cases:
    // - /foo/.hidden returns "" (empty basename)
    // - C:foo.txt on Windows returns "C:foo.txt" (does not handle drive-relative path)
    StringSlice basename(const StringSlice &file)
    {
        if(file.size() == 0)
            return {};

        // Look for the last '/', or '\\' on Windows.  If there are none, the
        // basename starts at the beginning of the path given
        std::size_t basenameStart = file.size() - 1;
        while(basenameStart > 0)
        {
            if(isPathSep(file[basenameStart-1]))
                break;  // Got a slash, basename starts here
            // Otherwise, keep looking
            --basenameStart;
        }

        // Look for the first '.' following the slash.  If there are none, the
        // basename extends to the end of the string.
        std::size_t basenameEnd = basenameStart;
        while(basenameEnd < file.size())
        {
            if(file[basenameEnd] == '.')
                break;  // Got a dot, basename ends here
            // Otherwise, keep looking
            ++basenameEnd;
        }

        return {file.data() + basenameStart, file.data() + basenameEnd};
    }

    using mutex_lock = std::lock_guard<std::mutex>;
}

namespace log
{
    struct LogData
    {
        // Mutex protection all parts of LogData
        std::mutex _dataMutex;
        // Current log callback
        std::shared_ptr<LogCallback> _pCallback;
        // Whether logging is enabled
        bool _enabled;
    };

    LogData &logData()
    {
        static LogData _data{};
        return _data;
    }

    void init(std::shared_ptr<LogCallback> pCallback)
    {
        auto &data = logData();
        mutex_lock l{data._dataMutex};

        data._pCallback = std::move(pCallback);
    }

    void enableLogging(bool enable)
    {
        auto &data = logData();
        mutex_lock l{data._dataMutex};

        data._enabled = enable;
    }

    bool loggingEnabled()
    {
        auto &data = logData();
        mutex_lock l{data._dataMutex};

        return data._enabled;
    }

    void write(LogMessage msg)
    {
        auto &data = logData();
        mutex_lock l{data._dataMutex};

        if(data._pCallback)
            data._pCallback->write(std::move(msg));
        // Otherwise, discard the message.  We could consider counting discarded
        // messages and tracing that count when a callback is installed to
        // validate that we're not missing tracing during initialization.
    }
}

namespace
{
    class LogModulePrefixCompare
    {
    public:
        // Enable comparing a path in a StringSlice directly to LogModules in
        // the set below
        using is_transparent = std::true_type;
        // Compare two paths, ignoring '/' and '\\' differences on Windows.
        bool operator()(const StringSlice &first, const StringSlice &second) const
        {
            return std::lexicographical_compare(first.begin(), first.end(),
                                                second.begin(), second.end(),
                                                &pathCharLess);
        }
        // Compare a path with a LogModule's path prefix or two LogModules'
        // path prefixes.
        StringSlice modulePtrPrefix(const LogModule *pModule) const
        {
            return pModule ? pModule->pathPrefix() : StringSlice{};
        }
        bool operator()(const StringSlice &first, const LogModule *pSecond) const
        {
            return operator()(first, modulePtrPrefix(pSecond));
        }
        bool operator()(const LogModule *pFirst, const StringSlice &second) const
        {
            return operator()(modulePtrPrefix(pFirst), second);
        }
        bool operator()(const LogModule *pFirst, const LogModule *pSecond) const
        {
            return operator()(modulePtrPrefix(pFirst), modulePtrPrefix(pSecond));
        }
    };

    struct LogModuleData
    {
        // Mutex synchronizing access to LogModuleData.
        std::mutex _dataMutex;
        // All modules by directory prefix - used to look up modules for file
        // contexts.
        std::set<LogModule*, LogModulePrefixCompare> _allModules;
        // Generated file basename categories, keyed by __FILE__ pointers.  Note
        // that this keys based on the pointer value, not the string content -
        // two copies of the same string are distinct.  Since this is called a
        // _lot_, the gains of a quick pointer comparison are probably
        // significant.
        //
        // If duplicates would occur, we'd just create two duplicate but
        // equivalent LogCategory objects.
        std::unordered_map<const char *, LogCategory> _fileCategories;
    };

    LogModuleData &logModuleData()
    {
        static LogModuleData data;
        return data;
    }
}

const LogCategory *LogModule::getAutoFileCategory(const StringSlice &refFile)
{
    auto &data = logModuleData();
    mutex_lock l{data._dataMutex};

    // Is there already a generated file category?
    auto itFileCat = data._fileCategories.find(refFile.data());
    if(itFileCat != data._fileCategories.end())
        return &itFileCat->second;

    // There isn't, create one.  Find the module.  We want the nearest module
    // ordered _before_ this file's path in the order imposed by the set, so
    // find the upper bound and then move back to the previous element.
    auto itModule = data._allModules.upper_bound(refFile);
    if(itModule == data._allModules.begin())
        return nullptr; // No module for this file
    // itModule == ...end() is OK, that indicates that we should look at the
    // last module.
    --itModule;
    assert(*itModule);  // Invariant, no nullptrs here

    // Make sure this file is actually in the module's build directory - e.g.
    // given "/repo/foo/main.cpp", the lower_bound() would still give us
    // "/repo/bar" if there is no module for "/repo/foo".
    if(refFile.size() < (*itModule)->pathPrefix().size() ||
       !std::equal((*itModule)->pathPrefix().begin(), (*itModule)->pathPrefix().end(),
                    refFile.begin(), &pathCharEqual))
    {
        return nullptr;
    }

    auto emplaceResult = data._fileCategories.emplace(refFile.data(),
        LogCategory{**itModule, basename(refFile)});
    return &emplaceResult.first->second;
}

const LogModule *LogModule::getFileModule(const StringSlice &refFile)
{
    // Although we could look in data._allModules() for this, using the file
    // category map leverages the _fileCategories cache, which uses a hashed
    // lookup on the address of the exact file path.
    //
    // The only cost is that we create a file basename category for every
    // logging file even if all tracing uses manual categories.  This should be
    // rare since the vast majority of tracing uses automatic categories, and
    // the cost of a LogCategory is small.
    const LogCategory *pCategory = getAutoFileCategory(refFile);
    return pCategory ? pCategory->module() : nullptr;
}

const LogCategory &LogModule::getEffectiveCategory(const LogCategory *pManualCategory,
                                                   const StringSlice &refFile)
{
    if(pManualCategory)
        return *pManualCategory;
    const LogCategory *pAutoFileCategory{getAutoFileCategory(refFile)};
    if(pAutoFileCategory)
        return *pAutoFileCategory;

    // As a last resort, return a default category - there is _always_ an
    // effective logging category
    return getDefaultCategory();
}

const LogCategory &LogModule::getDefaultCategory()
{
    static const LogModule defaultModule{"??", "", ""};
    static const LogCategory defaultCategory{defaultModule, "??"};
    return defaultCategory;
}

LogModule::LogModule(const StringSlice &name, const StringSlice &thisFileRel,
                     const StringSlice &thisFileAbs)
    : _name{name}
{
    // Make sure the absolute path ends with the relative path specified.
    // If any check fails, the path prefix is left empty and the module is not
    // added to data._allModules.
    if(thisFileAbs.size() < thisFileRel.size())
        return;

    auto suffixStart = thisFileAbs.end() - thisFileRel.size();
    if(!std::equal(thisFileRel.begin(), thisFileRel.end(), suffixStart,
                   &pathCharEqual))
    {
        return; // Suffixes don't match
    }

    // Suffixes match, grab the absolute path prefix
    _pathPrefix = {thisFileAbs.begin(), suffixStart};

    // Add to _allModules
    auto &data = logModuleData();
    mutex_lock l{data._dataMutex};

    if(!data._allModules.insert(this).second)
    {
        // Failed to insert; this is a duplicate module.  Wipe out _pathPrefix
        _pathPrefix = {};
    }
}

LogModule::~LogModule()
{
    if(!_pathPrefix.empty())
    {
        auto &data = logModuleData();
        mutex_lock l{data._dataMutex};

        data._allModules.erase(this);
    }
}

LogCategory::LogCategory(const StringSlice &refFile, const StringSlice &name)
    : _pModule{LogModule::getFileModule(refFile)}, _name{name}
{
}

SourceLocation::SourceLocation(const StringSlice &refFile, int line)
    : _pCategory{LogModule::getAutoFileCategory(refFile)}, _file{refFile},
      _line{line}
{
    // If we couldn't get a file category, use a default category so
    // SourceLocation always has a category
    if(!_pCategory)
        _pCategory = &LogModule::getDefaultCategory();  // Default category, don't trim
    // If there was an automatic file category, LogModule validated the path
    // prefix, so we can trim the file path using this module.
    else if(_pCategory->module())
    {
        _file = {refFile.begin() + _pCategory->module()->pathPrefix().size(),
                refFile.end()};
    }
}

LogWriter::LogWriter(SourceLocation loc, LogMessage::Level level,
                     const LogCategory *pManualCategory)
    : _loc{loc}, _level{level},
      _category{pManualCategory ? *pManualCategory : loc.category()},
      _spacesEnabled{true}, _spaceBeforeNext{false}
{
    // Skip the message entirely if logging is not enabled
    if(log::loggingEnabled())
    {
        _pMsg.emplace(std::ios_base::out);
    }
}

LogWriter::~LogWriter()
{
    if(_pMsg)
    {
        // This copies the message unnecessarily - in C++14 there is no way to
        // move out the string content.  C++20 adds an rvalue str() overload, or
        // in C++14 we could write our own streambuf implementation.
        log::write({_loc, _level, _category, _pMsg->str()});
    }
}

void LogWriter::prepareForInsert()
{
    if(_pMsg)
    {
        if(_spacesEnabled && _spaceBeforeNext)
            *_pMsg << ' ';
        _spaceBeforeNext = true;
    }
}

LogWriter &LogWriter::macroParams(const char *msg, ...)
{
    // Since we have to traverse the args twice (once to determine the buffer size
    // and once to actually render), we need two copies of the va_list.
    std::va_list argsLen, argsRender;
    va_start(argsLen, msg);
    va_copy(argsRender, argsLen);

    // Compute the size of the buffer needed
    int bufSize = std::vsnprintf(nullptr, 0, msg, argsLen);
    // If nothing was written, there's nothing to do.  Negatives can be returned
    // for errors, which are ignored.
    if(bufSize > 0)
    {
        std::string formatted;
        formatted.resize(bufSize+1);
        int actualSize = std::vsnprintf(&formatted[0], formatted.size(), msg, argsRender);
        formatted.resize(std::min(actualSize, bufSize));
        *this << msg;
    }

    va_end(argsLen);
    va_end(argsRender);

    return *this;
}

}}
