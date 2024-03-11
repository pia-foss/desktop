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

#pragma once
#include <kapps_core/core.h>
#include "typename.h"

namespace kapps { namespace core {

namespace detail_
{
    KAPPS_CORE_EXPORT void traceGuardException(const std::exception &ex);
    KAPPS_CORE_EXPORT void verify(const void *p, const StringSlice name);
    KAPPS_CORE_EXPORT void verify(const void *p, size_t count, const StringSlice name);
}

// Common guards used to implement external C APIs

// Verify that any pointer is non-null, for example:
//
//   int KACFooGetIndex(const KACFoo *pFoo)
//   {
//       return guard([&]
//       {
//           verify(pFoo);
//           return pFoo->getIndex();
//       });
//   }
//
// If nullptr was passed, guard() traces a warning and throws.  The trace
// includes the type of the pointer ("KACFoo"), which is usually sufficient
// context, but a trace name can be specified too if needed.
template<class T>
void verify(const T *p, const StringSlice name = typeName<T>())
{
    detail_::verify(p, name);
}

// Verify that an array pointer is non-null _unless_ count is 0.
template<class T>
void verify(const T *p, size_t count, const StringSlice valueName = typeName<T>())
{
    detail_::verify(p, count, valueName);
}

// Default fail results for guard() based on type.  For now everything returns
// T{}; might specialize to -1 for signed integer types; etc.
template<class T>
T defaultFailResult() { return {}; }

// Handle C++ exceptions occurring in an API call implementation.  C++
// exceptions are the preferred way of handling errors internally, but we do not
// propagate them through APIs.  If an exception occurs, it is traced and
// failResult is returned - by default, defaultFailResult<RetT>(), which is
// -1 for signed arithmetic types and RetT{} otherwise.
//
// For example:
//
//   KACIPv4Address KARServerIpAddress(const KARServer *pServer)
//   {
//       return guard([&]
//       {
//           verify(pServer);
//           return toApi(pServer->address());
//       });
//   }
template<class ImplFuncT>
auto guard(ImplFuncT impl,
            typename std::result_of<ImplFuncT()>::type
             failResult = defaultFailResult<typename std::result_of<ImplFuncT()>::type>())
    -> typename std::result_of<ImplFuncT()>::type
{
    try
    {
        return std::move(impl)();
    }
    catch(const std::exception &ex)
    {
        detail_::traceGuardException(ex);
    }
    return failResult;
}

template<class ImplFuncT>
auto guard(ImplFuncT impl)
    -> typename std::enable_if<std::is_void<typename std::result_of<ImplFuncT()>::type>::value, void>::type
{
    try
    {
        std::move(impl)();
    }
    catch(const std::exception &ex)
    {
        detail_::traceGuardException(ex);
    }
}

// Guard against exceptions _and_ verify the 'this' pointer for the API call at
// the same time.
//
// It's extremely common for API methods to take a 'this' pointer, and it's even
// very common for that to be the only parameter (i.e. virtually all "getters").
//
// This abbreviates a guard() and verify() call.  The first parameter can be
// anything for which verify(thisParam) is valid.
//
// For example:
//
//   KACIPv4Address KARServerIpAddress(const KARServer *pServer)
//   {
//       return guard(pServer, [&] {return toApi(pServer->address());});
//   }
template<class ThisParamT, class ImplFuncT>
auto guard(ThisParamT thisParam, ImplFuncT impl,
            typename std::result_of<ImplFuncT()>::type
             failResult = defaultFailResult<typename std::result_of<ImplFuncT()>::type>())
    -> typename std::result_of<ImplFuncT()>::type
{
    return guard([&thisParam, impl = std::move(impl)]
    {
        verify(thisParam);
        return std::move(impl)();
    });
}

template<class ThisParamT, class ImplFuncT>
auto guard(ThisParamT thisParam, ImplFuncT impl)
    -> typename std::enable_if<std::is_void<typename std::result_of<ImplFuncT()>::type>::value, void>::type
{
    guard([&thisParam, impl = std::move(impl)]
    {
        verify(thisParam);
        std::move(impl)();
    });
}

}}
