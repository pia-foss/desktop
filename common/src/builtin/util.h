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
#line HEADER_FILE("builtin/util.h")

#ifndef BUILTIN_UTIL_H
#define BUILTIN_UTIL_H
#pragma once

#include <QtDebug>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QObject>
#include <QFile>
#include <QDeadlineTimer>
#include <QHostAddress>

#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>

#if defined(Q_COMPILER_TEMPLATE_AUTO)
#define TEMPLATE_AUTO_DECL(name)     auto name
#define TEMPLATE_AUTO(value)         value
#define TEMPLATE_AUTO_DECLTYPE(name) decltype(name)
#else
#define TEMPLATE_AUTO_DECL(name)     typename CONCAT(TYPE_,name), CONCAT(TYPE_,name) name
#define TEMPLATE_AUTO(value)         decltype(value), value
#define TEMPLATE_AUTO_DECLTYPE(name) CONCAT(TYPE_,name)
#endif

class Error;

class noncopyable;
class nonmovable;
template<typename Handle> class raii_dynamic_t;
template<typename Handle, Handle Null = 0> class raii_dynamic_nullable_t;
template<typename Handle, typename FreeFnType, FreeFnType FreeFn> class raii_static_t;
template<typename Handle, typename FreeFnType, FreeFnType FreeFn, Handle Null = 0> class raii_static_nullable_t;


// Helper base class to force subclass to be noncopyable.
//
class COMMON_EXPORT noncopyable
{
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
protected:
    noncopyable() {}
};

// Helper base class to force subclass to be nonmovable.
//
class COMMON_EXPORT nonmovable
{
    nonmovable(nonmovable&&) = delete;
    nonmovable& operator=(nonmovable&&) = delete;
protected:
    nonmovable() {}
};


template<typename T>
class nullable_t;

template<typename T>
struct get_nullable;

/*
template<typename T> static inline nullable_t<T> make_nullable(T&& value);
template<typename T> static inline T* make_nullable(T* ptr) { return ptr; }
template<typename T, typename D> static inline std::shared_ptr<T, D> make_nullable(std::shared_ptr<T, D> ptr) { return ptr; }
template<typename T, typename D> static inline std::unique_ptr<T, D> make_nullable(std::shared_ptr<T, D> ptr) { return ptr; }
*/

template<typename Handle, typename FreeFnType = int, FreeFnType... FreeFn>
class raii_t;


template<typename Class, typename Ptr, typename Return, typename... Args>
static inline auto bind_this(Return (Class::*fn)(Args...), Ptr instance)
{
    return [instance = std::move(instance), fn](Args&&... args) -> Return { return ((*instance).*fn)(std::forward<Args>(args)...); };
}
template<typename Class, typename Ptr, typename... Args>
static inline auto bind_this(void (Class::*fn)(Args...), Ptr instance)
{
    return [instance = std::move(instance), fn](Args&&... args) { ((*instance).*fn)(std::forward<Args>(args)...); };
}


namespace impl {

template<typename Handle>
class wrapped_handle
{
    Handle _handle;
    template<typename H, typename F, F... Fs> friend class raii_t;
    template<typename FreeFnType, FreeFnType... FreeFn> friend class raii_static_helper;
public:
    wrapped_handle(Handle handle) : _handle(std::move(handle)) {}
};


template<typename T =
         #ifdef Q_OS_WIN
         unsigned long
         #else
         int
         #endif
         >
class check_error_holder
{
    T _error;
public:
    check_error_holder(const std::nullptr_t&) {}
    void set(T error) { _error = std::move(error); }
    operator bool() const { return true; }
    T& error() { return _error; }
};


// Remove the Nth type in a tuple
template<size_t N, typename Tuple>
struct remove_nth_type;

template<typename T, typename... Ts>
struct remove_nth_type<0, std::tuple<T, Ts...>> { typedef std::tuple<Ts...> type; };

template<size_t N, typename T, typename... Ts>
struct remove_nth_type<N, std::tuple<T, Ts...>> { typedef decltype(std::tuple_cat(std::declval<std::tuple<T>>(), std::declval<typename remove_nth_type<N - 1, std::tuple<Ts...>>::type>())) type; };


// Split a tuple around the Nth type into left, middle and right
template<size_t N, typename Tuple>
struct split_tuple;

template<typename T, typename... Ts>
struct split_tuple<0, std::tuple<T, Ts...>> { typedef std::tuple<> left; typedef T middle; typedef std::tuple<Ts...> right; };

template<size_t N, typename T, typename... Ts>
struct split_tuple<N, std::tuple<T, Ts...>>
{
    typedef split_tuple<N - 1, std::tuple<Ts...>> inner;
    typedef decltype(std::tuple_cat(std::declval<std::tuple<T>>(), std::declval<typename inner::left>())) left;
    typedef typename inner::middle middle;
    typedef typename inner::right right;
};


// Implementation of callback wrapper
template<typename ContextArgType, typename Return, typename PrefixArgsTuple, typename InnerArgsTuple>
class callback_impl;

template<typename ContextArgType, typename Return, typename... LeftArgs, typename... RightArgs>
class callback_impl<ContextArgType, Return, std::tuple<LeftArgs...>, std::tuple<RightArgs...>> : public std::function<Return(LeftArgs..., RightArgs...)>
{
    typedef std::function<Return(LeftArgs..., RightArgs...)> base;

public:
    using base::base;
    using base::operator=;

