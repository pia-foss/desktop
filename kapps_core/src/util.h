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

#ifndef KAPPS_CORE_UTIL_H
#define KAPPS_CORE_UTIL_H

#include <kapps_core/core.h>
#include <type_traits>
#include <algorithm>
#include <vector>
#include <string>
#include <ostream>
#include <sstream>
#include <type_traits>
#include <cstring>
#include <memory>
#include <cassert>
#include <chrono>

#ifdef KAPPS_CORE_OS_WINDOWS
#include "winapi.h"
#include <filesystem> // For std::filesystem::{u8path, remove}
#else
#include <unistd.h>  // For unlink()
#endif

namespace kapps { namespace core {

// CRTP ostream inserter - implement void trace(std::ostream &os) const to
// provide operator<<().
template<class Derived>
class OStreamInsertable
{
public:
    friend std::ostream &operator<<(std::ostream &os, const OStreamInsertable &value)
    {
        static_cast<const Derived&>(value).trace(os);
        return os;
    }
};

// Add a value to a hash - shuffle the existing hash and incorporate the hash of
// the new value.  Useful when the fields are dynamic, like an array - for
// fields known at compile time, hashFields() is more convenient.
template<class T>
[[nodiscard]] std::size_t hashAccumulate(std::size_t hash, const T &value)
{
    // The shuffle constant is derived from the golden ratio - first 64 or 32
    // bits of the fractional part.
    static const std::size_t shuffleConstant{ (sizeof(std::size_t) == sizeof(std::uint64_t)) ?
        static_cast<std::size_t>(0x9e3779b97f4a7c15) :    // 64-bit
        0x9e3779b9};            // 32-bit
    hash ^= std::hash<T>{}(value) + shuffleConstant + (hash<<6) + (hash >>2);
    return hash;
}

// Combine hashes from any number of fields - useful to implement std::hash or
// hash()
inline std::size_t hashFields() {return 0;}

template<class T, class... Ts>
std::size_t hashFields(const T &first, const Ts &...rest)
{
    return hashAccumulate(hashFields(rest...), first);
}

// Helper class to grant a non-pointer type an additional possible assignable
// value of nullptr, similar to std::optional.
//
template<typename T>
class nullable_t : public OStreamInsertable<nullable_t<T>>
{
    static_assert(!std::is_pointer<T>::value, "not instantiable for pointer types");
    alignas(T) char _data[sizeof(T)];
    bool _valid;
public:
    nullable_t() : _valid(false) {}
    nullable_t(const std::nullptr_t&) : _valid(false) {}
    nullable_t(const T& value) : _valid(true) { new(_data) T(value); }
    nullable_t(T&& value) : _valid(true) { new(_data) T(std::move(value)); }
    nullable_t(const nullable_t& copy) : _valid(copy._valid) { if (_valid) new(_data) T(copy.get()); }
    nullable_t(nullable_t&& move) : _valid(move._valid) { if (_valid) new(_data) T(std::move(move.get())); }
    template<typename U> nullable_t(const nullable_t<U>& copy) : _valid(copy._valid) { if (_valid) new(_data) T(copy.get()); }
    template<typename U> nullable_t(nullable_t<U>&& move) : _valid(move._valid) { if (_valid) new(_data) T(std::move(move.get())); }
    ~nullable_t() { clear(); }

    nullable_t& operator=(const std::nullptr_t&) { clear(); return *this; }
    nullable_t& operator=(const T& value) { if (_valid) get() = value; else { new(_data) T(value); _valid = true; } return *this; }
    nullable_t& operator=(T&& value) { if (_valid) get() = std::move(value); else { new(_data) T(std::move(value)); _valid = true; } return *this; }
    nullable_t& operator=(const nullable_t& copy) { if (copy._valid) { *this = copy.get(); } else clear(); return *this; }
    nullable_t& operator=(nullable_t&& move) { if (move._valid) { *this = std::move(move.get()); } else clear(); return *this; }
    template<typename U> nullable_t& operator=(const nullable_t<U>& copy) { if (copy._valid) { *this = copy.get(); } else clear(); return *this; }
    template<typename U> nullable_t& operator=(nullable_t<U>&& move) { if (move._valid) { *this = std::move(move.get()); } else clear(); return *this; }

    void clear() { if (_valid) { _valid = false; get().~T(); } }

