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

#ifndef KAPPS_CORE_LOGGER_H
#define KAPPS_CORE_LOGGER_H

#include "util.h"
#include "stringslice.h"
#include "typename.h"
#include <kapps_core/logger.h>
#include <set>
#include <string>
#include <memory>
#include <sstream>
#include <ostream>
#include <cassert>
#include <deque>
#include <vector>
#include <unordered_set>
#include <type_traits>

// **********
// * Logger *
// **********
//
// The logger is used throughout shared Desktop modules to collect debug log
// messages and send them to a product-controlled logging sink.  PIA Desktop
// also uses this logger directly for its own logging.
//
// Log messages capture a source file/line location, a logging category, and the
// module from which the log occurred.  The context is all captured
// automatically (using macros since C++14 lacks std::source_location).
//
// ===Setup===
//
// Define a LogModule in each executable module using the logger:
//    // In a source file, say
//    // /home/user/workspace/pia_desktop/kapps-core/src/kapps-core.cpp:
//    KAPPS_CORE_LOG_MODULE(core, "src/kapps-core.cpp")
//
//    (This tells Logger the build root for this module, file paths will be
//    shown relative to the ".../kapps-core/" directory.)
//
// ===Usage===
//
// Use KAPPS_CORE_<level> to create log messages:
//    KAPPS_CORE_INFO() << "Read servers list, got" << servers.count() << "servers";
//    ...
//    KAPPS_CORE_WARNING() << "No servers for service" << service << "were found";
//    ...
//    KAPPS_CORE_ERROR() << "Can't load OpenSSL:" << ex.what();
//
// ===Product integration===
//
// 1. Create a logging sink by implementing ::kapps::core::LogCallback.
// 2. Initialize the logger with ::kapps::core::log::init().
//    - This must be done prior to using any other kapps module functionality.
// 3. Enable/disable logging (at any time) with ::kapps::core::log::enable().
//
// ===Custom categories===
//
// Categories are by default inferred from file paths.  For example, logging
// from a file at "/home/user/workspace/pia_desktop/kapps-core/src/executor.cpp"
// would infer the "executor" category (file basename) in the "core" module
// (from the LogModule defined above).  The file path would be traced as
// "src/executor.cpp" (from the paths specified to the LogModule).
//
// Custom categories can also be defined for classes, namespaces, functions,
// etc.:
//
//    class Executor
//    {
//        KAPPS_CORE_CLASS_CATEGORY("executor")
//        ...
//    }
//
//    namespace MethodTypes
//    {
//        KAPPS_CORE_NAMESPACE_CATEGORY("methodtypes")
//        ...
//    }
//
//    void receiveLatencyMeasurements()
//    {
//        KAPPS_CORE_FUNCTION_CATEGORY("latency")
//        ...
//    }
//
// ===Threading===
//
// Logging can occur from any thread.  LogModule and LogCategory objects can be
// used on any thread, but if categories (or modules) are constructed
// dynamically, then destruction must be serialized with any threads that might
// log using that category/module.  (Most categories are defined statically, so
// this isn't an issue.)
//
// Calls to the message callback are serialized.  The message callback itself
// cannot log when it is being invoked (but it could write directly to its own
// output).
//
// ===Custom argument types===
//
// To trace arbitrary types, define an operator<<() (using std::ostream) in that
// type's namespace (to be found with ADL):
//
//  namespace product
//  {
//      class Rule {};
//      std::ostream &operator<<(std::ostream &os, const Rule &);
//  }
//
// In case this isn't possible (mainly STL types), or if there is already an
// operator<<() and different output is needed for logging only, define an
// operator<<() using LogStream instead of std::ostream.  In this case, it can
// be in the kapps::core::log namespace if it's not possible to put it in the
// argument's namespace.

namespace kapps { namespace core {

class LogCategory;

// A module name for traces, emitted as context for the trace.  Module names
// should be string literals (they must outlive the LogModule), and should
// usually be a single label identifier (alphanumerics / dash / underscore are
// accepted).
class KAPPS_CORE_EXPORT LogModule
{
public:
    // Get the automatic log category for a __FILE__ value.  This creates the
    // category if it doesn't exist yet and a LogModule can be found.
    static const LogCategory *getAutoFileCategory(const StringSlice &refFile);