    static_assert(sizeof(ContextArgType) == sizeof(void*) && (std::is_convertible<void*, ContextArgType>::value || std::is_convertible<ContextArgType, uintptr_t>::value), "unrecognized context argument type");

#   define IMPLEMENT_CALLING_CONVENTION(...) \
    typedef Return(__VA_ARGS__ *CONCAT(__VA_ARGS__,Signature))(LeftArgs... left, ContextArgType ctx, RightArgs... right); \
    static Return __VA_ARGS__ CONCAT(__VA_ARGS__,_thunk)(LeftArgs... left, ContextArgType ctx, RightArgs... right) { return fn(ctx)(left..., right...); } \
    operator CONCAT(__VA_ARGS__,Signature)() { return CONCAT(__VA_ARGS__,_thunk); }

    ITERATE_CALLING_CONVENTIONS(IMPLEMENT_CALLING_CONVENTION)

#   undef IMPLEMENT_CALLING_CONVENTION

    operator void*() { return reinterpret_cast<void*>(this); }
    operator intptr_t() { return reinterpret_cast<intptr_t>(this); }
    operator uintptr_t() { return reinterpret_cast<uintptr_t>(this); }

protected:
    static base& fn(ContextArgType ctx) { return *static_cast<base*>(reinterpret_cast<callback_impl*>(ctx)); }
};


// Helper template alias for base class below
template<size_t ContextArgIndex, typename Return, typename... Args>
using select_callback_impl = callback_impl<
        typename split_tuple<ContextArgIndex, std::tuple<Args...>>::middle,
        Return,
        typename split_tuple<ContextArgIndex, std::tuple<Args...>>::left,
        typename split_tuple<ContextArgIndex, std::tuple<Args...>>::right
>;

}


// Helper class to grant a non-pointer type an additional possible assignable
// value of nullptr, similar to std::optional.
//
template<typename T>
class nullable_t
{
    static_assert(!std::is_pointer<T>::value, "not instantiable for pointer types");
    Q_DECL_ALIGN(T) char _data[sizeof(T)];
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
    template<class... Args_t>
    T& emplace(Args_t &&... args) { clear(); _valid = true; new(_data) T{std::forward<Args_t>(args)...}; return get(); }
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

// Helper template to get an appropriate nullable type for a type T.
//
template<typename T> struct get_nullable { typedef nullable_t<T> type; };
template<typename T> struct get_nullable<T*> { typedef T* type; };
template<typename T> struct get_nullable<nullable_t<T>> { typedef nullable_t<T> type; };
template<typename T> using get_nullable_t = typename get_nullable<T>::type;

template<typename T> using Nullable = get_nullable_t<T>;
template<typename T> using Optional = get_nullable_t<T>;


template<typename T>
inline QDebug& operator<<(QDebug &d, const nullable_t<T> &nullable)
{
    if(nullable)
        d << nullable.get();
    else
        d << QStringLiteral("<null>");
    return d;
}

// Convenience macro to schedule a block of code (actually a lambda) to run
// at the end of the current scope. If multiple sentinels are listed in the
// same scope, they will run in reverse order.
//
#define RAII_SENTINEL(...) auto CONCAT(_raii_sentinel_,__LINE__) = raii_sentinel([&](){ __VA_ARGS__; })
#define FINALLY RAII_SENTINEL
#define CLEANUP RAII_SENTINEL
#define AT_SCOPE_EXIT RAII_SENTINEL

// Silence pedantic clang warnings related to extra semicolons after a block
QT_WARNING_DISABLE_CLANG("-Wextra-semi")


// Lightweight RAII handle class, type-bound to a free function at compile
// time for the smallest possible overhead.
//
template<typename Handle, typename FreeFnType, FreeFnType FreeFn>
class raii_t<Handle, FreeFnType, FreeFn>
{
    typedef get_nullable_t<Handle> NullableHandle;
    NullableHandle _handle;
    template<typename H, typename F, F...> friend class raii_t;
public:
    raii_t() {}
    explicit raii_t(Handle handle) : _handle(std::move(handle)) {}
    raii_t(const raii_t& copy) = delete;
    raii_t& operator=(const raii_t& copy) = delete;
    raii_t(raii_t&& move) : _handle(std::exchange(move._handle, nullptr)) {}
    raii_t& operator=(raii_t& move) { if (_handle != nullptr) FreeFn(std::exchange(_handle, nullptr)); _handle = std::exchange(move._handle, nullptr); return *this; }
    ~raii_t() { if (_handle != nullptr) FreeFn(std::exchange(_handle, nullptr)); }