    bool isNull() const { return !_valid; }
    explicit operator bool() const { return _valid; }
    bool operator!() const { return !_valid; }

    // Caution: these are not safe unless you check against nullptr first!
    T& get() { return *reinterpret_cast<T*>(_data); }
    const T& get() const { return *reinterpret_cast<const T*>(_data); }
    T* operator->() { return reinterpret_cast<T*>(_data); }
    const T* operator->() const { return reinterpret_cast<const T*>(_data); }
    T& operator*() { return *reinterpret_cast<T*>(_data); }
    const T& operator*() const { return *reinterpret_cast<const T*>(_data); }

    // Retrieve value as a pointer (only safe as long as the nullable_t lives)
    T* ptr() { return _valid ? reinterpret_cast<T*>(_data) : nullptr; }
    const T* ptr() const { return _valid ? reinterpret_cast<const T*>(_data) : nullptr; }

    T& defaultConstructIfNull() { if (!_valid) { emplace(); } return get(); }

    // Construct a T in-place.  If a T already exists, destroys it first.
    // If the new T's constructor throws, the nullable_t becomes empty.
    template<class... Args_t>
    T& emplace(Args_t &&... args)
    {
        clear();
        new(_data) T{std::forward<Args_t>(args)...};
        _valid = true;
        return get();
    }

    void trace(std::ostream &os) const
    {
        if(*this)
            os << get();
        else
            os << "<null>";
    }
};

template<typename T, typename U> static inline bool operator==(const nullable_t<T>& a, const nullable_t<U>& b) { return a ? b && *a == *b : !b; }
template<typename T, typename U> static inline bool operator!=(const nullable_t<T>& a, const nullable_t<U>& b) { return a ? !b || *a != *b : !!b; }

template<typename T, typename U> static inline bool operator==(const nullable_t<T>& a, const U& b) { return a && *a == b; }
template<typename T, typename U> static inline bool operator==(const U& b, const nullable_t<T>& a) { return a && *a == b; }
template<typename T, typename U> static inline bool operator!=(const nullable_t<T>& a, const U& b) { return !a || *a != b; }
template<typename T, typename U> static inline bool operator!=(const U& b, const nullable_t<T>& a) { return !a || *a != b; }

template<typename T> static inline bool operator==(const nullable_t<T>& a, const std::nullptr_t&) { return !a; }
template<typename T> static inline bool operator==(const std::nullptr_t&, const nullable_t<T>& a) { return !a; }
template<typename T> static inline bool operator!=(const nullable_t<T>& a, const std::nullptr_t&) { return !!a; }
template<typename T> static inline bool operator!=(const std::nullptr_t&, const nullable_t<T>& a) { return !!a; }

// Trace an errno value - writes numeric value and name from qt_error_string()
class KAPPS_CORE_EXPORT ErrnoTracer : public OStreamInsertable<ErrnoTracer>
{
public:
    ErrnoTracer(int code) : _code{code} {}
    ErrnoTracer() : ErrnoTracer{errno} {}

public:
    void trace(std::ostream &os) const
    {
        os << "(code: " << _code << ") " << strerror(_code);
    }

private:
    int _code;
};

// Trace a duration
class KAPPS_CORE_EXPORT MsecTracer : public OStreamInsertable<MsecTracer>
{
public:
    MsecTracer(std::chrono::milliseconds msec) : _msec{msec} {}

public:
    void trace(std::ostream &os) const
    {
        auto count = _msec.count();
        if(count < 0)
        {
            os << '-';
            count = -count;
        }
        auto minutes = count / 60000;
        auto msec = count % 60000;
        double secDbl = msec / 1000.0;

        if(minutes > 0)
            os << minutes << " min ";
        os << secDbl << " sec";
    }
private:
    std::chrono::milliseconds _msec;
};

inline MsecTracer traceMsec(std::chrono::milliseconds msec) {return msec;}

struct PtrValueLess
{
    template<class Ptr_t>
    bool operator()(const Ptr_t &first, const Ptr_t &second) const
    {
        return *first < *second;
    }
};

namespace detail_
{
    // Implementation of Any - basically a polymorphic value holder.
    // AnyImplBase defines the abstract operations needed on values (get the
    // typeid, get a value pointer).
    class AnyImplBase
    {
    public:
        virtual ~AnyImplBase() = default;