    // Get the logging module for a file path - used when defining a manual
    // log category with macros.
    static const LogModule *getFileModule(const StringSlice &refFile);

    // Get the effective logging module and category from the specified category
    // and the referencing file.  If a manual category is present, that
    // category is used.  Otherwise, if a LogModule can be found for the file
    // path, creates a file-based category in that module.  If neither of those
    // is possible, returns a default category.
    static const LogCategory &getEffectiveCategory(const LogCategory *pManualCategory,
                                                   const StringSlice &refFile);

    // Get the default category used when no category or module can be found
    // ("??") - this is also used by SourceLocation
    static const LogCategory &getDefaultCategory();

public:
    // Create LogModule with the module name (such as 'core', 'firewall', etc.),
    // relative path of this file within the module root, and absolute path to
    // this module (from __FILE__).
    //
    // The file relative path must use slashes (/), not backslashes, on all
    // platforms.  On Windows, backslashes are matched to slashes in module
    // prefixes.
    //
    // The file relative and absolute paths are used to determine the module
    // build root - the file path is stripped from the absolute path, and the
    // prefix is used as the module root.  For example, with relative path
    // "src/core.cpp" and absolute path
    // "/home/user/pia-desktop/kapps-core/src/core.cpp", then
    // "/home/user/pia-desktop/kapps-core/" is used as the module root - paths
    // logged in the 'core' component are relative to this directory.
    LogModule(const StringSlice &name, const StringSlice &thisFileRel,
              const StringSlice &thisFileAbs);
    ~LogModule();

private:
    // Not copiable - breaks invariant that there's one unique LogModule for a
    // given path prefix
    LogModule(const LogModule &) = delete;
    LogModule &operator=(const LogModule &) = delete;
    // Moves could be possible by swapping or clearing _pathPrefix but are not
    // currently needed.

public:
    const StringSlice name() const {return _name;}
    const StringSlice &pathPrefix() const {return _pathPrefix;}

private:
    const StringSlice _name;
    StringSlice _pathPrefix;
};

// A logging category for traces, emitted as context for the trace.  Categories
// should almost always be string literals - the string must outlive the
// LogCategory.
//
// Currently this is just emitted in logs - there is no way to filter individual
// categories, etc.  However, for consistency with Qt categories and for
// possible future expansion, category names should generally be "dotted label"
// identifiers following these rules:
//
// - Use alphanumerics and basic punctuation like dash(-) / underscore (_),
//   avoid other characters (especially asterisk(*), space, or equal(=), these
//   are significant in the Qt logging rules format).
// - Don't begin a category with 'qt.', or name a module 'qt'.
// - Don't end a category with '.debug', '.info', '.warning', or '.critical'.
//   (Qt interprets these as message level filters.)
// - For heirarchical labels, order labels from least- to most-specific.  (i.e.
//   'process.exec', 'process.run', 'win.handle'.)
class KAPPS_CORE_EXPORT LogCategory : public OStreamInsertable<LogCategory>
{
public:
    // Define a LogCategory with a LogModule and a category name
    LogCategory(const LogModule &module, const StringSlice &name) : _pModule{&module}, _name{name} {}
    // Define a LogCategory by looking up the default module for a file
    LogCategory(const StringSlice &refFile, const StringSlice &name);

public:
    // operator() is used in the manualLogCategory() lookup; if
    // manualLogCategory is a LogCategory object (not a function returning one),
    // this makes `manualLogCategory()` valid.
    const LogCategory *operator()() const {return this;}

    const LogModule *module() const {return _pModule;}
    const StringSlice &name() const {return _name;}

