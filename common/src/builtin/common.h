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

#ifndef BUILTIN_COMMON_H
#define BUILTIN_COMMON_H

#include <QtGlobal>

#ifdef QT_DEBUG
#ifndef _DEBUG
#define _DEBUG 1
#endif
#ifndef DEBUG
#define DEBUG 1
#endif
#ifdef _RELEASE
#undef _RELEASE
#endif
#ifdef RELEASE
#undef RELEASE
#endif
#endif
#ifdef QT_NO_DEBUG
#ifdef _DEBUG
#undef _DEBUG
#endif
#ifdef DEBUG
#undef DEBUG
#endif
#ifndef _RELEASE
#define _RELEASE 1
#endif
#ifndef RELEASE
#define RELEASE 1
#endif
#endif

// Convenience macros for including an inline code snippet only in debug or release mode
#ifdef _DEBUG
#define _D(...) __VA_ARGS__
#else
#define _D(...)
#endif
#ifdef _RELEASE
#define _R(...) __VA_ARGS__
#else
#define _R(...)
#endif


// Compiler specific
#if defined(Q_PROCESSOR_X86_64)
# define ATTR_stdcall
# define ATTR_cdecl
#elif defined(Q_CC_GNU) || defined(Q_CC_CLANG)
# define ATTR_stdcall __attribute__((stdcall))
# define ATTR_cdecl __attribute__((cdecl))
#else
# define ATTR_stdcall __stdcall
# define ATTR_cdecl __cdecl
#endif
#if defined(Q_CC_MSVC) && !defined(Q_PROCESSOR_X86_64)
# define ITERATE_CALLING_CONVENTIONS(X) X(ATTR_cdecl) X(ATTR_stdcall)
#else
# define ITERATE_CALLING_CONVENTIONS(X) X()
#endif
#ifndef __has_cpp_attribute
# define __has_cpp_attribute(name) 0
#endif
// Qt's definition of Q_REQUIRED_RESULT doesn't match the compilers we use, so redefine it
#if defined(Q_CC_MSVC) && _MSVC_LANG >= 201703
# undef Q_REQUIRED_RESULT
# define Q_REQUIRED_RESULT [[nodiscard]]
#elif defined(Q_CC_CLANG) || defined(Q_CC_GNU)
# undef Q_REQUIRED_RESULT
# define Q_REQUIRED_RESULT __attribute__((__warn_unused_result__))
#endif

#ifdef STATIC_COMMON
    #define COMMON_EXPORT   // Statically linking, no annotation
#else
    #ifdef _WIN32
        #ifdef BUILD_COMMON
            #define COMMON_EXPORT __declspec(dllexport)
        #else
            #define COMMON_EXPORT __declspec(dllimport)
        #endif
    #else
        #define COMMON_EXPORT __attribute__((visibility("default")))
    #endif
#endif

// In rare cases, a template specialization may need to be both manually
// instantiated and exported - this occurs for templates like Singleton that:
// - use CRTP
// - have static data members
// - may reference those static data members from more than one module.
//
// In Clang/GCC this is the similar to exporting any other symbol - you would
// declare the template specialization with
// "extern template class COMMON_EXPORT ...;" and then instantiate it with
// "template class COMMON_EXPORT ...;".
//
// However, MSVC doesn't allow this, it complains about extern being combined
// with dllexport (even though the meaning seems clear, the template _will_ be
// instantiated with dllexport, but don't instantiate it here).  We can't
// explicitly instantiate the template with dllexport / dllimport because it
// uses CRTP.  MSVC implicitly applies dllexport when deriving a dllexport type
// from a template instantiation, so we just have to remove dllexport from the
// extern declaration.
#if !defined(STATIC_COMMON) && defined(_WIN32) && defined(BUILD_COMMON)
    // No annotation on the declaration of the specialiation - would conflict with extern
    #define COMMON_EXPORT_TMPL_SPEC_DECL
#else
    // Static build, not MSVC, or importing common - same as COMMON_EXPORT
    #define COMMON_EXPORT_TMPL_SPEC_DECL COMMON_EXPORT
#endif

// Annotation-only version of the throw(...) declaration specifier;
// documents without runtime impact which exceptions a function may throw.
//
#define throws(...) noexcept(false)

// HEADER_FILE() and SOURCE_FILE() macros were historically used to trim down
// file paths manually in release builds (to reduce logging verbosity).  These
// are obsolete, paths are now trimmed by the logger, any instances of these can
// be deleted on sight.
#define HEADER_FILE(name) NEXT_LINE(__LINE__) __FILE__
#define SOURCE_FILE(name) NEXT_LINE(__LINE__) __FILE__

#define NEXT_LINE_1 2
#define NEXT_LINE_2 3
#define NEXT_LINE_3 4
#define NEXT_LINE_4 5
#define NEXT_LINE_5 6
#define NEXT_LINE_6 7
#define NEXT_LINE_7 8
#define NEXT_LINE_8 9
#define NEXT_LINE_9 10
#define NEXT_LINE_10 11
#define NEXT_LINE_11 12
#define NEXT_LINE_12 13
#define NEXT_LINE_13 14
#define NEXT_LINE_14 15
#define NEXT_LINE_15 16
#define NEXT_LINE_16 17
#define NEXT_LINE_17 18
#define NEXT_LINE_18 19
#define NEXT_LINE_19 20
#define NEXT_LINE_20 21
#define NEXT_LINE_21 22
#define NEXT_LINE_22 23
#define NEXT_LINE_23 24
#define NEXT_LINE_24 25
#define NEXT_LINE_25 26
#define NEXT_LINE_26 27
#define NEXT_LINE_27 28
#define NEXT_LINE_28 29
#define NEXT_LINE_29 30
#define NEXT_LINE_30 31
#define NEXT_LINE_31 32
#define NEXT_LINE_32 33
#define NEXT_LINE_33 34
#define NEXT_LINE_34 35
#define NEXT_LINE_35 36
#define NEXT_LINE_36 37
#define NEXT_LINE_37 38
#define NEXT_LINE_38 39
#define NEXT_LINE_39 40
#define NEXT_LINE_40 41
#define NEXT_LINE(line) CONCAT(NEXT_LINE_,__LINE__)
#define CONCAT(a,b) CONCAT_(a,b)
#define CONCAT_(a,b) a##b


#undef Q_D
#define Q_D(...) auto const d = d_func()
#undef Q_Q
#define Q_Q(...) auto const q = q_func()

#endif // BUILTIN_COMMON_H