        // Test if the other object holds the same type, and if the values are
        // equal
        //virtual bool equal(const AnyImplBase &other) const = 0;
        // Get the type of the contained object
        virtual const std::type_info &type() const = 0;
        // Returns a pointer of the type indicated by type(), cast to void*
        virtual void *valuePtr() = 0;
    };

    // Implementation of AnyImplBase for a specific type - holds a T value and
    // provides typeid/value pointer.
    template<class T>
    class AnyImpl : public AnyImplBase
    {
    public:
        AnyImpl(T value) : _value{std::move(value)} {}

    public:
        virtual const std::type_info &type() const override {return typeid(T);}
        virtual void *valuePtr() override {return reinterpret_cast<void*>(&_value);}

    private:
        T _value;
    };
}

// Holder for a value of any type (like std::any, which requires C++17).
//
// Any can be empty or contain a value of type - the type does not have to be
// copiable (although this means that Any also can't be copied).  Moving the Any
// transfers ownership of the contained value (it does not have to be movable).
//
// To act on the value inside Any, use handle(), which allows visitor functors
// to act on matching types.  For example:
//
//    Any value = ...; // Get a value of unknown type
//    value.handle<int>([](int i){ log("int:", i); })
//         .handle<double>([](double d){ log("double:", d); })
//         .handle<std::string>([](const std::string &s){ log("string:", s); });
//
// Visitor functors matching the current type are called; visitors not matching
// the type are not.  This must currently be an exact match to the value type;
// for example, `handle<std::exception>` will not match a
// std::runtime_exception.
//
// The visitor is passed const T& if the Any is const, or T& otherwise.  (The
// visitor can of course still accept a const T& parameter, etc.)
//
// Any::handle simply returns the Any, so any number of visitors can be chained.
// All matching visitors are invoked in the order specified.
class KAPPS_CORE_EXPORT Any
{
public:
    Any() = default;
    template<class T>
    Any(T value)
        : _pImpl{new detail_::AnyImpl<std::remove_cv_t<std::remove_reference_t<T>>>{std::move(value)}}
    {}
    Any(Any &&) = default;

    Any &operator=(Any &&) = default;

    explicit operator bool() const {return !empty();}
    bool operator!() const {return empty();}

private:
    template<class T, class Func>
    void handleImpl(Func f) const
    {
        // handleImpl<const T&> applies to a value of type T
        using ValueT = std::remove_cv_t<std::remove_reference_t<T>>;
        if(containsType<T>())
        {
            // value() returns a pointer to ValueT (not cv or reference)
            // Keep cv on the argument pointer type though - if the caller asked
            // for cv, the functor can't take a non-cv parameter.
            // (IOW, handleImpl<const T&>([](T &value){}) is not valid.)
            //
            // This is important for the const overload of handle() to enforce
            // const-ness.
            std::remove_reference_t<T> *pValue =
                reinterpret_cast<ValueT*>(_pImpl->valuePtr());
            assert(pValue); // Postcondition of AnyImpl::value
            f(*pValue);
        }
    }

public:
    bool empty() const {return !_pImpl;}

    // Name of the contained type, or nullptr if empty.  Note that the name is
    // implementation-defined, and may be mangled, so this is generally only
    // useful for tracing.
    const char *typeName() const {return _pImpl ? _pImpl->type().name() : nullptr;}

    // Test whether Any contains a given type.  This is equivalent to whether
    // handle<>() with the given type will match and invoke the functor.
    template<class T>
    bool containsType() const
    {
        return _pImpl && _pImpl->type() == typeid(std::remove_cv_t<std::remove_reference_t<T>>);
    }