    void trace(std::ostream &os) const
    {
        if(module() && module()->name())
            os << module()->name();
        else
            os << "??";
        os << '.' << name();
    }

private:
    const LogModule *_pModule;
    StringSlice _name;
};

// Specifies a specific location in a source file - used for trace references
// as well as Error locations, etc.  In addition to a file and line, this also
// captures a log category (which in turn contains a log module).
//
// The log category always reflects the file referenced, not necessarily any
// manual category that might be in effect.  (That is captured by LogWriter.)
// SourceLocation uses the default category if no file category can be created;
// it always has a category.
class KAPPS_CORE_EXPORT SourceLocation : public OStreamInsertable<SourceLocation>
{
public:
    SourceLocation() : _pCategory{}, _file{}, _line{} {}
    SourceLocation(const StringSlice &refFile, int line);

public:
    // Get the effective category - if no manual category was present, this can
    // be a file category if a module was matched.  It is "??" if no category
    // could be found - there is always an effective category.
    const LogCategory &category() const
    {
        assert(_pCategory); // Class invariant
        return *_pCategory;
    }
    // Get the category's name, if present
    StringSlice categoryName() const {return category().name();}
    // Get the trace file path - if a category and module are present (and the
    // path actually matched the module's prefix), this is a path relative to
    // the module root.  Otherwise, it is the absolute path.
    //
    // This may contain slashes and/or backslashes depending on the platform.
    const StringSlice &file() const {return _file;}
    // Get the trace line
    int line() const {return _line;}

    void trace(std::ostream &os) const
    {
        os << '[' << category() << "] " << _file << ':' << _line;
    }

private:
    // There is always a log category - this is never nullptr.
    const LogCategory *_pCategory;
    StringSlice _file;
    int _line;
};

// Everything in a message sent to the LogCallback.
class KAPPS_CORE_EXPORT LogMessage
{
public:
    enum class Level
    {
        Fatal,
        Error,
        Warning,
        Info,
        Debug,
    };

public:
    LogMessage(SourceLocation loc, Level level, const LogCategory &category,
               std::string message)
        : _loc{std::move(loc)}, _level{level}, _category{category},
          _message{std::move(message)}
    {}

public:
    const SourceLocation &loc() const {return _loc;}
    Level level() const {return _level;}
    const LogCategory &category() const {return _category;}
    const std::string &message() const & {return _message;}
    std::string message() && {return std::move(_message);}

private:
    SourceLocation _loc;
    Level _level;
    const LogCategory &_category;
    std::string _message;
};

// The callback implemented by the application to receive log messages.
//
// The write() callback can be called on any thread, including APC threads on
// Windows.  Calls to the logger are serialized (by the logger mutex).  The
// log callback itself MUST NOT create any log messages; behavior if it does is
// undefined.  (The log callback could write diagnostics directly to is own
// output though.)
class KAPPS_CORE_EXPORT LogCallback
{
public:
    virtual void write(LogMessage msg) = 0;
};

namespace log
{
    // Initialize the logger with a callback.  This must be called before any
    // traces can be written; traces that occur before this are discarded.
    //
    // The shared_ptr can be aliased to another object if the log callback is
    // part of or otherwise owned by another object.
    void KAPPS_CORE_EXPORT init(std::shared_ptr<LogCallback> pCallback);

    // Enable or disable logging.  Note that LogWriter captures this state when
    // beginning a log message (to skip rendering the message if logging is
    // disabled), so log messages may still be written for a short time after
    // disabling logging.
    void KAPPS_CORE_EXPORT enableLogging(bool enable);
    bool KAPPS_CORE_EXPORT loggingEnabled();


    // A stringstream used to construct log messages.  This just allows
    // specializations of operator<<() to be defined in the kapps::core::log
    // namespace, which is used for STL types since we couldn't otherwise define
    // them in the argument's namespace.
    //
    // (It's not legal to define std::operator<<(std::ostream &, const std::vector<...> &),
    // as specializations in namespace std are only allowed when they depend on a
    // user type.)
    class LogStream : public std::stringstream
    {
    public:
        using std::stringstream::stringstream;
    };