    // Typed variables can be assigned to from a wrapped handle that
    // has no defined free function of its own.
    raii_t(::impl::wrapped_handle<Handle>&& move) : _handle(std::move(move._handle)) {}
    raii_t& operator=(::impl::wrapped_handle<Handle>&& move) { if (_handle != nullptr) FreeFn(std::exchange(_handle, nullptr)); _handle = std::move(move._handle); return *this; }

    operator Handle() const { return _handle; }
    template<typename PointerHandle = Handle> std::enable_if_t<std::is_pointer<PointerHandle>::value, PointerHandle> operator->() { return _handle; }
    bool valid() const { return _handle != nullptr; }
    Handle detach() { return std::exchange(_handle, nullptr); }
};

// Lightweight RAII handle class, where the free function is of a known
// type but is assigned at runtime. Mainly useful for lambdas or functors
// which cannot be part of a type signature.
//
template<typename Handle, typename FreeFnType>
class raii_t<Handle, FreeFnType>
{
    typedef get_nullable_t<Handle> NullableHandle;
    NullableHandle _handle;
    FreeFnType _free;
    template<typename H, typename F, F...> friend class raii_t;
public:
    raii_t() {}
    explicit raii_t(Handle handle, FreeFnType free) : _handle(std::move(handle)), _free(std::move(free)) {}
    raii_t(const raii_t& copy) = delete;
    raii_t& operator=(const raii_t& copy) = delete;
    raii_t(raii_t&& move) : _handle(std::exchange(move._handle, nullptr)), _free(std::move(move._free)) {}
    raii_t& operator=(raii_t&& move) { if (_handle != nullptr) _free(std::exchange(_handle, nullptr)); _handle = std::exchange(move._handle, nullptr); _free = std::move(move._free); return *this; }
    ~raii_t() { if (_handle != nullptr) _free(std::exchange(_handle, nullptr)); }

    operator Handle() const { return _handle; }
    template<typename PointerHandle = Handle> std::enable_if_t<std::is_pointer<PointerHandle>::value, PointerHandle> operator->() { return _handle; }
    bool valid() const { return _handle != nullptr; }
    Handle detach() { return std::exchange(_handle, nullptr); }
};

// Lightweight RAII handle class, where the free function is of a known
// type but is assigned at runtime. Mainly useful for lambdas or functors
// which cannot be part of a type signature. Specialization for function
// pointer based free functions.
//
template<typename Handle, typename FreeFnType>
class raii_t<Handle, FreeFnType*>
{
    Handle _handle;
    FreeFnType* _free;
    template<typename H, typename F, F...> friend class raii_t;
public:
    raii_t() {}
    explicit raii_t(Handle handle, FreeFnType* free) : _handle(std::move(handle)), _free(free) {}
    raii_t(const raii_t& copy) = delete;
    raii_t& operator=(const raii_t& copy) = delete;
    raii_t(raii_t&& move) : _handle(std::move(move._handle)), _free(std::exchange(move._free, nullptr)) {}
    raii_t& operator=(raii_t&& move) { if (_free) std::exchange(_free, nullptr)(_handle); _handle = std::move(move._handle); _free = std::exchange(move._free, nullptr); return *this; }
    ~raii_t() { if (_free) std::exchange(_free, nullptr)(_handle); }

    operator Handle() const { return _handle; }
    template<typename PointerHandle = Handle> std::enable_if_t<std::is_pointer<PointerHandle>::value, PointerHandle> operator->() { return _handle; }
    bool valid() const { return _free != nullptr; }
    Handle detach() { _free = nullptr; return _handle; }
};


// Lightweight RAII handle class, where the free function is of a known
// type but is assigned at runtime and stored in a generic std::function
// object. This is the most general version of the class, and is assignable
// from any of the other specializations.
//
template<typename Handle, int... dummy>
class raii_t<Handle, int, dummy...>
{
    Handle _handle;
    std::function<void(Handle&)> _free;
    template<typename H, typename F, F...> friend class raii_t;
public:
    raii_t() {}
    template<typename FreeFnType>
    explicit raii_t(Handle handle, FreeFnType&& free) : _handle(std::move(handle)), _free(std::forward<FreeFnType>(free)) {}
    raii_t(const raii_t& copy) = delete;
    raii_t& operator=(const raii_t& copy) = delete;
    raii_t(raii_t&& move) : _handle(std::move(move._handle)), _free(std::exchange(move._free, nullptr)) {}
    raii_t& operator=(raii_t&& move) { if (_free) std::exchange(_free, nullptr)(_handle); _handle = std::move(move._handle); _free = std::exchange(move._free, nullptr); return *this; }
    ~raii_t() { if (_free) std::exchange(_free, nullptr)(_handle); }