    template<class T, class Func>
    Any &handle(Func f) {handleImpl<T>(std::move(f)); return *this;}
    template<class T, class Func>
    const Any &handle(Func f) const {handleImpl<const T>(std::move(f)); return *this;}

private:
    std::unique_ptr<detail_::AnyImplBase> _pImpl;
};

// Trace the value of a pointer if it's valid, or nullptr
template<class PointerT>
class TracePointer : public OStreamInsertable<TracePointer<PointerT>>
{
public:
    TracePointer(const PointerT &ptr) : _ptr{ptr} {}
public:
    void trace(std::ostream &os) const
    {
        if(_ptr)
            os << *_ptr;
        else
            os << "<null>";
    }
private:
    const PointerT &_ptr;
};

template<class PointerT>
TracePointer<PointerT> tracePointer(const PointerT &ptr){return ptr;}

// Add const to a type - or don't - based on a template parameter
template<bool makeConst, class T>
struct add_const_if;
template<class T>
struct add_const_if<true, T> { using type = const T; };
template<class T>
struct add_const_if<false, T> { using type = T; };

// Copy const-ness from T to U - const U if T was const; U otherwise
template<class T, class U>
struct copy_const { using type = typename add_const_if<std::is_const<T>::value, U>::type; };

// We can't rely on std::filesystem exsting in macos unfortunately, so
// we have to provide our own implementation of this
bool KAPPS_CORE_EXPORT removeFile(const std::string &path);
}}

namespace qs
{
std::string KAPPS_CORE_EXPORT joinVec(const std::vector<std::string> &vec, const std::string &del);

namespace detail
{
    void KAPPS_CORE_EXPORT formatImpl(std::stringstream &s, const char *formatString);

    template <typename T, typename... Targs>
    void formatImpl(std::stringstream &s, const char *formatString, T value, Targs &&...args)
    {
        for(; *formatString != '\0'; formatString++)
        {
            if(*formatString == '%')
            {
                s << value;
                formatImpl(s, formatString + 1, std::forward<Targs>(args)...);
                return;
            }
            s << *formatString;
        }
    }
}

template <typename...Targs>
std::string format(const char* formatString, Targs&&...args)
{
    std::stringstream s;
    detail::formatImpl(s, formatString, std::forward<Targs>(args)...);
    return s.str();
}

template <typename T>
T concat(T first, T second)
{
    using std::begin;
    using std::end;
    T result;
    result.reserve(first.size() + second.size());
    result.insert(end(result), begin(first), end(first));
    result.insert(end(result), begin(second), end(second));
    return result;
}

template <typename T, typename...Targs>
T concat(T first, Targs&&...rest)
{
    return concat(first, concat(std::forward<Targs>(rest)...));
}

template <typename T>
auto setDifference(const T &first, const T &second) -> std::vector<typename T::value_type>
{
    using std::begin;
    using std::end;

    std::vector<typename T::value_type> diff;
    diff.reserve(first.size());

    std::set_difference(begin(first), end(first), begin(second), end(second), std::back_inserter(diff));

    return diff;
}
}

// Annoyingly, in C++17, it is not possible to provide std::hash<>
// specializations automatically using a trait (e.g. "Hashable<T>", like
// "OStreamInsertable<T>"), because:
//
// 1. class template specializations must actually specialize the primary template
// 2. class template specializations can't have default arguments
//   --> because of 1 and 2, there is no way to define a std::hash for "any T,
//       where {trait}(T) is true", e.g. any T derived from Hashable<T> or with
//       a hash() method
//
// In C++20, this is possible using a concept as a constraint on the template
// parameter.
//
// As the least-bad alternative, provide a macro to implement std::hash :-/
// Note that this must be used at global scope, and since std::hash be defined
// before using the type in an unordered container, this may involve leaving and
// reentering the type's namespace.
//
// KAPPS_CORE_STD_HASH(BogusClass);
#define KAPPS_CORE_STD_HASH(T) \
namespace std   \
{   \
    template<>  \
    struct hash<T>  \
    {   \
        std::size_t operator()(const T &v) const { return v.hash(); }   \
    };  \
}
// Similarly, for a template, specify the template parameters too.  Since the
// preprocessor does not understand <> in argument lists, use KAPPS_CORE_COMMA
// for the commas :-/
//
// KAPPS_STD_HASH_TEMPLATE(MyPtr<ValueT KAPPS_CORE_COMMA AllocT>, class ValueT, class AllocT);
#define KAPPS_CORE_COMMA ,
#define KAPPS_CORE_STD_HASH_TEMPLATE(T, ...) \
namespace std   \
{   \
    template<__VA_ARGS__>   \
    struct hash<T>  \
    {   \
        std::size_t operator()(const T &v) const { return v.hash(); }   \
    };  \
}

// Retry a syscall until it no longer fails due to signal interruption
#define NO_EINTR(stmt) while((stmt) == -1 && errno == EINTR)

#endif