    // A streamer is provided for std::vector (other STL containers can be
    // added as needed).  streamContainer() can also be used to implement
    // operator<<() for other container types.
    //
    // Tracing a container traces each element, surrounded by (..., ...).
    //
    // This is instantiated explicitly for each supported container type to
    // prevent it from applying to raw arrays, std::string directly, etc.
    template<class ContainerT>
    void streamContainer(std::ostream &os, const ContainerT &container)
    {
        auto itVal = container.begin();
        if(itVal == container.end())
        {
            os << "()";  // Empty list
            return;
        }

        os << '(' << *itVal;
        ++itVal;
        while(itVal != container.end())
        {
            os << ", " << *itVal;
            ++itVal;
        }
        os << ')';
    }
    template<class ValueT>
    LogStream &operator<<(LogStream &os, const std::vector<ValueT> &c)
    {
        streamContainer(os, c);
        return os;
    }
    template<class ValueT>
    LogStream &operator<<(LogStream &os, const std::deque<ValueT> &c)
    {
        streamContainer(os, c);
        return os;
    }
    template<class ValueT>
    LogStream &operator<<(LogStream &os, const std::unordered_set<ValueT> &c)
    {
        streamContainer(os, c);
        return os;
    }

    // Trace std::wstring using WStringSlice to convert to UTF-8
    inline LogStream &operator<<(LogStream &os, const std::wstring &s)
    {
        os << WStringSlice{s};
        return os;
    }

    // Trace enums using their names when possible
    template<class E>
    auto operator<<(LogStream &os, E value)
        -> std::enable_if_t<std::is_enum<E>::value, LogStream &>
    {
        auto name = enumValueName(value);
        if(name)
            os << name;
        else
            os << static_cast<std::underlying_type_t<E>>(value);
        return os;
    }

    // For any other type that doesn't have a streamer in the kapps::core::log
    // namespace, forward to the regular std::ostream streamer.
    //
    // This resolves ambiguity when tracing an object that has its own streamer
    // as well as a conversion operator to one of the supported types above,
    // since otherwise the type's own streamer would have the same number of
    // conversions as a streamer above (one - either LogStream->std::ostream, or
    // type->std::wstring).  By providing this template, we prefer the type's
    // specialized streamer rather than a conversion to one of the above types.
    template<class ValueT>
    auto operator<<(LogStream &os, const ValueT &v)
        -> std::enable_if_t<!std::is_enum<ValueT>::value, LogStream&>
    {
        std::ostream &stdOs{os};
        stdOs << v;
        return os;
    }
}

// This object writes a message to the log.  Text and data can be inserted with
// operator<<().  When the object is destroyed, the message is sent to the log.
// Values are inserted with operator<<(std::ostream &, <value>).
//
// LogWriter inserts spaces between values by default, like Qt's logger.
// nospace() and space() toggle this behavior.  Note that this only occurs when
// inserting into the LogWriter, not when inserting into std::ostream (such as
// in operator<<()).
//
// Messages can include embedded line breaks; such messages would be passed as-
// is to the log sink.
class KAPPS_CORE_EXPORT LogWriter
{
public:
    // When creating a LogWriter, it checks whether logging is currently
    // enabled.  If it isn't, then the message is not rendered - calls to
    // operator<<(std::ostream &, <value>) are skipped.  See the logging macros
    // below for more details.
    //
    // If a manual category is given, LogWriter uses that category.  Otherwise,
    // it uses the SourceLocation's category.
    LogWriter(SourceLocation loc, LogMessage::Level level,
              const LogCategory *pManualCategory);
    ~LogWriter();

private:
    LogWriter(const LogWriter &) = delete;
    LogWriter &operator=(const LogWriter &) = delete;

private:
    // Do work before inserting a value - the actual rendering of the value is
    // handled by insert().
    void prepareForInsert();

public:
    LogWriter &space() {_spacesEnabled = true; return *this;}
    LogWriter &nospace() {_spacesEnabled = false; return *this;}
    template<class T>
    void insert(T &&value)
    {
        if(_pMsg)
        {
            prepareForInsert();
            *_pMsg << std::forward<T>(value);
        }
    }