    // Dynamic RAII wrappers can also be constructed or assigned from static
    // RAII wrappers, by wrapping the free function in a std::function and
    // checking the handle value for nullptr.
    template<typename FreeFnType, FreeFnType FreeFn>
    raii_t(raii_t<Handle, FreeFnType, FreeFn>&& move) { if (move._handle != nullptr) { _handle = std::exchange(move._handle, nullptr); _free = FreeFn; } }
    template<typename FreeFnType, FreeFnType FreeFn>
    raii_t& operator=(raii_t<Handle, FreeFnType, FreeFn>&& move) { if (_free) std::exchange(_free, nullptr)(_handle); if (move._handle != nullptr) { _handle = std::exchange(move._handle, nullptr); _free = FreeFn; } return *this; }
    template<typename FreeFnType>
    raii_t(raii_t<Handle, FreeFnType>&& move) { if (move._handle != nullptr) { _handle = std::exchange(move._handle, nullptr); _free = std::move(move._free); } }
    template<typename FreeFnType>
    raii_t& operator=(raii_t<Handle, FreeFnType>&& move) { if (_free) std::exchange(_free, nullptr)(_handle); if (move._handle != nullptr) { _handle = std::exchange(move._handle, nullptr); _free = std::move(move._free); } return *this; }
    template<typename FreeFnType>
    raii_t(raii_t<Handle, FreeFnType*>&& move) { if (move._free != nullptr) { _handle = std::move(move._handle); _free = std::exchange(move._free, nullptr); } }
    template<typename FreeFnType>
    raii_t& operator=(raii_t<Handle, FreeFnType*>&& move) { if (_free) std::exchange(_free, nullptr)(_handle); if (move._free != nullptr) { _handle = std::move(move._handle); _free = std::exchange(move._free, nullptr); } }

