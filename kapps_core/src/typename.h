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
#include "stringslice.h"

namespace kapps { namespace core {

// These utilities find, at compile time:
// - the name of an arbitrary type
// - the name of any enum value
//
// Neither of these are normally available, but by slipping them into a template
// function's name, we can slice them out of the generated function namees.
// This is a little off-book but is reliable on the compilers we target.
//
// NOTE: The results of this in some cases vary across compilers (such as
// "Foo" versus "class Foo", etc.).  This is fine for tracing, but consider this
// before using these values in program logic.

namespace detail_
{
    // Get a function decorated name containing a type.
    template<class T>
    StringSlice typeFunc()
    {
#if defined(_MSC_VER)
        return __FUNCSIG__;
#else
        return __PRETTY_FUNCTION__;
#endif
    }

    // Get a function decorated name containing an enum value.  This also
    // includes the type; there's no way to eliminate that.
    template<class E, E V>
    StringSlice enumValueFunc()
    {
#if defined(_MSC_VER)
        return __FUNCSIG__;
#else
        return __PRETTY_FUNCTION__;
#endif
    }

    // Object to find the prefix/suffix length for prototype values and then
    // use them to slice the actual values out of the template function name.
    class KAPPS_CORE_EXPORT NameSlicer
    {
    public:
        NameSlicer();
    public:
        StringSlice sliceType(StringSlice typeFuncName) const;
        StringSlice sliceEnumValue(StringSlice enumName, StringSlice enumValueFuncName) const;

    private:
        unsigned _typePrefixLen, _typeSuffixLen;
        unsigned _enumValPrefixLen, _enumValSuffixLen;
    };

    // The single name slicer
    KAPPS_CORE_EXPORT const NameSlicer &nameSlicer();
}

// Get the name of a type.  Returns a value like "int", "Foo", or "class Foo" -
// note that class/struct keywords vary by compiler.
template<class T>
StringSlice typeName()
{
    return detail_::nameSlicer().sliceType(detail_::typeFunc<T>());
}

// Get the name of an enumeration value known at compile time.  Returns a value
// like "Read", "Type::Read", etc. - the qualification varies by compiler and
// type of enumeration.  For invalid values, usually returns a value like "15"
// (a stringified interger) or "(enum Type)(15)" (stringified type name and
// integer).

template<class E, E V>
StringSlice enumValueName()
{
    return detail_::nameSlicer().sliceEnumValue(typeName<E>(),
        detail_::enumValueFunc<E, V>());
}

// Get the name of an enumeration value known at runtime.
// TODO - Right now this just switches over numeric values 0-9.  It should
// be possible to detect the actual last valid value by determining if a numeric
// value corresponds to a real value.
template<class E>
StringSlice enumValueName(E value)
{
    // If we used a switch for this, clang generates "case value not in
    // enumerated type" warnings, which are reasonable normally but not here
    if(value == static_cast<E>(0))
        return enumValueName<E, static_cast<E>(0)>();
    if(value == static_cast<E>(1))
        return enumValueName<E, static_cast<E>(1)>();
    if(value == static_cast<E>(2))
        return enumValueName<E, static_cast<E>(2)>();
    if(value == static_cast<E>(3))
        return enumValueName<E, static_cast<E>(3)>();
    if(value == static_cast<E>(4))
        return enumValueName<E, static_cast<E>(4)>();
    if(value == static_cast<E>(5))
        return enumValueName<E, static_cast<E>(5)>();
    if(value == static_cast<E>(6))
        return enumValueName<E, static_cast<E>(6)>();
    if(value == static_cast<E>(7))
        return enumValueName<E, static_cast<E>(7)>();
    if(value == static_cast<E>(8))
        return enumValueName<E, static_cast<E>(8)>();
    if(value == static_cast<E>(9))
        return enumValueName<E, static_cast<E>(9)>();

    return {};
}

}}