    // These methods are used to handle parameters passed to KAPPS_CORE_<level>()
    // macro invocations.  As mentioned there, this is not preferred but is
    // needed for compatibility with some Qt headers that include tracing in the
    // headers themselves.
    LogWriter &macroParams() {return *this;} // No-op, no params
    // Trace a printf-formatted format string.  Note that this is fragile and
    // dangerous as with any printf-style formatting, and shouldn't be used for
    // PIA code.  The PIA product proper actually can get away without this, but
    // QSignalSpy in unit tests does this in its header.
    LogWriter &macroParams(const char *msg, ...);

private:
    SourceLocation _loc;
    LogMessage::Level _level;
    const LogCategory &_category;
    nullable_t<log::LogStream> _pMsg;
    bool _spacesEnabled;
    bool _spaceBeforeNext;
};

template<class T>
LogWriter &operator<<(LogWriter &lw, T &&value)
{
    lw.insert(std::forward<T>(value));
    return lw;
}
template<class T>
LogWriter &&operator<<(LogWriter &&lw, T &&value)
{
    lw.insert(std::forward<T>(value));
    return std::move(lw);
}

}}

// Macros to simplify creation of the above objects and handle automatic
// categories - these primarily exist because C++14 lacks
// std::source_location(), so we have to use the
// __FILE__/__LINE__ macros.
#define KAPPS_CORE_LOG_FILE __FILE__
#define KAPPS_CORE_LOG_LINE __LINE__

// KAPPS_CORE_LOG_MODULE() can be used to declare a LogModule filling in the
// file absolute path.  The name must be a safe C++ identifier prefix in this
// case.
#define KAPPS_CORE_LOG_MODULE(name, thisFileRel) \
    namespace \
    { \
        ::kapps::core::LogModule name##LogModule{#name, thisFileRel, KAPPS_CORE_LOG_FILE}; \
    }

// These macros permit specifying manual logging categories for a class, file,
// function scope, namespace, etc.
//
// The current manual logging category is found by resolving
// `manualLogCategory()` in the current scope.  Because of
// LogCategory::operator(), manualLogCategory can be a LogCategory object or a
// function returning one.  If no manual category exists, this returns nullptr
// (default global manualLogCategory()) and a file-based category will be
// used.
//
// Note that this returns LogCategory*, because there might not be one - it can
// be nullptr.
#define KAPPS_CORE_CURRENT_MANUAL_CATEGORY (manualLoggingCategory())
// Get the actual effective category at this location - a manual category if
// there is one, or a file category otherwise.  (Not needed to create a
// SourceLocation but may be useful in other contexts.)
//
// This returns LogCategory&, there is always an effective category ("??" as a
// last resort).
#define KAPPS_CORE_CURRENT_CATEGORY \
    (::kapps::core::LogModule::getEffectiveCategory(KAPPS_CORE_CURRENT_MANUAL_CATEGORY, KAPPS_CORE_LOG_FILE))

// Define a logging category that applies to a class (and by default, to
// derived classes).
#define KAPPS_CORE_CLASS_CATEGORY(name) \
    protected: \
    static const ::kapps::core::LogCategory *manualLoggingCategory() \
    { \
        static const ::kapps::core::LogCategory cat{KAPPS_CORE_LOG_FILE, name}; \
        return &cat; \
    } \
    private:

// Define a logging category that applies to a function or any explicit scope
// within a function
#define KAPPS_CORE_SCOPE_CATEGORY(name) \
    static const ::kapps::core::LogCategory manualLoggingCategory{KAPPS_CORE_LOG_FILE, name};
#define KAPPS_CORE_FUNCTION_CATEGORY(name) \
    KAPPS_CORE_SCOPE_CATEGORY(name)

// Define a logging category that applies to a namespace.  If this is used from
// multiple files, use ..._DECL() to declare the category in a header, then
// implement it in a source file.
#define KAPPS_CORE_NAMESPACE_CATEGORY_DECL() \
    const ::kapps::core::LogCategory *manualLoggingCategory();