    operator Handle() const { return _handle; }
    template<typename PointerHandle = Handle> std::enable_if_t<std::is_pointer<PointerHandle>::value, PointerHandle> operator->() { return _handle; }
    bool valid() const { return _free != nullptr; }
    Handle detach() { _free = nullptr; return _handle; }
};


// Macro that expands to a RAII wrapper class type with an optional statically
// bound free function. If the free function is omitted, this becomes a dynamic
// RAII wrapper that instead gets the free function as a constructor argument.
//
#define RAII(type, ...) raii_t<type, decltype(::impl::raii_free_helper(__VA_ARGS__)),##__VA_ARGS__>

// Macro to wrap a handle into an RAII class with the free function
// statically typed in. This works for named functions but not for lambdas
// or functors. The free function can be omitted, but the resulting handle
// must then be assigned to a statically typed RAII(type, free) variable.
//
#define RAII_WRAP(handle, ...) ::impl::raii_static_helper<decltype(::impl::raii_free_helper(__VA_ARGS__)),##__VA_ARGS__>::make(handle)

// Function to wrap a handle into an RAII class with the free function
// dynamically bound to a typed instance. Use this when RAII_WRAP() fails to
// compile, such as when you need functors or lambdas as a free function.
//
template<typename Handle, typename FreeFnType>
static inline auto raii_wrap(Handle&& handle, FreeFnType&& free) { return raii_t<Handle, FreeFnType>(std::forward<Handle>(handle), std::forward<FreeFnType>(free)); }



namespace impl {

template<typename FreeFnType, FreeFnType... FreeFn>
struct raii_static_helper;
template<typename FreeFnType, FreeFnType FreeFn>
struct raii_static_helper<FreeFnType, FreeFn>
{
    template<typename Handle>
    static auto make(Handle&& handle) { return raii_t<Handle, FreeFnType, FreeFn>(std::forward<Handle>(handle)); }
};
template<int... dummy>
struct raii_static_helper<int, dummy...>
{
    template<typename Handle>
    static auto make(Handle&& handle) { return wrapped_handle<Handle>(std::forward<Handle>(handle)); }
};

// Helper function to fall back to a dynamic RAII class when no free function
// is specified in the type.
//template<typename T> static inline constexpr std::decay_t<T> raii_free_helper(T&& value) { return std::forward<T>(value); }
template<typename T> static inline constexpr T* raii_free_helper(T* value) { return value; }
static inline constexpr int raii_free_helper() { return 0; }

}


// Helper type for implementing RAII_SENTINEL.
//
template<typename FnType>
class raii_sentinel_t : noncopyable
{
    FnType _fn;
public:
    raii_sentinel_t(FnType&& fn) : _fn(fn) {}
    raii_sentinel_t(raii_sentinel_t&& move) : _fn(std::move(move._fn)) {}
    ~raii_sentinel_t() { _fn(); }
    raii_sentinel_t& operator=(raii_sentinel_t&& move) { _fn = std::move(move._fn); return *this; }
};

// Helper function for instantiating raii_sentinel_t.
//
template<typename FnType>
static Q_ALWAYS_INLINE raii_sentinel_t<FnType> raii_sentinel(FnType&& fn) { return std::forward<FnType>(fn); }



// A lightweight framework to handle errors in an exception-like
// fashion, but local to the current function (e.g. when calling
// a lot of platform APIs and wanting to bail out of a sequence
// early when an error is encountered). C++ scopes are respected
// allowing RAII style resource handling.
//
// Usage:
//   LOCAL_TRY(int)
//   {
//       if (!callFunction())
//           LOCAL_THROW(errno);
//       return true;
//   }
//   LOCAL_CATCH(error)
//   {
//       qWarning() << "Got error" << error;
//       return false;
//   }
//
// Notes:
// - You can have at most one LOCAL_TRY block in a function.
//
#define LOCAL_TRY(...) if (::impl::check_error_holder<__VA_ARGS__> _local_error_holder = nullptr)
#define LOCAL_THROW(error) do { _local_error_holder.set(error); goto _local_error_exit; } while(0)
#define LOCAL_CATCH(var) else _local_error_exit: if (bool _local_error_guard = false) {} else for (auto& var = _local_error_holder.error(); !_local_error_guard; _local_error_guard = true) if (const bool _local_error_holder = false) {} else



// Evaluates to the class type of the current 'this' pointer
#define THIS_CLASS std::decay_t<decltype(*this)>
// Creates a closure of a member function, taking the same arguments
#define THIS_FUNCTION(name) ::bind_this(&THIS_CLASS::name, this)


// Wrapper for passing an arbitrary std::function to a platform
// function that takes a callback function pointer plus context
// pointer. The position of the context pointer in the callback
// parameter list is passed as the second template parameter,
// and is spliced out of the parameter list passed to the inner
// callable.
//
// Usage:
//   extern void callCallback(void(*fn)(void*), void* ctx);
//   callback<void(void*), 0> cb = [&]() { /* ... */ };
//   callCallback(cb, cb);
//
template<typename Signature, size_t ContextArgIndex = 0>
class callback_signature_t;

#define IMPLEMENT_CALLBACK(...) \
    template<size_t ContextArgIndex, typename Return, typename... Args> \
    class callback_signature_t<Return __VA_ARGS__ (Args...), ContextArgIndex> : public ::impl::select_callback_impl<ContextArgIndex, Return, Args...> \
    { \
        typedef ::impl::select_callback_impl<ContextArgIndex, Return, Args...> base; \
    public: \
        using base::base; \
        using base::operator=; \
    };

ITERATE_CALLING_CONVENTIONS(IMPLEMENT_CALLBACK)

#undef IMPLEMENT_CALLBACK


template<typename Signature, size_t ContextArgIndex = 0>
using callback_t = callback_signature_t<std::remove_pointer_t<Signature>, ContextArgIndex>;

// Base class to implement the singleton pattern; used for classes for which
// there should only be a single instance. The class must still be manually
// instantiated (on the heap or on the stack), upon which its instance will
// be tracked by the class. To instantiate the singleton on the heap, it is
// sufficient to simply call `new Derived(...)` - the result does not need
// to be assigned anywhere.
//
// Singleton is _not_ thread-safe.  A thread-safe singleton class should define
// its own static mutex and _instance pointer.  (Note that locking a mutex in
// the Singleton constructor and in Singleton::instance() would not be
// sufficient; there would be no guarantee that an object returned by
// Singleton::instance() was still valid at the point of use.)
//
// NOTE: This template contains a static member in a dynamic library.  If a
// specialization of this template might be used by both code in common and in
// the linking executable, Singleton::_instance _must_ be explicitly
// instantiated and exported (to ensure that both common and the executable link
// to the same _instance member).
//
// For example, consider a Service type derived from Singleton<Service>.  Both
// Service (in common) and the executable can call Service::instance() (actually
// Singleton<Service>::instance()).  Singleton<Service>:_instance must be
// exported:
//    service.h:
//       extern template class COMMON_EXPORT_TMPL_SPEC_DECL Singleton<Service>;
//    service.cpp:
//       template class COMMON_EXPORT Singleton<Service>;
//
// If instantiating the template in a module other than common, use the
// appropriate annotations for that module.   COMMON_EXPORT_TMPL_SPEC_DECL is
// used to work around strange behavior in MSVC specifically for this type of
// exported template specialization.
template<class Derived>
class Singleton
{
public:
    Singleton() { Q_ASSERT(_instance == nullptr); _instance = this; }
    ~Singleton() { _instance = nullptr; }
    static Derived* instance() { return static_cast<Derived*>(_instance); }
private:
    static Singleton* _instance;
};

template<class Derived>
Singleton<Derived>* Singleton<Derived>::_instance = nullptr;

// Base class to implement the singleton pattern by having the class
// instantiate itself on the heap automatically. To explicitly delete
// the singleton instance, use `delete Derived::instance()`.
//
template<class Derived>
class AutoSingleton : public Singleton<Derived>
{
public:
    static Derived* instance() { if (auto i = Singleton<Derived>::instance()) return i; else return new Derived(); }
};


void throwIfEnumCastFailed(bool result, const char* name);

// Get the name of a value in a registered Q_ENUM enum
template<typename Enum>
static inline QLatin1String qEnumToString(Enum value)
{
    static QMetaEnum meta = QMetaEnum::fromType<Enum>();
    return QLatin1String(meta.valueToKey(static_cast<int>(value)));
}

// Get the names of a set of values in a registered Q_FLAG enum
template<typename Flags>
static inline QByteArray qFlagsToString(Flags flags)
{
    static QMetaEnum meta = QMetaEnum::fromType<Flags>();
    return meta.valueToKeys(static_cast<int>(flags));
}

// Parse the value of a name in a registered Q_ENUM enum
template<typename Enum>
static inline bool qStringToEnum(const char* name, Enum& value)
{
    static QMetaEnum meta = QMetaEnum::fromType<Enum>();
    bool ok;
    value = meta.keyToValue(name, &ok);
    return ok;
}
template<typename Enum>
static inline Enum qStringToEnum(const char* name) throws(Error)
{
    Enum value;
    throwIfEnumCastFailed(qStringToEnum(name, value), name);
    return value;
}

// Parse the value of a set of names in a registered Q_FLAG enum
template<typename Flags>
static inline bool qStringToFlags(const char* names, Flags& values)
{
    static QMetaEnum meta = QMetaEnum::fromType<Flags>();
    bool ok;
    values = meta.keysToValue(names, &ok);
    return ok;
}
template<typename Flags>
static inline Flags qStringToFlags(const char* names) throws(Error)
{
    Flags values;
    throwIfEnumCastFailed(qStringToFlags(names, values), names);
    return values;
}

// CRTP QDebug tracer - implement trace() to provide operator<<
template<class Derived>
class DebugTraceable
{
public:
    friend QDebug &operator<<(QDebug &dbg, const DebugTraceable &traceable)
    {
        static_cast<const Derived&>(traceable).trace(dbg);
        return dbg;
    }
};

// Trace an enum value - traces the name if the value is known, falls back to
// the numeric value otherwise.
template<class Enum>
class EnumTracer : public DebugTraceable<EnumTracer<Enum>>
{
private:
    static const QMetaEnum &meta()
    {
        static const QMetaEnum _meta = QMetaEnum::fromType<Enum>();
        return _meta;
    }

public:
    EnumTracer(Enum value) : _value{value} {}