#define KAPPS_CORE_NAMESPACE_CATEGORY(name) \
    const ::kapps::core::LogCategory *manualLoggingCategory() \
    { \
        static const ::kapps::core::LogCategory cat{KAPPS_CORE_LOG_FILE, name}; \
        return cat; \
    }
// Define a logging category that applies to a file.
#define KAPPS_CORE_FILE_CATEGORY(name) \
    namespace { KAPPS_CORE_NAMESPACE_CATEGORY(name) }

// Lowest priority match for the current manual category that returns nullptr.
// (It's a template so that a `manualLoggingCategory()` can still be defined in
// the global namespace or an anonymous namespace that would be preferred over
// this one.)
template<typename... Args> static inline const ::kapps::core::LogCategory *manualLoggingCategory(Args&&...)
{
    return nullptr;
}

// SourceLocation for the current location
#define HERE \
    ::kapps::core::SourceLocation{KAPPS_CORE_LOG_FILE, KAPPS_CORE_LOG_LINE}

// Create a log message - shorthand for creating a LogWriter object.
// The log message object is always created, even if logging is disabled, and
// the insertions are always evaluated.  (This minimizes the risk that enabling
// logging will cause crashes, etc., and minimizes fragile macro trickery.)
//
// Although the parameters are always evaluated, the actual rendering of the
// message is skipped if logging is disabled, which should eliminate most of the
// performance impact of logging when disabled without fragility/trickery in the
// macros themselves.  If a log message does have an unavoidable performance,
// make sure the hard work is done by operator<<() rather than by the
// argument evaluation.
//
// For example, prefer
//   LOG_INFO() << "Huge JSON:" << myQJsonObject; // with an appropriate operator<<()
// instead of
//   LOG_INFO() << "Huge JSON:" << myQJsonObject.toJSON(); // renders JSON even when logging is disabled
//
// The level-specific macros also support including a message in the macro
// invocation rather than inserting it (KAPPS_CORE_DEBUG("Some debug info")).
// This is only for compatibility with Qt headers; PIA hooks up these macros to
// qDebug(), etc., and some Qt headers include tracing of this type in the
// header.  Additional arguments are not supported (but are forwarded from the
// macro to ensure they'll cause a compile-time error).
#define KAPPS_CORE_LOG(level) \
    ::kapps::core::LogWriter{HERE, ::kapps::core::LogMessage::Level::level, nullptr}
#define KAPPS_CORE_DEBUG(...) KAPPS_CORE_LOG(Debug).macroParams(__VA_ARGS__)
#define KAPPS_CORE_INFO(...) KAPPS_CORE_LOG(Info).macroParams(__VA_ARGS__)
#define KAPPS_CORE_WARNING(...) KAPPS_CORE_LOG(Warning).macroParams(__VA_ARGS__)
#define KAPPS_CORE_ERROR(...) KAPPS_CORE_LOG(Error).macroParams(__VA_ARGS__)
#define KAPPS_CORE_FATAL(...) KAPPS_CORE_LOG(Fatal).macroParams(__VA_ARGS__)

// Log with a manual category
#define KAPPS_CORE_LOG_CATEGORY(level, cat) \
    ::kapps::core::LogWriter{HERE, ::kapps::core::LogMessage::Level::level, &(cat)}
#define KAPPS_CORE_DEBUG_CATEGORY(cat) KAPPS_CORE_LOG_CATEGORY(Debug, cat)
#define KAPPS_CORE_INFO_CATEGORY(cat) KAPPS_CORE_LOG_CATEGORY(Info, cat)
#define KAPPS_CORE_WARNING_CATEGORY(cat) KAPPS_CORE_LOG_CATEGORY(Warning, cat)
#define KAPPS_CORE_ERROR_CATEGORY(cat) KAPPS_CORE_LOG_CATEGORY(Error, cat)
#define KAPPS_CORE_FATAL_CATEGORY(cat) KAPPS_CORE_LOG_CATEGORY(Fatal, cat)

#endif