    void trace(QDebug &dbg) const
    {
        const char *name = meta().valueToKey(static_cast<int>(_value));
        if(name)
            dbg << name;
        else
            dbg << static_cast<typename std::underlying_type<Enum>::type>(_value);
    }

private:
    Enum _value;
};

// Deduce the enum type by tracing an enum with this function
template<class Enum>
EnumTracer<Enum> traceEnum(Enum value) {return {value};}

namespace impl {
    // This cheekily borrows straight from Qt internals, but it was the only
    // way to provide this functionality in a SFINAE friendly manner.
    template<typename T>
    static inline std::enable_if_t<QMetaTypeId2<T>::Defined, const char*> qTypeName(T*) { return QMetaType::typeName(qMetaTypeId<T>()); }
    static inline const char* qTypeName(...) { return nullptr; }
}

// Helper to read out the type name of a type, if known to Qt, or nullptr otherwise.
template<typename T>
static inline const char* qTypeName() { return impl::qTypeName(static_cast<T*>(nullptr)); }

// Millsecond count from a duration as a qint64.  (Can be used with any
// duration units thanks to implicit conversions.)
//
// This is the preferred way to use durations with QTimer, since Qt doesn't
// provide the duration overloads of QTimer methods on all platforms we support.
//
// msec() returns the full count as a qint64 (std::chrono::milliseconds has at
// least 45 bits of precision).  msec32() truncates to 32-bit for use with
// QTimer.
//
// Consider:
//     std::chrono::milliseconds shortInterval(500);
//     std::chrono::minutes longInterval(2);
//
// Error-prone:
//     _timer.setInterval(shortInterval.count());  // OK
//     _timer.setInterval(longInterval.count());  // WRONG!  Sets to 2 ms
//
// Better:
//     _timer.setInterval(msec32(shortInterval));  // OK
//     _timer.setInterval(msec32(longInterval));  // OK
inline qint64 msec(const std::chrono::milliseconds &time)
{
    return static_cast<qint64>(time.count());
}
inline qint32 msec32(const std::chrono::milliseconds &time)
{
    qint64 count = msec(time);
    Q_ASSERT(count >= std::numeric_limits<qint32>::min());
    Q_ASSERT(count <= std::numeric_limits<qint32>::max());
    return static_cast<qint32>(count);
}

// Render a millisecond count for tracing (not localized, tracing only)
COMMON_EXPORT QString traceMsec(qint64 time);

inline QString traceMsec(const std::chrono::milliseconds &time)
{
    return traceMsec(msec(time));
}


// Simple collection type for a linked list where the list overhead is
// embedded into the held type itself, meaning zero heap overhead and
// O(1) time complexity for both insert and remove by reference.

template<class T> class Node;
template<class T> class NodeList;

template<class T>
class Node
{
    friend class NodeList<T>;
    Node<T>* _prev = nullptr;
    Node<T>* _next = nullptr;
    NodeList<T>* _list = nullptr;
public:
    T* prev() { return static_cast<T*>(_prev); }
    T* next() { return static_cast<T*>(_next); }
    NodeList<T>* list() { return _list; }
public:
    Node() {}
    Node(const Node&) = delete;
    Node(Node&&) = delete;
    ~Node() { remove(); }
    // Move the node to before another node.
    inline void insertBefore(Node<T>* node);
    // Move the node to after another node.
    inline void insertAfter(Node<T>* node);
    // Move the node to first in a list.
    inline void insertFirst(NodeList<T>* list);
    // Move the node to last in a list.
    inline void insertLast(NodeList<T>* list);
    // Remove the node from any list.
    inline void remove();
};

template<class T>
class NodeList
{
    friend class Node<T>;
    Node<T>* _first = nullptr;
    Node<T>* _last = nullptr;
    int _count = 0;
public:
    T* first() { return static_cast<T*>(_first); }
    T* last() { return static_cast<T*>(_last); }
    int count() { return _count; }
public:
    NodeList() {}
    NodeList(const NodeList&) = delete;
    NodeList(NodeList&&) = delete;
    ~NodeList() { clear(); }
    // Unlink all nodes in the list.
    inline void clear();
    // Check if a given node is inside this list.
    inline bool contains(const Node<T>* node) const;
};

template<class T>
inline void Node<T>::insertBefore(Node<T>* node)
{
    remove();
    if ((_prev = std::exchange(node->_prev, this)))
        _prev->_next = this;
    else if (node->_list)
        node->_list->_first = this;
    _next = node;
    if ((_list = node->_list))
        _list->_count++;
}
template<class T>
inline void Node<T>::insertAfter(Node<T>* node)
{
    remove();
    if ((_next = std::exchange(node->_next, this)))
        _next->_prev = this;
    else if (node->_list)
        node->_list->_last = this;
    _prev = node;
    if ((_list = node->_list))
        _list->_count++;
}
template<class T>
inline void Node<T>::insertFirst(NodeList<T>* list)
{
    remove();
    if ((_next = std::exchange(list->_first, this)))
        _next->_prev = this;
    else
        list->_last = this;
    _list = list;
    _list->_count++;
}
template<class T>
inline void Node<T>::insertLast(NodeList<T>* list)
{
    remove();
    if ((_prev = std::exchange(list->_last, this)))
        _prev->_next = this;
    else
        list->_first = this;
    _list = list;
    _list->_count++;
}
template<class T>
inline void Node<T>::remove()
{
    if (_prev)
        _prev->_next = _next;
    else if (_list)
        _list->_first = _next;
    if (_next)
        _next->_prev = _prev;
    else if (_list)
        _list->_last = _prev;
    if (_list)
        --_list->_count;
    _prev = nullptr;
    _next = nullptr;
    _list = nullptr;
}

template<class T>
inline void NodeList<T>::clear()
{
    while (_first)
    {
        _first->remove();
    }
}
template<class T>
inline bool NodeList<T>::contains(const Node<T>* node) const
{
    return node->_list == this;
}


// After starting a process (and writing any necessary input), call this
// convenience function to correctly wait for it to terminate with the same
// logic as QProcess::execute (with crashes and other error conditions
// normalized as negative exit codes).
//
COMMON_EXPORT int waitForExitCode(class QProcess& process);


// Mixin to grant ability to queue asynchronous notifications to oneself,
// with multiple requests to the same notification being coalesced into
// single invocations, and with the ability to cancel outstanding requests.
//
#define IMPLEMENT_NOTIFICATIONS(type) \
    typedef void (type::*NotificationFunction)(); \
    bool queueNotification(NotificationFunction fn) { \
        if (_notifications.contains(fn)) return false; \
        _notifications.append(fn); \
        if (!_notificationsPosted) { \
            _notificationsPosted = true; \
            QMetaObject::invokeMethod(this, &type::processNotifications, Qt::QueuedConnection); \
        } \
        return true; \
    } \
    bool isNotificationQueued(NotificationFunction fn) const { \
        return _notifications.contains(fn); \
    } \
    bool cancelNotification(NotificationFunction fn) { \
        return _notifications.removeOne(fn); \
    } \
    void cancelAllNotifications() { \
        _notifications.clear(); \
    } \
    private: Q_SLOT void processNotifications() { \
        _notificationsPosted = false; \
        QList<NotificationFunction> notifications; \
        _notifications.swap(notifications); \
        for (const auto& fn : notifications) { \
            (this->*fn)(); \
        } \
    } \
    private: QList<NotificationFunction> _notifications; \
    private: bool _notificationsPosted = false;

// Register a metatype in order to use it in queued connections, QVariant,
// QSignalSpy, etc.
// Q_DECLARE_METATYPE() must be used with this.
template<class T>
class RegisterMetaType
{
public:
    RegisterMetaType()
    {
        qRegisterMetaType<T>();
    }
    // Or for a typedef:
    RegisterMetaType(const char *name)
    {
        qRegisterMetaType<T>(name);
    }
};

// Provides comparison operators based on a three-way comparison function
// compare(), which returns <0 if *this<other, >0 if *this>other, or 0 if
// *this==other.
template<class T>
class Comparable
{
private:
    int typeCompare(const Comparable &other) const
    {
        return static_cast<const T&>(*this).compare(static_cast<const T&>(other));
    }
public:
    bool operator<(const Comparable &other) const {return typeCompare(other)<0;}
    bool operator<=(const Comparable &other) const {return typeCompare(other)<=0;}
    bool operator>(const Comparable &other) const {return typeCompare(other)>0;}
    bool operator>=(const Comparable &other) const {return typeCompare(other)>=0;}
    bool operator==(const Comparable &other) const {return typeCompare(other)==0;}
    bool operator!=(const Comparable &other) const {return typeCompare(other)!=0;}
};

// Class that can be used to define a "static" signal in a class.  Qt doesn't
// have any concept of a static signal; it has to be in an object.
//
// Instead, a static object of this type can be created and used as a plain
// signal.  Usually, the object should be named for the signal, such as:
//   - static StaticSignal myStaticVarChange;
//   ...meanwhile...
//   - QObject::connect(&myStaticVarChange, &StaticSignal::signal, this, ...);
//   ...elsewhere...
//   - emit myStaticVarChange.signal()
class COMMON_EXPORT StaticSignal : public QObject
{
    Q_OBJECT
signals:
    void signal();
};

// Returns true if a debugger is attached at process start
COMMON_EXPORT bool isDebuggerPresent();

// Start the support tool in a given "mode"
// currently the only supported values for mode is "logs" and "crash".
// Pass the path to the diagnostics file if one was written.
COMMON_EXPORT void startSupportTool(const QString &mode, const QString &diagFile);

// Set the default QTextCodec::codecForLocale() to UTF-8.
//
// Without this, QTextStream (and other uses of codecForLocale()) would by
// default use a codec that depends on the OS locale.  (For example, a Russian-
// language version of Windows uses Windows-1251 (Cyrillic) by default; English
// uses Windows-1251 by default.)
//
// This is virtually never what we want.  Files like log files might even be
// corrupted by this behavior if the OS locale changes, since we could write
// differently encoded text in the same file.  This is also error-prone and
// difficult to test.
//
// On Windows, this would depend on the "language for non-Unicode programs"
// setting buried in the region and language settings, which sets the system
// ANSI code page.  (It does _not_ depend on the system's display language,
// though it appears to be defaulted differently on installation for different
// languages.)  On Linux, the encoding can be part of the locale, but it's UTF-8
// on virtually all modern distributions.  On Mac, it's always UTF-8, this is
// hard-coded in Qt.
//
// Instead, call this in main() to set the default to UTF-8 universally.  Note
// that this is not thread-safe, it must be done before any threads or any text
// streams (including the Logger) have been created.
COMMON_EXPORT void setUtf8LocaleCodec();

#ifdef PIA_CRASH_REPORTING
COMMON_EXPORT void initCrashReporting ();
// Monitor for dumps from the daemon to automatically start the support tool
COMMON_EXPORT void monitorDaemonDumps();

#ifdef Q_OS_LINUX
COMMON_EXPORT void stopCrashReporting();
#endif
#endif

qint64 COMMON_EXPORT getMonotonicTime();

// Intentionally crash - for testing crash reporting.  (Exported from common
// so it's possible to test debug symbols for common in a dynamic library.)
void COMMON_EXPORT testCrash();

namespace std {
    // QString doesn't provide a std::hash<> specialization.  It provides
    // qHash(), but QHash/QMultiHash are poor container choices:
    // - they require the types to be copy-assignable, this breaks value type
    //   invariants
    // - QHash doesn't provide an invariant that it doesn't have duplicate
    //   key entries, unlike std::unordered_map.
    // - the iterator semantics are odd, they're iterator-to-value and the
    //   iterator itself has a key() method
    template<>
    struct hash<QString>
    {
        std::size_t operator()(const QString &str) const {return qHash(str);}
    };
    // Also, for QLatin1String
    template<>
    struct hash<QLatin1String>
    {
        std::size_t operator()(const QLatin1String &str) const {return qHash(str);}
    };
    template<>
    struct hash<QHostAddress>
    {
        std::size_t operator()(const QHostAddress &addr) const {return qHash(addr);}
    };
}

// Returns a rate-limited wrapper for a given function
// auto debouncedFoo = debounce(foo, 1000);
// debouncedFoo() can then be invoked at most once every 1000 ms
template <typename Func_t>
auto debounce(Func_t func, qint64 timeout)
{
    QDeadlineTimer timer;
    return [=](auto&&...args) mutable {
        // Since hasExpired() is true for a default-constructed object
        // func() will be executed the first time the debounced function is called
        // Successive calls will be rate-limited
        if(timer.hasExpired())
        {
             timer.setRemainingTime(timeout);
             func(std::forward<decltype(args)>(args)...);
        }
    };
}

#endif // BUILTIN_UTIL_H
